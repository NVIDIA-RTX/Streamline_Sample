//----------------------------------------------------------------------------------
// File:        SLWrapper.h
// SDK Version: 2.0
// Email:       StreamlineSupport@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2022-2025, NVIDIA CORPORATION. All rights reserved.
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
#include "NVWrapper.h"

// Set this to a game's specific sdk version
static constexpr uint64_t SDK_VERSION = sl::kSDKVersion;

#ifdef STREAMLINE_MANUAL_HOOKING
struct CreateVKConfig {
    std::vector<std::string> pluginDeviceExtensions;
    std::vector<std::string> pluginInstanceExtensions;
    uint32_t extraGraphicsQueues = 0;
    uint32_t extraComputeQueues = 0;
    std::vector<std::string> features12;
    std::vector<std::string> features13;
};
#endif

void logFunctionCallback(sl::LogType type, const char* msg);

bool successCheck(sl::Result result, char* location = nullptr);

class SLWrapper : public NVWrapper
{
private:

    SLWrapper() = default;

    static sl::Resource allocateResourceCallback(const sl::ResourceAllocationDesc* resDesc, void* device);
    static void releaseResourceCallback(sl::Resource* resource, void* device);

    sl::FrameToken* m_renderFrame = nullptr;
    sl::FrameToken* m_presentFrame = nullptr;
    sl::FrameToken* UpdateRenderFrame();
    sl::FrameToken* UpdatePresentFrame();

    struct
    {
        bool checkSig = true;
        bool enableLog = false;
        bool useNewSetTagAPI = true;
        bool allowSMSCG = false;
    } m_SLOptions{};

public:

    static SLWrapper& Get();
    SLWrapper(const SLWrapper&) = delete;
    SLWrapper(SLWrapper&&) = delete;
    SLWrapper& operator=(const SLWrapper&) = delete;
    SLWrapper& operator=(SLWrapper&&) = delete;

    virtual void SetSLOptions(const bool checkSig, const bool enableLog, const bool useNewSetTagAPI, const bool allowSMSCG);

    bool Initialize_preDevice(nvrhi::GraphicsAPI api) override;
    bool Initialize_postDevice(donut::app::DeviceManager* deviceManager) override;

    void SetDevice_raw(void* device_ptr) override;
    void SetDevice_nvrhi(nvrhi::IDevice* device) override;
    void UpdateFeatureAvailable(donut::app::DeviceManager* adapter) override;
    void Shutdown() override;
    void ProxyToNative(void* proxy, void** native) override;
    void NativeToProxy(void* proxy, void** native) override;
    void QueueGPUWaitOnSyncObjectSet(nvrhi::IDevice* pDevice, nvrhi::CommandQueue cmdQType, void* syncObj, uint64_t syncObjVal);

    sl::FeatureRequirements GetFeatureRequirements(sl::Feature feature) override;
    sl::FeatureVersion GetFeatureVersion(sl::Feature feature) override;

    void SetViewportHandle(sl::ViewportHandle vpHandle)
    {
        m_viewport = vpHandle;
    }

    void SetSLConsts(const sl::Constants& consts);
    void FeatureLoad(sl::Feature feature, const bool turn_on);
    void TagResources_General(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView* view,
        nvrhi::ITexture* motionVectors,
        nvrhi::ITexture* depth,
        nvrhi::ITexture* finalColorHudless) override;

#ifdef STREAMLINE_FEATURE_DLSS_RR
    void SLWrapper::TagResources_DLSS_RR(
        nvrhi::ICommandList * commandList,
        const donut::engine::IView* view,
        nvrhi::ITexture* inputColor,
        nvrhi::ITexture* diffuseAlbedo,
        nvrhi::ITexture* specAlbedo,
        nvrhi::ITexture* normalRoughness,
        nvrhi::ITexture* specHitDistance,
        nvrhi::ITexture* outputColor) override;
#endif // STREAMLINE_FEATURE_DLSS_RR

