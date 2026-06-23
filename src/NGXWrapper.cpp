//----------------------------------------------------------------------------------
// File:        NGXWrapper.cpp
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
#include <donut/core/log.h>
#include <filesystem>
#include <algorithm>
#include <dxgi.h>
#include <dxgi1_5.h>
#include "NGXWrapper.h"
#include "RenderTargets.h"
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_latewarp.h>

#include "pclstats.h"
#include "nvapi.h"
#include <nvrhi/nvrhi.h>

#if DONUT_WITH_DX11
#include <d3d11.h>
#include <nvrhi/d3d11.h>
#endif
#if DONUT_WITH_DX12
#include <d3d12.h>
#include <nvrhi/d3d12.h>
#endif
#if DONUT_WITH_VULKAN
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>
#include <nvsdk_ngx_helpers_latewarp_vk.h>
#include <vulkan/vulkan.h>
#include <../src/vulkan/vulkan-backend.h>
#include <nvrhi/vulkan.h>
#endif

#include "DeviceManagerOverride/DeviceManagerOverride.h"
#ifndef _WIN32
#include <unistd.h>
#include <cstdio>
#include <climits>
#else
#define PATH_MAX MAX_PATH
#endif // _WIN32

PCLSTATS_DEFINE();

using namespace donut;
using namespace donut::math;
using namespace donut::engine;

NGXWrapper& NGXWrapper::Get()
{
    static NGXWrapper instance;
    instance.m_currentFrame = &instance.m_currentFrameToken;
    return instance;
}

bool NGXWrapper::Initialize_preDevice(nvrhi::GraphicsAPI api)
{
    m_api = api;
    NvAPI_Status status = NvAPI_Initialize();
    m_nvapi_loaded = (status == NVAPI_OK);

    PCLSTATS_INIT(0);
    m_pcl_available = true;

    return true;
}

bool NGXWrapper::Initialize_postDevice(donut::app::DeviceManager* deviceManager)
{
#if DONUT_WITH_DX12
        if (m_api == nvrhi::GraphicsAPI::D3D12)
        {
            if (m_nvapi_loaded)
            {
                // Reflex is available if this check is successful.
                NV_GET_SLEEP_STATUS_PARAMS_V1 sleepStatus = {};
                sleepStatus.version = NV_GET_SLEEP_STATUS_PARAMS_VER1;
                NvAPI_Status status = NvAPI_D3D_GetSleepStatus((IUnknown*)m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device), (NV_GET_SLEEP_STATUS_PARAMS*)&sleepStatus);
                m_reflex_available = (status == NVAPI_OK);
            }

            auto path = std::filesystem::temp_directory_path().wstring();
            NVSDK_NGX_D3D12_Init(APP_ID, path.c_str(), m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device));
            NVSDK_NGX_D3D12_AllocateParameters(&m_ngx_parameters);
            NVSDK_NGX_D3D12_GetCapabilityParameters(&m_ngx_parameters); 
        }
#endif
#if DONUT_WITH_DX11
        if (m_api == nvrhi::GraphicsAPI::D3D11)
        {
            if (m_nvapi_loaded)
            {
                // Reflex is available if this check is successful.
                NV_GET_SLEEP_STATUS_PARAMS_V1 sleepStatus = {};
                sleepStatus.version = NV_GET_SLEEP_STATUS_PARAMS_VER1;
                NvAPI_Status status = NvAPI_D3D_GetSleepStatus((IUnknown*)m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_Device), (NV_GET_SLEEP_STATUS_PARAMS*)&sleepStatus);
                m_reflex_available = (status == NVAPI_OK);
            }

            auto path = std::filesystem::temp_directory_path().wstring();
            NVSDK_NGX_D3D11_Init(APP_ID, path.c_str(), m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_Device));
            NVSDK_NGX_D3D11_AllocateParameters(&m_ngx_parameters);
            NVSDK_NGX_D3D11_GetCapabilityParameters(&m_ngx_parameters);
        }
