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

#include "sl_imgui_nvrhi.h"
#include <donut/core/log.h>
#include <donut/engine/ShaderFactory.h>

using namespace donut::engine;
using namespace donut::app;

bool SL_ImGui_NVRHI::init(nvrhi::IDevice* device, std::shared_ptr<ShaderFactory> shaderFactory)
{
    // Initialize base class first
    if (!ImGui_NVRHI::init(device, shaderFactory))
    {
        return false;
    }

    // Create PSO for UI Color & Alpha output (for DLSS-G)
    // This outputs premultiplied color in RGB and alpha in A channel
    {
        nvrhi::BlendState uiBlendState;
        // For UI Color & Alpha texture:
        // The ImGui shader outputs NON-premultiplied color (vertex_color * texture).
        // We use SrcAlpha blend to multiply RGB by alpha during blending, which
        // produces premultiplied output in the render target:
        //   dst.rgb = src.rgb * src.a + dst.rgb * (1 - src.a)
        //   dst.a = src.a + dst.a * (1 - src.a)
        // This is exactly what DLSS-G expects for UI Color Alpha input.
        uiBlendState.targets[0].setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)      // Multiply RGB by alpha (premultiply during blend)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)  // Standard over compositing
            .setSrcBlendAlpha(nvrhi::BlendFactor::One)      // Accumulate alpha
            .setDestBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha);

        auto rasterState = nvrhi::RasterState()
            .setFillSolid()
            .setCullNone()
            .setScissorEnable(true)
            .setDepthClipEnable(true);

        auto depthStencilState = nvrhi::DepthStencilState()
            .disableDepthTest()
            .enableDepthWrite()
            .disableStencil()
            .setDepthFunc(nvrhi::ComparisonFunc::Always);

        nvrhi::RenderState uiRenderState;
        uiRenderState.blendState = uiBlendState;
        uiRenderState.depthStencilState = depthStencilState;
        uiRenderState.rasterState = rasterState;

        // Copy from base PSO desc and modify blend state
        uiColorAlphaPSODesc.primType = nvrhi::PrimitiveType::TriangleList;
        uiColorAlphaPSODesc.inputLayout = shaderAttribLayout;
        uiColorAlphaPSODesc.VS = vertexShader;
        uiColorAlphaPSODesc.PS = pixelShader;
        uiColorAlphaPSODesc.renderState = uiRenderState;
        uiColorAlphaPSODesc.bindingLayouts = { bindingLayout };
    }

    return true;
}

nvrhi::IGraphicsPipeline* SL_ImGui_NVRHI::getUIColorAlphaPSO(nvrhi::IFramebuffer* fb)
{
    if (uiColorAlphaPso)
        return uiColorAlphaPso;

    uiColorAlphaPso = m_device->createGraphicsPipeline(uiColorAlphaPSODesc, fb);
    assert(uiColorAlphaPso);

    return uiColorAlphaPso;
}

nvrhi::IBindingSet* SL_ImGui_NVRHI::getOrCreateBindingSet(nvrhi::ITexture* texture)
{
    // Check if binding already exists in base class cache
    auto iter = bindingsCache.find(texture);
    if (iter != bindingsCache.end())
    {
        return iter->second;
    }

    // Create new binding set using base class members
    nvrhi::BindingSetDesc desc;
    desc.bindings = {
        nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 2),
        nvrhi::BindingSetItem::Texture_SRV(0, texture),
        nvrhi::BindingSetItem::Sampler(0, fontSampler)
    };

    nvrhi::BindingSetHandle binding = m_device->createBindingSet(desc, bindingLayout);
    assert(binding);

    bindingsCache[texture] = binding;
    return binding;
}

