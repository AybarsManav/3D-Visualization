#pragma once
#include "render/ray.h"
#include "ui/trackball.h"
#include "render/render_config.h"
#include "volume/vf_volume.h"
#include "volume/volume.h"
#include <cstring> // memcmp
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gsl/span>
#include <memory>
#include <tuple>
#include <vector>

#ifdef __linux__
#include <GL/glew.h>
#else
#include <gl/glew.h>
#endif


namespace render {

class VectorRenderer {
public:
    VectorRenderer(const volume::Volume* pVolume,
        const volume::VectorFieldVolume* pVCVolume,
        const ui::Trackball* pCamera,
        const RenderConfig& config);

    void setConfig(const RenderConfig& config);
    void updateMatrices();
    void regenerateLineIntegrationBuffers();
    void linkShaderProgram(GLuint& shader, GLuint vertexShader, GLuint fragmentShader, GLuint geometryShader = 0);

    void render(GLuint tfTextureID);
    void setRenderSize(glm::ivec2 resolution);

protected:
    void renderBackground();
    void renderScalar();
    void renderLic();
    void renderHedgehogs();
    void renderLines();

    void appendArrow(const glm::vec2& position, const glm::vec2& direction);
    
    void updateHedgehogs();
    void updateLines();

    volume::Texture generateNoiseTexture() const;

    void integrateLine(const glm::vec2 seed, const float curTimeStep, const float stepSize, const float stepSizeTime, std::vector<float>& outLine);

    std::vector<glm::vec2> getStreamLines(const glm::vec2& seed, const float& curTimeStep, const float& stepSize, const float& stoppingMagnitude, const int& maxSteps, const LineMethod& method) const;

    std::vector<glm::vec2> getPathLines(const glm::vec2& seed, float curTimeStep, const float& stepSize, const float& numStepsTime, const int& maxSteps, const LineMethod& method) const;

    bool outOfBound(const glm::vec3& pos) const;

    glm::ivec2 renderResolution;

    glm::mat4 modelMatrix;
    glm::mat4 viewProjectionMatrix;

    const volume::Volume* m_pVolume;
    const volume::VectorFieldVolume* m_pVectorVolume;
    const ui::Trackball* m_pCamera;
    RenderConfig m_config {};

    GLuint m_vao;
    GLuint m_vao_triangles, m_vbo_triangles, m_ebo_triangles;
    std::vector<GLuint> m_vao_lines, m_vbo_lines, m_ebo_lines;

    GLuint m_scalar_shader;
    GLuint m_hedgehog_shader;
    GLuint m_field_line_shader;
    GLuint m_lic_shader;

    GLuint m_vc_texture;
    GLuint m_colormap_texture;
    GLuint m_noise_texture;

    RenderConfig_VectorField m_renderConfig;
    
    std::vector<float> m_arrow_vertices;
    std::vector<unsigned int> m_arrow_indices;

    int m_numLines;
    std::vector<std::vector<float>> m_line_vertices;
};

}
