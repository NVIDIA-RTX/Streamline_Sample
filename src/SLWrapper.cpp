//----------------------------------------------------------------------------------
// File:        SLWrapper.cpp
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
#include "SLWrapper.h" 

#include <donut/core/log.h>
#include <filesystem>
#include <dxgi.h>
#include <dxgi1_5.h>


#if DONUT_WITH_DX11
#include <d3d11.h>
#include <nvrhi/d3d11.h>
#endif
#if DONUT_WITH_DX12
#include <d3d12.h>
#include <nvrhi/d3d12.h>
#endif
#if DONUT_WITH_VULKAN
#include <vulkan/vulkan.h>
#include <nvrhi/vulkan.h>
#include <../src/vulkan/vulkan-backend.h>
#include <sl_helpers_vk.h>
#endif

#include "sl_security.h"

#include "DeviceManagerOverride/DeviceManagerOverride.h"

#ifndef _WIN32
#include <unistd.h>
#include <cstdio>
#include <climits>
#else
#define PATH_MAX MAX_PATH
#endif // _WIN32

using namespace donut;
using namespace donut::math;
using namespace donut::engine;

void logFunctionCallback(sl::LogType type, const char* msg) {

    if (type == sl::LogType::eError) {
        // Add a breakpoint here to break on errors
        donut::log::error(msg);
    }
    if (type == sl::LogType::eWarn) {
        // Add a breakpoint here to break on warnings
        donut::log::warning(msg);
    }
    else {
        donut::log::info(msg);
    }
}

static const std::map< const sl::Result, const std::string> errors = {
        {sl::Result::eErrorIO,"eErrorIO"},
        {sl::Result::eErrorDriverOutOfDate,"eErrorDriverOutOfDate"},
        {sl::Result::eErrorOSOutOfDate,"eErrorOSOutOfDate"},
        {sl::Result::eErrorOSDisabledHWS,"eErrorOSDisabledHWS"},
        {sl::Result::eErrorDeviceNotCreated,"eErrorDeviceNotCreated"},
        {sl::Result::eErrorAdapterNotSupported,"eErrorAdapterNotSupported"},
        {sl::Result::eErrorNoPlugins,"eErrorNoPlugins"},
        {sl::Result::eErrorVulkanAPI,"eErrorVulkanAPI"},
        {sl::Result::eErrorDXGIAPI,"eErrorDXGIAPI"},
        {sl::Result::eErrorD3DAPI,"eErrorD3DAPI"},
        {sl::Result::eErrorNRDAPI,"eErrorNRDAPI"},
        {sl::Result::eErrorNVAPI,"eErrorNVAPI"},
        {sl::Result::eErrorReflexAPI,"eErrorReflexAPI"},
        {sl::Result::eErrorNGXFailed,"eErrorNGXFailed"},
        {sl::Result::eErrorJSONParsing,"eErrorJSONParsing"},
        {sl::Result::eErrorMissingProxy,"eErrorMissingProxy"},
        {sl::Result::eErrorMissingResourceState,"eErrorMissingResourceState"},
        {sl::Result::eErrorInvalidIntegration,"eErrorInvalidIntegration"},
        {sl::Result::eErrorMissingInputParameter,"eErrorMissingInputParameter"},
        {sl::Result::eErrorNotInitialized,"eErrorNotInitialized"},
        {sl::Result::eErrorComputeFailed,"eErrorComputeFailed"},
        {sl::Result::eErrorInitNotCalled,"eErrorInitNotCalled"},
        {sl::Result::eErrorExceptionHandler,"eErrorExceptionHandler"},
        {sl::Result::eErrorInvalidParameter,"eErrorInvalidParameter"},
        {sl::Result::eErrorMissingConstants,"eErrorMissingConstants"},
        {sl::Result::eErrorDuplicatedConstants,"eErrorDuplicatedConstants"},
        {sl::Result::eErrorMissingOrInvalidAPI,"eErrorMissingOrInvalidAPI"},
        {sl::Result::eErrorCommonConstantsMissing,"eErrorCommonConstantsMissing"},
        {sl::Result::eErrorUnsupportedInterface,"eErrorUnsupportedInterface"},
        {sl::Result::eErrorFeatureMissing,"eErrorFeatureMissing"},
        {sl::Result::eErrorFeatureNotSupported,"eErrorFeatureNotSupported"},
        {sl::Result::eErrorFeatureMissingHooks,"eErrorFeatureMissingHooks"},
        {sl::Result::eErrorFeatureFailedToLoad,"eErrorFeatureFailedToLoad"},
        {sl::Result::eErrorFeatureWrongPriority,"eErrorFeatureWrongPriority"},
        {sl::Result::eErrorFeatureMissingDependency,"eErrorFeatureMissingDependency"},
        {sl::Result::eErrorFeatureManagerInvalidState,"eErrorFeatureManagerInvalidState"},
        {sl::Result::eErrorInvalidState,"eErrorInvalidState"},
        {sl::Result::eWarnOutOfVRAM,"eWarnOutOfVRAM"} };

bool successCheck(sl::Result result, char* location) {

    if (result == sl::Result::eOk)
        return true;

    auto a = errors.find(result);
    if (a != errors.end())
        logFunctionCallback(sl::LogType::eError, ("Error: " + a->second + (location == nullptr ? "" : (" encountered in " + std::string(location)))).c_str());
    else
        logFunctionCallback(sl::LogType::eError, ("Unknown error " + static_cast<int>(result) + (location == nullptr ? "" : (" encountered in " + std::string(location)))).c_str());
    
    return false;

}

std::wstring GetSlInterposerDllLocation() {

    wchar_t path[PATH_MAX] = { 0 };
#ifdef _WIN32
    if (GetModuleFileNameW(nullptr, path, dim(path)) == 0)
        return std::wstring();
#else // _WIN32
#error Unsupported platform for GetSlInterposerDllLocation!
#endif // _WIN32

    auto basePath = std::filesystem::path(path).parent_path();
    auto dllPath = basePath.wstring().append(L"\\sl.interposer.dll");
    return dllPath;
}

SLWrapper& SLWrapper::Get() {
    static SLWrapper instance;
    return instance;
}

void SLWrapper::SetSLOptions(const bool checkSig, const bool enableLog, const bool useNewSetTagAPI, const bool allowSMSCG)
{
    m_SLOptions.checkSig = checkSig;
    m_SLOptions.enableLog = enableLog;
    m_SLOptions.useNewSetTagAPI = useNewSetTagAPI;
    m_SLOptions.allowSMSCG = allowSMSCG;
}

