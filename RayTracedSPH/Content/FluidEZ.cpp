//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "FluidEZ.h"

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
const float POOL_SPACE_DIVISION = 64.0f;
const float INIT_PARTICLE_VOLUME_DIM = 0.5f;
const float INIT_PARTICLE_VOLUME_CENTER[3] = { 0.0f, POOL_VOLUME_DIM - INIT_PARTICLE_VOLUME_DIM * 0.5f, 0.0f };
const float PARTICLE_REST_DENSITY = 1000.0f;
const float PARTICLE_SMOOTH_RADIUS = POOL_VOLUME_DIM / POOL_SPACE_DIVISION;

struct CBSimulation
{
	uint32_t	NumParticles;
	float		SmoothRadius;
	float		PressureStiffness;
	float		RestDensity;
	float		DensityCoef;
	float		PressureGradCoef;
	float		ViscosityLaplaceCoef;
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
	m_shaderPool = ShaderPool::MakeUnique();
}

FluidEZ::~FluidEZ()
{
}

bool FluidEZ::Init(RayTracing::EZ::CommandList* pCommandList, uint32_t width, uint32_t height,
	Format rtFormat, Format dsFormat, vector<Resource::uptr>& uploaders,
	GeometryBuffer* pGeometry, uint32_t numParticles)
{
	const auto pDevice = pCommandList->GetRTDevice();

	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);
	m_numParticles = numParticles;

	// Create resources with data upload
	createParticleBuffers(pCommandList, uploaders);
	createConstBuffer(pCommandList, uploaders);

	// Create density buffer
	m_densityBuffer = TypedBuffer::MakeUnique();
	XUSG_N_RETURN(m_densityBuffer->Create(pCommandList->GetDevice(), m_numParticles, sizeof(float), Format::R32_FLOAT,
		ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);
	uploaders.emplace_back(Resource::MakeUnique());

	XUSG_N_RETURN(buildAccelerationStructures(pCommandList, pDevice, pGeometry), false);
	XUSG_N_RETURN(createShaders(), false);

	return true;
}

void FluidEZ::UpdateFrame(uint8_t frameIndex, CXMVECTOR eyePt, CXMMATRIX viewProj)
{
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

	computeDensity(pCommandList, frameIndex);
}

void FluidEZ::Visualize(RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex,
	RenderTarget* pRenderTarget, DepthStencil* pDepthStencil)
{
}

