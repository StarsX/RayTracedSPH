//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "FluidEZ.h"
#include "SharedConst.h"

using namespace std;
using namespace DirectX;
using namespace XUSG;
using namespace XUSG::RayTracing;

const wchar_t* FluidEZ::HitGroupName = L"hitGroup";
const wchar_t* FluidEZ::RaygenShaderName = L"raygenMain";
const wchar_t* FluidEZ::IntersectionShaderName = L"intersectionMain";
const wchar_t* FluidEZ::AnyHitShaderName = L"anyHitMain";
const wchar_t* FluidEZ::MissShaderName = L"missMain";

const float POOL_VOLUME_DIM = 1.0f;
const float POOL_SPACE_DIVISION = 50.0f;
const float INIT_PARTICLE_VOLUME_DIM = 0.6f;
const float INIT_PARTICLE_VOLUME_CENTER[] =
{
	-0.45f * (POOL_VOLUME_DIM - INIT_PARTICLE_VOLUME_DIM),
	POOL_VOLUME_DIM - INIT_PARTICLE_VOLUME_DIM * 0.5f,
	0.45f * (POOL_VOLUME_DIM - INIT_PARTICLE_VOLUME_DIM)
};
const float PARTICLE_REST_DENSITY = 1000.0f;
const float PARTICLE_SMOOTH_RADIUS = POOL_VOLUME_DIM / POOL_SPACE_DIVISION;

struct CBSimulation
{
	float SmoothRadius;
	float PressureStiffness;
	float RestDensity;
	float DensityCoef;
	float PressureGradCoef;
	float ViscosityLaplaceCoef;
	float WallStiffness;
	uint32_t NumParticles; // Padding
	XMFLOAT4 Planes[6];
};

struct CBVisualization
{
	XMFLOAT4X4 ViewProj;
};

struct CBPerFrame
{
	float TimeStep;
	XMFLOAT3 Gravity;
};

struct Particle
{
	XMFLOAT3 Pos;
	XMFLOAT3 Velocity;
};

struct ParticleAABB
{
	XMFLOAT3 Min;
	XMFLOAT3 Max;
};

FluidEZ::FluidEZ() :
	m_instances()
{
	m_shaderLib = ShaderLib::MakeUnique();
}

FluidEZ::~FluidEZ()
{
}