#endif
#if DONUT_WITH_VULKAN
        if (m_api == nvrhi::GraphicsAPI::VULKAN)
        {
            auto path = std::filesystem::temp_directory_path().wstring();
            auto device = static_cast<DeviceManagerOverride_VK*>(deviceManager);
            NVSDK_NGX_VULKAN_Init(APP_ID, path.c_str(), device->GetInstance(), device->GetPhysicalDevice(), m_Device->getNativeObject(nvrhi::ObjectTypes::VK_Device));
            NVSDK_NGX_VULKAN_AllocateParameters(&m_ngx_parameters);
            NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_ngx_parameters);
        }
#endif

    return true;
}

void NGXWrapper::UpdateFeatureAvailable(donut::app::DeviceManager* deviceManager)
{
    auto checkFeature = [this, deviceManager](NVSDK_NGX_Feature feature, std::string feature_name) -> bool {
        NVSDK_NGX_FeatureDiscoveryInfo info;
        memset(&info, 0, sizeof(NVSDK_NGX_FeatureDiscoveryInfo));
        info.SDKVersion = NVSDK_NGX_Version_API;
        info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id;
        info.Identifier.v.ApplicationId = APP_ID;
        info.FeatureID = feature;
        const wchar_t* dataPath = L".";
        info.ApplicationDataPath = dataPath;
        NVSDK_NGX_FeatureRequirement featureRequirement;
        memset(&featureRequirement, 0, sizeof(NVSDK_NGX_FeatureRequirement));
#if DONUT_WITH_DX12
        if (m_api == nvrhi::GraphicsAPI::D3D12)
        {
            auto device = static_cast<DeviceManagerOverride_DX12*>(deviceManager);
            return (NVSDK_NGX_D3D12_GetFeatureRequirements(device->GetAdapter(), &info, &featureRequirement) == NVSDK_NGX_Result_Success) && (featureRequirement.FeatureSupported == 0);
        }
#endif
#if DONUT_WITH_DX11
        if (m_api == nvrhi::GraphicsAPI::D3D11)
        {
            auto device = static_cast<DeviceManagerOverride_DX11*>(deviceManager);
            return (NVSDK_NGX_D3D11_GetFeatureRequirements(device->GetAdapter(), &info, &featureRequirement) == NVSDK_NGX_Result_Success) && (featureRequirement.FeatureSupported == 0);
        }
#endif
#if DONUT_WITH_VULKAN
        if (m_api == nvrhi::GraphicsAPI::VULKAN)
        {
            auto device = static_cast<DeviceManagerOverride_VK*>(deviceManager);
            auto ngxResult = NVSDK_NGX_VULKAN_GetFeatureRequirements(device->GetInstance(), device->GetPhysicalDevice(), &info, &featureRequirement);

            if (ngxResult == NVSDK_NGX_Result_Success)
            {
                {
                    uint32_t count{};
                    NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(device->GetInstance(), device->GetPhysicalDevice(), &info, &count, nullptr);
                    std::vector<VkExtensionProperties> extensions(count);
                    auto dataAddr = extensions.data();
                    std::vector<std::string> extensionNames;
                    NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(device->GetInstance(), device->GetPhysicalDevice(), &info, &count, &dataAddr);
                    for (uint32_t i = 0; i < count; i++) { extensionNames.push_back(dataAddr[i].extensionName); }
                    auto& superset = device->GetDeviceParams().requiredVulkanDeviceExtensions;
                    // false if main.cpp requested VK extensions needs to be updated
                    assert(std::all_of(extensionNames.begin(), extensionNames.end(), [&superset](const std::string& str) { return std::find(superset.begin(), superset.end(), str) != superset.end(); }));
                }
                {
                    uint32_t count{};
                    NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, nullptr);
                    std::vector<VkExtensionProperties> extensions(count);
                    auto dataAddr = extensions.data();
                    std::vector<std::string> extensionNames;
                    NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, &dataAddr);
                    for (uint32_t i = 0; i < count; i++) { extensionNames.push_back(dataAddr[i].extensionName); }
                    auto& superset = device->GetDeviceParams().requiredVulkanInstanceExtensions;
                    // false if main.cpp requested VK extensions needs to be updated
                    assert(std::all_of(extensionNames.begin(), extensionNames.end(), [&superset](const std::string& str) { return std::find(superset.begin(), superset.end(), str) != superset.end(); }));
                }
            }
            return ngxResult == NVSDK_NGX_Result_Success && featureRequirement.FeatureSupported == 0;
        }
