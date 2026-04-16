//----------------------------------------------------------------------------------
// File:        sl_demo.cpp
// SDK Version: 2.0
// Email:       StreamlineSupport@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------------

#pragma once

#include <donut/core/math/math.h>
#include <donut/engine/FramebufferFactory.h>
#include <donut/render/GBuffer.h>
#include <nvrhi/nvrhi.h>
#include <nvrhi/common/misc.h>


/// <summary>
/// This class stores all the color buffers we will use in our render pipeline.
/// </summary>
class RenderTargets : public donut::render::GBufferRenderTargets
{
public:
    nvrhi::TextureHandle HdrColor;
    nvrhi::TextureHandle LdrColor;
    nvrhi::TextureHandle ColorspaceCorrectionColor;
    nvrhi::TextureHandle AAResolvedColor;
    nvrhi::TextureHandle TemporalFeedback1;
    nvrhi::TextureHandle TemporalFeedback2;
    nvrhi::TextureHandle AmbientOcclusion;
    nvrhi::TextureHandle NisColor;
    nvrhi::TextureHandle PreUIColor;
    nvrhi::TextureHandle UIColorAlpha;  // RGBA16F texture for DLSS-G UI Color & Alpha input (4-channel)
    nvrhi::TextureHandle UIAlpha;       // R16F texture for DLSS-G UI Alpha-only input (1-channel)
    nvrhi::TextureHandle SpecHitDistance;
    nvrhi::TextureHandle GBufferDiffuseRR;
    nvrhi::TextureHandle GBufferSpecularRR;
    nvrhi::TextureHandle GBufferNormalsRR;
    nvrhi::TextureHandle GBufferEmissiveRR;

    nvrhi::HeapHandle Heap;

    std::shared_ptr<donut::engine::FramebufferFactory> ForwardFramebuffer;
    std::shared_ptr<donut::engine::FramebufferFactory> HdrFramebuffer;
    std::shared_ptr<donut::engine::FramebufferFactory> LdrFramebuffer;
    std::shared_ptr<donut::engine::FramebufferFactory> AAResolvedFramebuffer;
    std::shared_ptr<donut::engine::FramebufferFactory> PreUIFramebuffer;
    std::shared_ptr<donut::engine::FramebufferFactory> UIColorAlphaFramebuffer;
    std::shared_ptr<donut::engine::FramebufferFactory> UIAlphaFramebuffer;
    std::shared_ptr<donut::engine::FramebufferFactory> SpecHitDistanceBuffer;

    donut::math::int2 m_RenderSize;// size of render targets pre-DLSS
    donut::math::int2 m_DisplaySize; // size of render targets post-DLSS