bool FluidEZ::Init(RayTracing::EZ::CommandList* pCommandList, uint32_t width, uint32_t height,
	vector<Resource::uptr>& uploaders, uint32_t numParticles)
{
	const auto pDevice = pCommandList->GetRTDevice();

	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);
	m_numParticles = numParticles;

	// Create resources with data upload
	createParticleBuffers(pCommandList, uploaders);
	createConstBuffers(pCommandList, uploaders);

	// Create density buffer
	m_densityBuffer = TypedBuffer::MakeUnique();
	XUSG_N_RETURN(m_densityBuffer->Create(pDevice, m_numParticles, sizeof(float), Format::R32_FLOAT,
		ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);

	// Create particle Acceleration buffer
	m_accelerationBuffer = TypedBuffer::MakeUnique();
	XUSG_N_RETURN(m_accelerationBuffer->Create(pDevice, m_numParticles, sizeof(uint16_t[4]), Format::R16G16B16A16_FLOAT,
		ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);

	XUSG_N_RETURN(buildAccelerationStructures(pCommandList), false);
	XUSG_N_RETURN(createShaders(), false);

	return true;
}

void FluidEZ::UpdateFrame(uint8_t frameIndex, float timeStep, CXMMATRIX viewProj, CXMVECTOR viewY)
{
	const auto gravity = -9.8f * viewY;
	const auto pCbPerFrame = reinterpret_cast<CBPerFrame*>(m_cbPerFrame->Map(frameIndex));
	pCbPerFrame->TimeStep = timeStep;
	XMStoreFloat3(&pCbPerFrame->Gravity, gravity);

	const auto pCbVisualization = reinterpret_cast<CBVisualization*>(m_cbVisualization->Map(frameIndex));
	XMStoreFloat4x4(&pCbVisualization->ViewProj, XMMatrixTranspose(viewProj));
}

void FluidEZ::Render(RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex,
	RenderTarget* pRenderTarget, DepthStencil* pDepthStencil)
{
	Simulate(pCommandList, frameIndex);
	Visualize(pCommandList, frameIndex, pRenderTarget, pDepthStencil);
}

void FluidEZ::Simulate(RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex)
{
	pCommandList->BuildBLAS(m_bottomLevelAS.get());
	pCommandList->BuildTLAS(m_topLevelAS.get(), m_instances.get());

	// Set TLAS
	pCommandList->SetTopLevelAccelerationStructure(0, m_topLevelAS.get());

	// Set CBV
	const XUSG::EZ::ResourceView cbvs[] =
	{
		XUSG::EZ::GetCBV(m_cbSimulation.get()),
		XUSG::EZ::GetCBV(m_cbPerFrame.get(), frameIndex)
	};
	pCommandList->SetResources(Shader::Stage::CS, DescriptorType::CBV, 0, static_cast<uint32_t>(size(cbvs)), cbvs);

	computeDensity(pCommandList);
	computeAcceleration(pCommandList);
	integrate(pCommandList);
}

void FluidEZ::Visualize(RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex,
	RenderTarget* pRenderTarget, DepthStencil* pDepthStencil)
{
	// Set pipeline state
	pCommandList->SetGraphicsShader(Shader::Stage::VS, m_shaders[VS_DRAW_PARTICLES]);
	pCommandList->SetGraphicsShader(Shader::Stage::PS, m_shaders[PS_DRAW_PARTICLES]);
	pCommandList->DSSetState(XUSG::Graphics::DepthStencilPreset::DEFAULT_LESS);

	// Set render target
	const auto rtv = XUSG::EZ::GetRTV(pRenderTarget);
	const auto dsv = XUSG::EZ::GetDSV(pDepthStencil);
	pCommandList->OMSetRenderTargets(1, &rtv, &dsv);

	// Set CBV
	const auto cbv = XUSG::EZ::GetCBV(m_cbVisualization.get(), frameIndex);
	pCommandList->SetResources(Shader::Stage::VS, DescriptorType::CBV, 0, 1, &cbv);

	// Set SRV
	const auto srv = XUSG::EZ::GetSRV(m_particleBuffer.get());
	pCommandList->SetResources(Shader::Stage::VS, DescriptorType::SRV, 0, 1, &srv);

	// Set viewport
	Viewport viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	RectRange scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	pCommandList->RSSetViewports(1, &viewport);
	pCommandList->RSSetScissorRects(1, &scissorRect);

	// Set IA
	pCommandList->IASetPrimitiveTopology(PrimitiveTopology::POINTLIST);

	// Draw Command
	pCommandList->Draw(m_numParticles, 1, 0, 0);
}

bool FluidEZ::createParticleBuffers(RayTracing::EZ::CommandList* pCommandList, vector<Resource::uptr>& uploaders)
{
	const auto pDevice = pCommandList->GetDevice();

	// Init data
	vector<Particle> particles(m_numParticles);
	vector<ParticleAABB> particleAABBs(m_numParticles);

	const auto smoothRadius = PARTICLE_SMOOTH_RADIUS;
	const auto dimSize = static_cast<uint32_t>(ceil(std::cbrt(m_numParticles)));
	const auto slcSize = dimSize * dimSize;
	for (auto i = 0u; i < m_numParticles; ++i)
	{
		const auto n = i % slcSize;
		auto x = (n % dimSize) / static_cast<float>(dimSize);
		auto y = (n / dimSize) / static_cast<float>(dimSize);
		auto z = (i / slcSize) / static_cast<float>(dimSize);
		x = INIT_PARTICLE_VOLUME_DIM * (x - 0.5f) + INIT_PARTICLE_VOLUME_CENTER[0];
		y = INIT_PARTICLE_VOLUME_DIM * (y - 0.5f) + INIT_PARTICLE_VOLUME_CENTER[1];
		z = INIT_PARTICLE_VOLUME_DIM * (z - 0.5f) + INIT_PARTICLE_VOLUME_CENTER[2];

		particles[i].Pos = XMFLOAT3(x, y, z);
		particles[i].Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

		// AABB
		particleAABBs[i].Min.x = x - smoothRadius;
		particleAABBs[i].Max.x = x + smoothRadius;
		particleAABBs[i].Min.y = y - smoothRadius;
		particleAABBs[i].Max.y = y + smoothRadius;
#if POINT_QUERY
		particleAABBs[i].Min.z = z - smoothRadius;
		particleAABBs[i].Max.z = z + smoothRadius;
#else
		particleAABBs[i].Min.z = z - smoothRadius * 0.5f;
		particleAABBs[i].Max.z = z + smoothRadius * 0.5f;
#endif
	}

	// Create particle buffer
	m_particleBuffer = StructuredBuffer::MakeUnique();
	XUSG_N_RETURN(m_particleBuffer->Create(pDevice, m_numParticles, sizeof(Particle),
		ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);
	uploaders.emplace_back(Resource::MakeUnique());

	// upload data to the particle buffer
	XUSG_N_RETURN(m_particleBuffer->Upload(pCommandList->AsCommandList(), uploaders.back().get(), particles.data(),
		sizeof(Particle) * m_numParticles), false);

	// Create particle AABB buffer
	m_particleAABBBuffer = VertexBuffer::MakeUnique();
	XUSG_N_RETURN(m_particleAABBBuffer->Create(pDevice, m_numParticles, sizeof(ParticleAABB),
		ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT, 1, nullptr,
		1, nullptr, 1, nullptr, MemoryFlag::NONE, L"ParticleAABBs"), false);
	uploaders.emplace_back(Resource::MakeUnique());

	// upload data to the AABB buffer
	XUSG_N_RETURN(m_particleAABBBuffer->Upload(pCommandList->AsCommandList(), uploaders.back().get(), particleAABBs.data(),
		sizeof(ParticleAABB) * m_numParticles, 0, ResourceState::NON_PIXEL_SHADER_RESOURCE), false);

	return true;
}

bool FluidEZ::createConstBuffers(RayTracing::EZ::CommandList* pCommandList, vector<Resource::uptr>& uploaders)
{
	const auto pDevice = pCommandList->GetDevice();

	// Create constant buffers
	m_cbSimulation = ConstantBuffer::MakeUnique();
	XUSG_N_RETURN(m_cbSimulation->Create(pDevice, sizeof(CBSimulation), 1, nullptr, MemoryType::DEFAULT), false);

	m_cbPerFrame = ConstantBuffer::MakeUnique();
	XUSG_N_RETURN(m_cbPerFrame->Create(pDevice, sizeof(CBPerFrame[FrameCount]), FrameCount), false);

	m_cbVisualization = ConstantBuffer::MakeUnique();
	XUSG_N_RETURN(m_cbVisualization->Create(pDevice, sizeof(CBVisualization[FrameCount]), FrameCount), false);

	// Init constant data
	CBSimulation cbSimulation;
	{
		cbSimulation.SmoothRadius = PARTICLE_SMOOTH_RADIUS;
		cbSimulation.PressureStiffness = 200.0f;
		cbSimulation.RestDensity = PARTICLE_REST_DENSITY;
		cbSimulation.WallStiffness = 3000.0f;
		cbSimulation.NumParticles = m_numParticles;
		cbSimulation.Planes[0] = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
		cbSimulation.Planes[1] = XMFLOAT4(0.0f, -1.0f, 0.0f, POOL_VOLUME_DIM);
		cbSimulation.Planes[2] = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.5f * POOL_VOLUME_DIM);
		cbSimulation.Planes[3] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.5f * POOL_VOLUME_DIM);
		cbSimulation.Planes[4] = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.5f * POOL_VOLUME_DIM);
		cbSimulation.Planes[5] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.5f * POOL_VOLUME_DIM);

		const float initVolume = INIT_PARTICLE_VOLUME_DIM * INIT_PARTICLE_VOLUME_DIM * INIT_PARTICLE_VOLUME_DIM;
		const float mass = cbSimulation.RestDensity * initVolume / m_numParticles;
		const float viscosity = 0.4f;
		cbSimulation.DensityCoef = mass * 315.0f / (64.0f * XM_PI * pow(cbSimulation.SmoothRadius, 9.0f));
		cbSimulation.PressureGradCoef = mass * -45.0f / (XM_PI * pow(cbSimulation.SmoothRadius, 6.0f));
		cbSimulation.ViscosityLaplaceCoef = mass * viscosity * 45.0f / (XM_PI * pow(cbSimulation.SmoothRadius, 6.0f));
	}

	// Upload data to cbuffer
	uploaders.emplace_back(Resource::MakeUnique());

	return m_cbSimulation->Upload(pCommandList->AsCommandList(),
		uploaders.back().get(), &cbSimulation, sizeof(cbSimulation));
}