#endif
        return false;
    };

#ifdef STREAMLINE_FEATURE_LATEWARP
    m_latewarp_available = checkFeature(NVSDK_NGX_Feature_Reserved15, "Latewarp");
#endif

    return;
}

sl::FeatureRequirements NGXWrapper::GetFeatureRequirements(sl::Feature feature)
{
    sl::FeatureRequirements req {};
    // Queried in NGXWrapper::UpdateFeatureAvailable
    return req;
}

sl::FeatureVersion NGXWrapper::GetFeatureVersion(sl::Feature feature)
{
    sl::FeatureVersion ver {};
    // NGXWrapper::UpdateFeatureAvailable
    return ver;
}

void NGXWrapper::SetDevice_nvrhi(nvrhi::IDevice* device)
{
    m_Device = device;
}

void NGXWrapper::Shutdown()
{
    if (m_nvapi_loaded)
    {
        NvAPI_Unload();
        m_nvapi_loaded = false;
    }

    PCLSTATS_SHUTDOWN();
    m_pcl_available = false;

#if DONUT_WITH_DX12
        if (m_api == nvrhi::GraphicsAPI::D3D12)
        {
            if (m_ngx_initialised) {
                NVSDK_NGX_D3D12_DestroyParameters(m_ngx_parameters);
                NVSDK_NGX_D3D12_Shutdown1(m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device));
                m_ngx_initialised = false;
            }
        }
#endif
#if DONUT_WITH_DX11
        if (m_api == nvrhi::GraphicsAPI::D3D11)
        {
            if (m_ngx_initialised) {
                NVSDK_NGX_D3D11_DestroyParameters(m_ngx_parameters);
                NVSDK_NGX_D3D11_Shutdown1(m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_Device));
                m_ngx_initialised = false;
            }
        }
#endif
#if DONUT_WITH_VULKAN
        if (m_api == nvrhi::GraphicsAPI::VULKAN)
        {
            if (m_ngx_initialised) {
                NVSDK_NGX_VULKAN_DestroyParameters(m_ngx_parameters);
                NVSDK_NGX_VULKAN_Shutdown1(m_Device->getNativeObject(nvrhi::ObjectTypes::VK_Device));
                m_ngx_initialised = false;
            }
        }
#endif
}

void NGXWrapper::ProxyToNative(void* proxy, void** native)
{ 
    auto unknown = static_cast<IUnknown*>(proxy);

    ID3D12Device* d3d12Proxy{};
    if (SUCCEEDED(unknown->QueryInterface(&d3d12Proxy)))
    {
        d3d12Proxy->Release();
        *native = d3d12Proxy;
        d3d12Proxy->AddRef();
        return;
    }
    IDXGIFactory* factoryProxy{};
    if (SUCCEEDED(unknown->QueryInterface(&factoryProxy)))
    {
        factoryProxy->Release();
        *native = factoryProxy;
        factoryProxy->AddRef();
        return;
    }
    IDXGISwapChain* swapChainProxy{};
    if (SUCCEEDED(unknown->QueryInterface(&swapChainProxy)))
    {
        swapChainProxy->Release();
        *native = swapChainProxy;
        swapChainProxy->AddRef();
        return;
    }
    ID3D12CommandQueue* queue{};
    if (SUCCEEDED(unknown->QueryInterface(&queue)))
    {
        queue->Release();
        *native = queue;
        queue->AddRef();
        return;
    }
    ID3D12GraphicsCommandList* list{};
    if (SUCCEEDED(unknown->QueryInterface(&list)))
    {
        list->Release();
        *native = list;
        list->AddRef();
        return;
    }

    *native = proxy;
    unknown->AddRef();
    return;
};

