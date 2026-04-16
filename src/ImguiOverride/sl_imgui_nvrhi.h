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

#include <donut/app/imgui_nvrhi.h>
#include <donut/engine/ShaderFactory.h>

namespace donut::app
{
    //
    // SL_ImGui_NVRHI extends ImGui_NVRHI to support single-pass UI rendering for DLSS-G.
    // 
    // Instead of rendering UI twice (once to backbuffer, once to UIColorAlpha), this renders
    // UI once to a UIColorAlpha texture with premultiplied alpha. The texture can then be:
    //   1. Composited onto the backbuffer for display
    //   2. Tagged for DLSS-G frame generation (kBufferTypeUIColorAndAlpha)
    //
    struct SL_ImGui_NVRHI : public ImGui_NVRHI
    {
        // PSO for rendering to UIColorAlpha texture with premultiplied alpha blending
        nvrhi::GraphicsPipelineDesc uiColorAlphaPSODesc;
        nvrhi::GraphicsPipelineHandle uiColorAlphaPso;

        // Initialize base class and set up the UI PSO
        bool init(nvrhi::IDevice* device, std::shared_ptr<engine::ShaderFactory> shaderFactory);

        // Render UI to the UIColorAlpha texture (single pass, premultiplied alpha)
        bool renderToUITexture(nvrhi::IFramebuffer* uiFramebuffer);

        // Clear PSO handles on backbuffer resize
        void backbufferResizing();

    private:
        nvrhi::IGraphicsPipeline* getUIColorAlphaPSO(nvrhi::IFramebuffer* fb);
        nvrhi::IBindingSet* getOrCreateBindingSet(nvrhi::ITexture* texture);
        bool updateGeometryInternal(nvrhi::ICommandList* commandList);
    };
}
