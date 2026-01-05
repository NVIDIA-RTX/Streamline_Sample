//----------------------------------------------------------------------------------
// File:        NVWrapper.h
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

#include <donut/engine/View.h>
#include <donut/app/DeviceManager.h>
#include <donut/core/math/basics.h>
#include <nvrhi/nvrhi.h>
#include <donut/core/log.h>

#include "RenderTargets.h"

#if USE_SL
#include <sl.h>
#include <sl_consts.h>
#include <sl_hooks.h>
#include <sl_version.h>

// Streamline Features
#include <sl_dlss.h>
#include <sl_reflex.h>
#include <sl_nis.h>
#include <sl_dlss_g.h>
#include <sl_deepdvc.h>
#ifdef STREAMLINE_FEATURE_DLSS_RR
#include <sl_dlss_d.h>
#endif
#if STREAMLINE_FEATURE_LATEWARP
#include <sl_latewarp.h>
#endif
#else

//**********************
// The following code block up to class NVWrapper are condensed redefinitions of Streamline header files
// The USE_SL=0 path is only intended to show a NGX only Reflex Frame Warp (Latewarp) integration
// The types defined here should not be used in production nor used as proper sample code
// it is only done to simplify StreamlineSample.cpp's implementation to not include egregious SL vs NGX only branching
//**********************
namespace sl
{
struct Extent
{
    uint32_t top{};
    uint32_t left{};
    uint32_t width{};
    uint32_t height{};

    inline operator bool() const { return width != 0 && height != 0; }
    inline bool operator==(const Extent &rhs) const
    {
        return top == rhs.top && left == rhs.left &&
                width == rhs.width && height == rhs.height;
    }
    inline bool operator!=(const Extent &rhs) const
    {
        return !operator==(rhs);
    }
};
constexpr float INVALID_FLOAT = 3.40282346638528859811704183484516925440e38f;
struct float2
{
    float2() : x(INVALID_FLOAT), y(INVALID_FLOAT) {}
    float2(float _x, float _y) : x(_x), y(_y) {}
    float x, y;
};

struct float3
{
    float3() : x(INVALID_FLOAT), y(INVALID_FLOAT), z(INVALID_FLOAT) {}
    float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    float x, y, z;
};
struct float4
{
    float4() : x(INVALID_FLOAT), y(INVALID_FLOAT), z(INVALID_FLOAT), w(INVALID_FLOAT) {}
    float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    float x, y, z, w;
};
struct float4x4
{
    //! All access points take row index as a parameter
    inline float4 &operator[](uint32_t i) { return row[i]; }
    inline const float4 &operator[](uint32_t i) const { return row[i]; }
    inline void setRow(uint32_t i, const float4 &v) { row[i] = v; }
    inline const float4 &getRow(uint32_t i) { return row[i]; }

