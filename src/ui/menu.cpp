#include "menu.h"
#include "render/renderer.h"
#include <filesystem>
#include <fmt/format.h>
#include <imgui.h>
#include <iostream>
#include <nfd.h>
#include <glm/gtx/component_wise.hpp>
#include <volume/vf_volume.h>

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
    m_tfWidget = TransferFunctionWidget(volume, false);


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
    m_tfWidget = TransferFunctionWidget(volume, true);
    m_tfWidget->updateRenderConfig(m_renderConfig);

    const glm::ivec3 dim = volume.dims();
    m_volumeInfo = fmt::format("Volume info:\n{}\nDimensions: ({}, {}, {})",
        volume.fileName(), dim.x, dim.y, dim.z);
    m_volumeLoaded = true;
    
    m_dataType = volume::VolumeType::VectorField;
    m_renderConfig.renderMode = render::RenderMode::RenderVectorfield;
    m_renderConfig.vectorFieldParameters.volumeDims = volume.dims();
    m_renderConfig.vectorFieldParameters.timeStep = 0;
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
            showVectorFieldTab();
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
            nfdresult_t result = NFD_OpenDialog("fld,gri,dat", nullptr, &pOutPath);

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
        //ImGui::RadioButton("TriCubic", pInterpolationModeInt, int(volume::InterpolationMode::Cubic));

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
        //if (m_renderConfig.renderMode == render::RenderMode::RenderSlicer || m_renderConfig.renderMode == render::RenderMode::RenderTF2D) {
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

// This renders the 2D Transfer Function Widget.
/*void Menu::show2DTransFuncTab()
{
    if (ImGui::BeginTabItem("2D transfer function")) {
        m_tf2DWidget->draw();
        m_tf2DWidget->updateRenderConfig(m_renderConfig);
        ImGui::EndTabItem();
    }
}*/

