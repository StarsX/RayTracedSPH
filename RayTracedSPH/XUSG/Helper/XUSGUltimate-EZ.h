//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSG-EZ.h"
#include "Ultimate/XUSGUltimate.h"

namespace XUSG
{
	namespace Ultimate
	{
		namespace EZ
		{
			XUSG_INTERFACE XUSG::EZ::ResourceView GetUAV(SamplerFeedBack* pResource);

			//--------------------------------------------------------------------------------------
			// Ultimate command list
			//--------------------------------------------------------------------------------------
			class XUSG_INTERFACE CommandList :
				public virtual XUSG::EZ::CommandList
			{
			public:
				//CommandList();
				virtual ~CommandList() {}

				using uptr = std::unique_ptr<CommandList>;
				using sptr = std::shared_ptr<CommandList>;

				// By default maxSamplers[stage] = 16, maxCbvsEachSpace[stage] = 14, maxSrvsEachSpace[stage] = 32, and maxUavsEachSpace[stage] = 16
				virtual bool Create(Ultimate::CommandList* pCommandList,
					uint32_t samplerHeapSize, uint32_t cbvSrvUavHeapSize,
					const uint32_t maxSamplers[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxCbvsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxSrvsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxUavsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxCbvSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxSrvSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxUavSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t max32BitConstants[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t constantSlots[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t constantSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					uint32_t slotExt = 0, uint32_t spaceExt = 0x7FFF0ADE) = 0;
				virtual bool Create(const Device* pDevice, void* pHandle,
					uint32_t samplerHeapSize, uint32_t cbvSrvUavHeapSize,
					const uint32_t maxSamplers[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxCbvsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxSrvsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxUavsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxCbvSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxSrvSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxUavSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t max32BitConstants[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t constantSlots[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t constantSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					uint32_t slotExt = 0, uint32_t spaceExt = 0x7FFF0ADE,
					const wchar_t* name = nullptr) = 0;

				virtual void SetSamplePositions(uint8_t numSamplesPerPixel, uint8_t numPixels, SamplePosition* pPositions) const = 0;
				virtual void ResolveSubresourceRegion(Resource* pDstResource, uint32_t dstSubresource,
					uint32_t dstX, uint32_t dstY, Resource* pSrcResource, uint32_t srcSubresource,
					const RectRange& srcRect, Format format, ResolveMode resolveMode) = 0;

				virtual void RSSetShadingRate(ShadingRate baseShadingRate, const ShadingRateCombiner* pCombiners) const = 0;
				virtual void RSSetShadingRateImage(Resource* pShadingRateImage) = 0;

				virtual void MSSetPipelineState(const Pipeline& pipelineState, const State* pState = nullptr) = 0;
				virtual void MSSetBlendState(Graphics::BlendPreset preset, uint8_t numColorRTs = 1, uint32_t sampleMask = UINT_MAX) = 0;
				virtual void MSSetSample(uint8_t count, uint8_t quality = 0) = 0;
				virtual void MSSetRasterizerState(Graphics::RasterizerPreset preset) = 0;
				virtual void MSSetDepthStencilState(Graphics::DepthStencilPreset preset) = 0;
				virtual void MSSetShader(Shader::Stage stage, const Blob& shader) = 0;
				virtual void MSSet32BitConstant(Shader::Stage stage, uint32_t srcData, uint32_t destOffsetIn32BitValues = 0) const = 0;
				virtual void MSSet32BitConstants(Shader::Stage stage, uint32_t num32BitValuesToSet, const void* pSrcData, uint32_t destOffsetIn32BitValues = 0) const = 0;
				virtual void MSSetNodeMask(uint32_t nodeMask) = 0;
				virtual void DispatchMesh(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ) = 0;
				virtual void DispatchMeshIndirect(const CommandLayout* pCommandlayout,
					uint32_t maxCommandCount,
					Resource* pArgumentBuffer,
					uint64_t argumentBufferOffset = 0,
					Resource* pCountBuffer = nullptr,
					uint64_t countBufferOffset = 0) = 0;

				virtual const XUSG::PipelineLayout& GetMSPipelineLayout() const = 0;

				virtual uint32_t GetMSConstantParamIndex(Shader::Stage stage) const = 0;

				static uptr MakeUnique(API api = API::DIRECTX_12);
				static sptr MakeShared(API api = API::DIRECTX_12);
				static uptr MakeUnique(Ultimate::CommandList* pCommandList,
					uint32_t samplerHeapSize, uint32_t cbvSrvUavHeapSize,
					const uint32_t maxSamplers[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxCbvsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxSrvsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxUavsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxCbvSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxSrvSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxUavSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t max32BitConstants[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t constantSlots[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t constantSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					uint32_t slotExt = 0, uint32_t spaceExt = 0x7FFF0ADE,
					API api = API::DIRECTX_12);
				static sptr MakeShared(Ultimate::CommandList* pCommandList,
					uint32_t samplerHeapSize, uint32_t cbvSrvUavHeapSize,
					const uint32_t maxSamplers[Shader::Stage::NUM_STAGE],
					const uint32_t* pMaxCbvsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxSrvsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t* pMaxUavsEachSpace[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxCbvSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxSrvSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t maxUavSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t max32BitConstants[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t constantSlots[Shader::Stage::NUM_STAGE] = nullptr,
					const uint32_t constantSpaces[Shader::Stage::NUM_STAGE] = nullptr,
					uint32_t slotExt = 0, uint32_t spaceExt = 0x7FFF0ADE,
					API api = API::DIRECTX_12);
			};
		}
	}
}
