#pragma once
#include "render/render_config.h"
#include "render/gpu_mesh_config.h"
#include "render/gpu_volume_config.h"
#include "ui/transfer_func.h"
#include "volume/gradient_volume.h"
#include "volume/volume.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace render {
class Renderer;
}

namespace render {
class GPURenderer;
}

namespace ui {
class Menu {
public:
    Menu(const glm::ivec2& baseRenderResolution);

    using LoadVolumeCallback = std::function<void(const std::filesystem::path&)>;
    void setLoadVolumeCallback(LoadVolumeCallback&& callback);
    using RenderConfigChangedCallback = std::function<void(const render::RenderConfig&)>;
    void setRenderConfigChangedCallback(RenderConfigChangedCallback&& callback);
    using GPUMeshConfigChangedCallback = std::function<void(const render::GPUMeshConfig&)>;
    void setGPUMeshConfigChangedCallback(GPUMeshConfigChangedCallback&& callback);
    using GPUVolumeConfigChangedCallback = std::function<void(const render::GPUVolumeConfig&)>;
    void setGPUVolumeConfigChangedCallback(GPUVolumeConfigChangedCallback&& callback);
    using InterpolationModeChangedCallback = std::function<void(volume::InterpolationMode)>;
    void setInterpolationModeChangedCallback(InterpolationModeChangedCallback&& callback);

    render::RenderConfig renderConfig() const;
    render::GPUMeshConfig meshConfig() const;
    render::GPUVolumeConfig volumeConfig() const;
    volume::InterpolationMode interpolationMode() const;

    void setBaseRenderResolution(const glm::ivec2& baseRenderResolution);
    void setLoadedVolume(const volume::Volume& volume, const volume::GradientVolume& gradientVolume);
    void setLoadedVolume(const volume::Volume& volume);

    void drawMenu(const glm::ivec2& pos, const glm::ivec2& size, std::chrono::duration<double> renderTime, std::chrono::duration<double> renderTimeFrame);

    bool getCPURendererInUse();

    void setMouseRect(glm::vec4 mouseRect);

private:
    void showLoadVolTab();
    void showRayCastTab(std::chrono::duration<double> renderTime, std::chrono::duration<double> renderTimeFrame);
    void showGPURayCastTab(std::chrono::duration<double> renderTime, std::chrono::duration<double> renderTimeFrame);
    void showTransFuncTab();

    void callRenderConfigChangedCallback() const;
    void callGPUMeshConfigChangedCallback() const;
    void callGPUVolumeConfigChangedCallback() const;
    void callInterpolationModeChangedCallback() const;

private:
    bool m_volumeLoaded = false;
    bool CPURendererInUse = true;
    std::string m_volumeInfo;
    int m_volumeMax;
    glm::ivec3 m_volumeDimensions;
    volume::VolumeType m_dataType;

    glm::vec4 m_mouseRect;

    std::optional<TransferFunctionWidget> m_tfWidget;

    glm::ivec2 m_baseRenderResolution;
    float m_resolutionScale { 1.0f };
    render::RenderConfig m_renderConfig {};
    render::GPUMeshConfig m_gpuMeshConfig {};
    render::GPUVolumeConfig m_gpuVolumeConfig {};
    volume::InterpolationMode m_interpolationMode { volume::InterpolationMode::NearestNeighbour };

    std::optional<LoadVolumeCallback> m_optLoadVolumeCallback;
    std::optional<RenderConfigChangedCallback> m_optRenderConfigChangedCallback;
    std::optional<GPUMeshConfigChangedCallback> m_optGPUMeshConfigChangedCallback;
    std::optional<GPUVolumeConfigChangedCallback> m_optGPUVolumeConfigChangedCallback;
    std::optional<InterpolationModeChangedCallback> m_optInterpolationModeChangedCallback;
};

}
