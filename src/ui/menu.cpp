#include "menu.h"
#include "render/renderer.h"
#include <filesystem>
#include <fmt/format.h>
#include <imgui.h>
#include <iostream>
#include <nfd.h>
#include <glm/gtx/component_wise.hpp>

namespace ui {

Menu::Menu(const glm::ivec2& baseRenderResolution)
    : m_baseRenderResolution(baseRenderResolution)
{
    m_renderConfig.renderResolution = m_baseRenderResolution;
}

void Menu::setLoadVolumeCallback(LoadVolumeCallback&& callback)
{
    m_optLoadVolumeCallback = std::move(callback);
}

void Menu::setRenderConfigChangedCallback(RenderConfigChangedCallback&& callback)
{
    m_optRenderConfigChangedCallback = std::move(callback);
}

void Menu::setGPUMeshConfigChangedCallback(GPUMeshConfigChangedCallback&& callback)
{
    m_optGPUMeshConfigChangedCallback = std::move(callback);
}

void Menu::setGPUVolumeConfigChangedCallback(GPUVolumeConfigChangedCallback&& callback)
{
    m_optGPUVolumeConfigChangedCallback = std::move(callback);
}

void Menu::setInterpolationModeChangedCallback(InterpolationModeChangedCallback&& callback)
{
    m_optInterpolationModeChangedCallback = std::move(callback);
}

render::RenderConfig Menu::renderConfig() const
{
    return m_renderConfig;
}

render::GPUMeshConfig Menu::meshConfig() const
{
    return m_gpuMeshConfig;
}

render::GPUVolumeConfig Menu::volumeConfig() const
{
    return m_gpuVolumeConfig;
}

volume::InterpolationMode Menu::interpolationMode() const
{
    return m_interpolationMode;
}

bool Menu::getCPURendererInUse()
{
    return CPURendererInUse;
}

void Menu::setMouseRect(glm::vec4 mouseRect)
{
    m_mouseRect = mouseRect;
}

void Menu::setBaseRenderResolution(const glm::ivec2& baseRenderResolution)
{
    m_baseRenderResolution = baseRenderResolution;
    m_renderConfig.renderResolution = glm::ivec2(glm::vec2(m_baseRenderResolution) * m_resolutionScale);
    callRenderConfigChangedCallback();
}

// This function handles a part of the volume loading where we create the widget histograms, set some config values
//  and set the menu volume information
void Menu::setLoadedVolume(const volume::Volume& volume, const volume::GradientVolume& gradientVolume)
{
    m_tfWidget = TransferFunctionWidget(volume);
    m_tfWidget->updateRenderConfig(m_renderConfig);

    const glm::ivec3 dim = volume.dims();
    m_volumeInfo = fmt::format("Volume info:\n{}\nDimensions: ({}, {}, {})\nVoxel value range: {} - {}\n",
        volume.fileName(), dim.x, dim.y, dim.z, volume.minimum(), volume.maximum());
    m_volumeMax = int(volume.maximum());
    m_volumeDimensions = volume.dims();
    m_volumeLoaded = true;
    m_dataType = volume::VolumeType::Volume;

    // change to correct render mode when load data from vector field to volume
    m_renderConfig.renderMode = render::RenderMode::RenderSlicer;
}

//This overloaded function is used for the vector fields instead of the DVR implementation
void Menu::setLoadedVolume(const volume::Volume& volume)
{
    m_tfWidget = TransferFunctionWidget(volume);
    m_tfWidget->updateRenderConfig(m_renderConfig);

    const glm::ivec3 dim = volume.dims();
    m_volumeInfo = fmt::format("Volume info:\n{}\nDimensions: ({}, {}, {})",
        volume.fileName(), dim.x, dim.y, dim.z);
    m_volumeLoaded = true;
}

// This function draws the menu
void Menu::drawMenu(const glm::ivec2& pos, const glm::ivec2& size, std::chrono::duration<double> renderTime, std::chrono::duration<double> renderTimeFrame)
{
    static bool open = 1;
    ImGui::Begin("3D Visualization", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowPos(ImVec2(float(pos.x), float(pos.y)));
    ImGui::SetWindowSize(ImVec2(float(size.x), float(size.y)));

    ImGui::BeginTabBar("3DVisTabs");
    showLoadVolTab();
    if (m_volumeLoaded) {
        const auto renderConfigBefore = m_renderConfig;
        const auto transferFunctionBefore = m_renderConfig.tfColorMap;
        const auto gpuMeshConfigBefore = m_gpuMeshConfig;
        const auto gpuVolumeConfigBefore = m_gpuVolumeConfig;
        const auto interpolationModeBefore = m_interpolationMode;

        if (m_dataType == volume::VolumeType::Volume) {
            showRayCastTab(renderTime, renderTimeFrame);
            showGPURayCastTab(renderTime, renderTimeFrame);
            showTransFuncTab();
        } else {
            showTransFuncTab();
        }

        if (m_renderConfig != renderConfigBefore) {
            callRenderConfigChangedCallback();
            if (transferFunctionBefore != m_renderConfig.tfColorMap) {
                m_renderConfig.updateTF = true;
            }
        }
        if (m_gpuMeshConfig != gpuMeshConfigBefore)
            callGPUMeshConfigChangedCallback();
        if (m_gpuVolumeConfig != gpuVolumeConfigBefore)
            callGPUVolumeConfigChangedCallback();
        if (m_interpolationMode != interpolationModeBefore)
            callInterpolationModeChangedCallback();
    }

    ImGui::EndTabBar();
    ImGui::End();
}

// This renders the Load Data tab, which shows a "Load" button and some volume information
void Menu::showLoadVolTab()
{
    if (ImGui::BeginTabItem("Load")) {

        if (ImGui::Button("Load Data")) {
            nfdchar_t* pOutPath = nullptr;
            nfdresult_t result = NFD_OpenDialog("fld,dat", nullptr, &pOutPath);

            if (result == NFD_OKAY) {
                // Convert from char* to std::filesystem::path
                std::filesystem::path path = pOutPath;
                if (m_optLoadVolumeCallback)
                    (*m_optLoadVolumeCallback)(path);
            }
        }

        if (m_volumeLoaded)
            ImGui::Text("%s", m_volumeInfo.c_str());

        ImGui::EndTabItem();
    }
}

// This renders the RayCast tab, where the user can set the render mode, interpolation mode and other
//  render-related settings
void Menu::showRayCastTab(std::chrono::duration<double> renderTime, std::chrono::duration<double> renderTimeFrame)
{
    if (ImGui::BeginTabItem("CPU Raycaster")) {
        CPURendererInUse = true;

        const std::string renderText = fmt::format("rendering time(last new frame): {}ms\n{} FPS\nrendering resolution: ({}, {})\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(renderTime).count(), 1.0 / renderTimeFrame.count() , m_renderConfig.renderResolution.x, m_renderConfig.renderResolution.y);
        ImGui::Text("%s", renderText.c_str());
        ImGui::NewLine();

        int* pRenderModeInt = reinterpret_cast<int*>(&m_renderConfig.renderMode);
        ImGui::Text("Render Mode:");
        ImGui::RadioButton("Slicer", pRenderModeInt, int(render::RenderMode::RenderSlicer));
        ImGui::RadioButton("MIP", pRenderModeInt, int(render::RenderMode::RenderMIP));
        ImGui::RadioButton("IsoSurface Rendering", pRenderModeInt, int(render::RenderMode::RenderIso));
        ImGui::RadioButton("Compositing", pRenderModeInt, int(render::RenderMode::RenderComposite));
        
        ImGui::NewLine();
        ImGui::Checkbox("Volume Shading", &m_renderConfig.volumeShading);

        ImGui::NewLine();
        ImGui::DragFloat("Iso Value", &m_renderConfig.isoValue, 1.0f, 0.0f, float(m_volumeMax));
        
        ImGui::Checkbox("Use Bisection", &m_renderConfig.bisection);

        ImGui::NewLine();
        ImGui::DragFloat("Step Size", &m_renderConfig.stepSize, 0.25f, 0.25f, 5.0f);

        ImGui::NewLine();
        int* pInterpolationModeInt = reinterpret_cast<int*>(&m_interpolationMode);
        ImGui::Text("Interpolation:");
        ImGui::RadioButton("Nearest Neighbour", pInterpolationModeInt, int(volume::InterpolationMode::NearestNeighbour));
        ImGui::RadioButton("Linear", pInterpolationModeInt, int(volume::InterpolationMode::Linear));

        ImGui::EndTabItem();
    }
}

// This renders the GPURayCast tab
void Menu::showGPURayCastTab(std::chrono::duration<double> renderTime, std::chrono::duration<double> renderTimeFrame)
{
    if (ImGui::BeginTabItem("GPU Raycaster")) {
        CPURendererInUse = false;

        const std::string renderText = fmt::format("rendering time(last new frame): {}ms\n{} FPS\nrendering resolution: ({}, {})\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(renderTime).count(), 1.0 / renderTimeFrame.count(), m_renderConfig.renderResolution.x, m_renderConfig.renderResolution.y);
        ImGui::Text("%s", renderText.c_str());
        ImGui::NewLine();
        if (m_renderConfig.renderMode == render::RenderMode::RenderSlicer ) {
            m_renderConfig.renderMode = render::RenderMode::RenderMIP;
        }

        int* pRenderModeInt = reinterpret_cast<int*>(&m_renderConfig.renderMode);
        ImGui::Text("Render Mode:");
        ImGui::RadioButton("MIP", pRenderModeInt, int(render::RenderMode::RenderMIP));
        ImGui::RadioButton("IsoSurface Rendering", pRenderModeInt, int(render::RenderMode::RenderIso));
        ImGui::RadioButton("Compositing", pRenderModeInt, int(render::RenderMode::RenderComposite));

        ImGui::NewLine();
        ImGui::DragFloat("Step size", &m_renderConfig.stepSize, 0.25f, 0.25f, 5.0f);

        ImGui::NewLine();
        ImGui::Checkbox("Volume Shading", &m_renderConfig.volumeShading);

        ImGui::NewLine();
        ImGui::DragFloat("Iso Value", &m_renderConfig.isoValue, 1.0f, 0.0f, float(m_volumeMax));

        ImGui::NewLine();
        ImGui::Checkbox("Opacity Modulation", &m_renderConfig.useOpacityModulation);
        ImGui::DragFloat("Boundary kc", &m_renderConfig.illustrativeParams.x, 0.05f, 0.0f, 1.0f);
        ImGui::DragFloat("Boundary ks", &m_renderConfig.illustrativeParams.y, 0.25f, 0.0f, 10.0f);
        ImGui::DragFloat("Boundary ke", &m_renderConfig.illustrativeParams.z, 0.25f, 0.0f, 5.0f);

        ImGui::NewLine();
        ImGui::DragInt("RenderStep", &m_renderConfig.renderStep, 0.01f, 1, 3);

        ImGui::NewLine();
        ImGui::Checkbox("Empty space skipping", &m_gpuMeshConfig.useEmptySpaceSkipping);
        ImGui::DragInt("Block size", &m_gpuMeshConfig.blockSize, 1, 2, glm::compMax(m_volumeDimensions));

        ImGui::NewLine();
        ImGui::Checkbox("Use volume bricking", &m_gpuVolumeConfig.useVolumeBricking);
        ImGui::DragInt("Brick size", &m_gpuVolumeConfig.brickSize, 1, 8, glm::compMax(m_volumeDimensions));

        ImGui::NewLine();

        // There is no cubic in the GPU so we set it to linear
        if (m_interpolationMode == volume::InterpolationMode::Cubic) {
            m_interpolationMode = volume::InterpolationMode::Linear;
        }

        int* pInterpolationModeInt = reinterpret_cast<int*>(&m_interpolationMode);
        ImGui::Text("Interpolation:");
        ImGui::RadioButton("Nearest Neighbour", pInterpolationModeInt, int(volume::InterpolationMode::NearestNeighbour));
        ImGui::RadioButton("Linear", pInterpolationModeInt, int(volume::InterpolationMode::Linear));

        ImGui::NewLine();

        ImGui::EndTabItem();
    }
}

// This renders the 1D Transfer Function Widget.
void Menu::showTransFuncTab()
{
    if (ImGui::BeginTabItem("Transfer function")) {
        m_tfWidget->draw();
        m_tfWidget->updateRenderConfig(m_renderConfig);
        ImGui::EndTabItem();
    }
}

void Menu::callRenderConfigChangedCallback() const
{
    if (m_optRenderConfigChangedCallback)
        (*m_optRenderConfigChangedCallback)(m_renderConfig);
}

void Menu::callGPUMeshConfigChangedCallback() const
{
    if (m_optGPUMeshConfigChangedCallback)
        (*m_optGPUMeshConfigChangedCallback)(m_gpuMeshConfig);
}

void Menu::callGPUVolumeConfigChangedCallback() const
{
    if (m_optGPUVolumeConfigChangedCallback)
        (*m_optGPUVolumeConfigChangedCallback)(m_gpuVolumeConfig);
}

void Menu::callInterpolationModeChangedCallback() const
{
    if (m_optInterpolationModeChangedCallback)
        (*m_optInterpolationModeChangedCallback)(m_interpolationMode);
}

}