bool FluidEZ::createShaders()
{
	auto vsIndex = 0u;
	auto psIndex = 0u;
	auto csIndex = 0u;

	XUSG_X_RETURN(m_shaders[RT_DENSITY], m_shaderLib->CreateShader(
		Shader::Stage::CS, csIndex++, L"RTDensity.cso"), false);
	XUSG_X_RETURN(m_shaders[RT_FORCE], m_shaderLib->CreateShader(
		Shader::Stage::CS, csIndex++, L"RTForce.cso"), false);
	XUSG_X_RETURN(m_shaders[CS_INTEGRATE], m_shaderLib->CreateShader(
		Shader::Stage::CS, csIndex++, L"CSIntegrate.cso"), false);

	XUSG_X_RETURN(m_shaders[VS_DRAW_PARTICLES], m_shaderLib->CreateShader(
		Shader::Stage::VS, vsIndex++, L"VSDrawParticles.cso"), false);
	XUSG_X_RETURN(m_shaders[PS_DRAW_PARTICLES], m_shaderLib->CreateShader(
		Shader::Stage::PS, psIndex++, L"PSDrawParticles.cso"), false);

	return true;
}

bool FluidEZ::buildAccelerationStructures(RayTracing::EZ::CommandList* pCommandList)
{
	// Set geometries
	GeometryFlag geometryFlag = GeometryFlag::NONE; // Any hit needs non-opaque (the default flag is opaque only)
	BottomLevelAS::SetAABBGeometries(m_geometry, 1, &m_particleAABBBuffer->GetVBV(), &geometryFlag);

	// Prebuild
	m_bottomLevelAS = BottomLevelAS::MakeUnique();
	m_topLevelAS = TopLevelAS::MakeUnique();
	XUSG_N_RETURN(pCommandList->PreBuildBLAS(m_bottomLevelAS.get(), 1, m_geometry, BuildFlag::NONE), false);
	XUSG_N_RETURN(pCommandList->PreBuildTLAS(m_topLevelAS.get(), 1), false);

	// Set instance
	XMFLOAT3X4 matrix;
	XMStoreFloat3x4(&matrix, XMMatrixIdentity());
	float* const pTransform[] = { reinterpret_cast<float*>(&matrix) };
	m_instances = Resource::MakeUnique();
	const BottomLevelAS* const ppBottomLevelAS[] = { m_bottomLevelAS.get() };
	TopLevelAS::SetInstances(pCommandList->GetRTDevice(), m_instances.get(), 1, &ppBottomLevelAS[0], &pTransform[0]);

	// Build bottom level ASs
	pCommandList->BuildBLAS(m_bottomLevelAS.get());

	// Build top level AS
	pCommandList->BuildTLAS(m_topLevelAS.get(), m_instances.get());

	return true;
}