bool SLWrapper::Initialize_preDevice(nvrhi::GraphicsAPI api)
{

    if (m_sl_initialised) {
        log::info("SLWrapper is already initialised.");
        return true;
    }

    sl::Preferences pref;

    m_api = api;

    if (m_api != nvrhi::GraphicsAPI::VULKAN) {
        pref.allocateCallback = &allocateResourceCallback;
        pref.releaseCallback = &releaseResourceCallback;
    }
    pref.applicationId = APP_ID;

#if _DEBUG
    pref.showConsole = true;
    pref.logMessageCallback = &logFunctionCallback;
    pref.logLevel = sl::LogLevel::eDefault;
#else
    if (m_SLOptions.enableLog) {
        pref.showConsole = true;
        pref.logMessageCallback = &logFunctionCallback;
        pref.logLevel = sl::LogLevel::eDefault;
    }
    else {
        pref.logLevel = sl::LogLevel::eOff;
    }
#endif

    sl::Feature featuresToLoad[] = {
#ifdef STREAMLINE_FEATURE_DLSS_SR
        sl::kFeatureDLSS,
#endif
#ifdef STREAMLINE_FEATURE_DLSS_RR
        sl::kFeatureDLSS_RR,
#endif
#ifdef STREAMLINE_FEATURE_NIS
        sl::kFeatureNIS,
#endif
#ifdef STREAMLINE_FEATURE_DLSS_FG
        sl::kFeatureDLSS_G,
#endif
#ifdef STREAMLINE_FEATURE_REFLEX
        sl::kFeatureReflex,
#endif
#ifdef STREAMLINE_FEATURE_DEEPDVC
        sl::kFeatureDeepDVC,
#endif
#ifdef STREAMLINE_FEATURE_LATEWARP
        sl::kFeatureLatewarp,
#endif
#ifdef STREAMLINE_FEATURE_IMGUI
        sl::kFeatureImGUI,
#endif
        // PCL is always implicitly loaded, but request it to ensure we never have 0-sized array
        sl::kFeaturePCL
    };
    pref.featuresToLoad = featuresToLoad;
    pref.numFeaturesToLoad = static_cast<uint32_t>(std::size(featuresToLoad));

    switch (api) {
    case (nvrhi::GraphicsAPI::D3D11):
        pref.renderAPI = sl::RenderAPI::eD3D11;
        break;
    case (nvrhi::GraphicsAPI::D3D12):
        pref.renderAPI = sl::RenderAPI::eD3D12;
        break;
    case (nvrhi::GraphicsAPI::VULKAN):
        pref.renderAPI = sl::RenderAPI::eVulkan;
        break;
    }

    pref.flags |= sl::PreferenceFlags::eUseManualHooking;
    if (m_SLOptions.useNewSetTagAPI)
    {
        pref.flags |= sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    }

    auto pathDll = GetSlInterposerDllLocation();

    HMODULE interposer = {};
    if (m_SLOptions.checkSig && sl::security::verifyEmbeddedSignature(pathDll.c_str())) {
        interposer = LoadLibraryW(pathDll.c_str());
    }
    else {
        interposer = LoadLibraryW(pathDll.c_str());
    }

    if (!interposer)
    {
        donut::log::error("Unable to load Streamline Interposer");
        return false;
    }

    m_sl_initialised = successCheck(slInit(pref, SDK_VERSION), "slInit");
    if (!m_sl_initialised) {
        log::error("Failed to initialse SL.");
        return false;
    }

    // turn off dlssg
    if (api == nvrhi::GraphicsAPI::D3D12) {
        slSetFeatureLoaded(sl::kFeatureDLSS_G, false);
    }

    return true;
}

bool SLWrapper::Initialize_postDevice(donut::app::DeviceManager* deviceManager)
{

    // We set reflex consts to a default config. This can be changed at runtime in the UI.
    auto reflexConst = sl::ReflexOptions{};
    reflexConst.mode = sl::ReflexMode::eOff;
    reflexConst.useMarkersToOptimize = false; // Not supported on single stage engine.
    reflexConst.virtualKey = VK_F13;
    reflexConst.frameLimitUs = 0;
    SetReflexConsts(reflexConst);

    return true;
}

void SLWrapper::QueueGPUWaitOnSyncObjectSet(nvrhi::IDevice* pDevice, nvrhi::CommandQueue cmdQType, void* syncObj, uint64_t syncObjVal)
{
    if (pDevice == nullptr)
    {
        log::fatal("Invalid device!");
    }

    switch (pDevice->getGraphicsAPI())
    {
#if DONUT_WITH_DX12
    case nvrhi::GraphicsAPI::D3D12:
    {
        auto pD3d12Device = dynamic_cast<nvrhi::d3d12::IDevice*>(pDevice);
        // device could be recreated during swapchain recreation
        if (pD3d12Device == nullptr) break;
        auto d3d12Queue = static_cast<ID3D12CommandQueue*>(pD3d12Device->getNativeQueue(nvrhi::ObjectTypes::D3D12_CommandQueue, cmdQType));
        d3d12Queue->Wait(reinterpret_cast<ID3D12Fence*>(syncObj), syncObjVal);
    }
    break;
#endif

#if DONUT_WITH_VULKAN
    case nvrhi::GraphicsAPI::VULKAN:
    {
        auto pVkDevice = dynamic_cast<nvrhi::vulkan::IDevice*>(pDevice);
        assert(pVkDevice != nullptr);
        pVkDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, reinterpret_cast<VkSemaphore>(syncObj), syncObjVal);
    }
    break;
#endif

    default:
        break;
    }
}


sl::FeatureRequirements SLWrapper::GetFeatureRequirements(sl::Feature feature) {
    sl::FeatureRequirements req;
    slGetFeatureRequirements(feature, req);
    return req;
}

// Helper function to log feature requirements details
void LogFeatureRequirements(const char* featureName, const sl::FeatureRequirements& req) {
    log::info("=== %s Feature Requirements ===", featureName);
    log::info("Flags: eD3D12Supported=%s, eVulkanSupported=%s, eHardwareSchedulingRequired=%s",
        (req.flags & sl::FeatureRequirementFlags::eD3D12Supported) ? "true" : "false",
        (req.flags & sl::FeatureRequirementFlags::eVulkanSupported) ? "true" : "false",
        (req.flags & sl::FeatureRequirementFlags::eHardwareSchedulingRequired) ? "true" : "false");
    log::info("maxNumCPUThreads: %u", req.maxNumCPUThreads);
    log::info("maxNumViewports: %u", req.maxNumViewports);
    log::info("numRequiredTags: %u", req.numRequiredTags);
    log::info("OS Version Detected: major=%u, minor=%u, build=%u",
        req.osVersionDetected.major, req.osVersionDetected.minor, req.osVersionDetected.build);
    log::info("OS Version Required: major=%u, minor=%u, build=%u",
        req.osVersionRequired.major, req.osVersionRequired.minor, req.osVersionRequired.build);
    log::info("Driver Version Detected: major=%u, minor=%u, build=%u",
        req.driverVersionDetected.major, req.driverVersionDetected.minor, req.driverVersionDetected.build);
    log::info("Driver Version Required: major=%u, minor=%u, build=%u",
        req.driverVersionRequired.major, req.driverVersionRequired.minor, req.driverVersionRequired.build);
    log::info("Vulkan Requirements: ComputeQueues=%u, GraphicsQueues=%u",
        req.vkNumComputeQueuesRequired, req.vkNumGraphicsQueuesRequired);
    log::info("Vulkan Device Extensions: %u", req.vkNumDeviceExtensions);
    log::info("Vulkan Instance Extensions: %u", req.vkNumInstanceExtensions);
    log::info("Vulkan Features12: %u", req.vkNumFeatures12);
    log::info("Vulkan Features13: %u", req.vkNumFeatures13);
    log::info("Vulkan Optical Flow Queues Required: %u", req.vkNumOpticalFlowQueuesRequired);
    log::info("=====================================");
}

sl::FeatureVersion SLWrapper::GetFeatureVersion(sl::Feature feature) {
    sl::FeatureVersion ver;
    slGetFeatureVersion(feature, ver);
    return ver;
}

void SLWrapper::SetDevice_raw(void* device_ptr)
{
#if DONUT_WITH_DX11
    if (m_api == nvrhi::GraphicsAPI::D3D11)
        successCheck(slSetD3DDevice((ID3D11Device*) device_ptr), "slSetD3DDevice");
#endif

#if DONUT_WITH_DX12
    if (m_api == nvrhi::GraphicsAPI::D3D12)
        successCheck(slSetD3DDevice((ID3D12Device*) device_ptr), "slSetD3DDevice");
#endif

#if DONUT_WITH_VULKAN
    if (m_api == nvrhi::GraphicsAPI::VULKAN)
    {
        successCheck(slSetVulkanInfo(*((sl::VulkanInfo*)device_ptr)), "slSetVulkanInfo");
    }
#endif

}

void SLWrapper::SetDevice_nvrhi(nvrhi::IDevice* device)
{
    m_Device = device;
}

void SLWrapper::UpdateFeatureAvailable(donut::app::DeviceManager* deviceManager){

    sl::AdapterInfo adapterInfo;

#if DONUT_WITH_DX11
    if (m_api == nvrhi::GraphicsAPI::D3D11) {
        auto device = static_cast<DeviceManagerOverride_DX11*>(deviceManager);
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(device->GetAdapter()->GetDesc(&desc)))
        {
            adapterInfo.deviceLUID = (uint8_t*)&desc.AdapterLuid;
            adapterInfo.deviceLUIDSizeInBytes = sizeof(LUID);
        }
    }