    //! Row major matrix
    float4 row[4];
};
enum class ResourceType : char
{
    eTex2d,
    eBuffer,
    eCommandQueue,
    eCommandBuffer,
    eCommandPool,
    eFence,
    eSwapchain,
    eCount
};
typedef struct ID3D11Resource ID3D11Resource;
typedef struct ID3D11Buffer ID3D11Buffer;
typedef struct ID3D11Texture2D ID3D11Texture2D;
typedef struct ID3D12Resource ID3D12Resource;
struct Resource
{
    Resource(ResourceType _type, void *_native, void *_mem, void *_view, uint32_t _state = UINT_MAX) : type(_type), native(_native), memory(_mem), view(_view), state(_state) {};
    Resource(ResourceType _type, void *_native, uint32_t _state = UINT_MAX) : type(_type), native(_native), state(_state) {};
    Resource() {};
    inline operator ID3D12Resource *() { return reinterpret_cast<ID3D12Resource *>(native); }
    inline operator ID3D11Resource *() { return reinterpret_cast<ID3D11Resource *>(native); }
    inline operator ID3D11Buffer *() { return reinterpret_cast<ID3D11Buffer *>(native); }
    inline operator ID3D11Texture2D *() { return reinterpret_cast<ID3D11Texture2D *>(native); }
    ResourceType type = ResourceType::eTex2d;
    void *native{};
    void *memory{};
    void *view{};
    uint32_t state = UINT_MAX;
    uint32_t width{};
    uint32_t height{};
    uint32_t nativeFormat{};
    uint32_t mipLevels{};
    uint32_t arrayLayers{};
    uint64_t gpuVirtualAddress{};
    uint32_t flags;
    uint32_t usage{};
    uint32_t reserved{};
};
enum Boolean : char
{
    eFalse,
    eTrue,
    eInvalid
};
enum class DLSSPreset
{
    eDefault,
    ePresetA,
    ePresetB,
    ePresetC,
    ePresetD,
    ePresetE,
    ePresetF,
    ePresetG,   // reverts to default, not recommended to use
    ePresetH,   // reverts to default, not recommended to use
    ePresetI,   // reverts to default, not recommended to use
    ePresetJ,
    ePresetK,
    ePresetL,
    ePresetM,
    ePresetN,   // reverts to default, not recommended to use
    ePresetO,   // reverts to default, not recommended to use
};
enum class NISMode
{
    eOff,
    eScaler
};
enum class DeepDVCMode
{
    eOff,
    eOn
};
enum class ReflexMode
{
    eOff,
    eLowLatency,
    eLowLatencyWithBoost
};

enum class DLSSMode
{
    eOff,
    eMaxQuality,
    eUltraQuality,
    eBalanced,
    eMaxPerformance,
    eUltraPerformance,
    eDLAA,
    eCount
};
struct DLSSOptions
{
    DLSSMode mode = DLSSMode::eOff;
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
    float sharpness = 0.0f;
    float preExposure = 1.0f;
    float exposureScale = 1.0f;
    Boolean colorBuffersHDR = Boolean::eTrue;
    Boolean indicatorInvertAxisX = Boolean::eFalse;
    Boolean indicatorInvertAxisY = Boolean::eFalse;
    DLSSPreset dlaaPreset = DLSSPreset::eDefault;
    DLSSPreset qualityPreset = DLSSPreset::eDefault;
    DLSSPreset balancedPreset = DLSSPreset::eDefault;
    DLSSPreset performancePreset = DLSSPreset::eDefault;
    DLSSPreset ultraPerformancePreset = DLSSPreset::eDefault;
    DLSSPreset ultraQualityPreset = DLSSPreset::eDefault;
    Boolean useAutoExposure = Boolean::eFalse;
    Boolean alphaUpscalingEnabled = Boolean::eFalse;
};

enum class NISHDR : uint32_t
{
    eNone,
    eLinear,
    ePQ,
    eCount
};
struct NISOptions
{
    NISMode mode = NISMode::eScaler;
    NISHDR hdrMode = NISHDR::eNone;
    float sharpness = 0.0f;
};
struct DeepDVCOptions
{
    DeepDVCMode mode = DeepDVCMode::eOff;
    float intensity = 0.5f;
    float saturationBoost = 0.25f;
};
enum DLSSGFlags : uint32_t
{
    eShowOnlyInterpolatedFrame = 1 << 0,
    eDynamicResolutionEnabled = 1 << 1,
    eRequestVRAMEstimate = 1 << 2,
    eRetainResourcesWhenOff = 1 << 3,
    eEnableFullscreenMenuDetection = 1 << 4,
};
enum class DLSSGMode
{
    eOff,
    eOn
};
struct DLSSGOptions
{
    DLSSGMode mode = DLSSGMode::eOff;
    uint32_t numFramesToGenerate = 1;
    uint32_t flags{};
    uint32_t dynamicResWidth{};
    uint32_t dynamicResHeight{};
    uint32_t numBackBuffers{};
    uint32_t mvecDepthWidth{};
    uint32_t mvecDepthHeight{};
    uint32_t colorWidth{};
    uint32_t colorHeight{};
    uint32_t colorBufferFormat{};
    uint32_t mvecBufferFormat{};
    uint32_t depthBufferFormat{};
    uint32_t hudLessBufferFormat{};
    uint32_t uiBufferFormat{};
};

enum DLSSGStatus
{
    eOk = 0,
    eFailResolutionTooLow = 1 << 0,
    eFailReflexNotDetectedAtRuntime = 1 << 1,
    eFailHDRFormatNotSupported = 1 << 2,
    eFailCommonConstantsInvalid = 1 << 3,
    eFailGetCurrentBackBufferIndexNotCalled = 1 << 4,
    eFailUnableToGetReflexMatrices = 1 << 5
};
enum DLSSGState
{
};
struct ReflexOptions
{
    ReflexMode mode = ReflexMode::eOff;
    uint32_t frameLimitUs = 0;
    bool useMarkersToOptimize = false;
    uint16_t virtualKey = 0;
    uint32_t idThread = 0;
};
struct FrameToken
{
    FrameToken(uint32_t init) : m_frameID(init) {};
    virtual operator uint32_t() const { return m_frameID; }
    uint32_t m_frameID = 0;
};
struct ViewportHandle
{
    operator uint32_t() const { return value; }
    uint32_t value = UINT_MAX;
    ViewportHandle() : value(UINT_MAX) {}
    ViewportHandle(uint32_t v) : value(v) {}
    ViewportHandle(int32_t v) : value(v) {}
};
struct FeatureRequirements
{
};
struct FeatureVersion
{
};
typedef uint32_t Feature;
struct Constants
{
    float4x4 cameraViewToClip;
    float4x4 clipToCameraView;
    float4x4 clipToLensClip;
    float4x4 clipToPrevClip;
    float4x4 prevClipToClip;
    float2 jitterOffset;
    float2 mvecScale;
    float2 cameraPinholeOffset;
    float3 cameraPos;
    float3 cameraUp;
    float3 cameraRight;
    float3 cameraFwd;
    float cameraNear = INVALID_FLOAT;
    float cameraFar = INVALID_FLOAT;
    float cameraFOV = INVALID_FLOAT;
    float cameraAspectRatio = INVALID_FLOAT;
    float motionVectorsInvalidValue = INVALID_FLOAT;
    Boolean depthInverted = Boolean::eInvalid;
    Boolean cameraMotionIncluded = Boolean::eInvalid;
    Boolean motionVectors3D = Boolean::eInvalid;
    Boolean reset = Boolean::eInvalid;
    Boolean orthographicProjection = Boolean::eFalse;
    Boolean motionVectorsDilated = Boolean::eFalse;
    Boolean motionVectorsJittered = Boolean::eFalse;
    float minRelativeLinearDepthObjectSeparation = 40.0f;
};
struct LatewarpOptions
{
    bool latewarpActive = false;
    bool predictiveRenderActive = false;
};

struct ReflexCameraData
{
    float4x4 worldToViewMatrix;
    float4x4 viewToClipMatrix;
    float4x4 prevRenderedWorldToViewMatrix;
    float4x4 prevRenderedViewToClipMatrix;
};

constexpr Feature kFeatureDLSS = 0;
constexpr Feature kFeatureNRD = 1;
constexpr Feature kFeatureNIS = 2;
constexpr Feature kFeatureReflex = 3;
constexpr Feature kFeaturePCL = 4;
constexpr Feature kFeatureDeepDVC = 5;
constexpr Feature kFeatureLatewarp = 6;
constexpr Feature kFeatureDLSS_G = 1000;
constexpr Feature kFeatureDLSS_RR = 1001;
constexpr Feature kFeatureNvPerf = 1002;
constexpr Feature kFeatureImGUI = 9999;
constexpr Feature kFeatureCommon = UINT_MAX;

inline float2 make_sl_float2(donut::math::float2 donutF) { return float2{donutF.x, donutF.y}; }
inline float3 make_sl_float3(donut::math::float3 donutF) { return float3{donutF.x, donutF.y, donutF.z}; }
inline float4 make_sl_float4(donut::math::float4 donutF) { return float4{donutF.x, donutF.y, donutF.z, donutF.w}; }
inline float4x4 make_sl_float4x4(donut::math::float4x4 donutF4x4)
{
    float4x4 outF4x4;
    outF4x4.setRow(0, make_sl_float4(donutF4x4.row0));
    outF4x4.setRow(1, make_sl_float4(donutF4x4.row1));
    outF4x4.setRow(2, make_sl_float4(donutF4x4.row2));
    outF4x4.setRow(3, make_sl_float4(donutF4x4.row3));
    return outF4x4;
}
} // namespace SL
#endif