void NGXWrapper::NativeToProxy(void* native, void** proxy)
{
    *proxy = native;
};

void NGXWrapper::FeatureLoad(sl::Feature feature, const bool turn_on) {
    if (turn_on)
    {
        switch (feature)
        {
        case sl::kFeatureLatewarp:
            m_latewarp_shouldLoad = true;
            break;
        }
    }
    else
    {
        switch (feature)
        {
        case sl::kFeatureLatewarp:
            if (m_latewarp_handle)
            {
#if DONUT_WITH_DX12
            if (m_api == nvrhi::GraphicsAPI::D3D12)
            {
                NVSDK_NGX_D3D12_ReleaseFeature(m_latewarp_handle);
            }
#endif
#if DONUT_WITH_DX11
            if (m_api == nvrhi::GraphicsAPI::D3D11)
            {
                NVSDK_NGX_D3D11_ReleaseFeature(m_latewarp_handle);
            }
#endif
#if DONUT_WITH_VULKAN
            if (m_api == nvrhi::GraphicsAPI::VULKAN)
            {
                NVSDK_NGX_VULKAN_ReleaseFeature(m_latewarp_handle);
            }
#endif
                m_latewarp_handle = nullptr;
            }
            m_latewarp_shouldLoad = false;
            break;
        }
    }
}

void NGXWrapper::TagResources_General(
    nvrhi::ICommandList* commandList,
    const donut::engine::IView* view,
    nvrhi::ITexture* motionVectorsTex,
    nvrhi::ITexture* depthTex,
    nvrhi::ITexture* finalColorHudlessTex)
{
    renderExtent = sl::Extent{ 0, 0, depthTex->getDesc().width, depthTex->getDesc().height };
    fullExtent = sl::Extent{ 0, 0, finalColorHudlessTex->getDesc().width, finalColorHudlessTex->getDesc().height };
}

void NGXWrapper::SetReflexConsts(const sl::ReflexOptions options)
{
    if (GetReflexAvailable())
    {
        NV_SET_SLEEP_MODE_PARAMS_V1 sleepMode = {};
        sleepMode.version = NV_SET_SLEEP_MODE_PARAMS_VER1;
        sleepMode.bLowLatencyMode = (options.mode != sl::ReflexMode::eOff);
        sleepMode.bLowLatencyBoost = (options.mode == sl::ReflexMode::eLowLatencyWithBoost);
        sleepMode.bUseMarkersToOptimize = false; // Not supported on single stage engine.
        sleepMode.minimumIntervalUs = options.frameLimitUs;

        NvAPI_D3D_SetSleepMode((IUnknown*)m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device), (NV_SET_SLEEP_MODE_PARAMS*)&sleepMode);
    }

    PCLSTATS_SET_VIRTUAL_KEY(options.virtualKey);
    PCLSTATS_SET_ID_THREAD(options.idThread);
}

void NGXWrapper::ReflexSetMarker(donut::app::DeviceManager& manager, NV_LATENCY_MARKER_TYPE markerType, uint64_t frameNumber)
{
    if (!NGXWrapper::Get().GetReflexAvailable())
    {
        return;
    }

    NV_LATENCY_MARKER_PARAMS_V1 marker = {};
    marker.version = NV_LATENCY_MARKER_PARAMS_VER1;
    marker.markerType = markerType;
    marker.frameID = frameNumber;
#if DONUT_WITH_DX12
    if (m_api == nvrhi::GraphicsAPI::D3D12)
    {
        NvAPI_D3D_SetLatencyMarker((IUnknown*)manager.GetDevice()->getNativeObject(nvrhi::ObjectTypes::D3D12_Device), (NV_LATENCY_MARKER_PARAMS*)&marker);
    }
#endif
#if DONUT_WITH_DX11
    if (m_api == nvrhi::GraphicsAPI::D3D11)
    {
        NvAPI_D3D_SetLatencyMarker((IUnknown*)manager.GetDevice()->getNativeObject(nvrhi::ObjectTypes::D3D11_Device), (NV_LATENCY_MARKER_PARAMS*)&marker);
    }
#endif
#if DONUT_WITH_VULKAN
    // todo
#endif
}