#endif
#if DONUT_WITH_DX12
    if (m_api == nvrhi::GraphicsAPI::D3D12) {
        auto a = ((ID3D12Device*)m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device))->GetAdapterLuid();
        adapterInfo.deviceLUID = (uint8_t*) &a;
        adapterInfo.deviceLUIDSizeInBytes = sizeof(LUID);
    }
#endif
#if DONUT_WITH_VULKAN
    if (m_api == nvrhi::GraphicsAPI::VULKAN) {
        adapterInfo.vkPhysicalDevice = m_Device->getNativeObject(nvrhi::ObjectTypes::VK_PhysicalDevice);
    }
#endif


    // Check if features are fully functional (2nd call of slIsFeatureSupported onwards)
#ifdef STREAMLINE_FEATURE_DLSS_SR
    sl::FeatureRequirements dlss_requirements;
    slGetFeatureRequirements(sl::kFeatureDLSS, dlss_requirements);
    LogFeatureRequirements("DLSS", dlss_requirements);

    m_dlss_available = slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo) == sl::Result::eOk;
    if (m_dlss_available) log::info("DLSS is supported on this system.");
    else log::warning("DLSS is not fully functional on this system.");
#endif

#ifdef STREAMLINE_FEATURE_DLSS_RR
    sl::FeatureRequirements dlssrr_requirements;
    slGetFeatureRequirements(sl::kFeatureDLSS_RR, dlssrr_requirements);
    LogFeatureRequirements("DLSS-RR", dlssrr_requirements);

    m_dlssrr_available = successCheck(slIsFeatureSupported(sl::kFeatureDLSS_RR, adapterInfo), "slIsFeatureSupported_DLSSRR");
    if (m_dlssrr_available) log::info("DLSS-RR is supported on this system.");
    else log::warning("DLSS-RR is not fully functional on this system.");
#endif

#ifdef STREAMLINE_FEATURE_NIS
    sl::FeatureRequirements nis_requirements;
    slGetFeatureRequirements(sl::kFeatureNIS, nis_requirements);
    LogFeatureRequirements("NIS", nis_requirements);

    m_nis_available = slIsFeatureSupported(sl::kFeatureNIS, adapterInfo) == sl::Result::eOk;
    if (m_nis_available) log::info("NIS is supported on this system.");
    else log::warning("NIS is not fully functional on this system.");
#endif

#ifdef STREAMLINE_FEATURE_DLSS_FG
    sl::FeatureRequirements dlssg_requirements;
    slGetFeatureRequirements(sl::kFeatureDLSS_G, dlssg_requirements);
    LogFeatureRequirements("DLSS-G", dlssg_requirements);

    m_dlssg_available = slIsFeatureSupported(sl::kFeatureDLSS_G, adapterInfo) == sl::Result::eOk;
    if (m_dlssg_available) log::info("DLSS-G is supported on this system.");
    else log::warning("DLSS-G is not fully functional on this system.");
#endif

    m_pcl_available = successCheck(slIsFeatureSupported(sl::kFeaturePCL, adapterInfo), "slIsFeatureSupported_PCL");
    if (m_pcl_available) log::info("PCL is supported on this system.");
    else log::warning("PCL is not fully functional on this system.");

#ifdef STREAMLINE_FEATURE_REFLEX
    sl::FeatureRequirements reflex_requirements;
    slGetFeatureRequirements(sl::kFeatureReflex, reflex_requirements);
    LogFeatureRequirements("Reflex", reflex_requirements);

    m_reflex_available = slIsFeatureSupported(sl::kFeatureReflex, adapterInfo) == sl::Result::eOk;
    if (m_reflex_available) log::info("Reflex is supported on this system.");
    else log::warning("Reflex is not fully functional on this system.");
#endif

#ifdef STREAMLINE_FEATURE_DEEPDVC
    sl::FeatureRequirements deepdvc_requirements;
    slGetFeatureRequirements(sl::kFeatureDeepDVC, deepdvc_requirements);
    LogFeatureRequirements("DeepDVC", deepdvc_requirements);

    m_deepdvc_available = slIsFeatureSupported(sl::kFeatureDeepDVC, adapterInfo) == sl::Result::eOk;
    if (m_deepdvc_available) log::info("DeepDVC is supported on this system.");
    else log::warning("DeepDVC is not fully functional on this system.");
#endif

#ifdef STREAMLINE_FEATURE_LATEWARP
    sl::FeatureRequirements latewarp_requirements;
    slGetFeatureRequirements(sl::kFeatureLatewarp, latewarp_requirements);
    LogFeatureRequirements("Latewarp", latewarp_requirements);

    m_latewarp_available = slIsFeatureSupported(sl::kFeatureLatewarp, adapterInfo) == sl::Result::eOk;
    if (m_latewarp_available) log::info("Latewarp is supported on this system.");
    else log::warning("Latewarp is not fully functional on this system.");
#endif
}

void SLWrapper::Shutdown()
{

    // Un-set all tags
    sl::ResourceTag inputs[] = {
        sl::ResourceTag{nullptr, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent} };
    successCheck(SetTag(inputs, _countof(inputs), nullptr), "slSetTag_clear");

    // Shutdown Streamline
    if (m_sl_initialised) {
        successCheck(slShutdown(), "slShutdown");
        m_sl_initialised = false;
    }

}

void SLWrapper::ProxyToNative(void* proxy, void** native)
{ 
    successCheck(slGetNativeInterface(proxy, native), "slGetNativeInterface");
    assert(native != nullptr);
};

void SLWrapper::NativeToProxy(void* native, void** proxy)
{
    proxy = &native;
    successCheck(slUpgradeInterface(proxy), "slUpgradeInterface");
    assert(proxy != nullptr);
};

void SLWrapper::SetSLConsts(const sl::Constants& consts) {
    if (!m_sl_initialised) {
        log::warning("SL not initialised.");
        return;
    }

    successCheck(slSetConstants(consts, *m_currentFrame, m_viewport), "slSetConstants");
}

void SLWrapper::FeatureLoad(sl::Feature feature, const bool turn_on) {

    if (m_api == nvrhi::GraphicsAPI::D3D12) {
        bool loaded;
        slIsFeatureLoaded(feature, loaded);
        if (loaded && !turn_on) {
            slSetFeatureLoaded(feature, turn_on);
        }
        else if (!loaded && turn_on) {
            slSetFeatureLoaded(feature, turn_on);
        }
    }
}

void SLWrapper::SetDLSSOptions(const sl::DLSSOptions consts)
{
    if (!m_sl_initialised || !m_dlss_available) {
        log::warning("SL not initialised or DLSS not available.");
        return;
    }

    m_dlss_consts = consts;
    successCheck(slDLSSSetOptions(m_viewport , m_dlss_consts), "slDLSSSetOptions");

}

#ifdef STREAMLINE_FEATURE_DLSS_RR
void SLWrapper::GetDLSSRROptions(const sl::DLSSDOptions& options, sl::DLSSDOptimalSettings& outSettings)
{
    if (!m_sl_initialised || !m_dlssrr_available) {
        log::warning("SL not initialised or DLSSRR not available.");
        return;
    }
    
    successCheck(slDLSSDGetOptimalSettings(options, outSettings), "slDLSSDGetOptimalSettings");

}

void SLWrapper::SetDLSSRROptions(const sl::DLSSDOptions& options)
{
    if (!m_sl_initialised)
    {
        log::error("Streamline not initialized");
    }
    successCheck(slDLSSDSetOptions(m_viewport, options), "slSetDLSSRROptions");
}
#endif // STREAMLINE_FEATURE_DLSS_RR

