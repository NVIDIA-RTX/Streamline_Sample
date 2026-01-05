//----------------------------------------------------------------------------------
// File:        UIRenderer.h
// SDK Version: 2.0
// Email:       StreamlineSupport@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#include "StreamlineSample.h"
#include "NVWrapper.h"
#include "UIData.h"

#include <donut/app/DeviceManager.h>
#include <imgui_internal.h>
#include <donut/engine/Scene.h>
#include <nvrhi/nvrhi.h>
#include <donut/app/imgui_renderer.h>

void pushDisabled() {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}
void popDisabled() {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
}

constexpr ImU32 TITLE_COL = IM_COL32(0, 255, 0, 255);

class UIRenderer : public ImGui_Renderer
{
private:
    std::shared_ptr<StreamlineSample> m_app;
    UIData& m_ui;

    bool    m_dev_view = false;
    int     m_dev_view_dlss_mode = 0;
    int     m_dev_view_TopLevelDLSS = 1;
    int     m_dev_view_RayTracing = 1;
    int     m_dev_view_dlssrr_mode = 0;

    bool    m_last_rt_mode = false;
    bool    m_rt_changed = false;


public:
    UIRenderer(DeviceManager* deviceManager, std::shared_ptr<StreamlineSample> app, UIData& ui)
        : ImGui_Renderer(deviceManager)
        , m_app(app)
        , m_ui(ui) {

        // IMGUI by default writes in srgb colorSpace, but our back buffer is in rgb, we will simply pre-empt this by gamma shifting the colors.
        auto invGamma = 1.f / 2.2f;
        for (auto i = 0; i < ImGuiCol_COUNT; i++) {
            ImGui::GetStyle().Colors[i].x = powf(ImGui::GetStyle().Colors[i].x, invGamma);
            ImGui::GetStyle().Colors[i].y = powf(ImGui::GetStyle().Colors[i].y, invGamma);
            ImGui::GetStyle().Colors[i].z = powf(ImGui::GetStyle().Colors[i].z, invGamma);
            ImGui::GetStyle().Colors[i].w = powf(ImGui::GetStyle().Colors[i].w, invGamma);
        }

        if (m_ui.DLSS_Mode != sl::DLSSMode::eOff) {
            m_dev_view_dlssrr_mode = 0;
            if (m_ui.DLSS_Mode == sl::DLSSMode::eMaxQuality) m_dev_view_dlss_mode = 2;
            else if (m_ui.DLSS_Mode == sl::DLSSMode::eBalanced) m_dev_view_dlss_mode = 3;
            else if (m_ui.DLSS_Mode == sl::DLSSMode::eMaxPerformance) m_dev_view_dlss_mode = 4;
            else if (m_ui.DLSS_Mode == sl::DLSSMode::eUltraPerformance) m_dev_view_dlss_mode = 5;
            else if (m_ui.DLSS_Mode == sl::DLSSMode::eDLAA) m_dev_view_dlss_mode = 6;
        }

#ifdef STREAMLINE_FEATURE_DLSS_RR
        if (m_ui.DLSSRR_Mode != sl::DLSSMode::eOff) {
            m_dev_view_dlss_mode = 0;
            if (m_ui.DLSSRR_Mode == sl::DLSSMode::eMaxQuality) m_dev_view_dlssrr_mode = 2;
            else if (m_ui.DLSSRR_Mode == sl::DLSSMode::eBalanced) m_dev_view_dlssrr_mode = 3;
            else if (m_ui.DLSSRR_Mode == sl::DLSSMode::eMaxPerformance) m_dev_view_dlssrr_mode = 4;
            else if (m_ui.DLSSRR_Mode == sl::DLSSMode::eUltraPerformance) m_dev_view_dlssrr_mode = 5;
            else if (m_ui.DLSSRR_Mode == sl::DLSSMode::eDLAA) m_dev_view_dlssrr_mode = 6;
        }
#endif // STREAMLINE_FEATURE_DLSS_RR

    }