void NGXWrapper::ReflexCallback_Sleep(donut::app::DeviceManager& manager, uint32_t frameID)
{
    m_currentFrame->m_frameID = frameID;
    if (NGXWrapper::Get().GetReflexAvailable())
    {
#if DONUT_WITH_DX12
    if (m_api == nvrhi::GraphicsAPI::D3D12)
    {
        NvAPI_D3D_Sleep((IUnknown*)manager.GetDevice()->getNativeObject(nvrhi::ObjectTypes::D3D12_Device));
    }
#endif
#if DONUT_WITH_DX11
    if (m_api == nvrhi::GraphicsAPI::D3D11)
    {
        NvAPI_D3D_Sleep((IUnknown*)manager.GetDevice()->getNativeObject(nvrhi::ObjectTypes::D3D11_Device));
    }
#endif
#if DONUT_WITH_VULKAN
    // todo
#endif
    }
}

void NGXWrapper::ReflexCallback_SimStart(donut::app::DeviceManager& manager, uint32_t frameID)
{
    ReflexSetMarker(manager, NV_LATENCY_MARKER_TYPE::SIMULATION_START, frameID);
    PCLSTATS_MARKER(PCLSTATS_SIMULATION_START, frameID);
}

void NGXWrapper::ReflexCallback_SimEnd(donut::app::DeviceManager& manager, uint32_t frameID)
{
    ReflexSetMarker(manager, NV_LATENCY_MARKER_TYPE::SIMULATION_END, frameID);
    PCLSTATS_MARKER(PCLSTATS_SIMULATION_END, frameID);
}

void NGXWrapper::ReflexCallback_RenderStart(donut::app::DeviceManager& manager, uint32_t frameID)
{
    ReflexSetMarker(manager, NV_LATENCY_MARKER_TYPE::RENDERSUBMIT_START, frameID);
    PCLSTATS_MARKER(PCLSTATS_RENDERSUBMIT_START, frameID);
}

void NGXWrapper::ReflexCallback_RenderEnd(donut::app::DeviceManager& manager, uint32_t frameID)
{
    ReflexSetMarker(manager, NV_LATENCY_MARKER_TYPE::RENDERSUBMIT_END, frameID);
    PCLSTATS_MARKER(PCLSTATS_RENDERSUBMIT_END, frameID);
}

void NGXWrapper::ReflexCallback_PresentStart(donut::app::DeviceManager& manager, uint32_t frameID)
{
    if (frameID != 0 && m_latewarp_shouldLoad)
    {
        // latency marker type will be added in future versions of NVAPI
        ReflexSetMarker(manager, static_cast<NV_LATENCY_MARKER_TYPE>(PCLSTATS_LATE_WARP_PRESENT_START), frameID + 1);
        PCLSTATS_MARKER(PCLSTATS_LATE_WARP_PRESENT_START, frameID + 1);
    }

    ReflexSetMarker(manager, NV_LATENCY_MARKER_TYPE::PRESENT_START, frameID);
    PCLSTATS_MARKER(PCLSTATS_PRESENT_START, frameID);
}

void NGXWrapper::ReflexCallback_PresentEnd(donut::app::DeviceManager& manager, uint32_t frameID)
{
    ReflexSetMarker(manager, NV_LATENCY_MARKER_TYPE::PRESENT_END, frameID);
    PCLSTATS_MARKER(PCLSTATS_PRESENT_END, frameID);

    if (frameID != 0 && m_latewarp_shouldLoad)
    {
        // latency marker type will be added in future versions of NVAPI
        ReflexSetMarker(manager, static_cast<NV_LATENCY_MARKER_TYPE>(PCLSTATS_LATE_WARP_PRESENT_START), frameID + 1);
        PCLSTATS_MARKER(PCLSTATS_LATE_WARP_PRESENT_END, frameID + 1);
    }
}