void FluidEZ::computeDensity(RayTracing::EZ::CommandList* pCommandList)
{
	// Set pipeline state
	static const wchar_t* shaderNames[] = { RaygenShaderName, IntersectionShaderName, AnyHitShaderName, MissShaderName };
	pCommandList->RTSetShaderLibrary(0, m_shaders[RT_DENSITY], static_cast<uint32_t>(size(shaderNames)), shaderNames);
	pCommandList->RTSetHitGroup(0, HitGroupName, nullptr, AnyHitShaderName, IntersectionShaderName, HitGroupType::PROCEDURAL);
	pCommandList->RTSetShaderConfig(sizeof(float), sizeof(float));
	pCommandList->RTSetMaxRecursionDepth(1);

	// Set UAV
	const auto uav = XUSG::EZ::GetUAV(m_densityBuffer.get());
	pCommandList->SetResources(Shader::Stage::CS, DescriptorType::UAV, 0, 1, &uav);

	// Set SRV
	const auto srv = XUSG::EZ::GetSRV(m_particleBuffer.get());
	pCommandList->SetResources(Shader::Stage::CS, DescriptorType::SRV, 0, 1, &srv);

	// Dispatch command
	pCommandList->DispatchRays(m_numParticles, 1, 1, RaygenShaderName, MissShaderName);
}