    virtual bool ShouldRenderUnfocused() override
    { 
        return true; 
    }
protected:
    virtual void buildUI(void) override
    {

        int width, height;
        GetDeviceManager()->GetWindowDimensions(width, height);

        if (m_app->IsSceneLoading())
        {
            BeginFullScreenWindow();

            char messageBuffer[256];
            const auto& stats = Scene::GetLoadingStats();
            snprintf(messageBuffer, std::size(messageBuffer), "Loading scene %s, please wait...\nObjects: %d/%d, Textures: %d/%d",
                m_app->GetCurrentSceneName().c_str(), stats.ObjectsLoaded.load(), stats.ObjectsTotal.load(), m_app->GetTextureCache()->GetNumberOfLoadedTextures(), m_app->GetTextureCache()->GetNumberOfRequestedTextures());

            DrawScreenCenteredText(messageBuffer);

            EndFullScreenWindow();

            return;
        }

        if (m_ui.MouseOverUI) {
            ImGui::SetNextWindowBgAlpha(0.5f);
            ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
            ImGui::SetNextWindowSize(ImVec2(float(width), float(height)));
            ImGui::Begin("BackDrop", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::End();
        }


        ImGui::SetNextWindowPos(ImVec2(width * 0.02f, height * 0.5f), 0, ImVec2(0.f, 0.5f));
        ImGui::SetNextWindowBgAlpha(m_ui.MouseOverUI ? 0.f : 0.2f);
        ImGui::Begin("Settings", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

        if (ImGui::IsAnyItemHovered() || ImGui::IsMouseHoveringRect(ImGui::GetWindowPos(), ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y))) {
            m_ui.MouseOverUI = true;
        }
        else
            m_ui.MouseOverUI = false;

        ImGui::Text("Renderer: %s", GetDeviceManager()->GetRendererString());
        double fps = 1.0 / GetDeviceManager()->GetAverageFrameTimeSeconds();
        ImGui::Text("Engine FPS: %.0f ", fps);
        if (m_ui.DLSSG_mode != sl::DLSSGMode::eOff ) {
            ImGui::Text("True FPS: %.0f ", m_ui.DLSSG_fps);
        }
        // Vsync
        if (m_ui.DLSSG_mode != sl::DLSSGMode::eOff && !m_dev_view) {
            pushDisabled();
            m_ui.EnableVsync = false;
        }
        ImGui::Checkbox("VSync", &m_ui.EnableVsync);
        if (m_ui.DLSSG_mode != sl::DLSSGMode::eOff && !m_dev_view) {
            popDisabled();
        }

        // Resolution 
        std::vector<std::string> Resolutions_strings = { "1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160" };
        std::vector<int2> Resolutions_values = { {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160} };
        int resIndex = -1;
        for (auto i = 0; i < Resolutions_values.size(); ++i) {
            if (Resolutions_values[i].x == m_ui.Resolution.x && Resolutions_values[i].y == m_ui.Resolution.y) {
                resIndex = i;
            }
        }
        if (resIndex == -1) {
            Resolutions_strings.push_back(std::to_string(m_ui.Resolution.x) + " x " + std::to_string(m_ui.Resolution.y) + " (custom)");
            Resolutions_values.push_back(m_ui.Resolution);
            resIndex = (int)Resolutions_strings.size() - 1;
        }
        auto preIndex = resIndex;
        if (ImGui::BeginCombo("Resolution", Resolutions_strings[resIndex].c_str()))
        {
            for (auto i = 0; i < Resolutions_strings.size(); ++i)
            {
                bool is_selected = i == resIndex;
                auto is_selected_pre = i == resIndex;
                if (ImGui::Selectable(Resolutions_strings[i].c_str(), is_selected)) resIndex = i;
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        m_ui.Resolution = Resolutions_values[resIndex];
        if (preIndex != resIndex) {
            m_ui.Resolution_changed = true;
        }

#ifdef STREAMLINE_FEATURE_DLSS_RR
        if (m_app->GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11) pushDisabled();
        ImGui::Combo("##Ray Tracing", &m_ui.RayTracing_Mode, "Off\0On\0");
        ImGui::SameLine();
        ImGui::Text("Ray Tracing");
        if (m_app->GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11)
        {
            popDisabled();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Not supported with D3D11)");
        }
#endif // STREAMLINE_FEATURE_DLSS_RR

        ImGui::Separator();
        ImGui::Checkbox("Developer Menu", &m_dev_view);

        if (!m_dev_view) {
            //
            //  Reflex & Reflex Frame Warp
            //
            ImGui::Separator();
            
            ImGui::Text("Nvidia Reflex Low Latency");
            if (!m_ui.REFLEX_Supported) pushDisabled();
            ImGui::SameLine();

            if (m_ui.DLSSG_mode != sl::DLSSGMode::eOff)
            {
                auto i = (int)m_ui.REFLEX_Mode - 1;
                i = i < 0 ? 0 : i;
                ImGui::Combo("##Reflex", &i, "On\0On + Boost\0");
                m_ui.REFLEX_Mode = i + 1;
            }
            else
            {
                ImGui::Combo("##Reflex", (int*)&m_ui.REFLEX_Mode, "Off\0On\0On + Boost\0");
            }
            if (m_ui.REFLEX_Mode != 0) {
                ImGui::Indent();
                bool useFrameCap = m_ui.REFLEX_CapedFPS != 0;
                ImGui::Checkbox("Reflex FPS Capping", &useFrameCap);
                if (useFrameCap) {
                    if (m_ui.REFLEX_CapedFPS == 0) { m_ui.REFLEX_CapedFPS = 60; }
                    ImGui::SameLine();
                    ImGui::DragInt("##FPSReflexCap", &m_ui.REFLEX_CapedFPS, 1.f, 20, 240);
                }
                else {
                    m_ui.REFLEX_CapedFPS = 0;
                }
                ImGui::Unindent();
            }
            if (!m_ui.REFLEX_Supported) popDisabled();

            ImGui::Text("Frame Warp");
            ImGui::SameLine();
            if (!m_ui.Latewarp_Supported) pushDisabled();
            if (ImGui::Combo("##Latewarp", &m_ui.Latewarp_active, "Off\0On\0"))
            {
                if (!m_ui.Latewarp_active)
                {
                    m_ui.Latewarp_cleanup_needed = true;
                }
            }
            if (!m_ui.Latewarp_Supported) popDisabled();

            //
            //  Generic DLSS
            //

            ImGui::Separator();

            ImGui::Text("NVIDIA DLSS");
            ImGui::SameLine();
            ImGui::Combo("##DLSSMode", &m_dev_view_TopLevelDLSS, "Off\0On\0");

            ImGui::Indent();
            if (m_dev_view_TopLevelDLSS == 0) {
                pushDisabled();
                m_ui.DLSS_Mode = sl::DLSSMode::eOff;
                m_dev_view_dlss_mode = 0;
                m_ui.DLSSG_mode = sl::DLSSGMode::eOff;
                m_ui.REFLEX_Mode = 0;
                m_ui.NIS_Mode = sl::NISMode::eOff;
            }

#ifdef STREAMLINE_FEATURE_DLSS_RR
            // Check for Ray Tracing mode changes and handle functionality
            m_rt_changed = (m_last_rt_mode != m_dev_view_RayTracing);
            m_last_rt_mode = m_dev_view_RayTracing;

            // Automatically turn OFF DLSS-RR when RT is turned OFF
            if (m_ui.RayTracing_Mode == 0)
            {
                m_ui.DLSSRR_Mode = sl::DLSSMode::eOff;
                m_dev_view_dlssrr_mode = 0;
            }
#endif // STREAMLINE_FEATURE_DLSS_RR

            
            //
            //  DLSS Frame Gen
            //

            ImGui::Text("Frame Generation");
            ImGui::SameLine();
            if (!m_ui.DLSSG_Supported || !m_ui.REFLEX_Supported) pushDisabled();
            if (ImGui::Combo("##FrameGeneration", (int*)&m_ui.DLSSG_mode, "Off\0On\0Auto (Dynamic Frame Generation)\0"))
            {
                if (m_ui.DLSSG_mode == sl::DLSSGMode::eOff)
                {
                    m_ui.DLSSG_cleanup_needed = true;
                }
            }
            if (m_ui.DLSSG_mode != sl::DLSSGMode::eOff)
            {
                ImGui::Indent();
                ImGui::Text("Generated Frames");
                ImGui::SameLine();
                ImGui::SliderInt("##MultiframeCount", &m_ui.DLSSG_numFrames, 2, m_ui.DLSSG_numFramesMaxMultiplier, "%dx", ImGuiSliderFlags_AlwaysClamp);
                ImGui::Unindent();
            }
            if (!m_ui.DLSSG_Supported || !m_ui.REFLEX_Supported) popDisabled();
            if (m_ui.DLSSG_status != "") ImGui::Text((std::string("State: ") + m_ui.DLSSG_status).c_str());

            //
            //  DLSS SuperRes
            //

            auto DLSSModeNames = std::vector<std::string>({
                "Off##DLSSModes",
                "Auto##DLSSModes",
                "Quality##DLSSModes",
                "Balanced##DLSSModes",
                "Performance##DLSSModes",
                "UltraPerformance##DLSSModes",
                "DLAA##DLSSModes"
            });

            ImGui::Text("Super Resolution");
            ImGui::SameLine();
            if (!m_ui.DLSS_Supported) pushDisabled();
            if (!m_ui.DLSS_Supported
#ifdef STREAMLINE_FEATURE_DLSS_RR
                || m_ui.DLSSRR_Mode != sl::DLSSMode::eOff
#endif
                ) pushDisabled();
            if (ImGui::BeginCombo("##SuperRes", DLSSModeNames[m_dev_view_dlss_mode].data()))
            {
                for (int i = 0; i < DLSSModeNames.size(); ++i)
                {
                    bool is_selected = i == m_dev_view_dlss_mode;

                    if (ImGui::Selectable(DLSSModeNames[i].data(), is_selected)) m_dev_view_dlss_mode = i;
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
                if (ImGui::IsItemHovered()) m_ui.MouseOverUI = true;
            }
#ifdef STREAMLINE_FEATURE_DLSS_RR
            if (m_ui.DLSSRR_Mode != sl::DLSSMode::eOff)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Ray Reconstruction is active)");
            }
#endif// STREAMLINE_FEATURE_DLSS_RR
            if (!m_ui.DLSS_Supported
#ifdef STREAMLINE_FEATURE_DLSS_RR
                    || m_ui.DLSSRR_Mode != sl::DLSSMode::eOff
#endif
                ) popDisabled();
            if (!m_ui.DLSS_Supported) popDisabled();

            if (m_dev_view_dlss_mode == 0) m_ui.DLSS_Mode = sl::DLSSMode::eOff;
            else if (m_dev_view_dlss_mode == 2) m_ui.DLSS_Mode = sl::DLSSMode::eMaxQuality;
            else if (m_dev_view_dlss_mode == 3) m_ui.DLSS_Mode = sl::DLSSMode::eBalanced;
            else if (m_dev_view_dlss_mode == 4) m_ui.DLSS_Mode = sl::DLSSMode::eMaxPerformance;
            else if (m_dev_view_dlss_mode == 5) m_ui.DLSS_Mode = sl::DLSSMode::eUltraPerformance;
            //else if (m_dev_view_dlss_mode == 6) m_ui.DLSS_Mode = sl::DLSSMode::eUltraQuality;
            else if (m_dev_view_dlss_mode == 6) m_ui.DLSS_Mode = sl::DLSSMode::eDLAA;
            else if (m_dev_view_dlss_mode == 1) {
                if (m_ui.Resolution.x < 1920) m_ui.DLSS_Mode = sl::DLSSMode::eOff;
                else if (m_ui.Resolution.x < 2560) m_ui.DLSS_Mode = sl::DLSSMode::eMaxQuality;
                else if (m_ui.Resolution.x < 3840) m_ui.DLSS_Mode = sl::DLSSMode::eMaxPerformance;
                else m_ui.DLSS_Mode = sl::DLSSMode::eUltraPerformance;
            }

            if (m_ui.DLSS_Mode != sl::DLSSMode::eOff) {
                m_ui.AAMode = AntiAliasingMode::DLSS;
            }
            else {
                m_ui.AAMode = AntiAliasingMode::NONE;
            }
#ifdef STREAMLINE_FEATURE_DLSS_RR
            //
            //  DLSS Ray Reconstruction
            //

            ImGui::Text("Ray Reconstruction");
            ImGui::SameLine();
            if (!m_ui.DLSSRR_Supported || m_ui.RayTracing_Mode == 0 || m_app->GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11) pushDisabled();
            if (ImGui::BeginCombo("##RayReconstruction",  DLSSModeNames[m_dev_view_dlssrr_mode].data()))
            {
                for (int i = 0; i < DLSSModeNames.size(); ++i)
                {
                    bool is_selected = i == m_dev_view_dlssrr_mode;

                    if (ImGui::Selectable(DLSSModeNames[i].data(), is_selected)) 
                    {
                        m_dev_view_dlssrr_mode = i;
                        
                        // Automatically turn OFF DLSS-SR when DLSS-RR is turned ON
                        if (m_dev_view_dlssrr_mode != 0)
                        {
                            m_ui.DLSS_Mode = sl::DLSSMode::eOff;
                            m_dev_view_dlss_mode = 0;
                            m_ui.AAMode = AntiAliasingMode::NONE;
                        }
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
                if (ImGui::IsItemHovered()) m_ui.MouseOverUI = true;
            }

            if (!m_ui.DLSSRR_Supported || m_ui.RayTracing_Mode == 0 || m_app->GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11) popDisabled();

            if (m_app->GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Not supported with D3D11)");
            }

            if (m_dev_view_dlssrr_mode == 0) m_ui.DLSSRR_Mode = sl::DLSSMode::eOff;
            else if (m_dev_view_dlssrr_mode == 2) m_ui.DLSSRR_Mode = sl::DLSSMode::eMaxQuality;
            else if (m_dev_view_dlssrr_mode == 3) m_ui.DLSSRR_Mode = sl::DLSSMode::eBalanced;
            else if (m_dev_view_dlssrr_mode == 4) m_ui.DLSSRR_Mode = sl::DLSSMode::eMaxPerformance;
            else if (m_dev_view_dlssrr_mode == 5) m_ui.DLSSRR_Mode = sl::DLSSMode::eUltraPerformance;
            //else if (m_dev_view_dlssrr_mode == 6) m_ui.DLSSRR_Mode = sl::DLSSMode::eUltraQuality;
            else if (m_dev_view_dlssrr_mode == 6) m_ui.DLSSRR_Mode = sl::DLSSMode::eDLAA;
            else if (m_dev_view_dlssrr_mode == 1) {
                if (m_ui.Resolution.x < 1920) m_ui.DLSSRR_Mode = sl::DLSSMode::eOff;
                else if (m_ui.Resolution.x < 2560) m_ui.DLSSRR_Mode = sl::DLSSMode::eMaxQuality;
                else if (m_ui.Resolution.x < 3840) m_ui.DLSSRR_Mode = sl::DLSSMode::eMaxPerformance;
                else m_ui.DLSSRR_Mode = sl::DLSSMode::eUltraPerformance;
            }
#endif // STREAMLINE_FEATURE_DLSS_RR
            //
            //  NIS Sharpening
            //

            ImGui::Text("NIS Sharpening");
            ImGui::SameLine();
            if (! m_ui.NIS_Supported) pushDisabled();
            int nis_mode = m_ui.NIS_Mode == sl::NISMode::eScaler ? 1 : 0;
            ImGui::Combo("##NISMode", &nis_mode, "Off\0On\0");
            m_ui.NIS_Mode = nis_mode == 1 ? sl::NISMode::eScaler : sl::NISMode::eOff;
            if (nis_mode == 1)
            {
                ImGui::DragFloat("Sharpness", &m_ui.NIS_Sharpness, 0.05f, 0, 1);
            }
            if (! m_ui.NIS_Supported) popDisabled();

            
            if (ImGui::IsItemHovered()) m_ui.MouseOverUI = true;

            if (m_dev_view_TopLevelDLSS == 0) popDisabled();
            ImGui::Unindent();

            //
            //  DeepDVC
            //

            ImGui::Separator();
            ImGui::Text("NVIDIA DeepDVC");
            ImGui::Indent();
            ImGui::Text("Supported: %s", m_ui.DeepDVC_Supported ? "yes" : "no");
            if (m_ui.DeepDVC_Supported) {
                int deeoDVC_mode = m_ui.DeepDVC_Mode == sl::DeepDVCMode::eOn ? 1 : 0;
                ImGui::Text("DeepDVC Mode");
                ImGui::SameLine();
                ImGui::Combo("##DeepDVC Mode", &deeoDVC_mode, "Off\0On\0");
                m_ui.DeepDVC_Mode = deeoDVC_mode == 1 ? sl::DeepDVCMode::eOn : sl::DeepDVCMode::eOff;
                if (m_ui.DeepDVC_Mode == sl::DeepDVCMode::eOn)
                {
                    ImGui::Text("VRAM = %4.2f MB", m_ui.DeepDVC_VRAM / 1024 / 1024.0f);
                    ImGui::Text("Intensity");
                    ImGui::SameLine();
                    ImGui::DragFloat("##Intensity", &m_ui.DeepDVC_Intensity, 0.01f, 0, 1);
                    ImGui::Text("Saturation Boost");
                    ImGui::SameLine();
                    ImGui::DragFloat("##Saturation Boost", &m_ui.DeepDVC_SaturationBoost, 0.01f, 0, 1);
                }
            }
            ImGui::Dummy(ImVec2(0.0f, 50.0f));

            ImGui::Separator();

        }

        else { // if m_dev_view

            //
            // DLSS SuperRes and AA
            //

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
            ImGui::Text("AA and DLSS");
            ImGui::PopStyleColor();

            ImGui::Text("DLSS_Supported: %s", m_ui.DLSS_Supported ? "yes" : "no");

            if (m_ui.DLSS_Supported) {
#ifdef STREAMLINE_FEATURE_DLSS_RR
                if (m_ui.DLSSRR_Mode != sl::DLSSMode::eOff) pushDisabled();
#endif
                ImGui::Combo("AA Mode", (int*)&m_ui.AAMode, "None\0TemporalAA\0DLSS\0");
#ifdef STREAMLINE_FEATURE_DLSS_RR
                if (m_ui.DLSSRR_Mode != sl::DLSSMode::eOff) popDisabled();
#endif
            }
            else {
#ifdef STREAMLINE_FEATURE_DLSS_RR
                if (m_ui.DLSSRR_Mode != sl::DLSSMode::eOff) pushDisabled();
#endif
                ImGui::Combo("TAA Fallback", (int*)&m_ui.AAMode, "None\0TemporalAA");
#ifdef STREAMLINE_FEATURE_DLSS_RR
                if (m_ui.DLSSRR_Mode != sl::DLSSMode::eOff) popDisabled();
#endif
            }

            if (m_ui.AAMode == AntiAliasingMode::TEMPORAL) {
                ImGui::Combo("TAA Camera Jitter", (int*)&m_ui.TemporalAntiAliasingJitter, "MSAA\0Halton\0R2\0White Noise\0");
            }


            if (m_ui.AAMode == AntiAliasingMode::DLSS
#ifdef STREAMLINE_FEATURE_DLSS_RR
                && m_ui.DLSSRR_Mode == sl::DLSSMode::eOff
#endif // STREAMLINE_FEATURE_DLSS_RR
               )
            {
                if (m_ui.DLSS_Mode == sl::DLSSMode::eOff) m_ui.DLSS_Mode = sl::DLSSMode::eBalanced;

                // We do not show 'eOff' 
                const char* DLSSModeNames[] = {
                    "Off",
                    "Performance",
                    "Balanced",
                    "Quality",
                    "Ultra-Performance",
                    "Ultra-Quality",
                    "DLAA"
                };

                if (ImGui::BeginCombo("DLSS Mode", DLSSModeNames[(int)m_ui.DLSS_Mode]))
                {
                    for (int i = 0; i < static_cast<int>(sl::DLSSMode::eCount); ++i)
                    {
                        if ((i == static_cast<int>(sl::DLSSMode::eUltraQuality)) || (i == static_cast<int>(sl::DLSSMode::eOff))) continue;

                        bool is_selected = (i == (int)m_ui.DLSS_Mode);

                        if (ImGui::Selectable(DLSSModeNames[i], is_selected)) {
                            m_ui.DLSS_Mode = (sl::DLSSMode)i;
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                auto PresetSlotNames = std::vector<std::string>({
                    "Off##Presets",
                    "MaxPerformance##Presets",
                    "Balanced##Presets",
                    "MaxQuality##Presets",
                    "UltraPerformance##Presets",
                    "UltraQuality##Presets",
                    "DLAA##Presets"
                    });

                const std::map<sl::DLSSPreset,std::string> DLSSPresetToDropdownMap = {
                    {sl::DLSSPreset::eDefault, "Default##Presets"},
                    {sl::DLSSPreset::ePresetF, "Preset F##Presets"},
                    {sl::DLSSPreset::ePresetJ, "Preset J##Presets"},
                    {sl::DLSSPreset::ePresetK, "Preset K##Presets"},
                    {sl::DLSSPreset::ePresetL, "Preset L##Presets"},
                    {sl::DLSSPreset::ePresetM, "Preset M##Presets"}
                };

                if (ImGui::CollapsingHeader("Presets")) {
                    ImGui::Indent();

                    for (int j = 0; j < static_cast<int>(sl::DLSSMode::eCount); j++) {

                        if ((j == static_cast<int>(sl::DLSSMode::eUltraQuality)) || (j == static_cast<int>(sl::DLSSMode::eOff))) continue;

                        const std::string* currentPresetString = nullptr;

                        auto currentPreset = DLSSPresetToDropdownMap.find(m_ui.DLSS_presets[j]);

                        if (currentPreset != DLSSPresetToDropdownMap.end()) {
                            currentPresetString = &DLSSPresetToDropdownMap.at(m_ui.DLSS_presets[j]);
                        }
                        else {
                            currentPresetString = &DLSSPresetToDropdownMap.at(sl::DLSSPreset::eDefault);
                            donut::log::info("Warning: There is a mismatch in the preset supported by the sample and the preset selected by the snippet");
                        }

                        if (ImGui::BeginCombo(PresetSlotNames[j].c_str(), currentPresetString->data())) {
                            for (const auto& [presetEnum, presetName] : DLSSPresetToDropdownMap) {

                                bool is_selected = (presetEnum == m_ui.DLSS_presets[j]);

                                if (ImGui::Selectable(presetName.data(), is_selected)) m_ui.DLSS_presets[j] = presetEnum;
                                if (is_selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                            if (ImGui::IsItemHovered()) m_ui.MouseOverUI = true;
                        }
                    }
                    ImGui::Unindent();
                }

                static const char* DLSSResModeNames[] = {
                    "Fixed",
                    "Dynamic"
                };

                if (ImGui::BeginCombo("DLSS Resolution Mode", DLSSResModeNames[(int)m_ui.DLSS_Resolution_Mode]))
                {
                    for (int i = 0; i < (int)RenderingResolutionMode::COUNT; ++i)
                    {
                        bool is_selected = (i == (int)m_ui.DLSS_Resolution_Mode);
                        if (ImGui::Selectable(DLSSResModeNames[i], is_selected)) m_ui.DLSS_Resolution_Mode = (RenderingResolutionMode)i;
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (m_ui.DLSS_Resolution_Mode == RenderingResolutionMode::DYNAMIC)
                {
                    if (ImGui::Button("Change Res"))
                    {
                        m_ui.DLSS_Dynamic_Res_change = true;
                    }
                }

                ImGui::Checkbox("Debug: Show full input buffer", &m_ui.DLSS_DebugShowFullRenderingBuffer);
                ImGui::Checkbox("Debug: Force Extent use", &m_ui.DLSS_always_use_extents);

                ImGui::Checkbox("Overide LOD Bias", &m_ui.DLSS_lodbias_useoveride);
                if (m_ui.DLSS_lodbias_useoveride) {
                    ImGui::SameLine();
                    ImGui::SliderFloat("", &m_ui.DLSS_lodbias_overide, -2, 2);
                }

            }
#ifdef STREAMLINE_FEATURE_DLSS_RR
            else if (m_ui.DLSSRR_Mode != sl::DLSSMode::eOff) {
                ImGui::Text("DLSS RR needs to be disabled for DLSS to be enabled");
            }
#endif // STREAMLINE_FEATURE_DLSS_RR
            else if (m_ui.DLSS_Mode != sl::DLSSMode::eOff) {
                ImGui::Text("DLSS needs to be disabled for DLSS RR to be enabled");
            }

            else {
                ImGui::Text("DLSS is not enabled");
            }



#ifdef STREAMLINE_FEATURE_DLSS_RR
            // Add after the DLSS section and before Reflex section

            //
            // DLSS Ray Reconstruction
            //
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
            ImGui::Text("DLSS Ray Reconstruction");
            ImGui::PopStyleColor();

            ImGui::Text("DLSS RR Supported: %s", (m_ui.DLSSRR_Supported && m_app->GetDevice()->getGraphicsAPI() != nvrhi::GraphicsAPI::D3D11) ? "yes" : "no");
            if (m_ui.DLSSRR_Supported && m_app->GetDevice()->getGraphicsAPI() != nvrhi::GraphicsAPI::D3D11) {
                if (m_ui.DLSS_Mode != sl::DLSSMode::eOff) {
                    ImGui::Text("DLSS needs to be disabled for DLSS RR to be enabled");
                    m_ui.DLSSRR_Mode = sl::DLSSMode::eOff;
                } else {
                    // We do not show 'eOff' 
                    
                    const char* DLSSRRModeNames[] = {
                        "Off",
                        "Performance",
                        "Balanced",
                        "Quality",
                        "Ultra-Performance",
                        "Ultra-Quality",
                        "DLAA"
                    };

                    if (ImGui::BeginCombo("DLSS RR Mode", DLSSRRModeNames[(int)m_ui.DLSSRR_Mode])) {
                        for (int i = 0; i < static_cast<int>(sl::DLSSMode::eCount); ++i) {
                            if ((i == static_cast<int>(sl::DLSSMode::eUltraQuality)) || 
                                (i == static_cast<int>(sl::DLSSMode::eOff))) continue;

                            bool is_selected = (i == (int)m_ui.DLSSRR_Mode);

                            if (ImGui::Selectable(DLSSRRModeNames[i], is_selected)) {
                                m_ui.DLSSRR_Mode = (sl::DLSSMode)i;
                                
                                // Automatically turn OFF DLSS-SR when DLSS-RR is turned ON
                                if (m_ui.DLSSRR_Mode != sl::DLSSMode::eOff)
                                {
                                    m_ui.DLSS_Mode = sl::DLSSMode::eOff;
                                    m_ui.AAMode = AntiAliasingMode::NONE;
                                }
                                
                                // Add RT resource recreation trigger here
                                // m_ui.rt_changed = true;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    auto PresetSlotNames = std::vector<std::string>({
                        "Off##RRPresets",
                        "MaxPerformance##RRPresets",
                        "Balanced##RRPresets",
                        "MaxQuality##RRPresets",
                        "UltraPerformance##RRPresets",
                        "UltraQuality##RRPresets",
                        "DLAA##RRPresets"
                    });

                    const std::map<sl::DLSSDPreset, std::string> DLSSRRPresetToDropdownMap = {
                        {sl::DLSSDPreset::eDefault, "Default##RRPresets"},
                        {sl::DLSSDPreset::ePresetD, "Preset D##RRPresets"},
                        {sl::DLSSDPreset::ePresetE, "Preset E##RRPresets"}
                    };

                    if (ImGui::CollapsingHeader("RR Presets")) {
                        ImGui::Indent();

                        for (int j = 0; j < static_cast<int>(sl::DLSSMode::eCount); j++) {
                            if ((j == static_cast<int>(sl::DLSSMode::eUltraQuality)) || 
                                (j == static_cast<int>(sl::DLSSMode::eOff))) continue;

                            const std::string* currentPresetString = nullptr;
                            auto currentPreset = DLSSRRPresetToDropdownMap.find(m_ui.DLSSRR_presets[j]);

                            if (currentPreset != DLSSRRPresetToDropdownMap.end()) {
                                currentPresetString = &DLSSRRPresetToDropdownMap.at(m_ui.DLSSRR_presets[j]);
                            } else {
                                currentPresetString = &DLSSRRPresetToDropdownMap.at(sl::DLSSDPreset::eDefault);
                                donut::log::info("Warning: There is a mismatch in the RR preset supported by the sample and the preset selected");
                            }

                            if (ImGui::BeginCombo(PresetSlotNames[j].c_str(), currentPresetString->data())) {
                                for (const auto& [presetEnum, presetName] : DLSSRRPresetToDropdownMap) {
                                    bool is_selected = (presetEnum == m_ui.DLSSRR_presets[j]);

                                    if (ImGui::Selectable(presetName.data(), is_selected)) {
                                        m_ui.DLSSRR_presets[j] = presetEnum;
                                    }
                                    if (is_selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                                if (ImGui::IsItemHovered()) m_ui.MouseOverUI = true;
                            }
                        }
                        ImGui::Unindent();
                    }
                }
            }
#endif // STREAMLINE_FEATURE_DLSS_RR

            //
            // Reflex
            //

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
            ImGui::Text("Reflex");
            ImGui::PopStyleColor();

            ImGui::Text("Reflex Supported (PCL tracking): %s", m_ui.REFLEX_Supported ? "yes" : "no");
            ImGui::Text("Reflex LowLatency Supported: %s", m_ui.REFLEX_LowLatencyAvailable ? "yes" : "no");
            if (m_ui.REFLEX_Supported && m_ui.REFLEX_LowLatencyAvailable) {
                ImGui::Combo("Reflex Low Latency", (int*)&m_ui.REFLEX_Mode, "Off\0On\0On + Boost\0");

                bool useFrameCap = m_ui.REFLEX_CapedFPS != 0;
                ImGui::Checkbox("Reflex FPS Capping", &useFrameCap);

                if (useFrameCap) {
                    if (m_ui.REFLEX_CapedFPS == 0) { m_ui.REFLEX_CapedFPS = 60; }
                    ImGui::SameLine();
                    ImGui::DragInt("##FPSReflexCap", &m_ui.REFLEX_CapedFPS, 1.f, 20, 240);
                }
                else {
                    m_ui.REFLEX_CapedFPS = 0;
                }

                if (ImGui::CollapsingHeader("Stats Report")) {
                    ImGui::Indent();
                    ImGui::Text(m_ui.REFLEX_Stats.c_str());
                    ImGui::Unindent();
                }
            }

            ImGui::Text("Frame Warp");
            ImGui::SameLine();
            if (!m_ui.Latewarp_Supported) pushDisabled();
            if (ImGui::Combo("##Latewarp", &m_ui.Latewarp_active, "Off\0On\0"))
            {
                if (!m_ui.Latewarp_active)
                {
                    m_ui.Latewarp_cleanup_needed = true;
                }
            }
            if (!m_ui.Latewarp_Supported) popDisabled();

            //
            // DLSS Frame Generatioon
            //

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
            ImGui::Text("DLSS-G");
            ImGui::PopStyleColor();

            ImGui::Text("DLSS-G Supported: %s", m_ui.DLSSG_Supported ? "yes" : "no");
            if (m_ui.DLSSG_Supported) {

                if (m_ui.REFLEX_Mode == (int)sl::ReflexMode::eOff) {
                    ImGui::Text("Reflex needs to be enabled for DLSSG to be enabled");
                    m_ui.DLSSG_mode = sl::DLSSGMode::eOff;
                }
                else {
                    if (ImGui::Combo("DLSS-G Mode", (int*)&m_ui.DLSSG_mode, "Off\0On\0Auto (Dynamic Frame Generation)\0"))
                    {
                        if (m_ui.DLSSG_mode == sl::DLSSGMode::eOff)
                        {
                            m_ui.DLSSG_cleanup_needed = true;
                        }
                    }
                }


            }


            //
            //  NIS Sharpening
            //

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
            ImGui::Text("NIS Sharpening");
            ImGui::PopStyleColor();

            ImGui::Text("NIS Supported: %s", m_ui.NIS_Supported ? "yes" : "no");

            if (m_ui.NIS_Supported){
                int nis_mode = m_ui.NIS_Mode == sl::NISMode::eScaler ? 1 : 0;
                ImGui::Combo("NIS Mode", &nis_mode, "Off\0On\0");
                m_ui.NIS_Mode = nis_mode == 1 ? sl::NISMode::eScaler : sl::NISMode::eOff;
                ImGui::DragFloat("Sharpness", &m_ui.NIS_Sharpness, 0.05f, 0, 1);
            }

            //
            //  Additional Settings
            //

            ImGui::Separator();

            if (ImGui::CollapsingHeader("Additional settings")) {

                ImGui::Indent();

                // Scene
                ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
                ImGui::Text("Scene");
                ImGui::PopStyleColor();

                // Scene 
                const std::string currentScene = m_app->GetCurrentSceneName();
#ifdef STREAMLINE_FEATURE_DLSS_RR
                m_ui.RayTracing_Mode = m_app->GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D11 ? 0 : 1;
#endif // STREAMLINE_FEATURE_DLSS_RR
                if (ImGui::BeginCombo("Scene", currentScene.c_str()))
                {
                    const std::vector<std::string>& scenes = m_app->GetAvailableScenes();
                    for (const std::string& scene : scenes)
                    {
                        bool is_selected = scene == currentScene;
                        if (ImGui::Selectable(scene.c_str(), is_selected))
                            m_app->SetCurrentSceneName(scene);
                        if (is_selected)
                            ImGui::SetItemDefaultFocus(); 
                    }
                    ImGui::EndCombo();
                }

                // Animation
                ImGui::Checkbox("Animate", &m_ui.EnableAnimations);
                if (m_ui.EnableAnimations) {
                    ImGui::SameLine();
                    ImGui::DragFloat("Speed", &m_ui.AnimationSpeed, 0.01f, 0.01f, 2.f);
                }

                ImGui::SliderFloat("Ambient Intensity", &m_ui.AmbientIntensity, 0.f, 1.f);

                ImGui::Checkbox("Enable Procedural Sky", &m_ui.EnableProceduralSky);
                if (m_ui.EnableProceduralSky && ImGui::CollapsingHeader("Sky Parameters"))
                {
                    ImGui::Indent();
                    ImGui::SliderFloat("Brightness", &m_ui.SkyParams.brightness, 0.f, 1.f);
                    ImGui::SliderFloat("Glow Size", &m_ui.SkyParams.glowSize, 0.f, 90.f);
                    ImGui::SliderFloat("Glow Sharpness", &m_ui.SkyParams.glowSharpness, 1.f, 10.f);
                    ImGui::SliderFloat("Glow Intensity", &m_ui.SkyParams.glowIntensity, 0.f, 1.f);
                    ImGui::SliderFloat("Horizon Size", &m_ui.SkyParams.horizonSize, 0.f, 90.f);
                    ImGui::Unindent();
                }

                // Additional Load
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
                ImGui::Text("Additional Load");
                ImGui::PopStyleColor();

                // CPU Load
                bool enableCPULoad = m_ui.CpuLoad != 0;
                ImGui::Checkbox("Additional CPU Load", &enableCPULoad);
                if (enableCPULoad) {
                    if (m_ui.CpuLoad == 0) { m_ui.CpuLoad = 0.5; }
                    ImGui::SameLine();
                    ImGui::DragFloat("##CPULoad", &m_ui.CpuLoad, 1.f, 0.001f, 50);
                }
                else {
                    m_ui.CpuLoad = 0;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Runs a while loop for a given number of ms");


                // GPU Load
                bool enableGPULoad = m_ui.GpuLoad != 0;
                ImGui::Checkbox("Additional GPU Load", &enableGPULoad);
                if (enableGPULoad) {
                    if (m_ui.GpuLoad == 0) { m_ui.GpuLoad = 1; }
                    ImGui::SameLine();
                    ImGui::DragInt("##GPULoad", &m_ui.GpuLoad, 1.f, 1, 300);
                }
                else {
                    m_ui.GpuLoad = 0;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Repeats the Gbuffer pass an additional number of times");

                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
                ImGui::Text("Debug visualisation");
                ImGui::PopStyleColor();

                ImGui::Checkbox("Overlay Buffers", &m_ui.VisualiseBuffers);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Shows the depth and motion vector buffers.");

                // Pipeline
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
                ImGui::Text("Pipeline options");
                ImGui::PopStyleColor();

                ImGui::Checkbox("Enable SSAO", &m_ui.EnableSsao);
                ImGui::Checkbox("Enable Bloom", &m_ui.EnableBloom);

                if (m_ui.EnableBloom && ImGui::CollapsingHeader("Bloom Settings"))
                {
                    ImGui::Indent();
                    ImGui::DragFloat("Bloom Sigma", &m_ui.BloomSigma, 0.01f, 0.1f, 100.f);
                    ImGui::DragFloat("Bloom Alpha", &m_ui.BloomAlpha, 0.01f, 0.01f, 1.0f);
                    ImGui::Unindent();
                }

                ImGui::Checkbox("Enable Shadows", &m_ui.EnableShadows);
                ImGui::Checkbox("Enable Tonemapping", &m_ui.EnableToneMapping);
                if (m_ui.EnableToneMapping && ImGui::CollapsingHeader("ToneMapping Params"))
                {
                    ImGui::Indent();
                    ImGui::DragFloat("Exposure Bias", &m_ui.ToneMappingParams.exposureBias, 0.1f, -2, 2);
                    ImGui::Unindent();
                }

                if (m_ui.NIS_Mode == sl::NISMode::eOff)
                {
                    // Viewport Extent
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, TITLE_COL);
                    ImGui::Text("Backbuffer Viewport Extent");
                    ImGui::PopStyleColor();

                    uint32_t nViewports = (uint32_t)m_ui.BackBufferExtents.size();
                    nViewports = std::max(1u, nViewports); // can't have 0 viewports
                    std::vector<const char*> nViewports_strings = { "1", "2", "3" };
                    if (ImGui::BeginCombo("nViewports", nViewports_strings[nViewports - 1]))
                    {
                        for (auto i = 0; i < nViewports_strings.size(); ++i)
                        {
                            bool is_selected = (i == (nViewports - 1));
                            if (ImGui::Selectable(nViewports_strings[i], is_selected)) nViewports = i + 1;
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    // check if we need to add/remove viewports based on the new selection
                    if (nViewports == 3)
                    {
                        m_ui.BackBufferExtents.resize(3);
                    }
                    else if (nViewports == 2)
                    {
                        m_ui.BackBufferExtents.resize(2);
                    }
                    else
                    {
                        m_ui.BackBufferExtents.resize(1);
                        float viewportX{ static_cast<float>(m_ui.BackBufferExtents[0].left) };
                        float viewportY{ static_cast<float>(m_ui.BackBufferExtents[0].top) };
                        float viewportW{ static_cast<float>(m_ui.BackBufferExtents[0].width) };
                        float viewportH{ static_cast<float>(m_ui.BackBufferExtents[0].height) };

                        // ensure values set to the slider are in the valid range.
                        viewportX = std::clamp(viewportX, 0.f, static_cast<uint32_t>(viewportW) > 1 ? viewportW - 1 : 0);
                        ImGui::SliderFloat("OffsetLeft", &(viewportX), 0.f, static_cast<uint32_t>(viewportW) > 1 ? viewportW - 1 : 0);

                        viewportY = std::clamp(viewportY, 0.f, static_cast<uint32_t>(viewportH) > 1 ? viewportH - 1 : 0);
                        ImGui::SliderFloat("OffsetTop", &(viewportY), 0.f, static_cast<uint32_t>(viewportH) > 1 ? viewportH - 1 : 0);

                        viewportW = std::clamp(viewportW, 0.f, static_cast<float>(width - viewportX));
                        ImGui::SliderFloat("Width", &(viewportW), 0.f, static_cast<float>(width - viewportX));

                        viewportH = std::clamp(viewportH, 0.f, static_cast<float>(height - viewportY));
                        ImGui::SliderFloat("Height", &(viewportH), 0.f, static_cast<float>(height - viewportY));

                        m_ui.BackBufferExtents[0] = { static_cast<uint32_t>(viewportY), static_cast<uint32_t>(viewportX), static_cast<uint32_t>(viewportW), static_cast<uint32_t>(viewportH) };
                    }
                }

                ImGui::Unindent();

            }

        }

        ImGui::End();


        if (m_ui.MouseOverUI) {
            ImGui::SetNextWindowPos(ImVec2(width * 0.5f, height * 0.5f), 0, ImVec2(0.5f, 0.5f));
            ImGui::Begin("SettingText", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Settings Menu");
            auto text = "sl::DLSSGMode::eOff is set.";
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetWindowWidth() / 2 - ImGui::CalcTextSize(text).x / 2);
            ImGui::Text(text);
            text = "Streamline features may behave differently while your mouse is hovering the UI.";
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetWindowWidth() / 2 - ImGui::CalcTextSize(text).x / 2);
            ImGui::Text(text);

            ImGui::End();
        }

    }
};