void NGXWrapper::ReflexTriggerFlash()
{
    if (GetReflexAvailable())
    {
        NV_LATENCY_MARKER_PARAMS_V1 marker = {};
        marker.version = NV_LATENCY_MARKER_PARAMS_VER1;
        marker.markerType = NV_LATENCY_MARKER_TYPE::TRIGGER_FLASH;
        marker.frameID = *m_currentFrame;
#if DONUT_WITH_DX12
        if (m_api == nvrhi::GraphicsAPI::D3D12)
        {
            NvAPI_D3D_SetLatencyMarker((IUnknown*)m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device), (NV_LATENCY_MARKER_PARAMS*)&marker);
        }
#endif
#if DONUT_WITH_DX11
        if (m_api == nvrhi::GraphicsAPI::D3D11)
        {
            NvAPI_D3D_SetLatencyMarker((IUnknown*)m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_Device), (NV_LATENCY_MARKER_PARAMS*)&marker);
        }
#endif
#if DONUT_WITH_VULKAN
#endif
    }

    PCLSTATS_MARKER(PCLSTATS_TRIGGER_FLASH, *m_currentFrame);
}

void NGXWrapper::ReflexTriggerPcPing()
{
    PCLSTATS_MARKER(PCLSTATS_PC_LATENCY_PING, *m_currentFrame);
}

void NGXWrapper::QueryReflexStats(bool& reflex_lowLatencyAvailable, bool& reflex_flashAvailable, std::string& stats) { }

void NGXWrapper::SetReflexCameraData(sl::FrameToken& frameToken, const sl::ReflexCameraData& cameraData) {
    sl::ReflexCameraData frameCopy;
    memcpy(&frameCopy, &cameraData, sizeof(sl::ReflexCameraData));
    m_latewarp_frames[frameToken % MAX_FRAMES_IN_FLIGHT] = frameCopy;

    if (GetReflexAvailable())
    {
        NV_LATENCY_MARKER_PARAMS_V1 marker = {};
        marker.version = NV_LATENCY_MARKER_PARAMS_VER1;
        // will be added in future versions of NVAPI
        marker.markerType = static_cast<NV_LATENCY_MARKER_TYPE>(PCLSTATS_CAMERA_CONSTRUCTED);
        marker.frameID = frameToken;
#if DONUT_WITH_DX12
        if (m_api == nvrhi::GraphicsAPI::D3D12)
        {
            NvAPI_D3D_SetLatencyMarker((IUnknown*)m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device), (NV_LATENCY_MARKER_PARAMS*)&marker);
        }
#endif
#if DONUT_WITH_DX11
        if (m_api == nvrhi::GraphicsAPI::D3D11)
        {
            NvAPI_D3D_SetLatencyMarker((IUnknown*)m_Device->getNativeObject(nvrhi::ObjectTypes::D3D11_Device), (NV_LATENCY_MARKER_PARAMS*)&marker);
        }
#endif
#if DONUT_WITH_VULKAN
        // todo
#endif
    }

    PCLSTATS_MARKER(PCLSTATS_CAMERA_CONSTRUCTED, frameToken);
}

NVSDK_NGX_Resource_VK TextureToResourceVK(nvrhi::ITexture * tex, nvrhi::TextureSubresourceSet subresources)
{
    nvrhi::TextureDesc desc = tex->getDesc();
    NVSDK_NGX_Resource_VK resourceVK = {};
    VkImageView imageView = tex->getNativeView(nvrhi::ObjectTypes::VK_ImageView, nvrhi::Format::UNKNOWN, subresources);
    VkFormat format = (VkFormat)nvrhi::vulkan::convertFormat(desc.format);
    VkImage image = tex->getNativeObject(nvrhi::ObjectTypes::VK_Image);
    VkImageSubresourceRange subresourceRange = { 1, subresources.baseMipLevel, subresources.numMipLevels, subresources.baseArraySlice, subresources.numArraySlices };

    return NVSDK_NGX_Create_ImageView_Resource_VK(imageView, image, subresourceRange, format, desc.width, desc.height, desc.isUAV);
}