void SLWrapper::QueryDLSSOptimalSettings(DLSSSettings& settings) {
    if (!m_sl_initialised || !m_dlss_available) {
        log::warning("SL not initialised or DLSS not available.");
        settings = DLSSSettings{};
        return;
    }

    sl::DLSSOptimalSettings dlssOptimal = {};
    successCheck(slDLSSGetOptimalSettings(m_dlss_consts, dlssOptimal), "slDLSSGetOptimalSettings");

    settings.optimalRenderSize.x = static_cast<int>(dlssOptimal.optimalRenderWidth);
    settings.optimalRenderSize.y = static_cast<int>(dlssOptimal.optimalRenderHeight);
    settings.sharpness = dlssOptimal.optimalSharpness;

    settings.minRenderSize.x = dlssOptimal.renderWidthMin;
    settings.minRenderSize.y = dlssOptimal.renderHeightMin;
    settings.maxRenderSize.x = dlssOptimal.renderWidthMax;
    settings.maxRenderSize.y = dlssOptimal.renderHeightMax;
}

void SLWrapper::CleanupDLSS(bool wfi) {
    if (!m_sl_initialised) {
        log::warning("SL not initialised.");
        return;
    }
    if (!m_dlss_available)
    {
        return;
    }

    if (!m_dlss_available)
    {
        log::warning("DLSS not available.");
        return;
    }
    
    if (wfi) {
        m_Device->waitForIdle();
    }

    sl::Result status = slFreeResources(sl::kFeatureDLSS, m_viewport);
    // if we've never ran the feature on this viewport, this call may return 'eErrorInvalidParameter'
    assert(status == sl::Result::eOk || status == sl::Result::eErrorInvalidParameter);
}

#ifdef STREAMLINE_FEATURE_DLSS_RR
void SLWrapper::CleanupDLSSRR(bool wfi) {
    if (!m_sl_initialised) {
        log::warning("SL not initialised.");
        return;
    }
    if (!m_dlssrr_available)
    {
        return;
    }

    if (!m_dlssrr_available)
    {
        log::warning("DLSS-RR not available.");
        return;
    }
    
    if (wfi) {
        m_Device->waitForIdle();
    }

    sl::Result status = slFreeResources(sl::kFeatureDLSS_RR, m_viewport);
    // if we've never ran the feature on this viewport, this call may return 'eErrorInvalidParameter'
    assert(status == sl::Result::eOk || status == sl::Result::eErrorInvalidParameter);
}
#endif // STREAMLINE_FEATURE_DLSS_RR

void SLWrapper::SetNISOptions(const sl::NISOptions consts)
{
    if (!m_sl_initialised || !m_nis_available) {
        log::warning("SL not initialised or DLSS not available.");
        return;
    }

    m_nis_consts = consts;
    successCheck(slNISSetOptions(m_viewport, m_nis_consts), "slNISSetOptions");

}

void SLWrapper::CleanupNIS(bool wfi) {
    if (!m_sl_initialised) {
        log::warning("SL not initialised.");
        return;
    }

    if (!m_nis_available)
    {
        log::warning("NIS not available.");
        return;
    }

    if (wfi) {
        m_Device->waitForIdle();
    }

    sl::Result result = slFreeResources(sl::kFeatureNIS, m_viewport);
    // add an exception for eErrorMissingOrInvalidAPI for NIS plugin that doesn't export slFreeResources
    successCheck((result == sl::Result::eErrorMissingOrInvalidAPI ? sl::Result::eOk : result), "slFreeResources_NIS");
}

void SLWrapper::SetDeepDVCOptions(const sl::DeepDVCOptions consts)
{
    if (!m_sl_initialised || !m_deepdvc_available) {
        log::warning("SL not initialised or DeepDVC not available.");
        return;
    }

    m_deepdvc_consts = consts;
    successCheck(slDeepDVCSetOptions(m_viewport, m_deepdvc_consts), "slDeepDVCSetOptions");

}

void SLWrapper::CleanupDeepDVC() {
    if (!m_sl_initialised) {
        log::warning("SL not initialised.");
        return;
    }

    if (!m_deepdvc_available)
    {
        log::warning("DeepDVC not available.");
        return;
    }

    m_Device->waitForIdle();
    successCheck(slFreeResources(sl::kFeatureDeepDVC, m_viewport), "slFreeResources_DeepDVC");
}

void SLWrapper::SetDLSSGOptions(const sl::DLSSGOptions consts) {
    if (!m_sl_initialised || !m_dlssg_available) {
        log::warning("SL not initialised or DLSSG not available.");
        return;
    }

    m_dlssg_consts = consts;
    if (m_SLOptions.allowSMSCG)
    {
        if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
        {
            m_dlssg_consts.queueParallelismMode = sl::DLSSGQueueParallelismMode::eBlockNoClientQueues;
        }
    }

    successCheck(slDLSSGSetOptions(m_viewport, m_dlssg_consts), "slDLSSGSetOptions");
}

void SLWrapper::QueryDLSSGState(uint64_t& estimatedVRamUsage, int& fps_multiplier, sl::DLSSGStatus& status, int& minSize, int& maxFrameCount, void*& pFence, uint64_t& fenceValue) {
    if (!m_sl_initialised || !m_dlssg_available) {
        log::warning("SL not initialised or DLSSG not available.");
        return;
    }

    successCheck(slDLSSGGetState(m_viewport, m_dlssg_settings, &m_dlssg_consts), "slDLSSGGetState");

    estimatedVRamUsage = m_dlssg_settings.estimatedVRAMUsageInBytes;
    fps_multiplier = m_dlssg_settings.numFramesActuallyPresented;
    status = m_dlssg_settings.status;
    minSize = m_dlssg_settings.minWidthOrHeight;
    maxFrameCount = m_dlssg_settings.numFramesToGenerateMax;
    pFence = m_dlssg_settings.inputsProcessingCompletionFence;
    fenceValue = m_dlssg_settings.lastPresentInputsProcessingCompletionFenceValue;
}

uint64_t SLWrapper::GetDLSSGLastFenceValue() { return m_dlssg_settings.lastPresentInputsProcessingCompletionFenceValue; }

bool SLWrapper::Get_DLSSG_SwapChainRecreation(bool& turn_on) const {
    turn_on = m_dlssg_shoudLoad;
    auto tmp = m_dlssg_triggerswapchainRecreation;
    return tmp;
}