static constexpr int APP_ID = 231313132;

// We define a few functions to help with format conversion
inline sl::float2 make_sl_float2(donut::math::float2 donutF) { return sl::float2{donutF.x, donutF.y}; }
inline sl::float3 make_sl_float3(donut::math::float3 donutF) { return sl::float3{donutF.x, donutF.y, donutF.z}; }
inline sl::float4 make_sl_float4(donut::math::float4 donutF) { return sl::float4{donutF.x, donutF.y, donutF.z, donutF.w}; }
inline sl::float4x4 make_sl_float4x4(donut::math::float4x4 donutF4x4)
{
sl::float4x4 outF4x4;
outF4x4.setRow(0, make_sl_float4(donutF4x4.row0));
outF4x4.setRow(1, make_sl_float4(donutF4x4.row1));
outF4x4.setRow(2, make_sl_float4(donutF4x4.row2));
outF4x4.setRow(3, make_sl_float4(donutF4x4.row3));
return outF4x4;
}

class NVWrapper
{
protected:
    NVWrapper() = default;

    bool m_sl_initialised = false;
    nvrhi::GraphicsAPI m_api = nvrhi::GraphicsAPI::D3D12;
    nvrhi::IDevice *m_Device = nullptr;

    bool m_dlss_available = false;
    sl::DLSSOptions m_dlss_consts{};

#ifdef STREAMLINE_FEATURE_DLSS_RR
    bool m_dlssrr_available = false;
    sl::DLSSDOptions m_dlssrr_consts{};
#endif //STREAMLINE_FEATURE_DLSS_RR

