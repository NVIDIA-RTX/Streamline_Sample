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

#include "sl_imgui_renderer.h"
#include <donut/engine/BindingCache.h>

using namespace donut::engine;
using namespace donut::app;

SL_ImGui_Renderer::SL_ImGui_Renderer(DeviceManager* devManager)
    : ImGui_Renderer(devManager)
{
}

bool SL_ImGui_Renderer::Init(std::shared_ptr<ShaderFactory> shaderFactory)
{
    // Create our extended SL_ImGui_NVRHI instead of the base ImGui_NVRHI
    auto sl_imgui = std::make_unique<SL_ImGui_NVRHI>();
    
    // Initialize it
    if (!sl_imgui->init(GetDevice(), shaderFactory))
    {
        return false;
    }

    // Store raw pointer for our use before moving ownership
    m_sl_imgui_nvrhi = sl_imgui.get();

    // Move ownership to base class member
    // Note: imgui_nvrhi is protected in base class, so we can access it
    imgui_nvrhi = std::move(sl_imgui);

    // Create CommonRenderPasses for UI compositing
    m_commonPasses = std::make_shared<CommonRenderPasses>(GetDevice(), shaderFactory);

    // Create binding cache
    m_bindingCache = std::make_unique<BindingCache>(GetDevice());

    // Load MRT composite shader (composites UI to backbuffer + writes UIAlpha)
    m_compositeAlphaPS = shaderFactory->CreateShader(
        "app/ImguiOverride/composite_ui_alpha.hlsl", "main", nullptr, nvrhi::ShaderType::Pixel);

    if (m_compositeAlphaPS)
    {
        // Create binding layout for composite shader (texture + sampler)
        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::Pixel;
        layoutDesc.bindings = {
            nvrhi::BindingLayoutItem::Texture_SRV(0),
            nvrhi::BindingLayoutItem::Sampler(0)
        };
        m_compositeBindingLayout = GetDevice()->createBindingLayout(layoutDesc);
    }

    return true;
}

void SL_ImGui_Renderer::RenderToUITexture(nvrhi::IFramebuffer* uiFramebuffer)
{
    if (!m_sl_imgui_nvrhi)
        return;

    // Ensure font texture is up to date (same as Animate does before Render)
    m_sl_imgui_nvrhi->updateFontTexture();

    // Build the UI (Animate was already called by the framework, which called ImGui::NewFrame)
    buildUI();

    // Finalize ImGui for this frame
    ImGui::Render();

    // Render UI to the UIColorAlpha texture (single pass)
    m_sl_imgui_nvrhi->renderToUITexture(uiFramebuffer);

    // Reset begin frame flag (matches base class behavior)
    m_beginFrameCalled = false;

    // Reconcile mouse button states (from base class)
    auto& io = ImGui::GetIO();
    for (size_t i = 0; i < mouseDown.size(); i++)
    {
        if (io.MouseDown[i] == true && mouseDown[i] == false)
        {
            io.MouseDown[i] = false;
        }
    }

    // Reconcile key states (from base class)
    for (size_t i = 0; i < keyDown.size(); i++)
    {
        if (io.KeysDown[i] == true && keyDown[i] == false)
        {
            io.KeysDown[i] = false;
        }
    }
}

void SL_ImGui_Renderer::CompositeUIToBackbuffer(
    nvrhi::ICommandList* commandList,
    nvrhi::IFramebuffer* backbuffer,
    nvrhi::ITexture* uiTexture)
{
    if (!m_commonPasses || !uiTexture)
        return;

    // Set up blit parameters with alpha blending
    BlitParameters blitParams;
    blitParams.targetFramebuffer = backbuffer;
    blitParams.sourceTexture = uiTexture;
    blitParams.sampler = BlitSampler::Linear;  // Use linear sampling for smooth UI composite

    // Configure alpha blending for compositing premultiplied alpha UI
    // srcBlend = One (color is already premultiplied)
    // destBlend = InvSrcAlpha (standard over compositing)
    blitParams.blendState.blendEnable = true;
    blitParams.blendState.srcBlend = nvrhi::BlendFactor::One;
    blitParams.blendState.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
    blitParams.blendState.srcBlendAlpha = nvrhi::BlendFactor::One;
    blitParams.blendState.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;

    // Composite UI onto backbuffer
    m_commonPasses->BlitTexture(commandList, blitParams, m_bindingCache.get());
}