void SLWrapper::CleanupDLSSG(bool wfi) {
    if (!m_sl_initialised) {
        log::warning("SL not initialised.");
        return;
    }

    if (!m_dlssg_available)
    {
        log::warning("DLSSG not available.");
        return;
    }

    if (wfi) {
        m_Device->waitForIdle();
    }

    sl::Result status = slFreeResources(sl::kFeatureDLSS_G, m_viewport);
    // if we've never ran the feature on this viewport, this call may return 'eErrorInvalidParameter'
    assert(status == sl::Result::eOk || status == sl::Result::eErrorInvalidParameter || status == sl::Result::eErrorFeatureMissing);
}
#if STREAMLINE_FEATURE_LATEWARP
void SLWrapper::CleanupLatewarp(bool wfi) {
    if (!m_sl_initialised) {
        log::warning("SL not initialised.");
        return;
    }

    if (!m_latewarp_available)
    {
        log::warning("Latewarp not available.");
        return;
    }

    if (wfi) {
        m_Device->waitForIdle();
    }

    sl::Result status = slFreeResources(sl::kFeatureLatewarp, m_viewport);
    assert(status == sl::Result::eOk || status == sl::Result::eErrorFeatureMissing);
}
#endif
sl::Resource SLWrapper::allocateResourceCallback(const sl::ResourceAllocationDesc* resDesc, void* device) {

    sl::Resource res = {};

    if (device == nullptr) {
        log::warning("No device available for allocation.");
        return res;
    }

    bool isBuffer = (resDesc->type == sl::ResourceType::eBuffer);

    if (isBuffer) {

#if DONUT_WITH_DX11

        if (Get().m_api == nvrhi::GraphicsAPI::D3D11)
        {
            D3D11_BUFFER_DESC* desc = (D3D11_BUFFER_DESC*)resDesc->desc;
            ID3D11Device* pd3d11Device = (ID3D11Device*)device;
            ID3D11Buffer* pbuffer;
            bool success = SUCCEEDED(pd3d11Device->CreateBuffer(desc, nullptr, &pbuffer));
            if (!success) log::error("Failed to create buffer in SL allocation callback");
            res.type = resDesc->type;
            res.native = pbuffer;

        }
#endif

#if DONUT_WITH_DX12
        if (Get().m_api == nvrhi::GraphicsAPI::D3D12)
        {
            D3D12_RESOURCE_DESC* desc = (D3D12_RESOURCE_DESC*)resDesc->desc;
            D3D12_HEAP_PROPERTIES* heap = (D3D12_HEAP_PROPERTIES*)resDesc->heap;
            D3D12_RESOURCE_STATES state = (D3D12_RESOURCE_STATES)resDesc->state;
            ID3D12Device* pd3d12Device = (ID3D12Device*)device;
            ID3D12Resource* pbuffer;
            bool success = SUCCEEDED(pd3d12Device->CreateCommittedResource(heap, D3D12_HEAP_FLAG_NONE, desc, state, nullptr, IID_PPV_ARGS(&pbuffer)));
            if (!success) log::error("Failed to create buffer in SL allocation callback");
            res.type = resDesc->type;
            res.native = pbuffer;
        }
#endif

    }

    else {

#if DONUT_WITH_DX11

        if (Get().m_api == nvrhi::GraphicsAPI::D3D11)
        {
            D3D11_TEXTURE2D_DESC* desc = (D3D11_TEXTURE2D_DESC*)resDesc->desc;
            ID3D11Device* pd3d11Device = (ID3D11Device*)device;
            ID3D11Texture2D* ptexture;
            bool success = SUCCEEDED(pd3d11Device->CreateTexture2D(desc, nullptr, &ptexture));
            if (!success) log::error("Failed to create texture in SL allocation callback");
            res.type = resDesc->type;
            res.native = ptexture;

        }
#endif

#if DONUT_WITH_DX12
        if (Get().m_api == nvrhi::GraphicsAPI::D3D12)
        {
            D3D12_RESOURCE_DESC* desc = (D3D12_RESOURCE_DESC*)resDesc->desc;
            D3D12_RESOURCE_STATES state = (D3D12_RESOURCE_STATES)resDesc->state;
            D3D12_HEAP_PROPERTIES* heap = (D3D12_HEAP_PROPERTIES*)resDesc->heap;
            ID3D12Device* pd3d12Device = (ID3D12Device*)device;
            ID3D12Resource* ptexture;
            D3D12_CLEAR_VALUE* pClearValue = nullptr;
            D3D12_CLEAR_VALUE clearValue;
            // specify the clear value to avoid D3D warnings on ClearRenderTarget()
            if (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
            {
                clearValue.Format = desc->Format;
                memset(clearValue.Color, 0, sizeof(clearValue.Color));
                pClearValue = &clearValue;
            }
            bool success = SUCCEEDED(pd3d12Device->CreateCommittedResource(heap, D3D12_HEAP_FLAG_NONE, desc, state, pClearValue, IID_PPV_ARGS(&ptexture)));
            if (!success) log::error("Failed to create texture in SL allocation callback");
            res.type = resDesc->type;
            res.native = ptexture;
        }
#endif

    }
    return res;

}

void SLWrapper::releaseResourceCallback(sl::Resource* resource, void* device)
{
    if (resource)
    {
        auto i = (IUnknown*)resource->native;
        i->Release();
    }
};

#if DONUT_WITH_DX12
D3D12_RESOURCE_STATES D3D12convertResourceStates(nvrhi::ResourceStates stateBits)
{
    if (stateBits == nvrhi::ResourceStates::Common)
        return D3D12_RESOURCE_STATE_COMMON;

    D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON; // also 0

    if ((stateBits & nvrhi::ResourceStates::ConstantBuffer) != 0) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if ((stateBits & nvrhi::ResourceStates::VertexBuffer) != 0) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if ((stateBits & nvrhi::ResourceStates::IndexBuffer) != 0) result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if ((stateBits & nvrhi::ResourceStates::IndirectArgument) != 0) result |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    if ((stateBits & nvrhi::ResourceStates::ShaderResource) != 0) result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if ((stateBits & nvrhi::ResourceStates::UnorderedAccess) != 0) result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if ((stateBits & nvrhi::ResourceStates::RenderTarget) != 0) result |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    if ((stateBits & nvrhi::ResourceStates::DepthWrite) != 0) result |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if ((stateBits & nvrhi::ResourceStates::DepthRead) != 0) result |= D3D12_RESOURCE_STATE_DEPTH_READ;
    if ((stateBits & nvrhi::ResourceStates::StreamOut) != 0) result |= D3D12_RESOURCE_STATE_STREAM_OUT;
    if ((stateBits & nvrhi::ResourceStates::CopyDest) != 0) result |= D3D12_RESOURCE_STATE_COPY_DEST;
    if ((stateBits & nvrhi::ResourceStates::CopySource) != 0) result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    if ((stateBits & nvrhi::ResourceStates::ResolveDest) != 0) result |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
    if ((stateBits & nvrhi::ResourceStates::ResolveSource) != 0) result |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    if ((stateBits & nvrhi::ResourceStates::Present) != 0) result |= D3D12_RESOURCE_STATE_PRESENT;
    if ((stateBits & nvrhi::ResourceStates::AccelStructRead) != 0) result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if ((stateBits & nvrhi::ResourceStates::AccelStructWrite) != 0) result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if ((stateBits & nvrhi::ResourceStates::AccelStructBuildInput) != 0) result |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if ((stateBits & nvrhi::ResourceStates::AccelStructBuildBlas) != 0) result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if ((stateBits & nvrhi::ResourceStates::ShadingRateSurface) != 0) result |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;

    return result;
}
#endif

static inline nvrhi::Object GetNativeCommandList(nvrhi::ICommandList* commandList)
{
    if (commandList == nullptr)
    {
        log::error("Invalid command list!");
        return nullptr;
    }

    if (commandList->getDevice() == nullptr)
    {
        log::error("No device available.");
        return nullptr;
    }

    nvrhi::ObjectType objType{};
    switch (commandList->getDevice()->getGraphicsAPI())
    {
#if DONUT_WITH_DX11
    case nvrhi::GraphicsAPI::D3D11:
        objType = nvrhi::ObjectTypes::D3D11_DeviceContext;
        break;
#endif

#if DONUT_WITH_DX12
    case nvrhi::GraphicsAPI::D3D12:
        objType = nvrhi::ObjectTypes::D3D12_GraphicsCommandList;
        break;
#endif

#if DONUT_WITH_VULKAN
    case nvrhi::GraphicsAPI::VULKAN:
        objType = nvrhi::ObjectTypes::VK_CommandBuffer;
        break;
#endif

    default:
        log::error("Unsupported graphics API!");
        break;
    }

    return commandList->getNativeObject(objType);
}

#if DONUT_WITH_VULKAN
static inline VkImageLayout toVkImageLayout(nvrhi::ResourceStates stateBits)
{
    switch (stateBits)
    {
    case nvrhi::ResourceStates::Common:
    case nvrhi::ResourceStates::UnorderedAccess: return VK_IMAGE_LAYOUT_GENERAL;
    case nvrhi::ResourceStates::ShaderResource: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case nvrhi::ResourceStates::RenderTarget: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case nvrhi::ResourceStates::DepthWrite: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case nvrhi::ResourceStates::DepthRead: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case nvrhi::ResourceStates::CopyDest:
    case nvrhi::ResourceStates::ResolveDest: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case nvrhi::ResourceStates::CopySource:
    case nvrhi::ResourceStates::ResolveSource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case nvrhi::ResourceStates::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    default: return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}
#endif

static void GetSLResource(
    nvrhi::ICommandList* commandList,
    sl::Resource& slResource,
    nvrhi::ITexture* inputTex,
    const donut::engine::IView* view)
{
    if (inputTex == nullptr)
    {
        log::error("GetSLResource: Invalid slResource!");
        return;
    }
    if (commandList == nullptr)
    {
        log::error("Invalid command list!");
        return;
    }

    if (commandList->getDevice() == nullptr)
    {
        log::error("No device available.");
        return;
    }

    switch (commandList->getDevice()->getGraphicsAPI())
    {
#if DONUT_WITH_DX11
    case nvrhi::GraphicsAPI::D3D11:
        slResource = sl::Resource{ sl::ResourceType::eTex2d, inputTex->getNativeObject(nvrhi::ObjectTypes::D3D11_Resource), 0 };
        break;
#endif

#if DONUT_WITH_DX12
    case nvrhi::GraphicsAPI::D3D12:
        slResource = sl::Resource{ sl::ResourceType::eTex2d, inputTex->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource), nullptr, nullptr, static_cast<uint32_t>(D3D12convertResourceStates(inputTex->getDesc().initialState)) };
        break;
#endif

#if DONUT_WITH_VULKAN
    case nvrhi::GraphicsAPI::VULKAN:
        {
            nvrhi::TextureSubresourceSet subresources = view->GetSubresources();
            auto const& desc = inputTex->getDesc();
            auto const& vkDesc = ((nvrhi::vulkan::Texture*)inputTex)->imageInfo;

            slResource = sl::Resource{ sl::ResourceType::eTex2d, inputTex->getNativeObject(nvrhi::ObjectTypes::VK_Image),
                inputTex->getNativeObject(nvrhi::ObjectTypes::VK_DeviceMemory),
                inputTex->getNativeView(nvrhi::ObjectTypes::VK_ImageView, desc.format, subresources),
                static_cast<uint32_t>(toVkImageLayout(desc.initialState)) };
            slResource.width = desc.width;
            slResource.height = desc.height;
            slResource.nativeFormat = static_cast<uint32_t>(nvrhi::vulkan::convertFormat(desc.format));
            slResource.mipLevels = desc.mipLevels;
            slResource.arrayLayers = vkDesc.arrayLayers;
            slResource.flags = static_cast<uint32_t>(vkDesc.flags);
            slResource.usage = static_cast<uint32_t>(vkDesc.usage);
        }
        break;
#endif

    default:
        log::error("Unsupported graphics API.");
        break;
    }
}

void SLWrapper::TagResources_General(
    nvrhi::ICommandList* commandList,
    const donut::engine::IView* view,
    nvrhi::ITexture* motionVectors,
    nvrhi::ITexture* depth,
    nvrhi::ITexture* finalColorHudless)
{
    if (!m_sl_initialised) {
        log::warning("Streamline not initialised.");
        return;
    }

    sl::Extent renderExtent{ 0, 0, depth->getDesc().width, depth->getDesc().height };
    sl::Extent fullExtent{ 0, 0, finalColorHudless->getDesc().width, finalColorHudless->getDesc().height };
    void* cmdbuffer = GetNativeCommandList(commandList);
    sl::Resource motionVectorsResource{}, depthResource{}, finalColorHudlessResource{};

    GetSLResource(commandList, motionVectorsResource, motionVectors, view);
    GetSLResource(commandList, depthResource, depth, view);
    GetSLResource(commandList, finalColorHudlessResource, finalColorHudless, view);

    sl::ResourceTag motionVectorsResourceTag = sl::ResourceTag{ &motionVectorsResource, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
    sl::ResourceTag depthResourceTag = sl::ResourceTag{ &depthResource, sl::kBufferTypeDepth, sl::ResourceLifecycle::eOnlyValidNow, &renderExtent }; // VK render path depth does not last until present
    sl::ResourceTag finalColorHudlessResourceTag = sl::ResourceTag{ &finalColorHudlessResource, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };

    sl::ResourceTag inputs[] = {motionVectorsResourceTag, depthResourceTag, finalColorHudlessResourceTag };
    successCheck(SetTag(inputs, _countof(inputs), cmdbuffer), "slSetTag_General");

}

#ifdef STREAMLINE_FEATURE_DLSS_RR
void SLWrapper::TagResources_DLSS_RR(
    nvrhi::ICommandList * commandList,
    const donut::engine::IView* view,
    nvrhi::ITexture* inputColor,
    nvrhi::ITexture* diffuseAlbedo,
    nvrhi::ITexture* specAlbedo,
    nvrhi::ITexture* normalRoughness,
    nvrhi::ITexture* specHitDistance,
    nvrhi::ITexture* outputColor)
{
    if (!m_sl_initialised) {
        log::warning("Streamline not initialised.");
        return;
    }
    if (m_Device == nullptr) {
        log::error("No device available.");
        return;
    }

    sl::Extent renderExtent{ 0, 0, inputColor->getDesc().width, inputColor->getDesc().height };
    sl::Extent fullExtent{ 0, 0, outputColor->getDesc().width, outputColor->getDesc().height };
    sl::Resource inputColorResource, diffuseAlbedoResource, specAlbedoResource, normalRoughnessResource, specHitDistanceResource, outputColorResource ;
    void* cmdbuffer = GetNativeCommandList(commandList);

    GetSLResource(commandList, inputColorResource, inputColor, view);
    GetSLResource(commandList, diffuseAlbedoResource, diffuseAlbedo, view);
    GetSLResource(commandList, specAlbedoResource, specAlbedo, view);
    GetSLResource(commandList, normalRoughnessResource, normalRoughness, view);
    GetSLResource(commandList, specHitDistanceResource, specHitDistance, view);
    GetSLResource(commandList, outputColorResource, outputColor, view);


    sl::ResourceTag inputColorResourceTag = sl::ResourceTag{ &inputColorResource, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
    sl::ResourceTag diffuseAlbedoResourceTag = sl::ResourceTag{ &diffuseAlbedoResource, sl::kBufferTypeAlbedo, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
    sl::ResourceTag specAlbedoResourceTag = sl::ResourceTag{ &specAlbedoResource, sl::kBufferTypeSpecularAlbedo, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
    sl::ResourceTag normalRoughnessResourceTag = sl::ResourceTag{ &normalRoughnessResource, sl::kBufferTypeNormalRoughness, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
    sl::ResourceTag specHitDistanceResourceTag = sl::ResourceTag{ &specHitDistanceResource, sl::kBufferTypeSpecularHitDistance, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
    sl::ResourceTag outputColorResourceTag = sl::ResourceTag{ &outputColorResource, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };

    sl::ResourceTag inputs[] = {inputColorResourceTag, diffuseAlbedoResourceTag, specAlbedoResourceTag, normalRoughnessResourceTag, specHitDistanceResourceTag, outputColorResourceTag};
    successCheck(SetTag(inputs, _countof(inputs), cmdbuffer), "slSetTag_DLSSRR");
}
#endif // STREAMLINE_FEATURE_DLSS_RR

void SLWrapper::TagResources_DLSS_NIS(
    nvrhi::ICommandList * commandList,
    const donut::engine::IView * view,
    nvrhi::ITexture * Output,
    nvrhi::ITexture * Input)
{
    if (!m_sl_initialised) {
        log::warning("Streamline not initialised.");
        return;
    }

    sl::Extent renderExtent{ 0, 0, Input->getDesc().width, Input->getDesc().height };
    sl::Extent fullExtent{ 0, 0, Output->getDesc().width, Output->getDesc().height };
    void* cmdbuffer = GetNativeCommandList(commandList);
    sl::Resource outputResource{}, inputResource{};

    GetSLResource(commandList, outputResource, Output, view);
    GetSLResource(commandList, inputResource, Input, view);

    sl::ResourceTag inputResourceTag = sl::ResourceTag{ &inputResource, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
    sl::ResourceTag outputResourceTag = sl::ResourceTag{ &outputResource, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };

    sl::ResourceTag inputs[] = { inputResourceTag, outputResourceTag };
    successCheck(SetTag(inputs, _countof(inputs), cmdbuffer), "slSetTag_dlss_nis");

}

void SLWrapper::TagResources_DLSS_FG(
    nvrhi::ICommandList* commandList,
    bool validViewportExtent,
    sl::Extent backBufferExtent)
{
    if (!m_sl_initialised) {
        log::warning("Streamline not initialised.");
        return;
    }

    void* cmdbuffer = GetNativeCommandList(commandList);

    // tag backbuffer resource mainly to pass extent data and therefore resource can be nullptr.
    // If the viewport extent is invalid - set extent to null. This informs streamline that full resource extent needs to be used
    sl::ResourceTag backBufferResourceTag = sl::ResourceTag{ nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle{}, validViewportExtent ? &backBufferExtent : nullptr };
    sl::ResourceTag inputs[] = { backBufferResourceTag };
    successCheck(SetTag(inputs, _countof(inputs), cmdbuffer), "slSetTag_dlss_fg");
}

void SLWrapper::TagResources_DeepDVC(
    nvrhi::ICommandList * commandList,
    const donut::engine::IView * view,
    nvrhi::ITexture * Output)
{
    if (!m_sl_initialised) {
        log::warning("Streamline not initialised.");
        return;
    }

    sl::Extent fullExtent{ 0, 0, Output->getDesc().width, Output->getDesc().height };
    void* cmdbuffer = GetNativeCommandList(commandList);
    sl::Resource outputResource{};

    GetSLResource(commandList, outputResource, Output, view);

    sl::ResourceTag outputResourceTag = sl::ResourceTag{ &outputResource, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent };

    sl::ResourceTag inputs[] = { outputResourceTag };
    successCheck(SetTag(inputs, _countof(inputs), cmdbuffer), "slSetTag_deepdvc");
}

void SLWrapper::TagResources_Latewarp(
    nvrhi::ICommandList* commandList,
    const donut::engine::IView* view,
    nvrhi::ITexture* backBuffer,
    nvrhi::ITexture* uiColorAlpha,
    nvrhi::ITexture* noWarpMask,
    sl::Extent backBufferExtent)
{
    if (!m_sl_initialised) {
        log::warning("Streamline not initialised.");
        return;
    }
    if (m_Device == nullptr) {
        log::error("No device available.");
        return;
    }

    void* cmdbuffer = GetNativeCommandList(commandList);
    sl::Resource noWarpMaskResource{}, uiColorAlphaResource{}, backBufferResource{};

    GetSLResource(commandList, backBufferResource, backBuffer, view);
    sl::ResourceTag backbufferResourceTag =   sl::ResourceTag{ &backBufferResource,   sl::kBufferTypeBackbuffer,      sl::ResourceLifecycle::eValidUntilPresent, &backBufferExtent };

    std::vector<sl::ResourceTag> inputs;
    if (uiColorAlpha)
    {
        GetSLResource(commandList, uiColorAlphaResource, uiColorAlpha, view);
        sl::Extent renderExtent{ 0, 0, uiColorAlpha->getDesc().width, uiColorAlpha->getDesc().height };
        sl::ResourceTag uiColorAlphaResourceTag = sl::ResourceTag{ &uiColorAlphaResource, sl::kBufferTypeUIColorAndAlpha, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
        inputs.push_back(uiColorAlphaResourceTag);
    }
    if (noWarpMask)
    {
        GetSLResource(commandList, noWarpMaskResource, noWarpMask, view);
        sl::Extent renderExtent{ 0, 0, noWarpMask->getDesc().width, noWarpMask->getDesc().height };
        sl::ResourceTag noWarpMaskTag = sl::ResourceTag{ &noWarpMaskResource,   sl::kBufferTypeNoWarpMask,      sl::ResourceLifecycle::eValidUntilPresent, &renderExtent };
        inputs.push_back(noWarpMaskTag);
    }

    inputs.push_back(backbufferResourceTag);
    successCheck(SetTag(inputs.data(), static_cast<uint32_t>(inputs.size()), cmdbuffer), "slSetTag_latewarp");
}

void SLWrapper::UnTagResources_DeepDVC()
{
    sl::ResourceTag outputResourceTag = sl::ResourceTag{ nullptr, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent };

    sl::ResourceTag inputs[] = { outputResourceTag };
    successCheck(SetTag(inputs, _countof(inputs), nullptr), "slSetTag_deepdvc_untag");
}

void SLWrapper::EvaluateDLSS(nvrhi::ICommandList* commandList) {

    void* nativeCommandList = nullptr;

#if DONUT_WITH_DX11
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11)
        nativeCommandList = m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_DeviceContext);
#endif

#if DONUT_WITH_DX12
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12)
        nativeCommandList = commandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList);
#endif

#if DONUT_WITH_VULKAN
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
        nativeCommandList = commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);
#endif

    if (nativeCommandList == nullptr) {
        log::warning("Failed to retrieve context for DLSS evaluation.");
        return;
    }

    sl::ViewportHandle view(m_viewport);
    const sl::BaseStructure* inputs[] = { &view };
    successCheck(slEvaluateFeature(sl::kFeatureDLSS, *m_currentFrame, inputs, _countof(inputs), nativeCommandList), "slEvaluateFeature_DLSS");

    //Our pipeline is very simple so we can simply clear it, but normally state tracking should be implemented.
    commandList->clearState();

}

#ifdef STREAMLINE_FEATURE_DLSS_RR
void SLWrapper::EvaluateDLSSRR(nvrhi::ICommandList* commandList)
{
    void* nativeCommandList = nullptr;

#ifdef DONUT_WITH_DX11
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11)
        nativeCommandList = m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_DeviceContext);
#endif

#ifdef DONUT_WITH_DX12
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12)
        nativeCommandList = commandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList);
#endif

#ifdef DONUT_WITH_VULKAN
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
        nativeCommandList = commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);