    bool m_nis_available = false;
    sl::NISOptions m_nis_consts{};

    bool m_deepdvc_available = false;
    sl::DeepDVCOptions m_deepdvc_consts{};

    bool m_dlssg_available = false;
    bool m_dlssg_triggerswapchainRecreation = false;
    bool m_dlssg_shoudLoad = false;
    sl::DLSSGOptions m_dlssg_consts{};
    sl::DLSSGState m_dlssg_settings{};

    bool m_latewarp_available = false;
    bool m_latewarp_triggerswapchainRecreation = false;
    bool m_latewarp_shouldLoad = false;

    bool m_reflex_available = false;
    sl::ReflexOptions m_reflex_consts{};
    bool m_reflex_driverFlashIndicatorEnable = false;
    bool m_pcl_available = false;

    sl::FrameToken *m_currentFrame;
    sl::ViewportHandle m_viewport = {0};

public:
    static NVWrapper& Get();
    NVWrapper(const NVWrapper &) = delete;
    NVWrapper(NVWrapper &&) = delete;
    NVWrapper &operator=(const NVWrapper &) = delete;
    NVWrapper &operator=(NVWrapper &&) = delete;

    virtual void SetSLOptions(const bool checkSig, const bool enableLog, const bool useNewSetTagAPI, const bool allowSMSCG) = 0;

    virtual bool Initialize_preDevice(nvrhi::GraphicsAPI api) = 0;
    virtual bool Initialize_postDevice(donut::app::DeviceManager* deviceManager) = 0;

    bool GetSLInitialized() { return m_sl_initialised; }
    virtual void SetDevice_raw(void *device_ptr) = 0;
    virtual void SetDevice_nvrhi(nvrhi::IDevice *device) = 0;
    virtual void UpdateFeatureAvailable(donut::app::DeviceManager *adapter) = 0;
    virtual void Shutdown() = 0;
    nvrhi::GraphicsAPI getAPI() { return m_api; }
    virtual void ProxyToNative(void *proxy, void **native) = 0;
    virtual void NativeToProxy(void *proxy, void **native) = 0;
    virtual void QueueGPUWaitOnSyncObjectSet(nvrhi::IDevice *pDevice, nvrhi::CommandQueue cmdQType, void *syncObj, uint64_t syncObjVal) = 0;
    virtual uint64_t GetDLSSGLastFenceValue() { return 0; };
    virtual void QueryDLSSGState(uint64_t &estimatedVRamUsage, int &fps_multiplier, sl::DLSSGStatus &status, int &minSize, int &framesMax, void *&pFence, uint64_t &fenceValue) = 0;