bool SL_ImGui_NVRHI::updateGeometryInternal(nvrhi::ICommandList* commandList)
{
    // NOTE: This duplicates the private updateGeometry() from ImGui_NVRHI base class.
    // The base class method is private and cannot be accessed from derived classes.
    // If donut's imgui_nvrhi.cpp updateGeometry() implementation changes, this method
    // must be updated to match. See: donut/src/app/imgui_nvrhi.cpp
    ImDrawData* drawData = ImGui::GetDrawData();
    
    if (drawData->TotalVtxCount == 0)
        return true;

    // Reallocate vertex buffer if needed
    size_t requiredVtxSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    if (vertexBuffer == nullptr || size_t(vertexBuffer->getDesc().byteSize) < requiredVtxSize)
    {
        nvrhi::BufferDesc desc;
        desc.byteSize = uint32_t((drawData->TotalVtxCount + 5000) * sizeof(ImDrawVert));
        desc.structStride = 0;
        desc.debugName = "ImGui vertex buffer";
        desc.canHaveUAVs = false;
        desc.isVertexBuffer = true;
        desc.isIndexBuffer = false;
        desc.isDrawIndirectArgs = false;
        desc.isVolatile = false;
        desc.initialState = nvrhi::ResourceStates::VertexBuffer;
        desc.keepInitialState = true;
        vertexBuffer = m_device->createBuffer(desc);
        if (!vertexBuffer) return false;
    }

    // Reallocate index buffer if needed
    size_t requiredIdxSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
    if (indexBuffer == nullptr || size_t(indexBuffer->getDesc().byteSize) < requiredIdxSize)
    {
        nvrhi::BufferDesc desc;
        desc.byteSize = uint32_t((drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx));
        desc.structStride = 0;
        desc.debugName = "ImGui index buffer";
        desc.canHaveUAVs = false;
        desc.isVertexBuffer = false;
        desc.isIndexBuffer = true;
        desc.isDrawIndirectArgs = false;
        desc.isVolatile = false;
        desc.initialState = nvrhi::ResourceStates::IndexBuffer;
        desc.keepInitialState = true;
        indexBuffer = m_device->createBuffer(desc);
        if (!indexBuffer) return false;
    }

    vtxBuffer.resize(vertexBuffer->getDesc().byteSize / sizeof(ImDrawVert));
    idxBuffer.resize(indexBuffer->getDesc().byteSize / sizeof(ImDrawIdx));

    // Copy vertices and indices
    ImDrawVert* vtxDst = &vtxBuffer[0];
    ImDrawIdx* idxDst = &idxBuffer[0];

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    commandList->writeBuffer(vertexBuffer, &vtxBuffer[0], vertexBuffer->getDesc().byteSize);
    commandList->writeBuffer(indexBuffer, &idxBuffer[0], indexBuffer->getDesc().byteSize);

    return true;
}

bool SL_ImGui_NVRHI::renderToUITexture(nvrhi::IFramebuffer* uiFramebuffer)
{
    // Render UI directly to the UIColorAlpha texture with premultiplied alpha blending.
    // This is the single-pass approach - UI is rendered once here, then composited onto backbuffer.

    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->TotalVtxCount == 0)
        return true;

    const auto& io = ImGui::GetIO();

    m_commandList->open();
    m_commandList->beginMarker("ImGUI_UITexture");

    if (!updateGeometryInternal(m_commandList))
    {
        m_commandList->close();
        return false;
    }

    // Handle DPI scaling
    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    float invDisplaySize[2] = { 1.f / io.DisplaySize.x, 1.f / io.DisplaySize.y };

    // Set up graphics state with UI Color Alpha PSO
    nvrhi::GraphicsState drawState;
    drawState.framebuffer = uiFramebuffer;
    assert(drawState.framebuffer);

    drawState.pipeline = getUIColorAlphaPSO(drawState.framebuffer);

    drawState.viewport.viewports.push_back(nvrhi::Viewport(
        io.DisplaySize.x * io.DisplayFramebufferScale.x,
        io.DisplaySize.y * io.DisplayFramebufferScale.y));
    drawState.viewport.scissorRects.resize(1);

    nvrhi::VertexBufferBinding vbufBinding;
    vbufBinding.buffer = vertexBuffer;
    vbufBinding.slot = 0;
    vbufBinding.offset = 0;
    drawState.vertexBuffers.push_back(vbufBinding);

    drawState.indexBuffer.buffer = indexBuffer;
    drawState.indexBuffer.format = (sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT);
    drawState.indexBuffer.offset = 0;

    // Render command lists
    int vtxOffset = 0;
    int idxOffset = 0;
    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        for (int i = 0; i < cmdList->CmdBuffer.Size; i++)
        {
            const ImDrawCmd* pCmd = &cmdList->CmdBuffer[i];

            if (pCmd->UserCallback)
            {
                pCmd->UserCallback(cmdList, pCmd);
            }
            else
            {
                drawState.bindings = { getOrCreateBindingSet((nvrhi::ITexture*)pCmd->TextureId) };
                assert(drawState.bindings[0]);

                drawState.viewport.scissorRects[0] = nvrhi::Rect(
                    int(pCmd->ClipRect.x),
                    int(pCmd->ClipRect.z),
                    int(pCmd->ClipRect.y),
                    int(pCmd->ClipRect.w));

                nvrhi::DrawArguments drawArguments;
                drawArguments.vertexCount = pCmd->ElemCount;
                drawArguments.startIndexLocation = idxOffset;
                drawArguments.startVertexLocation = vtxOffset;

                m_commandList->setGraphicsState(drawState);
                m_commandList->setPushConstants(invDisplaySize, sizeof(invDisplaySize));
                m_commandList->drawIndexed(drawArguments);
            }

            idxOffset += pCmd->ElemCount;
        }

        vtxOffset += cmdList->VtxBuffer.Size;
    }

    m_commandList->endMarker();
    m_commandList->close();
    m_device->executeCommandList(m_commandList);

    return true;
}

void SL_ImGui_NVRHI::backbufferResizing()
{
    // Call base class
    ImGui_NVRHI::backbufferResizing();

    // Clear our PSO
    uiColorAlphaPso = nullptr;
}
