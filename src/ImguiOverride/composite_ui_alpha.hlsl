//----------------------------------------------------------------------------------
// File:        composite_ui_alpha.hlsl
// SDK Version: 2.0
// Email:       StreamlineSupport@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2024-2026, NVIDIA CORPORATION. All rights reserved.
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
//
// MRT composite shader for DLSS-G UI tagging.
// Composites UI onto backbuffer while also writing alpha to a separate texture.
//
// Outputs to two render targets:
//   SV_Target0: Backbuffer with UI composited (uses hardware blend state)
//   SV_Target1: UIAlpha - just the alpha channel for DLSS-G tagging
//
//----------------------------------------------------------------------------------

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : UV;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;  // Backbuffer (blended via blend state)
    float  alpha : SV_Target1;  // UIAlpha (R16F)
};

// Vulkan binding macros - vk::binding attributes are only used for SPIRV compilation (DXC).
// FXC (DXBC/D3D11) doesn't support [[...]] attribute syntax.
#ifdef SPIRV
#define VK_BINDING(reg, dset) [[vk::binding(reg, dset)]]
#else
#define VK_BINDING(reg, dset)
#endif

// Vulkan binding indices follow NVRHI_DEFAULT_VK_REGISTER_OFFSETS:
//   t# (SRV)     -> binding 0+
//   s# (sampler) -> binding 128+
//   b# (CBV)     -> binding 256+
//   u# (UAV)     -> binding 384+
VK_BINDING(128, 0) SamplerState sampler0 : register(s0);
VK_BINDING(0, 0) Texture2D<float4> t_UIColorAlpha : register(t0);

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    
    // Sample the UI texture
    float4 uiColor = t_UIColorAlpha.Sample(sampler0, input.uv);
    
    // Output color to backbuffer (will be blended via blend state)
    output.color = uiColor;
    
    // Output just alpha to UIAlpha texture
    output.alpha = uiColor.a;
    
    return output;
}