    virtual sl::FeatureRequirements GetFeatureRequirements(sl::Feature feature) = 0;
    virtual sl::FeatureVersion GetFeatureVersion(sl::Feature feature) = 0;

    void SetViewportHandle(sl::ViewportHandle vpHandle)
    {
        m_viewport = vpHandle;
    }

    virtual void SetSLConsts(const sl::Constants &consts) = 0;
    virtual void FeatureLoad(sl::Feature feature, const bool turn_on) = 0;
    virtual void TagResources_General(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *motionVectors,
        nvrhi::ITexture *depth,
        nvrhi::ITexture *finalColorHudless) = 0;

#ifdef STREAMLINE_FEATURE_DLSS_RR
    virtual void TagResources_DLSS_RR(
            nvrhi::ICommandList* commandList,
            const donut::engine::IView *view,
            nvrhi::ITexture* inputColor,
            nvrhi::ITexture* diffuseAlbedo,
            nvrhi::ITexture* specAlbedo,
            nvrhi::ITexture* normalRoughness,
            nvrhi::ITexture* specHitDistance,
            nvrhi::ITexture* outputColor) = 0;
#endif //STREAMLINE_FEATURE_DLSS_RR

    virtual void TagResources_DLSS_NIS(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *output,
        nvrhi::ITexture *input) = 0;

    virtual void TagResources_DLSS_FG(
        nvrhi::ICommandList *commandList,
        bool validViewportExtent = false,
        sl::Extent backBufferExtent = {}) = 0;

    virtual void TagResources_DeepDVC(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *output) = 0;


    virtual void TagResources_Latewarp(
        nvrhi::ICommandList *commandList,
        const donut::engine::IView *view,
        nvrhi::ITexture *backbuffer,
        nvrhi::ITexture *uiColorAlpha,
        nvrhi::ITexture *noWarpMask,
        sl::Extent backBufferExtent) = 0;

    virtual void UnTagResources_DeepDVC() = 0;
    struct DLSSSettings
    {
        donut::math::int2 optimalRenderSize;
        donut::math::int2 minRenderSize;
        donut::math::int2 maxRenderSize;
        float sharpness;
    };

    virtual void SetDLSSOptions(const sl::DLSSOptions consts) = 0;
    bool GetDLSSAvailable() { return m_dlss_available; }
    bool GetDLSSLastEnable() { return m_dlss_consts.mode != sl::DLSSMode::eOff; }
    virtual void QueryDLSSOptimalSettings(DLSSSettings &settings) = 0;
    virtual void EvaluateDLSS(nvrhi::ICommandList *commandList) = 0;
    virtual void CleanupDLSS(bool wfi) = 0;

#ifdef STREAMLINE_FEATURE_DLSS_RR
    struct DLSSDSettings
    {
        donut::math::int2 optimalRenderSize;
        donut::math::int2 minRenderSize;
        donut::math::int2 maxRenderSize;
        float sharpness;
    };
    virtual void GetDLSSRROptions(const sl::DLSSDOptions& options, sl::DLSSDOptimalSettings& outSettings) = 0;
    virtual void SetDLSSRROptions(const sl::DLSSDOptions& options) = 0;
    bool GetDLSSRRAvailable() { return m_dlssrr_available; }
    bool GetDLSSRRLastEnable() { return m_dlssrr_consts.mode != sl::DLSSMode::eOff; }
    virtual void EvaluateDLSSRR(nvrhi::ICommandList *commandList) = 0;
    virtual void CleanupDLSSRR(bool wfi) = 0;
#endif //STREAMLINE_FEATURE_DLSS_RR

    virtual void SetNISOptions(const sl::NISOptions consts) = 0;
    bool GetNISAvailable() { return m_nis_available; }
    bool GetNISLastEnable() { return m_nis_consts.mode != sl::NISMode::eOff; }
    virtual void EvaluateNIS(nvrhi::ICommandList *commandList) = 0;
    virtual void CleanupNIS(bool wfi) = 0;