#if STREAMLINE_FEATURE_LATEWARP
void NGXWrapper::SetLatewarpOptions(const sl::LatewarpOptions& options) { (void)options; }

void NGXWrapper::CleanupLatewarp(bool wfi)
{
    // non swap-chain hooking has no extra resources
    (void)wfi;
    FeatureLoad(sl::kFeatureLatewarp, false);
}

void NGXWrapper::EvaluateLatewarp(donut::app::DeviceManager& manager, nvrhi::ICommandList* commandList, RenderTargets* renderTargets, nvrhi::ITexture* inputColor, nvrhi::ITexture* outputColor, const donut::engine::IView* view) {
    if (!m_latewarp_shouldLoad) return;
    // latency marker type will be added in future versions of NVAPI
    ReflexSetMarker(manager, static_cast<NV_LATENCY_MARKER_TYPE>(PCLSTATS_LATE_WARP_SUBMIT_START), m_currentFrameToken);
    PCLSTATS_MARKER(PCLSTATS_LATE_WARP_SUBMIT_START, m_currentFrameToken);
#if DONUT_WITH_DX12
    if (m_api == nvrhi::GraphicsAPI::D3D12)
    {
        auto cmdList = commandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList);
        if (m_latewarp_handle == nullptr)
        {
            NVSDK_NGX_Latewarp_Create_Params params{};
            params.InOutputWidth = fullExtent.width;
            params.InOutputHeight = fullExtent.height;
            NGX_D3D12_CREATE_LATEWARP_EXT(cmdList, &m_latewarp_handle, m_ngx_parameters, &params);
            commandList->clearState();
        }
        NVSDK_NGX_D3D12_Latewarp_Eval_Params params{};
        params.InBackbuffer = (ID3D12Resource*)inputColor->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
        params.InHudlessColor = (ID3D12Resource*)inputColor->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
        params.InUIColorAlpha = (ID3D12Resource*)inputColor->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
        params.InDepth = (ID3D12Resource*)renderTargets->Depth->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
        params.InMotionVectors = (ID3D12Resource*)renderTargets->MotionVectors->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
        params.OutColor = (ID3D12Resource*)outputColor->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
        params.InNoWarpMask = nullptr;

        params.FrameID = m_currentFrameToken;
        params.WorldToViewMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].worldToViewMatrix[0].x;
        params.ViewToClipMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].viewToClipMatrix[0].x;
        params.PrevRenderedWorldToViewMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].prevRenderedWorldToViewMatrix[0].x;
        params.PrevRenderedViewToClipMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].prevRenderedViewToClipMatrix[0].x;

        NGX_D3D12_EVALUATE_LATEWARP_EXT(cmdList, m_latewarp_handle, m_ngx_parameters, &params);
        commandList->clearState();
    }
