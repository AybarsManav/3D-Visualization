#pragma once
#include "ui/opengl.h"
#include "ui/trackball.h"
#include "render/render_config.h"
#include "render/gpu_mesh_config.h"
#include "volume/gpu_volume.h"
#include "volume/volume.h"
#include "volume/gradient_volume.h"


namespace render {


class GPURenderer {
public:
    GPURenderer(
        volume::GPUVolume* pGPUVolume,
        const volume::Volume* m_pVolume,
        const volume::GradientVolume* pGradientVolume,
        const ui::Trackball* pCamera,
        const RenderConfig& config,
        const GPUMeshConfig& meshConfig);

    void updateGPUMesh(bool updateMinMax);

    void setRenderConfig(const RenderConfig& config);
    void setMeshConfig(const GPUMeshConfig& config);

    // blocking / empty space skipping
    void updateBlockingMinMaxTable();    
    void updateActiveBlocks();
    void updateOpacitySumTable();

    // bricking
    void updateVolumeBricks();
    void setVolumeBricksSize();

    void setRenderSize(glm::ivec2 resolution);

    void render();


private:
    void updateMatrices();

    void drawGeometry(GLuint shaderID);
    void renderTextureToScreen(GLuint texture);

    void renderDirections();

    void renderIso();
    void renderMIP();
    void renderComposite();

    void linkShaderProgram(GLuint& shader, GLuint vertexShader, GLuint fragmentShader);

private:
    glm::ivec2 m_renderResolution;

    glm::mat4 m_modelMatrix;
    glm::mat4 m_viewProjectionMatrix;

    volume::GPUVolume* m_pGPUVolume;
    const volume::Volume* m_pVolume;
    const volume::GradientVolume* m_pGradientVolume;
    const ui::Trackball* m_pCamera;
    RenderConfig  m_renderConfig {};
    GPUMeshConfig m_meshConfig {};

    GLuint m_ibo, m_vbo, m_vao, m_fbo;
    GLuint m_quadVAO, m_quadVBO;
    GLuint m_facesShader, m_isoShader, m_mipShader, m_compositeShader, m_screenFillingQuadShader;
    GLuint m_backfaces_texture;
    GLuint m_frontfaces_texture;
    GLuint m_depthTexture;

    GLuint positionsBufferID, positionsTexID;
    GLuint m_blockActiveBufferID, m_blockActiveTexID;

    glm::vec3 m_numBlocks3D;
    std::vector<glm::vec3> m_positions;
    std::vector<int> m_blockActive;
    std::vector<glm::vec2> m_minMaxValues; // min = x, max = y in the vector
    std::array<float, 256> m_opacitySumTable;
};
}
