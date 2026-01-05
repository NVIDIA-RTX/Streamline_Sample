////----------------------------------------------------------------------------------
//// File:        StreamlineSample.h
//// SDK Version: 2.0
//// Email:       StreamlineSupport@nvidia.com
//// Site:        http://developer.nvidia.com/
////
//// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
////
//// Redistribution and use in source and binary forms, with or without
//// modification, are permitted provided that the following conditions
//// are met:
////  * Redistributions of source code must retain the above copyright
////    notice, this list of conditions and the following disclaimer.
////  * Redistributions in binary form must reproduce the above copyright
////    notice, this list of conditions and the following disclaimer in the
////    documentation and/or other materials provided with the distribution.
////  * Neither the name of NVIDIA CORPORATION nor the names of its
////    contributors may be used to endorse or promote products derived
////    from this software without specific prior written permission.
////
//// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
//// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
//// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////
////----------------------------------------------------------------------------------

#pragma once

#include "NVWrapper.h"
#include "RenderTargets.h"
#include "UIData.h"
#include <random>
#include <chrono>

// From Donut
#include <donut/core/vfs/VFS.h>
#include <donut/core/log.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/FramebufferFactory.h>
#include <donut/engine/Scene.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/TextureCache.h>
#include <donut/render/BloomPass.h>
#include <donut/render/CascadedShadowMap.h>
#include <donut/render/DeferredLightingPass.h>
#include <donut/render/DepthPass.h>
#include <donut/render/DrawStrategy.h>
#include <donut/render/ForwardShadingPass.h>
#include <donut/render/GBufferFillPass.h>
#include <donut/render/LightProbeProcessingPass.h>
#include <donut/render/PixelReadbackPass.h>
#include <donut/render/SkyPass.h>
#include <donut/render/SsaoPass.h>
#include <donut/render/TemporalAntiAliasingPass.h>
#include <donut/render/ToneMappingPasses.h>
#include <donut/app/ApplicationBase.h>
#include <donut/app/Camera.h>
#include <donut/app/DeviceManager.h>
#include <nvrhi/utils.h>

using namespace donut::math;
using namespace donut::app;
using namespace donut::vfs;
using namespace donut::engine;
using namespace donut::render;

struct ScriptingConfig {

    // Control at start behavior
    int maxFrames = -1;
    int DLSS_mode = -1;
    int DLSSRR_mode = -1;
    int Reflex_mode = -1;
    int Reflex_fpsCap = -1;
    int DLSSG_on = -1;
    int DLSSG_numFrameToGenerate = -1;
    int DeepDVC_on = -1;
    int Latewarp_on = -1;
    int GpuLoad = -1;
    sl::Extent viewportExtent{};

    ScriptingConfig(int argc, const char* const* argv)
    {

        for (int i = 1; i < argc; i++)
        {
            //MaxFrames
            if (!strcmp(argv[i], "-maxFrames"))
            {
                maxFrames = std::stoi(argv[++i]);
            }

            // DLSS
            else if (!strcmp(argv[i], "-DLSS_mode"))
            {
                DLSS_mode = std::stoi(argv[++i]);
            }

            // DLSSRR
            else if (!strcmp(argv[i], "-DLSSRR_mode"))
            {
                DLSSRR_mode = std::stoi(argv[++i]);
            }

            // Reflex
            else if (!strcmp(argv[i], "-Reflex_mode"))
            {
                Reflex_mode = std::stoi(argv[++i]);
            }
            else if (!strcmp(argv[i], "-Reflex_fpsCap"))
            {
                Reflex_fpsCap = std::stoi(argv[++i]);
            }

            // DLSSG
            else if (!strcmp(argv[i], "-DLSSG_on"))
            {
                DLSSG_on = 1;
            }
            else if (!strcmp(argv[i], "-DLSSG_numFrameToGenerate"))
            {
                int ret = sscanf(argv[++i], "%d", &DLSSG_numFrameToGenerate);
                assert(ret == 1);
            }
            // DeepDVC
            else if (!strcmp(argv[i], "-DeepDVC_on"))
            {
                DeepDVC_on = 1;
            }

            // Latewarp
            else if (!strcmp(argv[i], "-Latewarp_on"))
            {
                Latewarp_on = 1;
            }

            else if (!strcmp(argv[i], "-viewport"))
            {
                int ret = sscanf(argv[++i], "(%d,%d,%dx%d)", &viewportExtent.left, &viewportExtent.top, &viewportExtent.width, &viewportExtent.height);
                assert(ret == 4);
            }
            else if (!strcmp(argv[i], "-GpuLoad"))
            {
                int ret = sscanf(argv[++i], "%d", &GpuLoad);
                assert(ret == 1);
            }
        }
    }
};

class StreamlineSample : public ApplicationBase
{
private:
    typedef ApplicationBase Super;

    // Main Command queue and binding cache
    nvrhi::CommandListHandle                        m_CommandList;
    BindingCache                                    m_BindingCache;

