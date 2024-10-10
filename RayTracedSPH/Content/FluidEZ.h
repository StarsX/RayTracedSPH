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
		std::vector<XUSG::Resource::uptr>& uploaders, uint32_t numParticles = 65536);

	void UpdateFrame(uint8_t frameIndex, float timeStep, DirectX::CXMMATRIX viewProj, DirectX::CXMVECTOR viewY);
	void Render(XUSG::RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex,
		XUSG::RenderTarget* pRenderTarget, XUSG::DepthStencil* pDepthStencil);
	void Simulate(XUSG::RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex);
	void Visualize(XUSG::RayTracing::EZ::CommandList* pCommandList, uint8_t frameIndex,
		XUSG::RenderTarget* pRenderTarget, XUSG::DepthStencil* pDepthStencil);

	static const uint8_t FrameCount = 3;

protected:
	bool createParticleBuffers(XUSG::RayTracing::EZ::CommandList* pCommandList, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createConstBuffers(XUSG::RayTracing::EZ::CommandList* pCommandList, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createShaders();
	bool buildAccelerationStructures(XUSG::RayTracing::EZ::CommandList* pCommandList);

	void computeDensity(XUSG::RayTracing::EZ::CommandList* pCommandList);
	void computeAcceleration(XUSG::RayTracing::EZ::CommandList* pCommandList);
	void integrate(XUSG::RayTracing::EZ::CommandList* pCommandList);

	XUSG::RayTracing::BottomLevelAS::uptr m_bottomLevelAS;
	XUSG::RayTracing::TopLevelAS::uptr m_topLevelAS;

	XUSG::StructuredBuffer::uptr	m_particleBuffer;
	XUSG::VertexBuffer::uptr		m_particleAABBBuffer;
	XUSG::TypedBuffer::uptr			m_densityBuffer;
	XUSG::TypedBuffer::uptr			m_accelerationBuffer;
	XUSG::ConstantBuffer::uptr		m_cbSimulation;
	XUSG::ConstantBuffer::uptr		m_cbPerFrame;
	XUSG::ConstantBuffer::uptr		m_cbVisualization;

	XUSG::RayTracing::GeometryBuffer m_geometry;
	XUSG::Buffer::uptr m_instances;

	// Shader tables
	static const wchar_t* HitGroupName;
	static const wchar_t* RaygenShaderName;
	static const wchar_t* IntersectionShaderName;
	static const wchar_t* AnyHitShaderName;
	static const wchar_t* MissShaderName;

	enum ShaderIndex : uint8_t
	{
		RT_DENSITY,
		RT_FORCE,
		CS_INTEGRATE,
		VS_DRAW_PARTICLES,
		PS_DRAW_PARTICLES,

		NUM_SHADER
	};

	XUSG::ShaderLib::uptr	m_shaderLib;
	XUSG::Blob m_shaders[NUM_SHADER];

	DirectX::XMFLOAT2		m_viewport;

	uint32_t				m_numParticles;
};