void FluidEZ::computeAcceleration(RayTracing::EZ::CommandList* pCommandList)
{
	// Set pipeline state
	static const void* shaders[] = { RaygenShaderName, IntersectionShaderName, AnyHitShaderName, MissShaderName };
	pCommandList->RTSetShaderLibrary(0, m_shaders[RT_FORCE]);//, static_cast<uint32_t>(size(shaders)), shaders);
	pCommandList->RTSetHitGroup(0, HitGroupName, nullptr, AnyHitShaderName, IntersectionShaderName, HitGroupType::PROCEDURAL);
	pCommandList->RTSetShaderConfig(sizeof(XMFLOAT4[2]), sizeof(XMFLOAT4));
	pCommandList->RTSetMaxRecursionDepth(1);

	// Set UAV
	const auto uav = XUSG::EZ::GetUAV(m_accelerationBuffer.get());
	pCommandList->SetResources(Shader::Stage::CS, DescriptorType::UAV, 0, 1, &uav);

	// Set SRVs
	const XUSG::EZ::ResourceView srvs[] =
	{
		XUSG::EZ::GetSRV(m_particleBuffer.get()),
		XUSG::EZ::GetSRV(m_densityBuffer.get())
	};
	pCommandList->SetResources(Shader::Stage::CS, DescriptorType::SRV, 0, static_cast<uint32_t>(size(srvs)), srvs);

	// Dispatch command
	pCommandList->DispatchRays(m_numParticles, 1, 1, RaygenShaderName, MissShaderName);
}

void FluidEZ::integrate(RayTracing::EZ::CommandList* pCommandList)
{
	// Set pipeline state
	pCommandList->SetComputeShader(m_shaders[CS_INTEGRATE]);

	// Set UAVs
	const XUSG::EZ::ResourceView uavs[] =
	{
		XUSG::EZ::GetUAV(m_particleBuffer.get()),
		XUSG::EZ::GetUAV(m_particleAABBBuffer.get())
	};
	pCommandList->SetResources(Shader::Stage::CS, DescriptorType::UAV, 0, static_cast<uint32_t>(size(uavs)), uavs);

	// Set SRV
	const auto srv = XUSG::EZ::GetSRV(m_accelerationBuffer.get());
	pCommandList->SetResources(Shader::Stage::CS, DescriptorType::SRV, 0, 1, &srv);

	// Dispatch command
	pCommandList->Dispatch(XUSG_DIV_UP(m_numParticles, GROUP_SIZE), 1, 1);
}