#endif
#if DONUT_WITH_DX11
    if (m_api == nvrhi::GraphicsAPI::D3D11)
    {
        auto cmdList = commandList->getNativeObject(nvrhi::ObjectTypes::D3D11_DeviceContext);
        if (m_latewarp_handle == nullptr)
        {
            NVSDK_NGX_Latewarp_Create_Params params{};
            params.InOutputWidth = fullExtent.width;
            params.InOutputHeight = fullExtent.height;
            NGX_D3D11_CREATE_LATEWARP_EXT(cmdList, &m_latewarp_handle, m_ngx_parameters, &params);
            commandList->clearState();
        }
        NVSDK_NGX_D3D11_Latewarp_Eval_Params params{};
        params.InBackbuffer = (ID3D11Resource*)inputColor->getNativeObject(nvrhi::ObjectTypes::D3D11_Resource);
        params.InHudlessColor = (ID3D11Resource*)inputColor->getNativeObject(nvrhi::ObjectTypes::D3D11_Resource);
        params.InUIColorAlpha = (ID3D11Resource*)inputColor->getNativeObject(nvrhi::ObjectTypes::D3D11_Resource);
        params.InMotionVectors = (ID3D11Resource*)renderTargets->MotionVectors->getNativeObject(nvrhi::ObjectTypes::D3D11_Resource);
        params.InDepth = (ID3D11Resource*)renderTargets->Depth->getNativeObject(nvrhi::ObjectTypes::D3D11_Resource);
        params.OutColor = (ID3D11Resource*)outputColor->getNativeObject(nvrhi::ObjectTypes::D3D11_Resource);
        params.InNoWarpMask = nullptr;

        params.FrameID = m_currentFrameToken;
        params.WorldToViewMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].worldToViewMatrix[0].x;
        params.ViewToClipMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].viewToClipMatrix[0].x;
        params.PrevRenderedWorldToViewMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].prevRenderedWorldToViewMatrix[0].x;
        params.PrevRenderedViewToClipMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].prevRenderedViewToClipMatrix[0].x;

        NGX_D3D11_EVALUATE_LATEWARP_EXT(cmdList, m_latewarp_handle, m_ngx_parameters, &params);
        commandList->clearState();
    }
#endif
#if DONUT_WITH_VULKAN
    if (m_api == nvrhi::GraphicsAPI::VULKAN)
    {
        nvrhi::TextureSubresourceSet subresources = view->GetSubresources();
        auto cmdList = commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);
        if (m_latewarp_handle == nullptr)
        {
            NVSDK_NGX_Latewarp_Create_Params params{};
            params.InOutputWidth = fullExtent.width;
            params.InOutputHeight = fullExtent.height;
            NGX_VULKAN_CREATE_LATEWARP_EXT(cmdList, &m_latewarp_handle, m_ngx_parameters, &params);
            commandList->clearState();
        }
        auto InBackbuffer = TextureToResourceVK(inputColor, subresources);
        auto InHudlessColor = TextureToResourceVK(inputColor, subresources);
        auto InUIColorAlpha = TextureToResourceVK(inputColor, subresources);
        auto InMotionVectors = TextureToResourceVK(renderTargets->MotionVectors, subresources);
        auto InDepth = TextureToResourceVK(renderTargets->Depth, subresources);
        auto OutColor = TextureToResourceVK(outputColor, subresources);
        NVSDK_NGX_VULKAN_Latewarp_Eval_Params params{};
        params.InBackbuffer =    &InBackbuffer;
        params.InHudlessColor =  &InHudlessColor;
        params.InMotionVectors = &InMotionVectors;
        params.InUIColorAlpha =  &InUIColorAlpha;
        params.InDepth =         &InDepth;
        params.OutColor =        &OutColor;
        params.InNoWarpMask = nullptr;

        params.FrameID = m_currentFrameToken;
        params.WorldToViewMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].worldToViewMatrix[0].x;
        params.ViewToClipMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].viewToClipMatrix[0].x;
        params.PrevRenderedWorldToViewMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].prevRenderedWorldToViewMatrix[0].x;
        params.PrevRenderedViewToClipMatrix = &m_latewarp_frames[m_currentFrameToken % MAX_FRAMES_IN_FLIGHT].prevRenderedViewToClipMatrix[0].x;

        NGX_VULKAN_EVALUATE_LATEWARP_EXT(cmdList, m_latewarp_handle, m_ngx_parameters, &params);
        commandList->clearState();
    }
#endif
    // latency marker type will be added in future versions of NVAPI
    ReflexSetMarker(manager, static_cast<NV_LATENCY_MARKER_TYPE>(PCLSTATS_LATE_WARP_SUBMIT_END), m_currentFrameToken);
    PCLSTATS_MARKER(PCLSTATS_LATE_WARP_SUBMIT_END, m_currentFrameToken);
}
#endif
