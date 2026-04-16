//----------------------------------------------------------------------------------
// File:        NGXWrapper.h
// SDK Version: 2.0
// Email:       StreamlineSupport@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2024-2025, NVIDIA CORPORATION. All rights reserved.
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

#include <map>
#include <queue>
#include "NVWrapper.h"
#include "nvapi.h"

struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;
class RenderTargets;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 6;

class NGXWrapper : public NVWrapper
{
private:
    bool m_ngx_initialised = false;
    NVSDK_NGX_Parameter* m_ngx_parameters = nullptr;
    sl::FrameToken m_currentFrameToken {0};
    bool m_nvapi_loaded = false;

    sl::Extent renderExtent;
    sl::Extent fullExtent;
    std::vector<sl::ReflexCameraData> m_latewarp_frames;
    NVSDK_NGX_Handle* m_latewarp_handle = nullptr;
    NGXWrapper(): NVWrapper() { m_latewarp_frames.resize(MAX_FRAMES_IN_FLIGHT); }
    void ReflexSetMarker(donut::app::DeviceManager& manager, NV_LATENCY_MARKER_TYPE markerType, uint64_t frameNumber);

public:
    static NGXWrapper& Get();
    NGXWrapper(const NGXWrapper&) = delete;
    NGXWrapper(NGXWrapper&&) = delete;
    NGXWrapper& operator=(const NGXWrapper&) = delete;
    NGXWrapper& operator=(NGXWrapper&&) = delete;

    void SetSLOptions(const bool checkSig, const bool enableLog, const bool useNewSetTagAPI, const bool allowSMSCG) override {};

    bool Initialize_preDevice(nvrhi::GraphicsAPI api) override;
    bool Initialize_postDevice(donut::app::DeviceManager* deviceManager) override;

    void SetDevice_raw(void *device_ptr) override { };
    void QueueGPUWaitOnSyncObjectSet(nvrhi::IDevice *pDevice, nvrhi::CommandQueue cmdQType, void *syncObj, uint64_t syncObjVal) override { };
    void SetDevice_nvrhi(nvrhi::IDevice* device) override;
    void UpdateFeatureAvailable(donut::app::DeviceManager* adapter) override;
    void Shutdown() override;
    void ProxyToNative(void* proxy, void** native) override;
    void NativeToProxy(void* proxy, void** native) override;
    void QueryDLSSGState(uint64_t &estimatedVRamUsage, int &fps_multiplier, sl::DLSSGStatus &status, int &minSize, int &framesMax, void *&pFence, uint64_t &fenceValue, bool &vsyncSupported, bool &bIsDynamicMFGSupported) override { vsyncSupported = true; bIsDynamicMFGSupported = false; };

    sl::FeatureRequirements GetFeatureRequirements(sl::Feature feature) override;
    sl::FeatureVersion GetFeatureVersion(sl::Feature feature) override;

    void SetSLConsts(const sl::Constants &consts) override { };
    void TagResources_General(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *motionVectors,
        nvrhi::ITexture *depth,
        nvrhi::ITexture *finalColorHudless) override;

    void TagResources_DLSS_NIS(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *output,
        nvrhi::ITexture *input) override { };

    void TagResources_DLSS_FG(
        nvrhi::ICommandList *commandList,
        bool validViewportExtent = false,
        sl::Extent backBufferExtent = {}) override { };

    void TagResources_UIColorAlpha(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *uiColorAlpha) override { };

    void TagResources_UIAlpha(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *uiAlpha) override { };

    void TagResources_DeepDVC(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *output) override { };

    void TagResources_Latewarp(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *backbuffer,
        nvrhi::ITexture *uiColorAlpha,
        nvrhi::ITexture *noWarpMask,
        sl::Extent backBufferExtent) override { };

    void UnTagResources_DeepDVC() override { };

    void SetDLSSOptions(const sl::DLSSOptions consts) override { };
    void QueryDLSSOptimalSettings(DLSSSettings &settings) override { };
    void EvaluateDLSS(nvrhi::ICommandList *commandList) override { };
    void CleanupDLSS(bool wfi) override { };

    void SetNISOptions(const sl::NISOptions consts) override { };
    void EvaluateNIS(nvrhi::ICommandList *commandList) override { };
    void CleanupNIS(bool wfi) override { };

    void SetDeepDVCOptions(const sl::DeepDVCOptions consts) override { };
    void QueryDeepDVCState(uint64_t &estimatedVRamUsage) override { };
    void EvaluateDeepDVC(nvrhi::ICommandList *commandList) override { };
    void CleanupDeepDVC() override { };

    void SetDLSSGOptions(const sl::DLSSGOptions consts) override { };
    void CleanupDLSSG(bool wfi) override { };

    void FeatureLoad(sl::Feature feature, const bool turn_on) override;

    void SetReflexConsts(const sl::ReflexOptions consts) override;
    void ReflexCallback_Sleep(donut::app::DeviceManager& manager, uint32_t frameID) override;
    void ReflexCallback_SimStart(donut::app::DeviceManager& manager, uint32_t frameID) override;
    void ReflexCallback_SimEnd(donut::app::DeviceManager& manager, uint32_t frameID) override;
    void ReflexCallback_RenderStart(donut::app::DeviceManager& manager, uint32_t frameID) override;
    void ReflexCallback_RenderEnd(donut::app::DeviceManager& manager, uint32_t frameID) override;
    void ReflexCallback_PresentStart(donut::app::DeviceManager& manager, uint32_t frameID) override;
    void ReflexCallback_PresentEnd(donut::app::DeviceManager& manager, uint32_t frameID) override;

    void ReflexTriggerFlash() override;
    void ReflexTriggerPcPing() override;
    void QueryReflexStats(bool& reflex_lowLatencyAvailable, bool& reflex_flashAvailable, std::string& stats) override;

    void SetReflexCameraData(sl::FrameToken& frameToken, const sl::ReflexCameraData& cameraData) override;
#if STREAMLINE_FEATURE_LATEWARP
    void SetLatewarpOptions(const sl::LatewarpOptions& options) override;
    void EvaluateLatewarp(donut::app::DeviceManager& manager, nvrhi::ICommandList* commandList, RenderTargets* renderTargets, nvrhi::ITexture* inputColor, nvrhi::ITexture* outputColor, const donut::engine::IView* view) override;
    void CleanupLatewarp(bool wfi) override;
#endif
};
