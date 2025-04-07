#define GLM_SWIZZLE 
#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "vf_renderer.h"
#include "renderer.h"
#include "ui/opengl.h"
#include <algorithm>
#include <exception>
#include <random>
#include <cmath>
#include <functional>
#include <iostream>
#include <tuple>

namespace render {
VectorRenderer::VectorRenderer(const volume::Volume* pVolume,
    const volume::VectorFieldVolume* pVCVolume,
    const ui::Trackball* pCamera,
    const RenderConfig& initialConfig)
    : m_pVolume(pVolume)
    , m_pVectorVolume(pVCVolume)
    , m_pCamera(pCamera)
    , m_config(initialConfig)
    , m_colormap_texture(0)
    , m_numLines(1)
{
    // Vertex Shader is identical for all modes
    GLuint vertexShader = loadShader("vector_field_vert.glsl", GL_VERTEX_SHADER);
    GLuint simpleGeometryFragmentShader = loadShader("vector_field_simple_geometry_frag.glsl", GL_FRAGMENT_SHADER);

    // Link the shared Vertex Shader with Fragment shaders
    linkShaderProgram(m_scalar_shader, vertexShader, loadShader("vector_field_scalar_frag.glsl", GL_FRAGMENT_SHADER));
    linkShaderProgram(m_hedgehog_shader, vertexShader, simpleGeometryFragmentShader);
    linkShaderProgram(m_field_line_shader, vertexShader, simpleGeometryFragmentShader, loadShader("vector_field_line_geom.glsl", GL_GEOMETRY_SHADER));
    linkShaderProgram(m_lic_shader, vertexShader, loadShader("vector_field_line_integral_convolution_frag.glsl", GL_FRAGMENT_SHADER));

    // create basic geometry for screen filling quad
    float vertices[] = {
        // positions    // texture coords
        1.f, 1.f,       1.0f, 1.0f, // top right
        1.f, 0.f,       1.0f, 0.0f, // bottom right
        0.f, 0.f,       0.0f, 0.0f, // bottom left
        0.f, 1.f,       0.0f, 1.0f // top left
    };
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3 // second triangle
    };

    // enable blending for opacity blending of multiple modes (e.g., scalar on top of LIC)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Generate buffers
    unsigned int VBO, EBO;
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);    

    glBindVertexArray(0);

    // Create the texture for the vector data. We use a 3D texture for x+y+time
    glm::ivec3 dims = m_pVolume->dims();
    std::vector<glm::vec4> texData = m_pVectorVolume->getData();
    
    volume::Texture textureData(texData, dims);
    m_vc_texture = textureData.getTexId();

    // initialize the noise texture for LIC
    volume::Texture noiseTexture = generateNoiseTexture();
    m_noise_texture = noiseTexture.getTexId();

    // Create buffers for Hedgehogs
    glGenVertexArrays(1, &m_vao_triangles);
    glGenBuffers(1, &m_vbo_triangles);
    glGenBuffers(1, &m_ebo_triangles);

    m_arrow_vertices.clear();
    m_arrow_indices.clear();

    // Create buffers for Fieldlines
    regenerateLineIntegrationBuffers();

    glLineWidth(5.0f);
    glEnable(GL_LINE_SMOOTH);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Sets the render configuration from the GUI