    void Init(
        nvrhi::IDevice* device,
        donut::math::int2 renderSize,
        donut::math::int2 displaySize,
        nvrhi::Format backbufferFormat,
        dm::uint sampleCount = 1,
        bool enableMotionVectors = true,
        bool useReverseProjection = true)
    {
        GBufferRenderTargets::Init(device, (donut::math::uint2) renderSize, sampleCount, enableMotionVectors, useReverseProjection);


        m_RenderSize = renderSize;
        m_DisplaySize = displaySize;

        nvrhi::TextureDesc desc;
        desc.width = renderSize.x;
        desc.height = renderSize.y;
        desc.isRenderTarget = true;
        desc.useClearValue = true;
        desc.clearValue = nvrhi::Color(1.f);
        desc.sampleCount = sampleCount;
        desc.dimension = sampleCount > 1 ? nvrhi::TextureDimension::Texture2DMS : nvrhi::TextureDimension::Texture2D;
        desc.keepInitialState = true;
        desc.isVirtual = device->queryFeatureSupport(nvrhi::Feature::VirtualResources);

        desc.clearValue = nvrhi::Color(0.f);
        desc.isTypeless = false;
        desc.isUAV = sampleCount == 1;
        desc.format = nvrhi::Format::RGBA16_FLOAT;
        desc.initialState = nvrhi::ResourceStates::ShaderResource;
        desc.debugName = "HdrColor";
        HdrColor = device->createTexture(desc);

        desc.format = nvrhi::Format::R16_FLOAT;
        desc.debugName = "SpecHitDistance";
        desc.isUAV = true;
        desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
        SpecHitDistance = device->createTexture(desc);

        // The render targets below this point are non-MSAA
        desc.sampleCount = 1;
        desc.dimension = nvrhi::TextureDimension::Texture2D;
        desc.format = nvrhi::Format::R8_UNORM;
        desc.isUAV = true;
        desc.initialState = nvrhi::ResourceStates::RenderTarget;
        desc.debugName = "AmbientOcclusion";
        AmbientOcclusion = device->createTexture(desc);

        desc.format = nvrhi::Format::RGBA16_FLOAT;
        desc.width = displaySize.x;
        desc.height = displaySize.y;
        desc.isUAV = true;
        desc.debugName = "AAResolvedColor";
        AAResolvedColor = device->createTexture(desc);

        desc.format = nvrhi::Format::RGBA16_SNORM;
        desc.isUAV = true;
        desc.debugName = "TemporalFeedback1";
        TemporalFeedback1 = device->createTexture(desc);
        desc.debugName = "TemporalFeedback2";
        TemporalFeedback2 = device->createTexture(desc);

        desc.format = nvrhi::Format::SRGBA8_UNORM;
        desc.isUAV = false;
        desc.debugName = "LdrColor";
        LdrColor = device->createTexture(desc);

        // Redefined these RR G-Buffer textures locally to use custom formats, 
        // as the ones provided by Donut are not compatible with our requirements.
        // Keeping them separate gives us more control over the format and usage.
        desc.format = nvrhi::Format::RGBA16_FLOAT;
        desc.debugName = "GBufferDiffuseRR";
        GBufferDiffuseRR = device->createTexture(desc);

        desc.format = nvrhi::Format::RGBA16_FLOAT;
        desc.debugName = "GBufferSpecularRR";
        GBufferSpecularRR = device->createTexture(desc);

        desc.format = nvrhi::Format::RGBA16_FLOAT;
        desc.debugName = "GBufferNormalsRR";
        GBufferNormalsRR = device->createTexture(desc);

        desc.format = nvrhi::Format::RGBA16_FLOAT;
        desc.debugName = "GBufferEmissiveRR";
        GBufferEmissiveRR = device->createTexture(desc);

        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.isUAV = true;
        desc.debugName = "ColorspaceCorrectionColor";
        ColorspaceCorrectionColor = device->createTexture(desc);

        desc.format = backbufferFormat;
        desc.isUAV = true;
        desc.debugName = "NisColor";
        NisColor = device->createTexture(desc);

        desc.format = backbufferFormat;
        desc.isUAV = true;
        desc.debugName = "PreUIColor";
        PreUIColor = device->createTexture(desc);

        // UI Color & Alpha texture for DLSS-G: RGBA16F with RGB=color, A=alpha blending
        desc.format = nvrhi::Format::RGBA16_FLOAT;
        desc.isUAV = false;
        desc.debugName = "UIColorAlpha";
        UIColorAlpha = device->createTexture(desc);

        // UI Alpha-only texture for DLSS-G: R16F with alpha value only (1-channel)
        desc.format = nvrhi::Format::R16_FLOAT;
        desc.isUAV = false;
        desc.debugName = "UIAlpha";
        UIAlpha = device->createTexture(desc);



        if (desc.isVirtual)
        {
            uint64_t heapSize = 0;
            nvrhi::ITexture* const textures[] = {
                HdrColor,
                AAResolvedColor,
                SpecHitDistance,
                TemporalFeedback1,
                TemporalFeedback2,
                LdrColor,
                GBufferDiffuseRR,
                GBufferSpecularRR,
                GBufferNormalsRR,
                GBufferEmissiveRR,
                ColorspaceCorrectionColor,
                PreUIColor,
                UIColorAlpha,
                UIAlpha,
                NisColor,
                AmbientOcclusion,
                GBufferSpecularRR,
                GBufferDiffuseRR
            };

            for (auto texture : textures)
            {
                nvrhi::MemoryRequirements memReq = device->getTextureMemoryRequirements(texture);
                heapSize = nvrhi::align(heapSize, memReq.alignment);
                heapSize += memReq.size;
            }

            nvrhi::HeapDesc heapDesc;
            heapDesc.type = nvrhi::HeapType::DeviceLocal;
            heapDesc.capacity = heapSize;
            heapDesc.debugName = "RenderTargetHeap";

            Heap = device->createHeap(heapDesc);

            uint64_t offset = 0;
            for (auto texture : textures)
            {
                nvrhi::MemoryRequirements memReq = device->getTextureMemoryRequirements(texture);
                offset = nvrhi::align(offset, memReq.alignment);

                device->bindTextureMemory(texture, Heap, offset);

                offset += memReq.size;
            }
        }

        ForwardFramebuffer = std::make_shared<donut::engine::FramebufferFactory>(device);
        ForwardFramebuffer->RenderTargets = { HdrColor };
        ForwardFramebuffer->DepthTarget = Depth;

        HdrFramebuffer = std::make_shared<donut::engine::FramebufferFactory>(device);
        HdrFramebuffer->RenderTargets = { HdrColor };

        SpecHitDistanceBuffer = std::make_shared<donut::engine::FramebufferFactory>(device);
        SpecHitDistanceBuffer->RenderTargets = { SpecHitDistance };

        LdrFramebuffer = std::make_shared<donut::engine::FramebufferFactory>(device);
        LdrFramebuffer->RenderTargets = { LdrColor };

        AAResolvedFramebuffer = std::make_shared<donut::engine::FramebufferFactory>(device);
        AAResolvedFramebuffer->RenderTargets = { AAResolvedColor };

        PreUIFramebuffer = std::make_shared<donut::engine::FramebufferFactory>(device);
        PreUIFramebuffer->RenderTargets = { PreUIColor };

        UIColorAlphaFramebuffer = std::make_shared<donut::engine::FramebufferFactory>(device);
        UIColorAlphaFramebuffer->RenderTargets = { UIColorAlpha };

        UIAlphaFramebuffer = std::make_shared<donut::engine::FramebufferFactory>(device);
        UIAlphaFramebuffer->RenderTargets = { UIAlpha };

    }

    bool IsUpdateRequired(donut::math::int2 renderSize, donut::math::int2 displaySize, donut::math::uint sampleCount = 1) const
    {
        if (any(m_RenderSize != renderSize) || any(m_DisplaySize != displaySize) || m_SampleCount != sampleCount) return true;
        return false;
    }

    void Clear(nvrhi::ICommandList* commandList) override
    {
        GBufferRenderTargets::Clear(commandList);

        commandList->clearTextureFloat(HdrColor, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(LdrColor, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(NisColor, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(PreUIColor, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(UIColorAlpha, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(UIAlpha, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(AAResolvedColor, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(SpecHitDistance, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(GBufferDiffuseRR, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(GBufferSpecularRR, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(GBufferNormalsRR, nvrhi::AllSubresources, nvrhi::Color(0.f));
        commandList->clearTextureFloat(GBufferEmissiveRR, nvrhi::AllSubresources, nvrhi::Color(0.f));
    }
};