    void TagResources_DLSS_NIS(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView* view,
        nvrhi::ITexture* output,
        nvrhi::ITexture* input) override;

    void TagResources_DLSS_FG(
        nvrhi::ICommandList* commandList,
        bool validViewportExtent = false,
        sl::Extent backBufferExtent = {}) override;

    void TagResources_UIColorAlpha(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView* view,
        nvrhi::ITexture* uiColorAlpha) override;

    void TagResources_UIAlpha(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView* view,
        nvrhi::ITexture* uiAlpha) override;
    
    void TagResources_DeepDVC(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView* view,
        nvrhi::ITexture* output) override;
    
    void TagResources_Latewarp(
        nvrhi::ICommandList* commandList,
        const donut::engine::IView* view,
        nvrhi::ITexture* backbuffer,
        nvrhi::ITexture* uiColorAlpha,
        nvrhi::ITexture* noWarpMask,
        sl::Extent backBufferExtent) override;

    void UnTagResources_DeepDVC() override;

    void SetDLSSOptions(const sl::DLSSOptions consts) override;
    void QueryDLSSOptimalSettings(DLSSSettings& settings) override;
    void EvaluateDLSS(nvrhi::ICommandList* commandList) override;
    void CleanupDLSS(bool wfi) override;

#ifdef STREAMLINE_FEATURE_DLSS_RR
    void GetDLSSRROptions(const sl::DLSSDOptions& options, sl::DLSSDOptimalSettings& outSettings) override;
    void SetDLSSRROptions(const sl::DLSSDOptions& options) override;
    void EvaluateDLSSRR(nvrhi::ICommandList* commandList) override;
    void CleanupDLSSRR(bool wfi) override;
#endif // STREAMLINE_FEATURE_DLSS_RR

    void SetNISOptions(const sl::NISOptions consts) override;
    void EvaluateNIS(nvrhi::ICommandList* commandList) override;
    void CleanupNIS(bool wfi) override;

    void SetDeepDVCOptions(const sl::DeepDVCOptions consts) override;
    void QueryDeepDVCState(uint64_t& estimatedVRamUsage) override;
    void EvaluateDeepDVC(nvrhi::ICommandList* commandList) override;
    void CleanupDeepDVC() override;

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

    void SetDLSSGOptions(const sl::DLSSGOptions consts) override;
    void QueryDLSSGState(uint64_t& estimatedVRamUsage, int& fps_multiplier, sl::DLSSGStatus& status, int& minSize, int& framesMax, void*& pFence, uint64_t& fenceValue, bool& vsyncSupported, bool& bIsDynamicMFGSupported) override;
    bool Get_DLSSG_SwapChainRecreation(bool& turn_on) const override;
    void CleanupDLSSG(bool wfi) override;
    uint64_t GetDLSSGLastFenceValue() override;

    bool Get_Latewarp_SwapChainRecreation(bool& turn_on) const override;
    void SetReflexCameraData(sl::FrameToken& frameToken, const sl::ReflexCameraData& cameraData) override;
#if STREAMLINE_FEATURE_LATEWARP
    void SetLatewarpOptions(const sl::LatewarpOptions& options) override;
    void EvaluateLatewarp(donut::app::DeviceManager& manager, nvrhi::ICommandList* commandList, RenderTargets* renderTargets, nvrhi::ITexture* inputColor, nvrhi::ITexture* outputColor, const donut::engine::IView* view) override {}
    void CleanupLatewarp(bool wfi) override;
#endif

    sl::Result SetTag(const sl::ResourceTag* resources, uint32_t numResources, sl::CommandBuffer* cmdBuffer)
    {
        return m_SLOptions.useNewSetTagAPI ? slSetTagForFrame(*m_currentFrame, m_viewport, resources, numResources, cmdBuffer) : slSetTag(m_viewport, resources, numResources, cmdBuffer);
    }
};