void VectorRenderer::setConfig(const RenderConfig& config)
{
    m_config = config;
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Update the model view and projection matrices for the quad rendering
void VectorRenderer::updateMatrices()
{
    modelMatrix = glm::scale(glm::identity<glm::mat4>(), glm::vec3(m_pVolume->dims()));
    const glm::mat4 viewMatrix = m_pCamera->viewMatrix();
    const glm::mat4 projectionMatrix = m_pCamera->projectionMatrix();
    viewProjectionMatrix = projectionMatrix * viewMatrix * modelMatrix;
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Updates the geometry buffers for a variable number of field lines
void VectorRenderer::regenerateLineIntegrationBuffers()
{
    // free existing buffers
    for(int i = 0; i < m_vbo_lines.size(); i++) {
        glDeleteBuffers(1, &m_vbo_lines[i]);
    }
    for(int i = 0; i < m_ebo_lines.size(); i++) {
        glDeleteBuffers(1, &m_ebo_lines[i]);
    }
    for(int i = 0; i < m_vao_lines.size(); i++) {
        glDeleteVertexArrays(1, &m_vao_lines[i]);
    }

    // Create buffers for Fieldlines    
    m_vao_lines.resize(m_numLines);
    m_vbo_lines.resize(m_numLines);
    m_ebo_lines.resize(m_numLines);

    for (size_t i=0; i < m_numLines; i++) {
        glGenVertexArrays(1, &(m_vao_lines[i]));
        glGenBuffers(1, &(m_vbo_lines[i]));
        glGenBuffers(1, &(m_ebo_lines[i]));
    }

    m_line_vertices.resize(m_numLines);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Initialize the noise texture for the LIC shader
volume::Texture VectorRenderer::generateNoiseTexture() const
{
    // Create a random generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    // Prepare a buffer for the noise data
    glm::ivec3 dims = m_pVectorVolume->getDims();
    std::vector<float> noiseImage(dims.x * dims.y);

    // Fill the buffer with random values
    for (auto& pixel : noiseImage) {
        pixel = dis(gen);
    }

    // create the texture
    return volume::Texture(noiseImage, glm::ivec3(dims.x, dims.y, 0));
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Called to render a frame, passes the call along to the correct render method after some shared setup  
void VectorRenderer::render(GLuint tfTextureID)
{
    // We give the texture ID for the transferfunction to avoid adding it to the renderconfig 
    m_colormap_texture = tfTextureID;

    // update the model view projection
    updateMatrices();

    // clear the screen
    glClear(GL_COLOR_CLEAR_VALUE);
    
    // First we render the background using scaler and/or LIC
    renderBackground();


    // Then we render the hedgehogs on top
    if (m_config.vectorFieldParameters.geometryOverlay == render::VectorGeometry::Hedgehog) {
        updateHedgehogs();
        renderHedgehogs();
    }
    // Or we render the field lines
    if (m_config.vectorFieldParameters.geometryOverlay == render::VectorGeometry::Integration) {
        updateLines();
        renderLines();
    }
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// We render the Background using 
void VectorRenderer::renderBackground()
{
    if(m_config.vectorFieldParameters.isLicActive)
    {
        renderLic();
    }

    // Note this is not an else part by design.
    // We can render the scalar field on top with the opacity provided throug the GUI to blend scalar and LIC
    if(m_config.vectorFieldParameters.isScalarActive)
    {
        renderScalar();
    }
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Render call for the LIC renderer
// Uses the vector_field_scalar_frag.glsl fragment shader for the actual visualization
void VectorRenderer::renderScalar()
{
    glUseProgram(m_scalar_shader);

    // Pass Textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_vc_texture);
    glUniform1i(glGetUniformLocation(m_scalar_shader, "vfTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_colormap_texture);
    glUniform1i(glGetUniformLocation(m_scalar_shader, "tfTexture"), 1);

    glBindVertexArray(m_vao);
    
    // Uniform for MVP
    glUniformMatrix4fv(glGetUniformLocation(m_scalar_shader, "u_modelViewProjection"), 1, false, glm::value_ptr(viewProjectionMatrix));

    // Uniforms to tell the shader which components are active in the GUI
    glm::vec4 vectorComponents = glm::vec4(m_config.vectorFieldParameters.vectorComponents, 0.0f);
    glUniform4fv(glGetUniformLocation(m_scalar_shader, "vectorComponents"), 1, glm::value_ptr(vectorComponents));  

    glm::vec4 scalarComponents = glm::vec4(m_config.vectorFieldParameters.scalarComponents, 0.0f, 0.0f);
    glUniform4fv(glGetUniformLocation(m_scalar_shader, "scalarComponents"), 1, glm::value_ptr(scalarComponents));

    // timestep is normalized to the volume depth
    float timestep = (m_config.vectorFieldParameters.timeStep + 0.5f) / m_config.vectorFieldParameters.volumeDims.z;
    // timestep and alpha value for the compositing of the scalar method on top of LIC
    glUniform4f(glGetUniformLocation(m_scalar_shader, "visualProperties"), timestep, m_config.vectorFieldParameters.scalarOverlayAlpha, 1.0, 1.0);

    // rendering happens drawing a quad and representing the 2D field
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // disable buffers and shader
    glBindVertexArray(0);
    glUseProgram(0);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Render call for the scalar renderer
// Uses the vector_field_line_integral_convolution_frag.glsl fragment shader for the actual visualization
void VectorRenderer::renderLic()
{
    glUseProgram(m_lic_shader);

    // Pass Textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_vc_texture);
    glUniform1i(glGetUniformLocation(m_lic_shader, "vfTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_noise_texture);
    glUniform1i(glGetUniformLocation(m_lic_shader, "noiseTexture"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_colormap_texture);
    glUniform1i(glGetUniformLocation(m_lic_shader, "tfTexture"), 2);

    
    glBindVertexArray(m_vao);
    
    // uniform for MVP
    glUniformMatrix4fv(glGetUniformLocation(m_lic_shader, "u_modelViewProjection"), 1, false, glm::value_ptr(viewProjectionMatrix));

    // uniform containing the active normalized time step in the GUI, the inverse volume dimensions (all three for accesing the 3D texture), and the kernel width
    glm::ivec3 volumeDims = m_config.vectorFieldParameters.volumeDims;
    // We add 0.5 to the time step to center the sample in the voxel
    float timestep = (m_config.vectorFieldParameters.timeStep + 0.5f) / volumeDims.z;
    glUniform4f(glGetUniformLocation(m_lic_shader, "visualProperties"), timestep, 1.0f / volumeDims.x, 1.0f / volumeDims.y, m_config.vectorFieldParameters.licKernelWidth);

    // rendering happens drawing a quad and representing the 2D field
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // disable buffers and shader
    glBindVertexArray(0);
    glUseProgram(0);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Render call for the hedgehog/arrow glyph renderer 
void VectorRenderer::renderHedgehogs()
{
    glUseProgram( m_hedgehog_shader);

    // Glyphs are created on the CPU and uploaded to the m_vao_triangles buffer
    glBindVertexArray(m_vao_triangles);

    // Uniform for MVP
    glUniformMatrix4fv(glGetUniformLocation(m_hedgehog_shader, "u_modelViewProjection"), 1, false, glm::value_ptr(viewProjectionMatrix));

    // Uniform for color
    glUniform4f(glGetUniformLocation(m_hedgehog_shader, "color"), m_renderConfig.geoColor.x, m_renderConfig.geoColor.y, m_renderConfig.geoColor.z, m_renderConfig.geoColor.w);
    
    // draw the arrow geometry using the index buffer
    glDrawElements(GL_TRIANGLES, m_arrow_indices.size(), GL_UNSIGNED_INT, 0);

    // disable buffers and shader
    glBindVertexArray(0);
    glUseProgram(0);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Render call for the field line renderer for one line at a time
// Note rendering the lines one by one is not the most efficien way to do this, but it is simple and works for the purpose of this assignment
void VectorRenderer::renderLines()
{
    glUseProgram(m_field_line_shader);

    // Uniform for MVP
    glUniformMatrix4fv(glGetUniformLocation(m_field_line_shader, "u_modelViewProjection"), 1, false, glm::value_ptr(viewProjectionMatrix));

    // Uniform for color
    glUniform4f(glGetUniformLocation(m_field_line_shader, "color"), m_renderConfig.geoColor.x, m_renderConfig.geoColor.y, m_renderConfig.geoColor.z, m_renderConfig.geoColor.w);

    // Uniform for data size
    glUniform4f(glGetUniformLocation(m_field_line_shader, "visualParams"), m_pVolume->dims().x, m_pVolume->dims().y, m_renderConfig.lineWidth * 0.5f, 0.0f);

    for (size_t i=0; i < m_numLines; i++) {

        // Bind the correct vertex array
        glBindVertexArray(m_vao_lines[i]);

        // draw the arrow geometry
        glDrawArrays(GL_LINE_STRIP, 0, m_line_vertices[i].size() / 2);
    }

    // disable buffers and shader
    glBindVertexArray(0);
    glUseProgram(0);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// This function sets up the field line integration and rendering.
void VectorRenderer::updateLines()
{
    bool linesEmpty = true;
    for (size_t line=0; line < m_numLines; line++) {
        if (m_line_vertices[line].size() != 0) {
            linesEmpty = false;
            break;
        }
    }

    // We only want to update the field lines if the parameters have changed or if there is nothing to draw yet
    if ( m_renderConfig.fieldLinesNeedUpdate(m_config.vectorFieldParameters) || linesEmpty )  {

        // get the parameterts from the GUI
        m_renderConfig = m_config.vectorFieldParameters;

        // If the number of lines changed we need to refresh the GPU buffers
        if(m_numLines != m_renderConfig.numLines) {
            m_numLines = m_renderConfig.numLines;
            regenerateLineIntegrationBuffers();
        }

        // vector representing the seed line
        glm::vec2 seedLine = m_renderConfig.seedPoint.zw() - m_renderConfig.seedPoint.xy();
        // vector between the seed points
        glm::vec2 segment = seedLine / (m_numLines > 1 ? static_cast<float>(m_numLines - 1) : 1.0f);

        // we iterate over all lines
        for (size_t line=0; line < m_numLines; line++) {

            // calculate the starting position for the line based on the seed point and the distance 
            glm::vec2 lineStartingPosition = m_renderConfig.seedPoint.xy() + static_cast<float>(line) * segment;
            // bring into pixel coordinates
            lineStartingPosition *= glm::vec2(m_pVectorVolume->getDims().xy());

            // we set the step size to 0 for streamlines and according to the GUI for pathlines
            float stepSizeTime = m_renderConfig.mode == LineMode::VF_StreamLines ? 0.0f : 1.0f / m_renderConfig.numStepsTime;

            // integrate the line and save the vertice to pointLists
            integrateLine(lineStartingPosition, m_renderConfig.timeStep, m_renderConfig.stepSize, stepSizeTime, m_line_vertices[line]);

            // update the GPU buffers
            glBindVertexArray(m_vao_lines[line]);

            // upload the data to the GPU
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo_lines[line]);
            glBufferData(GL_ARRAY_BUFFER, m_line_vertices[line].size() * sizeof(float), m_line_vertices[line].data(), GL_STATIC_DRAW);

            // position attribute
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            // unbind the buffers
            glBindVertexArray(0);
        }
    }
}

// ======= TODO: IMPLEMENT ========
//
// Assignment 3, Part 2: Field Lines / Streamlines and Pathlines
//
// Implement the vector field integration for streamlines and pathlines here
// Use Euler and Runge-Kutta 2 integration methods depending on the acitve method in the GUI (test m_renderConfig.method)
//
// The parameters are given as follows:
// - seed: The starting position for the line in pixel coordinates
// - startTimeStep: The current timestep set in the GUI.
//   This is the timestep used for the complete line integration for Streamlines and the starting timestep for Pathlines
// - stepSize: The step size in pixels for the spatial integration
// - stepSizeTime: The step size in timesteps for the temporal integration. Set to 0 for streamlines
// - line: Vector to store the result. Should contain all vertices of the final line in order and in normalized coordinates
//
// You can get all other parameters from the m_renderConfig struct
// m_renderConfig.maxSteps, m_renderConfig.stoppingMagnitude, m_renderConfig.method
// You can get the interpolated vector from the volume using
// m_pVectorVolume->getVectorDirectionInterpolated(x, y, t);
void VectorRenderer::integrateLine(const glm::vec2 seed, const float startTimeStep, const float stepSize, const float stepSizeTime, std::vector<float>& line)
{
    // clear the data and reserve new memory
    line.clear();
    line.reserve(m_renderConfig.maxSteps * 2);

    glm::vec2 pos = seed;

    // push back the seed position, note: we want normalized positions in the vertex buffer
    glm::ivec3 dims = m_pVectorVolume->getDims();
    line.push_back(pos.x / dims.x);
    line.push_back(pos.y / dims.y);


    // you need to advance the position according to the vector field
    // replace this code by your loop over the integration steps.
    pos.x += 50;
    pos.y += 50;

    // push back the new position
    line.push_back(pos.x / dims.x);
    line.push_back(pos.y / dims.y);
}

// ======= TODO: IMPLEMENT ========
//
// Assignment 3, Part 1: Hedgehogs / Glyph-based Vector Field Visualization
//
// Creates the geometry for a simple arrow glyph
//
// Implement some suitable geometry for the arrow glyph here
// How complex the geometry should be is up to you, but make sure that it conveys all information to properly read the vector field
// Use m_arrow_vertices to store the vertices and m_arrow_indices to store the indices of the geometry
// The position parameter is the position of the sampled vector in pixel space
// The direction parameter is the sampled vector at the position in pixel space
//
// The geometry should be centered at the position and point in the direction given as parameters
// The length of the arrow should be scaled by the hedgehogBaseLength from the GUI
// The dimensions of the vector field in pixels (x,y) for the 2D spatial part can be obtained using m_pVectorVolume->getDims()
// The geometry should be stored in the m_arrow_vertices and m_arrow_indices vectors
//
// The provided sample code creates a simple quad that is centered at the position and ignores the direction
// It creates the four corners of the quad as vertices and then creates two triangles by adding the indices to the m_arrow_indices vector
// Note how index 1 and 2 are shared between the two triangles and thus added twice to m_arrow_indices
void VectorRenderer::appendArrow(const glm::vec2& position, const glm::vec2& direction)
{
    // The base length to scale the arrow with from the GUI
    float length = m_renderConfig.hedgehogBaseLength;

    // The dimensions of the vector field in pixels (x,y) for the 2D spatial part
    glm::ivec3 dims = m_pVectorVolume->getDims();

    // we create a simple quad here that will be rendered at position ignoring the actual vector information
    // You will need to modify this code to create somewhat more sophisticated geometry and orient and scale it correctly

    // we get the offset into the vertex buffer before adding to at corresponding indices to the index buffer, divide by 2 as there are two values per vertex (x,y)
    int index_base = m_arrow_vertices.size() / 2;

    // Create four vertices of a quad sized 1x4 centered on position
    m_arrow_vertices.push_back((position.x - 0.5)/dims.x);
    m_arrow_vertices.push_back((position.y - 0.5 * length)/dims.y);
    
    m_arrow_vertices.push_back((position.x + 0.5)/dims.x);
    m_arrow_vertices.push_back((position.y - 0.5 * length)/dims.y);
    
    m_arrow_vertices.push_back((position.x - 0.5)/dims.x);
    m_arrow_vertices.push_back((position.y + 0.5 * length)/dims.y);
    
    m_arrow_vertices.push_back((position.x + 0.5)/dims.x);
    m_arrow_vertices.push_back((position.y + 0.5 * length)/dims.y);

    // We use indexed rendering so that we can reused the shared vertices of the quad defined above for two triangles
    // first triangle of quad
    m_arrow_indices.push_back(index_base + 0);
    m_arrow_indices.push_back(index_base + 1);
    m_arrow_indices.push_back(index_base + 2);

    // second triangle of quad
    m_arrow_indices.push_back(index_base + 1);
    m_arrow_indices.push_back(index_base + 3);
    m_arrow_indices.push_back(index_base + 2);
}

// ======= TODO: IMPLEMENT ========
//
// Assignment 3, Part 1: Hedgehogs / Glyph-based Vector Field Visualization
//
// This function is called when the hedgehog geometry needs to be updated
// Implement a sampling loop here, that samples the vector field according to the hedgehogSampling parameter from the GUI
// The hedgehogSampling parameter defines the distance between the hedgehogs in pixels
// The arrow glyphs should be scaled according to the hedgehogFixedSize parameter from the GUI
// If hedgehogFixedSize is true, the arrows should all have the same size otherwise the arrows should be scaled according to the vector magnitude
// You can do this either here, by giving the appendArrow function the correctly scaled direction, or in the appendArrow function
void VectorRenderer::updateHedgehogs()
{
    // Avoid update if not needed
    if ( m_renderConfig.hedgehogsNeedUpdate(m_config.vectorFieldParameters) || m_arrow_indices.size() == 0) {

        m_renderConfig = m_config.vectorFieldParameters;

        // The distance between the hedgehogs in pixels from the GUI
        int samplingDistance = m_renderConfig.hedgehogSampling;
        // Use a fixed length or scale according to the vector magnitude
        bool fixedSize = m_renderConfig.hedgehogFixedSize;

        // The current timestep in the vector field needed to get the actual vector using getVectorDirectionInterpolated()
        float curTime = static_cast<float>(m_renderConfig.timeStep);

        // We clear the old geometry. Consider reserving memory according to the size of geometry you create and number of expected samples.
        m_arrow_vertices.clear();
        m_arrow_indices.clear();

        glm::ivec3 dims = m_pVectorVolume->getDims();

        // TODO:
        // Set up loop(s) to go over the vector field and sample the vectors according to the hedgehogSampling parameter
        // Use the appendArrow function to create the geometry for the hedgehogs
        appendArrow(glm::vec2(dims.x/2, dims.y/2), glm::vec2(0.5,0.5));
        // Bind the buffer array
        glBindVertexArray(m_vao_triangles);

        // Update the vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_triangles);
        glBufferData(GL_ARRAY_BUFFER, m_arrow_vertices.size() * sizeof(float), m_arrow_vertices.data(), GL_STATIC_DRAW);

        // Update the index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo_triangles);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_arrow_indices.size() * sizeof(unsigned int), m_arrow_indices.data(), GL_STATIC_DRAW);

        // ======= Optional ========
        // If you want to add additional information to the geometry you might want to adjust and add vertex attribute pointers here

        // We set the position attribute starting at 0 for two elements in the vertex array
        // If you want to add a second attribute in the same vertex buffer you need to change the stride here and set a second attribute pointer
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Unbind the buffer array
        glBindVertexArray(0);
    }
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// helper to test whether a position is inside the domain
bool VectorRenderer::outOfBound(const glm::vec3& pos) const
{
    glm::ivec3 dims = m_pVectorVolume->getDims();
    if (glm::any(glm::lessThan(pos, glm::vec3(0.0f))) || glm::any(glm::greaterThanEqual(pos, glm::vec3(dims)))) {
        return true;
    }

    return false;
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// sets the render resolution for the offscreen textures
void VectorRenderer::setRenderSize(glm::ivec2 resolution)
{
    renderResolution = resolution;
}


// ======= DO NOT MODIFY THIS FUNCTION ========
// Helper function to simplify shader creation
void VectorRenderer::linkShaderProgram(GLuint& shader, GLuint vertexShader, GLuint fragmentShader, GLuint geometryShader)
{
    shader = glCreateProgram();
    glAttachShader(shader, vertexShader);
    if(geometryShader != 0) glAttachShader(shader, geometryShader);
    glAttachShader(shader, fragmentShader);
    glLinkProgram(shader);

    glDetachShader(shader, vertexShader);
    if(geometryShader != 0) glDetachShader(shader, geometryShader);
    glDetachShader(shader, fragmentShader);
}

}