void SL_ImGui_Renderer::CompositeUIToBackbufferWithAlpha(
    nvrhi::ICommandList* commandList,
    nvrhi::ITexture* backbuffer,
    nvrhi::ITexture* uiTexture,
    nvrhi::ITexture* uiAlphaTexture)
{
    if (!m_commonPasses || !uiTexture || !uiAlphaTexture || !m_compositeAlphaPS)
    {
        // Fallback to regular composite if MRT shader not available
        return;
    }

    // Get or create MRT framebuffer (backbuffer + UIAlpha)
    nvrhi::FramebufferHandle& mrtFramebuffer = m_compositeMrtFramebuffers[backbuffer];
    if (!mrtFramebuffer)
    {
        nvrhi::FramebufferDesc fbDesc;
        fbDesc.addColorAttachment(backbuffer);
        fbDesc.addColorAttachment(uiAlphaTexture);
        mrtFramebuffer = GetDevice()->createFramebuffer(fbDesc);
    }

    // Create PSO if needed (lazy creation for correct framebuffer format)
    if (!m_compositeAlphaPso)
    {
        nvrhi::GraphicsPipelineDesc psoDesc;
        psoDesc.VS = m_commonPasses->m_FullscreenVS;
        psoDesc.PS = m_compositeAlphaPS;
        psoDesc.bindingLayouts = { m_compositeBindingLayout };
        psoDesc.primType = nvrhi::PrimitiveType::TriangleStrip;
        psoDesc.renderState.rasterState.setCullNone();
        psoDesc.renderState.depthStencilState.depthTestEnable = false;
        psoDesc.renderState.depthStencilState.stencilEnable = false;

        // RT0 (backbuffer): Alpha blending for UI composite
        psoDesc.renderState.blendState.targets[0].blendEnable = true;
        psoDesc.renderState.blendState.targets[0].srcBlend = nvrhi::BlendFactor::One;
        psoDesc.renderState.blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        psoDesc.renderState.blendState.targets[0].srcBlendAlpha = nvrhi::BlendFactor::One;
        psoDesc.renderState.blendState.targets[0].destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;

        // RT1 (UIAlpha): Additive blend to accumulate alpha values
        psoDesc.renderState.blendState.targets[1].blendEnable = true;
        psoDesc.renderState.blendState.targets[1].srcBlend = nvrhi::BlendFactor::One;
        psoDesc.renderState.blendState.targets[1].destBlend = nvrhi::BlendFactor::One;
        psoDesc.renderState.blendState.targets[1].srcBlendAlpha = nvrhi::BlendFactor::One;
        psoDesc.renderState.blendState.targets[1].destBlendAlpha = nvrhi::BlendFactor::One;

        m_compositeAlphaPso = GetDevice()->createGraphicsPipeline(psoDesc, mrtFramebuffer);
    }

    // Create binding set for the UI texture
    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings = {
        nvrhi::BindingSetItem::Texture_SRV(0, uiTexture),
        nvrhi::BindingSetItem::Sampler(0, m_commonPasses->m_LinearClampSampler)
    };
    nvrhi::BindingSetHandle bindingSet = m_bindingCache->GetOrCreateBindingSet(bindingSetDesc, m_compositeBindingLayout);

    // Set up graphics state
    nvrhi::GraphicsState state;
    state.pipeline = m_compositeAlphaPso;
    state.framebuffer = mrtFramebuffer;
    state.bindings = { bindingSet };

    const nvrhi::FramebufferInfoEx& fbInfo = mrtFramebuffer->getFramebufferInfo();
    state.viewport.addViewportAndScissorRect(nvrhi::Viewport(float(fbInfo.width), float(fbInfo.height)));

    commandList->setGraphicsState(state);

    // Draw fullscreen quad
    nvrhi::DrawArguments args;
    args.instanceCount = 1;
    args.vertexCount = 4;
    commandList->draw(args);
}

void SL_ImGui_Renderer::BackBufferResizing()
{
    // Call base class which will call imgui_nvrhi->backbufferResizing()
    // Since imgui_nvrhi actually points to our SL_ImGui_NVRHI, this will
    // properly clear both the base PSO and our UI PSOs
    ImGui_Renderer::BackBufferResizing();

    // Clear our MRT composite resources
    m_compositeAlphaPso = nullptr;
    m_compositeMrtFramebuffers.clear();
}