    virtual void SetDeepDVCOptions(const sl::DeepDVCOptions consts) = 0;
    bool GetDeepDVCAvailable() { return m_deepdvc_available; }
    bool GetDeepDVCLastEnable() { return m_deepdvc_consts.mode != sl::DeepDVCMode::eOff; }
    virtual void QueryDeepDVCState(uint64_t &estimatedVRamUsage) = 0;
    virtual void EvaluateDeepDVC(nvrhi::ICommandList *commandList) = 0;
    virtual void CleanupDeepDVC() = 0;

    bool GetReflexAvailable() { return m_reflex_available; }
    bool GetPCLAvailable() const { return m_pcl_available; }
    virtual void SetReflexConsts(const sl::ReflexOptions consts) = 0;
    virtual void ReflexCallback_Sleep(donut::app::DeviceManager &manager, uint32_t frameID) = 0;
    virtual void ReflexCallback_SimStart(donut::app::DeviceManager &manager, uint32_t frameID) = 0;
    virtual void ReflexCallback_SimEnd(donut::app::DeviceManager &manager, uint32_t frameID) = 0;
    virtual void ReflexCallback_RenderStart(donut::app::DeviceManager &manager, uint32_t frameID) = 0;
    virtual void ReflexCallback_RenderEnd(donut::app::DeviceManager &manager, uint32_t frameID) = 0;
    virtual void ReflexCallback_PresentStart(donut::app::DeviceManager &manager, uint32_t frameID) = 0;
    virtual void ReflexCallback_PresentEnd(donut::app::DeviceManager &manager, uint32_t frameID) = 0;
    sl::FrameToken *GetCurrentFrameToken() { return m_currentFrame; }

    virtual void ReflexTriggerFlash() = 0;
    virtual void ReflexTriggerPcPing() = 0;
    virtual void QueryReflexStats(bool &reflex_lowLatencyAvailable, bool &reflex_flashAvailable, std::string &stats) = 0;
    void SetReflexFlashIndicator(bool enabled) { m_reflex_driverFlashIndicatorEnable = enabled; }
    bool GetReflexFlashIndicatorEnable() { return m_reflex_driverFlashIndicatorEnable; }

    virtual void SetDLSSGOptions(const sl::DLSSGOptions consts) = 0;
    bool GetDLSSGAvailable() { return m_dlssg_available; }
    bool GetDLSSGLastEnable() { return m_dlssg_consts.mode != sl::DLSSGMode::eOff; }
    void Set_DLSSG_SwapChainRecreation(bool on)
    {
        m_dlssg_triggerswapchainRecreation = true;
        m_dlssg_shoudLoad = on;
    }
    virtual bool Get_DLSSG_SwapChainRecreation(bool &turn_on) const { return false; };
    void Quiet_DLSSG_SwapChainRecreation() { m_dlssg_triggerswapchainRecreation = false; }
    virtual void CleanupDLSSG(bool wfi) = 0;

    bool GetLatewarpAvailable() { return m_latewarp_available; }
    virtual bool Get_Latewarp_SwapChainRecreation(bool &turn_on) const { return false; };
    virtual void SetReflexCameraData(sl::FrameToken &frameToken, const sl::ReflexCameraData &cameraData) = 0;
#if STREAMLINE_FEATURE_LATEWARP
    virtual void SetLatewarpOptions(const sl::LatewarpOptions &options) = 0;
    void Set_Latewarp_SwapChainRecreation(bool on)
    {
        m_latewarp_triggerswapchainRecreation = true;
        m_latewarp_shouldLoad = on;
    }
    void Quiet_Latewarp_SwapChainRecreation() { m_latewarp_triggerswapchainRecreation = false; }
    virtual void EvaluateLatewarp(donut::app::DeviceManager& manager, nvrhi::ICommandList *commandList, RenderTargets *renderTargets, nvrhi::ITexture *inputColor, nvrhi::ITexture *outputColor, const donut::engine::IView *view) = 0;
    virtual void CleanupLatewarp(bool wfi) = 0;
#endif
};