void Menu::showVectorFieldTab()
{
    if (ImGui::BeginTabItem("Vector Field Visualization")) {

    ImGui::BeginGroup();
        ImGui::Text("Timeseries");
        ImGui::Separator();

        ImGui::SliderInt("Time Step", &m_renderConfig.vectorFieldParameters.timeStep, 0, m_renderConfig.vectorFieldParameters.volumeDims.z - 1);
    ImGui::EndGroup();

    ImGui::NewLine();

    ImGui::BeginGroup();
        ImGui::Text("Componentwise Scalar Visualization");
        ImGui::Separator();
        ImGui::Checkbox("Enable", &m_renderConfig.vectorFieldParameters.isScalarActive);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(285);
        ImGui::SliderFloat("Opacity", &m_renderConfig.vectorFieldParameters.scalarOverlayAlpha, 0, 1);

        glm::bvec3 pre_vectorComponents = m_renderConfig.vectorFieldParameters.vectorComponents;
        glm::bvec2 pre_scalarComponents = m_renderConfig.vectorFieldParameters.scalarComponents;

        ImGui::Text("Active Component");
        ImGui::Checkbox("X", &m_renderConfig.vectorFieldParameters.vectorComponents.x);
        ImGui::SameLine();
        ImGui::Checkbox("Y", &m_renderConfig.vectorFieldParameters.vectorComponents.y);
        ImGui::SameLine();
        ImGui::Checkbox("Magnitude", &m_renderConfig.vectorFieldParameters.vectorComponents.z);
        ImGui::SameLine();
        ImGui::Checkbox("Scalar 1", &m_renderConfig.vectorFieldParameters.scalarComponents.x);
        ImGui::SameLine();
        ImGui::Checkbox("Scalar 2", &m_renderConfig.vectorFieldParameters.scalarComponents.y);

        if (m_renderConfig.vectorFieldParameters.vectorComponents == glm::bvec3(false) && m_renderConfig.vectorFieldParameters.scalarComponents == glm::bvec2(false))
        {
            m_renderConfig.vectorFieldParameters.vectorComponents = pre_vectorComponents;
            m_renderConfig.vectorFieldParameters.scalarComponents = pre_scalarComponents;
        }
        else if ((pre_vectorComponents.x == false && m_renderConfig.vectorFieldParameters.vectorComponents.x == true) 
            || (pre_vectorComponents.y == false && m_renderConfig.vectorFieldParameters.vectorComponents.y == true))
        {
            m_renderConfig.vectorFieldParameters.vectorComponents.z = m_renderConfig.vectorFieldParameters.scalarComponents.x = m_renderConfig.vectorFieldParameters.scalarComponents.y = false;
        }
        else if (pre_vectorComponents.z == false && m_renderConfig.vectorFieldParameters.vectorComponents.z == true)
        {
            m_renderConfig.vectorFieldParameters.vectorComponents.x = m_renderConfig.vectorFieldParameters.vectorComponents.y  = m_renderConfig.vectorFieldParameters.scalarComponents.x = m_renderConfig.vectorFieldParameters.scalarComponents.y = false;
        }
        else if ((pre_scalarComponents.x == false && m_renderConfig.vectorFieldParameters.scalarComponents.x == true)
            || (pre_scalarComponents.y == false && m_renderConfig.vectorFieldParameters.scalarComponents.y == true)) {
            m_renderConfig.vectorFieldParameters.vectorComponents = glm::bvec3(false);
        }

    ImGui::EndGroup();

    ImGui::NewLine();

    ImGui::BeginGroup();
        ImGui::Text("Line Integral Convolution");
        ImGui::Separator();
        ImGui::Checkbox("Enable LIC", &m_renderConfig.vectorFieldParameters.isLicActive);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(260);
        ImGui::SliderInt("Kernel Size", &m_renderConfig.vectorFieldParameters.licKernelWidth, 1, 100);
    ImGui::EndGroup();

    ImGui::NewLine();

    ImGui::BeginGroup();
        ImGui::Text("Geometry Overlay");
        ImGui::Separator();
        static int geometryOverlay = static_cast<int>(m_renderConfig.vectorFieldParameters.geometryOverlay);
        ImGui::RadioButton("None", &geometryOverlay, (int)render::VectorGeometry::None);
        ImGui::RadioButton("Glyphs/Hedgehog", &geometryOverlay, (int)render::VectorGeometry::Hedgehog);
        ImGui::RadioButton("Vector field integration", &geometryOverlay, (int)render::VectorGeometry::Integration);
        m_renderConfig.vectorFieldParameters.geometryOverlay = (render::VectorGeometry) geometryOverlay;
    ImGui::EndGroup();

    ImGui::NewLine();

    ImGui::BeginGroup();
        ImGui::Text("Hedgehogs");
        ImGui::Separator();

        ImGui::Checkbox("Fixed Size", &m_renderConfig.vectorFieldParameters.hedgehogFixedSize);
        ImGui::SliderFloat("Base Arrow Length", &m_renderConfig.vectorFieldParameters.hedgehogBaseLength, 0.1f, 25.0f);
        ImGui::SliderInt("Sample every n-th Pixel", &m_renderConfig.vectorFieldParameters.hedgehogSampling, 1, 50);
    ImGui::EndGroup();

    ImGui::NewLine();

    ImGui::BeginGroup();
        ImGui::Text("Field Line Integration");
        ImGui::Separator();

        ImGui::SliderInt("Number of Lines", &m_renderConfig.vectorFieldParameters.numLines, 1, 100);
        ImGui::SliderInt("Line Width", &m_renderConfig.vectorFieldParameters.lineWidth, 1, 5);

        ImGui::NewLine();

        static int vf_line_method = (int)m_renderConfig.vectorFieldParameters.method;
        ImGui::Text("Integration Method");
        ImGui::SameLine();
        ImGui::RadioButton("Euler", &vf_line_method, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Runge-Kutta (2nd Order)", &vf_line_method, 1);
        m_renderConfig.vectorFieldParameters.method = (render::LineMethod)vf_line_method;

        static int vf_line_mode = (int)m_renderConfig.vectorFieldParameters.mode;
        ImGui::Text("Line Type");
        ImGui::SameLine();
        ImGui::RadioButton("Streamlines ", &vf_line_mode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Pathlines ", &vf_line_mode, 1);
        m_renderConfig.vectorFieldParameters.mode = (render::LineMode)vf_line_mode;

        ImGui::SliderFloat("Integration Step Size XY", &m_renderConfig.vectorFieldParameters.stepSize, 0.1f, 50.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderInt("Steps between time steps", &m_renderConfig.vectorFieldParameters.numStepsTime, 1, 1000);

        ImGui::Text("Stopping Criteria Line Integration");
        ImGui::SliderFloat("Minimum Vector Magnitude", &m_renderConfig.vectorFieldParameters.stoppingMagnitude, 0.01f, 1.0f);
        ImGui::SliderInt("Max # Integration Steps", &m_renderConfig.vectorFieldParameters.maxSteps, 1000, 10000);

        m_renderConfig.vectorFieldParameters.seedPoint = m_mouseRect;

        ImGui::Text("Geometry color:");
        ImGui::SameLine();
        
        // Display a button with the selected color
        if (ImGui::ColorButton("##colorbutton", m_renderConfig.vectorFieldParameters.geoColor, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20)))
        {
            ImGui::OpenPopup("ColorPickerPopup");
        }
    
        // Popup with color picker
        if (ImGui::BeginPopup("ColorPickerPopup"))
        {
            ImGui::Text("Adjust Color");
            if (ImGui::ColorPicker4("##picker", (float*)&m_renderConfig.vectorFieldParameters.geoColor))
            {
                // Color updated
            }
            ImGui::EndPopup();
        }
    ImGui::EndGroup();
    
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