    // Filesystem and scene
    std::shared_ptr<RootFileSystem>                 m_RootFs;
    std::vector<std::string>                        m_SceneFilesAvailable;
    std::string                                     m_CurrentSceneName;
    std::shared_ptr<Scene>				            m_Scene;
    float                                           m_WallclockTime = 0.f;
                                                    
    // Render Passes                                
    std::shared_ptr<ShaderFactory>                  m_ShaderFactory;
    std::shared_ptr<DirectionalLight>               m_SunLight;
    std::shared_ptr<CascadedShadowMap>              m_ShadowMap;
    std::shared_ptr<FramebufferFactory>             m_ShadowFramebuffer;
    std::shared_ptr<DepthPass>                      m_ShadowDepthPass;
    std::shared_ptr<InstancedOpaqueDrawStrategy>    m_OpaqueDrawStrategy;
    std::unique_ptr<GBufferFillPass>                m_GBufferPass;
    std::unique_ptr<DeferredLightingPass>           m_DeferredLightingPass;
    std::unique_ptr<SkyPass>                        m_SkyPass;
    std::unique_ptr<TemporalAntiAliasingPass>       m_TemporalAntiAliasingPass;
    std::unique_ptr<BloomPass>                      m_BloomPass;
    std::unique_ptr<ToneMappingPass>                m_ToneMappingPass;
    std::unique_ptr<SsaoPass>                       m_SsaoPass;
    std::shared_ptr<TransparentDrawStrategy>        m_TransparentDrawStrategy;

    // RenderTargets
    std::unique_ptr<RenderTargets>                  m_RenderTargets;

    //Views
    std::shared_ptr<IView>                          m_View;
    bool                                            m_PreviousViewsValid = false;
    std::shared_ptr<IView>                          m_ViewPrevious;
    std::shared_ptr<IView>                          m_TonemappingView;

    // Camera
    FirstPersonCamera                               m_FirstPersonCamera;
    float                                           m_CameraVerticalFov = 60.f;

    // Ray Tracing
    static constexpr size_t                         JITTER_SEQUENCE_LENGTH = 16;

    nvrhi::ShaderLibraryHandle                      m_ShaderLibrary;
    nvrhi::rt::PipelineHandle                       m_Pipeline;
    nvrhi::rt::ShaderTableHandle                    m_ShaderTable;
    nvrhi::BindingLayoutHandle                      m_GlobalBindingLayout;
    nvrhi::BindingLayoutHandle                      m_LocalBindingLayout;
    nvrhi::BindingSetHandle                         m_BindingSet;

    nvrhi::rt::AccelStructHandle                    m_BottomLevelAS;
    nvrhi::rt::AccelStructHandle                    m_TopLevelAS;

    nvrhi::BufferHandle                             m_ConstantBuffer;
    bool                                            CreateRayTracingPipeline(donut::engine::ShaderFactory& shaderFactory);
    void                                            CreateAccelStruct(nvrhi::ICommandList* commandList);

    // UI
    UIData& m_ui;
    donut::math::int2                               m_DLSS_Last_DisplaySize = { 0,0 };
    float3                                          m_AmbientTop = 0.f;
    float3                                          m_AmbientBottom = 0.f;

    // For Streamline
    int2                                            m_RenderingRectSize = { 0, 0 };
    int2                                            m_DisplaySize;
    NVWrapper::DLSSSettings                         m_RecommendedDLSSSettings;
#ifdef STREAMLINE_FEATURE_DLSS_RR
    sl::DLSSDOptions                                m_RayReconstructionOptions;
    sl::DLSSDOptimalSettings                        m_RayReconstructionSettings;
#endif //STREAMLINE_FEATURE_DLSS_RR
    std::default_random_engine                      m_Generator;
    float                                           m_PreviousLodBias;
    affine3                                         m_CameraPreviousMatrix;

    bool                                            m_presentStarted = false;

    sl::ViewportHandle                              m_viewport{};
    sl::Extent                                      m_backbufferViewportExtent{};

    // Scripting Behavior
    ScriptingConfig                                 m_ScriptingConfig;

    sl::DLSSMode                                    DLSS_Last_Mode = sl::DLSSMode::eOff;
    sl::DLSSMode                                    DLSSRR_Last_Mode = sl::DLSSMode::eOff;
    donut::math::int2                               m_DLSSRR_Last_DisplaySize = { 0,0 };

public:
    StreamlineSample(DeviceManager* deviceManager, sl::ViewportHandle vpHandle, UIData& ui, const std::string& sceneName, ScriptingConfig scriptingConfig);
    ~StreamlineSample();

    // Functions of interest
    bool SetupView();
    void CreateRenderPasses(bool& exposureResetRequired, float lodBias);
    virtual void RenderScene(nvrhi::IFramebuffer* framebuffer) override;

    void SetBackBufferExtent(sl::Extent &backBufferExtent)
    {
        m_backbufferViewportExtent = backBufferExtent;
    }