#endif

    if (nativeCommandList == nullptr) {
        log::warning("Failed to retrieve context for DLSS RR evaluation.");
        return;
    }

    sl::ViewportHandle view(m_viewport);
    const sl::BaseStructure* inputs[] = { &view };
    successCheck(slEvaluateFeature(sl::kFeatureDLSS_RR, *m_currentFrame, inputs, _countof(inputs), nativeCommandList), "slEvaluateFeature_DLSS_RR");

    //Our pipeline is very simple so we can simply clear it, but normally state tracking should be implemented.
    commandList->clearState();

}
#endif // STREAMLINE_FEATURE_DLSS_RR

void SLWrapper::EvaluateNIS(nvrhi::ICommandList* commandList) {

    void* nativeCommandList = nullptr;

#if DONUT_WITH_DX11
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11)
        nativeCommandList = m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_DeviceContext);
#endif

#if DONUT_WITH_DX12
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12)
        nativeCommandList = commandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList);
#endif

#if DONUT_WITH_VULKAN
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
        nativeCommandList = commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);
#endif

    if (nativeCommandList == nullptr) {
        log::warning("Failed to retrieve context for NIS evaluation.");
        return;
    }

    sl::ViewportHandle view(m_viewport);
    const sl::BaseStructure* inputs[] = { &view };
    successCheck(slEvaluateFeature(sl::kFeatureNIS, *m_currentFrame, inputs, _countof(inputs), nativeCommandList), "slEvaluateFeature_NIS");

    //Our pipeline is very simple so we can simply clear it, but normally state tracking should be implemented.
    commandList->clearState();

}

