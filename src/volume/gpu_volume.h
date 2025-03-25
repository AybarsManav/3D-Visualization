#ifndef VOLUME_GPU_VOLUME_H
#define VOLUME_GPU_VOLUME_H

#include "volume.h"
#include "texture.h"
#include <glm/vec3.hpp>
#include <string>
#include <vector>
#include <render/render_config.h>
#include <render/gpu_volume_config.h>
#ifdef __linux__
#include <GL/glew.h>
#else
#include <gl/glew.h>
#endif

namespace volume {
class GPUVolume {
public:
    InterpolationMode interpolationMode { InterpolationMode::NearestNeighbour };

public:
    GPUVolume(const Volume* volume);

    void setVolumeConfig(const render::GPUVolumeConfig& config);

    void brickSizeChanged(render::RenderConfig renderConfig, std::array<float, 256>& opacitySumTable);
    void updateBrickCache(render::RenderConfig renderConfig, std::array<float, 256>& opacitySumTable);

    GLuint getTexId() const;
    GLuint getIndexTexId() const;
    
    void updateInterpolation();

    // some accessors for the bricking properties
    inline int getBrickSize() { return m_brickSize; };
    inline glm::ivec3 getBrickVolumeSize() { return m_brickVolumeSize; };
    inline bool useBricking(){ return m_volumeConfig.useVolumeBricking; };
    inline glm::ivec3 getIndexVolumeSize() { return m_indexVolumeSize; };

private:

    Texture m_volumeTexture;
    Texture m_indexTexture;
    
    const volume::Volume* m_pVolume;

    render::GPUVolumeConfig m_volumeConfig;

    int m_brickSize;
    bool m_useBricking;
    int m_brickPadding;
    glm::ivec3 m_volumeDims;

    glm::ivec3 m_brickVolumeSize;
    std::vector<float> m_brickVolume;

    glm::ivec3 m_indexVolumeSize;
    std::vector<glm::vec3> m_indexVolume;
    std::vector<glm::vec2> m_minMaxValues; // min = x, max = y in the vector

    glm::ivec3 findOptimalDimensions(int N);

    void updateMinMax();
};
}

#endif // VOLUME_GPU_VOLUME_H
