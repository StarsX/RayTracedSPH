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
const wchar_t* FluidEZ::ClosestHitShaderName = L"closestHitMain";
const wchar_t* FluidEZ::MissShaderName = L"missMain";

FluidEZ::FluidEZ() :
	m_instances()
{
	m_shaderPool = ShaderPool::MakeUnique();
}

FluidEZ::~FluidEZ()
{
}

bool FluidEZ::Init(RayTracing::EZ::CommandList* pCommandList, uint32_t width, uint32_t height,
	Format rtFormat, Format dsFormat, vector<Resource::uptr>& uploaders, GeometryBuffer* pGeometry)
{
	const auto pDevice = pCommandList->GetRTDevice();

	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);

	//XUSG_N_RETURN(buildAccelerationStructures(pCommandList, pDevice, pGeometry), false);
	//XUSG_N_RETURN(createShaders(), false);

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
	pCommandList->RTSetHitGroup(0, HitGroupName, ClosestHitShaderName);
	// ...
	pCommandList->RTSetShaderConfig(sizeof(XMFLOAT4), sizeof(XMFLOAT2));
	pCommandList->RTSetMaxRecursionDepth(1);

	// Set TLAS
	pCommandList->SetTopLevelAccelerationStructure(0, m_topLevelAS.get());

	// Set UAV

	// Set SRVs

	// Dispatch command
	//pCommandList->DispatchRays(m_numParticles, 1, 1, RaygenShaderName, MissShaderName);
}