    // Logistic functions 
    std::shared_ptr<TextureCache> GetTextureCache();
    std::vector<std::string> const& GetAvailableScenes() const;
    std::string GetCurrentSceneName() const;
    void SetCurrentSceneName(const std::string& sceneName);
    std::shared_ptr<ShaderFactory> GetShaderFactory() const { return m_ShaderFactory; };
    std::shared_ptr<donut::vfs::IFileSystem> GetRootFs() const { return m_RootFs; };

    virtual bool KeyboardUpdate(int key, int scancode, int action, int mods) override;
    virtual bool MousePosUpdate(double xpos, double ypos) override;
    virtual bool MouseButtonUpdate(int button, int action, int mods) override;
    virtual bool MouseScrollUpdate(double xoffset, double yoffset) override;
    virtual void SetLatewarpOptions() override;
    virtual void Render(nvrhi::IFramebuffer* backBufferFramebuffer) override { RenderScene(backBufferFramebuffer); };
    virtual void Animate(float fElapsedTimeSeconds) override;
    virtual void SceneUnloading() override;
    virtual bool LoadScene(std::shared_ptr<IFileSystem> fs, const std::filesystem::path& fileName) override;
    virtual void SceneLoaded() override;
    virtual void RenderSplashScreen(nvrhi::IFramebuffer* framebuffer) override;
private:
    void ToggleFullscreen();
};

struct ViewportData
{
    std::shared_ptr<StreamlineSample> m_pSample;
};

struct MultiViewportApp : public ApplicationBase
{
    MultiViewportApp(DeviceManager* deviceManager, UIData& ui, const std::string& sceneName, ScriptingConfig scriptingConfig):
        Super(deviceManager),
        m_pDeviceManager(deviceManager),
        m_ui(ui),
        m_sceneName(sceneName),
        m_scripting(scriptingConfig)
    {
        m_pViewports.push_back(createViewport());
        SceneLoaded();
    }

    std::shared_ptr<ShaderFactory> GetShaderFactory() const { return m_pViewports[0]->m_pSample->GetShaderFactory(); };
    std::shared_ptr<StreamlineSample> getASample() const { return m_pViewports[0]->m_pSample; }

    virtual void RenderScene(nvrhi::IFramebuffer* framebuffer) override;
    virtual bool KeyboardUpdate(int key, int scancode, int action, int mods) override
    {
        return m_pViewports[0]->m_pSample->KeyboardUpdate(key, scancode, action, mods);
    }
    virtual bool MousePosUpdate(double xpos, double ypos) override
    {
        return m_pViewports[0]->m_pSample->MousePosUpdate(xpos, ypos);
    }
    virtual bool MouseButtonUpdate(int button, int action, int mods) override
    {
        return m_pViewports[0]->m_pSample->MouseButtonUpdate(button, action, mods);
    }
    virtual bool MouseScrollUpdate(double xoffset, double yoffset) override
    {
        return m_pViewports[0]->m_pSample->MouseScrollUpdate(xoffset, yoffset);
    }
    virtual void SetLatewarpOptions() override { getASample()->SetLatewarpOptions(); }
    virtual void Render(nvrhi::IFramebuffer* frameBuffer) override { getASample()->Render(frameBuffer); }
    virtual void Animate(float fElapsedTimeSeconds) override
    {
        for (uint32_t uV = 0; uV < m_pViewports.size(); ++uV)
        {
            m_pViewports[uV]->m_pSample->Animate(fElapsedTimeSeconds);
        }
    }
    virtual void SceneUnloading() override
    {
        m_pViewports[0]->m_pSample->SceneUnloading();
    }
    virtual bool LoadScene(std::shared_ptr<IFileSystem> fs, const std::filesystem::path& fileName) override
    {
        return m_pViewports[0]->m_pSample->LoadScene(fs, fileName);
    }
    virtual void SceneLoaded() override
    {
        Super::SceneLoaded();
    }
    virtual void RenderSplashScreen(nvrhi::IFramebuffer* framebuffer) override
    {
        m_pViewports[0]->m_pSample->RenderSplashScreen(framebuffer);
    }
    virtual bool ShouldRenderUnfocused() override
    { 
        return true; 
    }

private:
    typedef ApplicationBase Super;
    std::shared_ptr<ViewportData> createViewport()
    {
        std::shared_ptr<ViewportData> pVp = std::make_shared<ViewportData>();
        pVp->m_pSample = std::make_shared <StreamlineSample>(
            m_pDeviceManager, sl::ViewportHandle(m_nViewportsCreated++), m_ui, m_sceneName, m_scripting);
        return pVp;
    }
    uint32_t m_nViewportsCreated = 0;
    DeviceManager* m_pDeviceManager = nullptr;
    UIData& m_ui;
    std::string m_sceneName;
    ScriptingConfig m_scripting;
    std::vector<std::shared_ptr<ViewportData>> m_pViewports;
};
