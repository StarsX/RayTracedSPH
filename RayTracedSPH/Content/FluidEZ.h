//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Core/XUSG.h"
#include "Helper/XUSGRayTracing-EZ.h"
#include "RayTracing/XUSGRayTracing.h"

class FluidEZ
{
public:
	FluidEZ();
	virtual ~FluidEZ();

	bool Init(XUSG::RayTracing::EZ::CommandList* pCommandList, uint32_t width, uint32_t height,
		XUSG::Format rtFormat, XUSG::Format dsFormat, std::vector<XUSG::Resource::uptr>& uploaders,
		XUSG::RayTracing::GeometryBuffer* pGeometry, uint32_t numParticles = 65536);

	void UpdateFrame(uint8_t frameIndex, DirectX::CXMVECTOR eyePt, DirectX::CXMMATRIX viewProj);
	void Render(XUSG::RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex,
		XUSG::RenderTarget* pRenderTarget, XUSG::DepthStencil* pDepthStencil);
	void Simulate(XUSG::RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex);
	void Visualize(XUSG::RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex,
		XUSG::RenderTarget* pRenderTarget, XUSG::DepthStencil* pDepthStencil);

	static const uint8_t FrameCount = 3;

protected:
	bool createParticleBuffer();
	bool createConstBuffer();
	bool createShaders();
	bool buildAccelerationStructures(XUSG::RayTracing::EZ::CommandList* pCommandList,
		const XUSG::RayTracing::Device* pDevice,
		XUSG::RayTracing::GeometryBuffer* pGeometries);

	void computeDensity(XUSG::RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex);

	XUSG::RayTracing::BottomLevelAS::uptr m_bottomLevelAS;
	XUSG::RayTracing::TopLevelAS::uptr m_topLevelAS;

	XUSG::Resource::uptr m_instances;

	// Shader tables
	static const wchar_t* HitGroupName;
	static const wchar_t* RaygenShaderName;
	static const wchar_t* IntersectionShaderName;
	static const wchar_t* AnyHitShaderName;
	static const wchar_t* MissShaderName;

	enum ShaderIndex : uint8_t
	{
		RT_DENSITY,

		NUM_SHADER
	};

	XUSG::ShaderPool::uptr	m_shaderPool;
	XUSG::Blob m_shaders[NUM_SHADER];

	DirectX::XMFLOAT2		m_viewport;

	uint32_t				m_numParticles;
};
