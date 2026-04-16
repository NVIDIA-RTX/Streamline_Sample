/*
* Copyright (c) 2014-2026, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <donut/app/imgui_renderer.h>
#include <donut/engine/CommonRenderPasses.h>
#include "sl_imgui_nvrhi.h"
#include <unordered_map>

namespace donut::app
{
    //
    // SL_ImGui_Renderer extends ImGui_Renderer for single-pass UI rendering with DLSS-G support.
    //
    // Instead of rendering UI twice (once to backbuffer, once to UIColorAlpha), this:
    //   1. Renders UI once to a UIColorAlpha texture (RGBA16F, premultiplied alpha)
    //   2. Composites that texture onto the backbuffer
    //   3. The UIColorAlpha texture can be tagged for DLSS-G (kBufferTypeUIColorAndAlpha)
    //
    // For kBufferTypeUIAlpha support, use CompositeUIToBackbufferWithAlpha() which extracts
    // the alpha channel to a separate R16F texture during the composite pass.
    //
    class SL_ImGui_Renderer : public ImGui_Renderer
    {
    protected:
        // Pointer to our extended imgui implementation (same object as base class imgui_nvrhi)
        SL_ImGui_NVRHI* m_sl_imgui_nvrhi = nullptr;

        // Common render passes for BlitTexture compositing
        std::shared_ptr<engine::CommonRenderPasses> m_commonPasses;

        // Binding cache for composite pass
        std::unique_ptr<engine::BindingCache> m_bindingCache;

        // MRT composite shader/pipeline for extracting UIAlpha during composite
        nvrhi::ShaderHandle m_compositeAlphaPS;
        nvrhi::BindingLayoutHandle m_compositeBindingLayout;
        nvrhi::GraphicsPipelineHandle m_compositeAlphaPso;
        std::unordered_map<nvrhi::ITexture*, nvrhi::FramebufferHandle> m_compositeMrtFramebuffers;

    public:
        SL_ImGui_Renderer(DeviceManager* devManager);
        virtual ~SL_ImGui_Renderer() = default;

        // Override Init to create SL_ImGui_NVRHI and set up compositing
        bool Init(std::shared_ptr<engine::ShaderFactory> shaderFactory);

        // Render UI to the UIColorAlpha texture (RGBA16F)
        void RenderToUITexture(nvrhi::IFramebuffer* uiFramebuffer);

        // Composite the UIColorAlpha texture onto the backbuffer with alpha blending
        void CompositeUIToBackbuffer(
            nvrhi::ICommandList* commandList,
            nvrhi::IFramebuffer* backbuffer,
            nvrhi::ITexture* uiTexture);

        // Composite UI onto backbuffer AND extract alpha to UIAlpha texture (MRT)
        // Used for kBufferTypeUIAlpha tagging
        void CompositeUIToBackbufferWithAlpha(
            nvrhi::ICommandList* commandList,
            nvrhi::ITexture* backbuffer,
            nvrhi::ITexture* uiTexture,
            nvrhi::ITexture* uiAlphaTexture);

        virtual void BackBufferResizing() override;
    };
}