bool FluidEZ::createParticleBuffers(RayTracing::EZ::CommandList* pCommandList, vector<Resource::uptr>& uploaders)
{
	// Init data
	vector<Particle> particles(m_numParticles);
	vector<ParticleAABB> particleAABBs(m_numParticles);

	const float smoothRadius = PARTICLE_SMOOTH_RADIUS;
	const auto dimSize = static_cast<uint32_t>(ceil(std::cbrt(m_numParticles)));
	const auto sliceSize = dimSize * dimSize;
	for (auto i = 0u; i < m_numParticles; ++i)
	{
		auto x = static_cast<float>((i / sliceSize % dimSize) / dimSize);
		auto z = static_cast<float>((i / sliceSize / dimSize) / dimSize);
		auto y = static_cast<float>((i % sliceSize / dimSize) / dimSize);
		x = INIT_PARTICLE_VOLUME_DIM * (x - 0.5f) + INIT_PARTICLE_VOLUME_CENTER[0];
		y = INIT_PARTICLE_VOLUME_DIM * (y - 0.5f) + INIT_PARTICLE_VOLUME_CENTER[1];
		z = INIT_PARTICLE_VOLUME_DIM * (z - 0.5f) + INIT_PARTICLE_VOLUME_CENTER[2];

		particles[i].Pos = XMFLOAT3(x, y, z);
		particles[i].Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

		// AABB
		particleAABBs[i].Min = XMFLOAT3(particles[i].Pos.x - smoothRadius, particles[i].Pos.y - smoothRadius, particles[i].Pos.z - smoothRadius);
		particleAABBs[i].Max = XMFLOAT3(particles[i].Pos.x + smoothRadius, particles[i].Pos.y + smoothRadius, particles[i].Pos.z + smoothRadius);
	}

	// Create particle buffer
	m_particleBuffer = StructuredBuffer::MakeUnique();
	XUSG_N_RETURN(m_particleBuffer->Create(pCommandList->GetDevice(), m_numParticles, sizeof(Particle),
		ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);
	uploaders.emplace_back(Resource::MakeUnique());

	// upload data to the particle buffer
	XUSG_N_RETURN(m_particleBuffer->Upload(pCommandList->AsCommandList(), uploaders.back().get(), particles.data(),
		sizeof(Particle) * m_numParticles), false);

	// Create particle AABB buffer
	m_particleAABBBuffer = VertexBuffer::MakeUnique();
	XUSG_N_RETURN(m_particleAABBBuffer->Create(pCommandList->GetDevice(), m_numParticles, sizeof(ParticleAABB),
		ResourceFlag::NONE, MemoryType::DEFAULT), false);
	uploaders.emplace_back(Resource::MakeUnique());

	// upload data to the AABB buffer
	XUSG_N_RETURN(m_particleAABBBuffer->Upload(pCommandList->AsCommandList(), uploaders.back().get(), particleAABBs.data(),
		sizeof(ParticleAABB) * m_numParticles), false);

	return true;
}

bool FluidEZ::createConstBuffer(XUSG::RayTracing::EZ::CommandList* pCommandList, vector<Resource::uptr>& uploaders)
{
	// Create constant buffer
	m_cbSimulation = ConstantBuffer::MakeUnique();
	XUSG_N_RETURN(m_cbSimulation->Create(pCommandList->GetDevice(), sizeof(CBSimulation), FrameCount,
		nullptr, MemoryType::DEFAULT), false);

	// Init constant data
	CBSimulation cbSimulation;
	{
		cbSimulation.SmoothRadius = PARTICLE_SMOOTH_RADIUS;
		cbSimulation.PressureStiffness = 200.0f;
		cbSimulation.RestDensity = PARTICLE_REST_DENSITY;

		const float initVolume = INIT_PARTICLE_VOLUME_DIM * INIT_PARTICLE_VOLUME_DIM * INIT_PARTICLE_VOLUME_DIM;
		const float mass = cbSimulation.RestDensity * initVolume / m_numParticles;
		const float viscosity = 0.1f;
		cbSimulation.NumParticles = m_numParticles;
		cbSimulation.DensityCoef = mass * 315.0f / (64.0f * XM_PI * pow(cbSimulation.SmoothRadius, 9.0f));
		cbSimulation.PressureGradCoef = mass * -45.0f / (XM_PI * pow(cbSimulation.SmoothRadius, 6.0f));
		cbSimulation.ViscosityLaplaceCoef = mass * viscosity * 45.0f / (XM_PI * pow(cbSimulation.SmoothRadius, 6.0f));
	}

	// Upload data to cbuffer
	return m_cbSimulation->Upload(pCommandList->AsCommandList(),
		uploaders.back().get(), &cbSimulation, sizeof(cbSimulation));
}

bool FluidEZ::createShaders()
{
	auto vsIndex = 0u;
	auto psIndex = 0u;
	auto csIndex = 0u;

	XUSG_N_RETURN(m_shaderPool->CreateShader(Shader::Stage::CS, csIndex, L"RTDensity.cso"), false);
	m_shaders[RT_DENSITY] = m_shaderPool->GetShader(Shader::Stage::CS, csIndex++);

	return true;
}

bool FluidEZ::buildAccelerationStructures(RayTracing::EZ::CommandList* pCommandList,
	const RayTracing::Device* pDevice, GeometryBuffer* pGeometry)
{
	AccelerationStructure::SetFrameCount(FrameCount);

	// Set geometries
	GeometryFlag geometryFlag = GeometryFlag::NONE; // Any hit needs non-opaque (the default flag is opaque only)
	BottomLevelAS::SetAABBGeometries(*pGeometry, 1, &m_particleAABBBuffer->GetVBV(), &geometryFlag);

	// Prebuild
	m_bottomLevelAS = BottomLevelAS::MakeUnique();
	m_topLevelAS = TopLevelAS::MakeUnique();
	XUSG_N_RETURN(pCommandList->PreBuildBLAS(m_bottomLevelAS.get(), 1, *pGeometry, BuildFlag::PREFER_FAST_BUILD), false);
	XUSG_N_RETURN(pCommandList->PreBuildTLAS(m_topLevelAS.get(), 1), false);

	// Set instance
	XMFLOAT3X4 matrix;
	XMStoreFloat3x4(&matrix, XMMatrixIdentity());
	float* const pTransform[] = { reinterpret_cast<float*>(&matrix) };
	m_instances = Resource::MakeUnique();
	const BottomLevelAS* const ppBottomLevelAS[] = { m_bottomLevelAS.get() };
	TopLevelAS::SetInstances(pDevice, m_instances.get(), 1, &ppBottomLevelAS[0], &pTransform[0]);

	// Build bottom level ASs
	pCommandList->BuildBLAS(m_bottomLevelAS.get());

	// Build top level AS
	pCommandList->BuildTLAS(m_topLevelAS.get(), m_instances.get());

	return true;
}

void FluidEZ::computeDensity(RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex)
{
	// Set pipeline state
	pCommandList->RTSetShaderLibrary(m_shaders[RT_DENSITY]);
	pCommandList->RTSetHitGroup(0, HitGroupName, nullptr, AnyHitShaderName, IntersectionShaderName, HitGroupType::PROCEDURAL);
	pCommandList->RTSetShaderConfig(sizeof(float), sizeof(float));
	pCommandList->RTSetMaxRecursionDepth(1);

	// Set TLAS
	pCommandList->SetTopLevelAccelerationStructure(0, m_topLevelAS.get());

	// Set UAV
	const auto uav = XUSG::EZ::GetUAV(m_densityBuffer.get());
	pCommandList->SetComputeResources(DescriptorType::UAV, 0, 1, &uav, 0);

	// Set SRV
	const auto srv = XUSG::EZ::GetSRV(m_particleBuffer.get());
	pCommandList->SetComputeResources(DescriptorType::SRV, 0, 1, &srv, 0);

	// Set CBV
	const auto cbv = XUSG::EZ::GetCBV(m_cbSimulation.get());
	pCommandList->SetComputeResources(DescriptorType::CBV, 0, 1, &cbv, 0);

	// Dispatch command
	pCommandList->DispatchRays(m_numParticles, 1, 1, RaygenShaderName, MissShaderName);
}