void SLWrapper::EvaluateDeepDVC(nvrhi::ICommandList* commandList) {

    void* nativeCommandList = nullptr;

#if DONUT_WITH_DX11
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11)
        nativeCommandList = m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_DeviceContext);
#endif

#if DONUT_WITH_DX12
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12)
        nativeCommandList = commandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList);
#endif

#if DONUT_WITH_VULKAN
    if (m_Device->getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
        nativeCommandList = commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);
#endif

    if (nativeCommandList == nullptr) {
        log::warning("Failed to retrieve context for NIS evaluation.");
        return;
    }

    sl::ViewportHandle view(m_viewport);
    const sl::BaseStructure* inputs[] = { &view };
    successCheck(slEvaluateFeature(sl::kFeatureDeepDVC, *m_currentFrame, inputs, _countof(inputs), nativeCommandList), "slEvaluateFeature_DeepDVC");

    //Our pipeline is very simple so we can simply clear it, but normally state tracking should be implemented.
    commandList->clearState();

}

void SLWrapper::QueryDeepDVCState(uint64_t& estimatedVRamUsage)
{
    if (!m_sl_initialised || !m_deepdvc_available) {
        log::warning("SL not initialised or DeepDVC not available.");
        return;
    }
    sl::DeepDVCState state;
    successCheck(slDeepDVCGetState(m_viewport, state), "slDeepDVCGetState");
    estimatedVRamUsage = state.estimatedVRAMUsageInBytes;
}

