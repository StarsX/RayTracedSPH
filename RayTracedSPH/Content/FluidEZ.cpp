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

const float POOL_VOLUME_CENTER[3] = { 0.0f, 0.75f, 0.0f };
const float POOL_VOLUME_DIM_SIZE = 0.5f;
const float POOL_VOLUME_SIZE = POOL_VOLUME_DIM_SIZE * POOL_VOLUME_DIM_SIZE * POOL_VOLUME_DIM_SIZE;
const float POOL_VOLUME_DENSITY = 1000.0f;
const float POOL_VOLUME_MASS = POOL_VOLUME_SIZE * POOL_VOLUME_DENSITY;

struct CBSimulation
{
	uint32_t	numParticles;
	float		smoothRadius;
	float		pressureStiffness;
	float		restDensity;
	float		densityCoef;
	float		pressureGradCoef;
	float		viscosityLaplaceCoef;
};

struct Particle
{
	XMFLOAT3 Pos;
	XMFLOAT3 Velocity;
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

	// Create resources
	createParticleBuffer(pCommandList, uploaders);
	createConstBuffer(pCommandList, uploaders);

	// Create density buffer
	m_densityBuffer = TypedBuffer::MakeUnique();
	XUSG_N_RETURN(m_densityBuffer->Create(pCommandList->GetDevice(), m_numParticles, sizeof(float), Format::R32_FLOAT,
		ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);
	uploaders.emplace_back(Resource::MakeUnique());

	//XUSG_N_RETURN(buildAccelerationStructures(pCommandList, pDevice, pGeometry), false);
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
	computeDensity(pCommandList, frameIndex);
}

void FluidEZ::Visualize(RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex,
	RenderTarget* pRenderTarget, DepthStencil* pDepthStencil)
{
}

bool FluidEZ::createParticleBuffer(RayTracing::EZ::CommandList* pCommandList, vector<Resource::uptr>& uploaders)
{
	m_particleBuffer = StructuredBuffer::MakeUnique();
	XUSG_N_RETURN(m_particleBuffer->Create(pCommandList->GetDevice(), m_numParticles, sizeof(Particle),
		ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);
	uploaders.emplace_back(Resource::MakeUnique());

	vector<Particle> particles(m_numParticles);

	const uint32_t dimSize = static_cast<uint32_t>(ceil(std::cbrt(m_numParticles)));
	const uint32_t sliceSize = dimSize * dimSize;
	for (uint32_t i = 0; i < m_numParticles; ++i)
	{
		float x = (float)(i / sliceSize % dimSize) / dimSize;
		float z = (float)(i / sliceSize / dimSize) / dimSize;
		float y = (float)(i % sliceSize / dimSize) / dimSize;
		x = POOL_VOLUME_DIM_SIZE * (x - 0.5f) + POOL_VOLUME_CENTER[0];
		y = POOL_VOLUME_DIM_SIZE * (y - 0.5f) + POOL_VOLUME_CENTER[1];
		z = POOL_VOLUME_DIM_SIZE * (z - 0.5f) + POOL_VOLUME_CENTER[2];

		particles[i].Pos = { x, y, z };
		particles[i].Velocity = { 0.0f, 0.0f, 0.0f };
	}

	XUSG_N_RETURN(m_particleBuffer->Upload(pCommandList->AsCommandList(), uploaders.back().get(), particles.data(),
		sizeof(Particle) * m_numParticles), false);

	return true;
}

bool FluidEZ::createConstBuffer(XUSG::RayTracing::EZ::CommandList* pCommandList, vector<Resource::uptr>& uploaders)
{
	m_cbSimulation = ConstantBuffer::MakeUnique();
	XUSG_N_RETURN(m_cbSimulation->Create(pCommandList->GetDevice(), sizeof(CBSimulation), FrameCount,
		nullptr, MemoryType::DEFAULT), false);

	CBSimulation cbSimulation;
	{
		cbSimulation.smoothRadius = POOL_VOLUME_SIZE / 64;
		cbSimulation.pressureStiffness = 200.0f;
		cbSimulation.restDensity = POOL_VOLUME_DENSITY;

		const float mass = POOL_VOLUME_MASS / m_numParticles;
		const float viscosity = 0.1f;
		cbSimulation.numParticles = m_numParticles;
		cbSimulation.densityCoef = mass * 315.0f / (64.0f * XM_PI * pow(cbSimulation.smoothRadius, 9.0f));
		cbSimulation.pressureGradCoef = mass * -45.0f / (XM_PI * pow(cbSimulation.smoothRadius, 6.0f));
		cbSimulation.viscosityLaplaceCoef = mass * viscosity * 45.0f / (XM_PI * pow(cbSimulation.smoothRadius, 6.0f));
	}

	return m_cbSimulation->Upload(pCommandList->AsCommandList(), uploaders.back().get(), &cbSimulation,
		sizeof(cbSimulation));
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
	//BottomLevelAS::SetTriangleGeometries(*pGeometry, 1, Format::R32G32B32_FLOAT,
		//&m_vertexBuffer->GetVBV(), &m_indexBuffer->GetIBV());

	// Prebuild
	m_bottomLevelAS = BottomLevelAS::MakeUnique();
	m_topLevelAS = TopLevelAS::MakeUnique();
	XUSG_N_RETURN(pCommandList->PreBuildBLAS(m_bottomLevelAS.get(), 1, *pGeometry), false);
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
	pCommandList->RTSetHitGroup(0, HitGroupName, nullptr, AnyHitShaderName, IntersectionShaderName);
	// ...
	pCommandList->RTSetShaderConfig(sizeof(XMFLOAT4), sizeof(XMFLOAT2));
	pCommandList->RTSetMaxRecursionDepth(1);

	// Set TLAS
	pCommandList->SetTopLevelAccelerationStructure(0, m_topLevelAS.get());

	// Set UAV
	const auto uav = XUSG::EZ::GetUAV(m_densityBuffer.get());
	pCommandList->SetComputeResources(DescriptorType::UAV, 0, 1, &uav, 0);

	// Set SRVs
	const auto srv = XUSG::EZ::GetSRV(m_particleBuffer.get());
	pCommandList->SetComputeResources(DescriptorType::SRV, 0, 1, &srv, 0);

	// Dispatch command
	//pCommandList->DispatchRays(m_numParticles, 1, 1, RaygenShaderName, MissShaderName);
}