void SLWrapper::SetReflexConsts(const sl::ReflexOptions options)
{
    if (!m_sl_initialised || !m_reflex_available)
    {
        log::warning("SL not initialised or Reflex not available.");
        return;
    }

    m_reflex_consts = options;
    successCheck(slReflexSetOptions(m_reflex_consts), "Reflex_Options");

    return;
}

void SLWrapper::ReflexCallback_Sleep(donut::app::DeviceManager& manager, uint32_t frameID) {
    if (SLWrapper::Get().GetReflexAvailable()) {
        successCheck(slGetNewFrameToken(SLWrapper::Get().m_currentFrame, &frameID), "SL_GetFrameToken");
        successCheck(slReflexSleep(*SLWrapper::Get().m_currentFrame), "Reflex_Sleep");
    }
}

void SLWrapper::ReflexCallback_SimStart(donut::app::DeviceManager& manager, uint32_t frameID) {
    if (SLWrapper::Get().GetPCLAvailable()){
        sl::FrameToken* temp;
        successCheck(slGetNewFrameToken(temp, &frameID), "SL_GetFrameToken");
        successCheck(slPCLSetMarker(sl::PCLMarker::eSimulationStart, *temp), "PCL_SimStart");
    }
}

void SLWrapper::ReflexCallback_SimEnd(donut::app::DeviceManager& manager, uint32_t frameID) {
    if (SLWrapper::Get().GetPCLAvailable())
    {
        sl::FrameToken* temp;
        successCheck(slGetNewFrameToken(temp, &frameID), "SL_GetFrameToken");
        successCheck(slPCLSetMarker(sl::PCLMarker::eSimulationEnd, *temp), "PCL_SimEnd");
    }
}

void SLWrapper::ReflexCallback_RenderStart(donut::app::DeviceManager& manager, uint32_t frameID) {
    if (SLWrapper::Get().GetPCLAvailable())
    {
        sl::FrameToken* temp;
        successCheck(slGetNewFrameToken(temp, &frameID), "SL_GetFrameToken");
        successCheck(slPCLSetMarker(sl::PCLMarker::eRenderSubmitStart, *temp), "PCL_SubmitStart");
    }
}

void SLWrapper::ReflexCallback_RenderEnd(donut::app::DeviceManager& manager, uint32_t frameID) {
    if (SLWrapper::Get().GetPCLAvailable())
    {
        sl::FrameToken* temp;
        successCheck(slGetNewFrameToken(temp, &frameID), "SL_GetFrameToken");
        successCheck(slPCLSetMarker(sl::PCLMarker::eRenderSubmitEnd, *temp), "PCL_SubmitEnd");
    }
}

void SLWrapper::ReflexCallback_PresentStart(donut::app::DeviceManager& manager, uint32_t frameID) {
    if (SLWrapper::Get().GetPCLAvailable())
    {
        sl::FrameToken* temp;
        successCheck(slGetNewFrameToken(temp, &frameID), "SL_GetFrameToken");
        successCheck(slPCLSetMarker(sl::PCLMarker::ePresentStart, *temp), "PCL_PresentStart");
    }
}

void SLWrapper::ReflexCallback_PresentEnd(donut::app::DeviceManager& manager, uint32_t frameID) {
    if (SLWrapper::Get().GetPCLAvailable())
    {
        sl::FrameToken* temp;
        successCheck(slGetNewFrameToken(temp, &frameID), "SL_GetFrameToken");
        successCheck(slPCLSetMarker(sl::PCLMarker::ePresentEnd, *temp), "PCL_PresentEnd");
    }
}

void SLWrapper::ReflexTriggerFlash() {
    successCheck(slPCLSetMarker(sl::PCLMarker::eTriggerFlash, *SLWrapper::Get().m_currentFrame), "Reflex_Flash");
}

void SLWrapper::ReflexTriggerPcPing() {
    if (SLWrapper::Get().GetPCLAvailable())
    {
        successCheck(slPCLSetMarker(sl::PCLMarker::ePCLatencyPing, *SLWrapper::Get().m_currentFrame), "PCL_PCPing");
    }
}

void SLWrapper::QueryReflexStats(bool& reflex_lowLatencyAvailable, bool& reflex_flashAvailable, std::string& stats) {
    if (SLWrapper::GetReflexAvailable()) {
        sl::ReflexState state;
        successCheck(slReflexGetState(state), "Reflex_State");

        reflex_lowLatencyAvailable = state.lowLatencyAvailable;
        reflex_flashAvailable = state.flashIndicatorDriverControlled;

        auto rep = state.frameReport[63];
        if (state.latencyReportAvailable && rep.gpuRenderEndTime != 0) {

            auto frameID = rep.frameID;
            auto totalGameToRenderLatencyUs = rep.gpuRenderEndTime - rep.inputSampleTime;
            auto simDeltaUs = rep.simEndTime - rep.simStartTime;
            auto renderDeltaUs = rep.renderSubmitEndTime - rep.renderSubmitStartTime;
            auto presentDeltaUs = rep.presentEndTime - rep.presentStartTime;
            auto driverDeltaUs = rep.driverEndTime - rep.driverStartTime;
            auto osRenderQueueDeltaUs = rep.osRenderQueueEndTime - rep.osRenderQueueStartTime;
            auto gpuRenderDeltaUs = rep.gpuRenderEndTime - rep.gpuRenderStartTime;

            stats =  "frameID: " + std::to_string(frameID);
            stats += "\ntotalGameToRenderLatencyUs: " + std::to_string(totalGameToRenderLatencyUs);
            stats += "\nsimDeltaUs: " + std::to_string(simDeltaUs);
            stats += "\nrenderDeltaUs: " + std::to_string(renderDeltaUs);
            stats += "\npresentDeltaUs: " + std::to_string(presentDeltaUs);
            stats += "\ndriverDeltaUs: " + std::to_string(driverDeltaUs);
            stats += "\nosRenderQueueDeltaUs: " + std::to_string(osRenderQueueDeltaUs);
            stats += "\ngpuRenderDeltaUs: " + std::to_string(gpuRenderDeltaUs);
        }
        else {
            stats = "Latency Report Unavailable";
        }
    }

}

#if STREAMLINE_FEATURE_LATEWARP
void SLWrapper::SetLatewarpOptions(const sl::LatewarpOptions& options) {
    static bool toggle = options.latewarpActive;
    if (toggle != options.latewarpActive)
    {
        slLatewarpSetOptions(m_viewport, options);
        toggle = options.latewarpActive;
    }
}
#endif

bool SLWrapper::Get_Latewarp_SwapChainRecreation(bool& turn_on) const {
    turn_on = m_latewarp_shouldLoad;
    auto tmp = m_latewarp_triggerswapchainRecreation;
    return tmp;
}

void SLWrapper::SetReflexCameraData(sl::FrameToken &frameToken, const sl::ReflexCameraData& cameraData) {
    slReflexSetCameraData(m_viewport, frameToken, cameraData);
}