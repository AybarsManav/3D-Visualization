#include "gpu_renderer.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/component_wise.hpp>
#include <iostream>


namespace render {

GPURenderer::GPURenderer(
    volume::GPUVolume* pGPUVolume,
    const volume::Volume* pVolume,
    const volume::GradientVolume* pGradientVolume,
    const ui::Trackball* pCamera,
    const RenderConfig& config,
    const GPUMeshConfig& meshConfig)
    : m_pGPUVolume(pGPUVolume)
    , m_pVolume(pVolume)
    , m_pGradientVolume(pGradientVolume)
    , m_pCamera(pCamera)
    , m_renderConfig(config)
    , m_meshConfig(meshConfig)
    , m_positions(std::vector<glm::vec3>())
    , m_blockActive(std::vector<int>())
    , m_minMaxValues(std::vector<glm::vec2>())

{
    // The general framebuffer with depth component
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // the following textures are all render buffer targets so we do not add data
    // Create a texture for the depth buffer
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //Create textures for ray parameters, cube positions and cube min max values
    glGenTextures(1, &m_frontfaces_texture);
    glBindTexture(GL_TEXTURE_2D, m_frontfaces_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &m_backfaces_texture);
    glBindTexture(GL_TEXTURE_2D, m_backfaces_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Attach the depth buffer to the framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);

    // Attach the existing color texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frontfaces_texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Buffers to store block position offesets and block active state for empty space skipping
    // We use GL_TEXTURE_BUFFER instead of GL_TEXTURE_1D to avoid running into size limitations
    glGenBuffers(1, &positionsBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, positionsBufferID);
    glBufferData(GL_TEXTURE_BUFFER, m_positions.size() * sizeof(glm::vec3), m_positions.data(), GL_DYNAMIC_DRAW);
    glGenTextures(1, &positionsTexID);
    glBindTexture(GL_TEXTURE_BUFFER, positionsTexID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, positionsBufferID);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    glGenBuffers(1, &m_blockActiveBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, m_blockActiveBufferID);
    glBufferData(GL_TEXTURE_BUFFER, m_blockActive.size() * sizeof(int), m_blockActive.data(), GL_DYNAMIC_DRAW);
    glGenTextures(1, &m_blockActiveTexID);
    glBindTexture(GL_TEXTURE_BUFFER, m_blockActiveTexID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, m_blockActiveBufferID);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    // Vertex Shader for rendering the cube geometry
    GLuint renderCubesVertexShader = loadShader("gpu_optimization_vert.glsl", GL_VERTEX_SHADER);
    // Setup shaders
    linkShaderProgram(m_facesShader, renderCubesVertexShader, loadShader("volvis_colorcube_frag.glsl", GL_FRAGMENT_SHADER));

    // Vertex Shader for rendering a screen filling quad
    GLuint screenFillingQuadVertexShader = loadShader("volvis_screen_filling_quad_vert.glsl", GL_VERTEX_SHADER);
    linkShaderProgram(m_screenFillingQuadShader, screenFillingQuadVertexShader, loadShader("volvis_screen_filling_quad_frag.glsl", GL_FRAGMENT_SHADER));

    // Render Mode shaders
    linkShaderProgram(m_mipShader, screenFillingQuadVertexShader, loadShader("volvis_rendermode_mip_frag.glsl", GL_FRAGMENT_SHADER));
    linkShaderProgram(m_isoShader, screenFillingQuadVertexShader, loadShader("volvis_rendermode_isosurface_frag.glsl", GL_FRAGMENT_SHADER));
    linkShaderProgram(m_compositeShader, screenFillingQuadVertexShader, loadShader("volvis_rendermode_compositing_frag.glsl", GL_FRAGMENT_SHADER));

    // == geometry
    // screen filling quad
    const float quadVertices[] = {
        // Positions     // Texture Coords
        -1.0f,  1.0f,    0.0f, 1.0f,
        -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f, -1.0f,    1.0f, 0.0f,
    
        -1.0f,  1.0f,    0.0f, 1.0f,
         1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,    1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // single cube, can be used for instanced rendering in case of blocking
    const std::array vertices {
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 1.0f,
    };

    const std::array<unsigned, 36> indices {
        0, 6, 4,
        0, 2, 6,
        0, 3, 2,
        0, 1, 3,
        2, 7, 6,
        2, 3, 7,
        4, 6, 7,
        4, 7, 5,
        0, 4, 5,
        0, 5, 1,
        1, 5, 7,
        1, 7, 3
    };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glGenBuffers(1, &m_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    // set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // initialize the geometry
    updateGPUMesh(true);

    // initialize the summed up opacity table
    updateOpacitySumTable();
}

// ======= TODO: IMPLEMENT ========
//
// Part of **2. Empty Space Skipping**
// 
// This function should create the summed area table to help identifying active blocks quickly
// It will be updated every time the transfer function changes
// You can access the transfer function from the render config: m_renderConfig.tfColorMap
// Store the values in m_opacitySumTable 
void GPURenderer::updateOpacitySumTable()
{
    const std::array<glm::vec4, 256>& tf = m_renderConfig.tfColorMap;

    float sum = 0.0f;
    for (size_t i = 0; i < tf.size(); ++i) {
        sum += tf[i].a; // Accumulate opacity
        m_opacitySumTable[i] = sum;
    }
}

// ======= TODO: IMPLEMENT ========
//
// Part of **2. Empty Space Skipping**
//
// This function should calculate the min and max values for the volume blocks
// It will be updated when the volume is loaded or the block size changes
// You can access the volume using m_pVolume like in the CPU renderer
void GPURenderer::updateBlockingMinMaxTable()
{
    glm::ivec3 volumeDims = m_pVolume->dims();
    const int blockSize = m_meshConfig.blockSize;
    // If empty space skipping is disabled we will only have a single, active block
    // this contains the number of blocks in 3D
    // you need to update m_numBlocks3D with the correct values for this to work
    if (!m_meshConfig.useEmptySpaceSkipping) {

        m_numBlocks3D = glm::vec3(1);

    } else { // Compute number of blocks in each dim given a block size 

        m_numBlocks3D = glm::ivec3(
            (volumeDims.x + blockSize - 1) / blockSize,
            (volumeDims.y + blockSize - 1) / blockSize,
            (volumeDims.z + blockSize - 1) / blockSize);

    }

    int numBlocks = m_numBlocks3D.x * m_numBlocks3D.y * m_numBlocks3D.z;

    // you should save the offset to all blocks in 3D voxel coordinates this as glm::vec3
    // e.g., [ (0,0,0), (blocksize, 0, 0), (2*blocksize, 0, 0) ... (0, blocksize, 0) ... etc. ]
    m_positions.clear();
    m_minMaxValues.clear();

    if (m_meshConfig.useEmptySpaceSkipping) {
        m_positions.reserve(numBlocks);
        m_minMaxValues.reserve(numBlocks);

        // Loop through each and every block
        for (int z = 0; z < m_numBlocks3D.z; ++z) {
            for (int y = 0; y < m_numBlocks3D.y; ++y) {
                for (int x = 0; x < m_numBlocks3D.x; ++x) {
                    glm::ivec3 voxelOffset = glm::ivec3(x, y, z) * blockSize; // The starting voxel index in a certain block
                    glm::ivec3 blockEnd = glm::min(voxelOffset + glm::ivec3(blockSize), volumeDims); // Guarantee not going out of borders

                    float minVal = std::numeric_limits<float>::max();
                    float maxVal = std::numeric_limits<float>::lowest();

                    // Find the max and min values in each block
                    for (int bz = voxelOffset.z; bz < blockEnd.z; ++bz) {
                        for (int by = voxelOffset.y; by < blockEnd.y; ++by) {
                            for (int bx = voxelOffset.x; bx < blockEnd.x; ++bx) {
                                float val = m_pVolume->getVoxel(bx, by, bz);
                                minVal = std::min(minVal, val);
                                maxVal = std::max(maxVal, val);
                            }
                        }
                    }

                    m_positions.push_back(glm::ivec3(x, y, z)); // Apparently, this has to be directly the block index
                    m_minMaxValues.push_back(glm::vec2(minVal, maxVal));
                }
            }
        }
    } else { // Empty voxel skipping is not on
        // we set the corresponding min-max to 0 and volume max to make sure the block is always visible
        m_minMaxValues.reserve(numBlocks);
        m_minMaxValues.push_back(glm::vec2(0, m_pVolume->maximum()));
        // Push the origin position
        m_positions.reserve(numBlocks);
        m_positions.push_back(glm::vec3(0, 0, 0));
    }

    // after updating the positions we load them to the GPU
    glBindBuffer(GL_TEXTURE_BUFFER, positionsBufferID);
    glBufferData(GL_TEXTURE_BUFFER, m_positions.size() * sizeof(glm::vec3), m_positions.data(), GL_DYNAMIC_DRAW);
}

// ======= TODO: IMPLEMENT ========
//
// Part of **2. Empty Space Skipping**
//
// This function should calculate whether a block is active or not
// It will be updated whenever the volume is loaded or the block size changes
// and when the transfer function or iso value changes
// You need to acces the m_opacitySumTable for the compositing mode.
// This works the same way as accessing the TF in the CPU renderer. Have a look at the getTFValue in renderer.cpp
void GPURenderer::updateActiveBlocks()
{
    // this vector simply should contain 1 if the block is active and 0 otherwise
    // we resize it to the size of the positions vector (both are the size of the number of blocks)
    m_blockActive.resize(m_positions.size());

    // for the single cube case
    if (m_blockActive.size() == 1) {
        //  we set the active state to 1 to make it active
        m_blockActive[0] = 1;

    } else { // Empty space skipping is active
        RenderMode mode = m_renderConfig.renderMode;
        float isoValue = m_renderConfig.isoValue;

        for (size_t i = 0; i < m_positions.size(); ++i) {
            // Find minimum and maximum values (they are in [0, 255])
            float minVal = m_minMaxValues[i].x;
            float maxVal = m_minMaxValues[i].y;

            if (mode == RenderMode::RenderIso) {
                // In isosurface rendering, we are only interested in whether a block has an interval including the isovalue
                // and this is the criterion for setting a block active
                m_blockActive[i] = (isoValue >= minVal && isoValue <= maxVal) ? 1 : 0;

            } else if (mode == RenderMode::RenderComposite) {
                // Use summed opacity table to check if any opacity exists in [minVal, maxVal]
                int minIdx = static_cast<int>(minVal);
                int maxIdx = static_cast<int>(maxVal);

                minIdx = glm::clamp(minIdx, 0, 255);
                maxIdx = glm::clamp(maxIdx, 0, 255);

                float opacitySum = m_opacitySumTable[maxIdx] - (minIdx > 0 ? m_opacitySumTable[minIdx - 1] : 0.0f);
                m_blockActive[i] = (opacitySum > 0.0f) ? 1 : 0;
            } else {
                // Default fallback: always active
                m_blockActive[i] = 1;
            }
        }
    }

    // this works for any m_blockActive size
    // after updating the positions we load them to the GPU
    glBindBuffer(GL_TEXTURE_BUFFER, m_blockActiveBufferID);
    glBufferData(GL_TEXTURE_BUFFER, m_blockActive.size() * sizeof(int), m_blockActive.data(), GL_DYNAMIC_DRAW);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// updates the full blocking info
void GPURenderer::updateGPUMesh(bool updateMinMax)
{
    if(updateMinMax) {
        updateBlockingMinMaxTable();
    }
    updateOpacitySumTable();
    updateActiveBlocks();
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// sets the render configuration
void GPURenderer::setRenderConfig(const RenderConfig& config)
{
    m_renderConfig = config;
    updateGPUMesh(false);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// sets the blocking configuration
void GPURenderer::setMeshConfig(const GPUMeshConfig& config)
{
    m_meshConfig = config;
    updateGPUMesh(true);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// sets the render resolution for the offscreen textures
void GPURenderer::setRenderSize(glm::ivec2 resolution)
{
    m_renderResolution = resolution;
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// update the model view and projection matrices for the cube rendering
void GPURenderer::updateMatrices()
{
    m_modelMatrix = glm::scale(glm::identity<glm::mat4>(), glm::vec3(m_pVolume->dims()));
    const glm::mat4 viewMatrix = m_pCamera->viewMatrix();
    const glm::mat4 projectionMatrix = m_pCamera->projectionMatrix();
    m_viewProjectionMatrix = projectionMatrix * viewMatrix * m_modelMatrix;
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// function for the off screen front faces and directions render passes
void GPURenderer::renderDirections()
{
    // Manage the depth buffer
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_renderResolution.x, m_renderResolution.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glBindTexture(GL_TEXTURE_2D, m_frontfaces_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_renderResolution.x, m_renderResolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);

    // Bind the framebuffer and attach textures
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frontfaces_texture, 0);

    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // === render the front faces
    // Render frontfaces into a texture
    glUseProgram(m_facesShader);
    drawGeometry(m_facesShader);

    // === render the back faces
    // Update texture for the directions
    glBindTexture(GL_TEXTURE_2D, m_backfaces_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_renderResolution.x, m_renderResolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);

    // Attach direction texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_backfaces_texture, 0);

    // Clear the depth buffer for the next render pass
    glClearDepth(0.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Render backfaces and extract direction and length of each ray
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_GREATER);
    
    drawGeometry(m_facesShader);

    // Restore depth clear value
    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LEQUAL);

    // Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// draws the actual bounding geometry
void GPURenderer::drawGeometry(GLuint shaderID)
{
    // texture buffers for the vertex shader to decide whether to keep a block
    glActiveTexture(GL_TEXTURE5); //We start from texture 5 to avoid overlapping textures 
    glBindTexture(GL_TEXTURE_BUFFER, positionsTexID);
    glUniform1i(glGetUniformLocation(shaderID, "positionCube"), 5);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_BUFFER, m_blockActiveTexID);
    glUniform1i(glGetUniformLocation(shaderID, "blockActive"), 6);

    // the size of each cube in normalized volume coordinates
    glUniform3fv(glGetUniformLocation(shaderID, "cubeSize"), 1, glm::value_ptr(1.0f / m_numBlocks3D));

    // give the matrices to the shaders
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "u_model"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "u_modelViewProjection"), 1, GL_FALSE, glm::value_ptr(m_viewProjectionMatrix));

    glClear(GL_COLOR_BUFFER_BIT);

    // we draw an instance of the cube for every item in m_positions
    // we can then handle cubes that are empty in the vertex shader by moving them out of the view so that they are automatically culled
    glBindVertexArray(m_vao);
    glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, m_positions.size());
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Draw a screenfilling quad either just textured with the front/backfaces, or as the actual volume rendering pass
void GPURenderer::renderTextureToScreen(GLuint texture)
{
    glUseProgram(m_screenFillingQuadShader);

    glBindVertexArray(m_quadVAO);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(m_screenFillingQuadShader, "u_texture"), 0);
    
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Called to render a frame, passes the call along to the correct render method after some shared setup  
void GPURenderer::render()
{
    // update the model view projection
    updateMatrices();

    // we render front faces and directions into textures here
    renderDirections();

    // when setting the gui to show front faces or directions we render the corresponding texture
    if (m_renderConfig.renderStep == 1) {
        renderTextureToScreen(m_frontfaces_texture);
    } else
    if (m_renderConfig.renderStep == 2) {
        renderTextureToScreen(m_backfaces_texture);
    } else // otherwise render according to render mode
    if (m_renderConfig.renderStep == 3) {
        if (m_renderConfig.renderMode == render::RenderMode::RenderIso) {
            renderIso();
        } else 
        if (m_renderConfig.renderMode == render::RenderMode::RenderMIP) {
            renderMIP();
        } else
        if (m_renderConfig.renderMode == render::RenderMode::RenderComposite) {
            renderComposite();
        }
    }
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// GPU implementation of a MIP raycaster
// This should be fully working.
// MIP always needs the whole volume so it does not work with blocking and bricking
// also have a look at the volvis_rendermode_mip_frag.glsl fragment shader file for the ray traversal on the GPU
void GPURenderer::renderMIP()
{
    glUseProgram(m_mipShader);

    // Pass textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_backfaces_texture);
    glUniform1i(glGetUniformLocation(m_mipShader, "backFaces"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_frontfaces_texture);
    glUniform1i(glGetUniformLocation(m_mipShader, "frontFaces"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, m_pGPUVolume->getTexId());
    glUniform1i(glGetUniformLocation(m_mipShader, "volumeData"), 2);

    // we bring the stepsize into normalized volume coordinates
    // first we need the max volume extent
    glm::vec3 volDims = m_pVolume->dims();
    float maxExtent = std::max(volDims.x, std::max(volDims.y, volDims.z));
    float stepSizeNorm = m_renderConfig.stepSize / maxExtent;
    glm::vec4 renderOptions = glm::vec4(stepSizeNorm, 1.0f / stepSizeNorm, 0.0f, 0.0f);
    glUniform4fv(glGetUniformLocation(m_mipShader, "renderOptions"), 1, glm::value_ptr(renderOptions));

    // the reciprocal of the volDims is the voxelSize in 0..1 space, the reciprocal of the maximum vol value eases GPU load
    glm::vec4 volumeInfo = glm::vec4(1.0f / volDims, 1.0f / m_pVolume->maximum());
    glUniform4fv(glGetUniformLocation(m_mipShader, "volumeInfo"), 1, glm::value_ptr(volumeInfo));

    glClear(GL_COLOR_BUFFER_BIT);

    // rendering happens drawing the screenfilling quad and using the front faces and direction as input
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ======= TODO: IMPLEMENT ========
// 
// Part of **3. Volume Bricking**
// 
// This function is largely complete.
// You only need to add further uniforms as needed.
// 
// GPU implementation of a iso-surface raycaster
void GPURenderer::renderIso()
{
        glUseProgram(m_isoShader);

        // Pass textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_backfaces_texture);
        glUniform1i(glGetUniformLocation(m_isoShader, "backFaces"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_frontfaces_texture);
        glUniform1i(glGetUniformLocation(m_isoShader, "frontFaces"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, m_pGPUVolume->getTexId());
        glUniform1i(glGetUniformLocation(m_isoShader, "volumeData"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_3D, m_pGPUVolume->getIndexTexId());
        glUniform1i(glGetUniformLocation(m_isoShader, "volumeIndexData"), 3);

        glm::vec3 volDims = m_pVolume->dims();

        // the reciprocal of the volDims is the voxelSize in 0..1 space
        glm::vec4 volumeInfo = glm::vec4(1.0f / volDims, m_pGPUVolume->useBricking());
        glUniform4fv(glGetUniformLocation(m_isoShader, "volumeInfo"), 1, glm::value_ptr(volumeInfo));

        // we bring the stepsize into normalized volume coordinates
        float maxExtent = std::max(volDims.x, std::max(volDims.y, volDims.z));
        glUniform4fv(glGetUniformLocation(m_isoShader, "renderOptions"), 1, glm::value_ptr(glm::vec4( m_renderConfig.stepSize / maxExtent,
                                                                                                      maxExtent / m_renderConfig.stepSize,
                                                                                                      m_renderConfig.isoValue,
                                                                                                      m_renderConfig.volumeShading )));
 
// ======= TODO: IMPLEMENT ========
//
// Part of **3. Volume Bricking**
// Add further uniforms as needed
        glClear(GL_COLOR_BUFFER_BIT);

        // rendering happens drawing the screenfilling quad and using the front faces and direction as input
        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
}

// ======= TODO: IMPLEMENT ========
//
// Part of **3. Volume Bricking**
//
// This function is largely complete.
// You only need to add further uniforms as needed.
//
// GPU implementation of a Composite raycaster with a given 1D transferfunction 
void GPURenderer::renderComposite()
{
        glUseProgram(m_compositeShader);

        // Pass textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_backfaces_texture);
        glUniform1i(glGetUniformLocation(m_compositeShader, "backFaces"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_frontfaces_texture);
        glUniform1i(glGetUniformLocation(m_compositeShader, "frontFaces"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, m_pGPUVolume->getTexId());
        glUniform1i(glGetUniformLocation(m_compositeShader, "volumeData"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_3D, m_pGPUVolume->getIndexTexId());
        glUniform1i(glGetUniformLocation(m_compositeShader, "volumeIndexData"), 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, m_renderConfig.tfTexId);
        glUniform1i(glGetUniformLocation(m_compositeShader, "transferFunction"), 4);

        // we bring the stepsize into normalized volume coordinates
        // first we need the max volume extent
        glm::vec3 volDims = m_pVolume->dims();
        float maxExtent = std::max(volDims.x, std::max(volDims.y, volDims.z));
        glUniform4fv(glGetUniformLocation(m_compositeShader, "renderOptions"), 1, glm::value_ptr(glm::vec4( m_renderConfig.stepSize / maxExtent,
                                                                                                            maxExtent / m_renderConfig.stepSize,
                                                                                                            m_renderConfig.stepSize,
                                                                                                            m_renderConfig.volumeShading )));

        glUniform4fv(glGetUniformLocation(m_compositeShader, "gmParams"), 1, glm::value_ptr(glm::vec4( m_renderConfig.illustrativeParams.x,
                                                                                                       m_renderConfig.illustrativeParams.y,
                                                                                                       m_renderConfig.illustrativeParams.z,
                                                                                                       m_renderConfig.useOpacityModulation)));
        // the reciprocal of the volDims is the voxelSize in 0..1 space
        glm::vec4 volumeInfo = glm::vec4(1.0f / volDims, m_pGPUVolume->useBricking());
        glUniform4fv(glGetUniformLocation(m_compositeShader, "volumeInfo"), 1, glm::value_ptr(volumeInfo));

        // Here we provide maximum volume and maximum gradient magnitude for normalization in the shader
        // Note: we actually give the reciprocal, to avoid division in the shader
        glUniform2fv(glGetUniformLocation(m_compositeShader, "volumeMaxValues"), 1, glm::value_ptr(glm::vec2( 1.0f/m_pVolume->maximum(),
                                                                                                              1.0f/m_pGradientVolume->maxMagnitude())));
        
        // ======= TODO: IMPLEMENT ========
        //
        // Part of **3. Volume Bricking**
        // Add further uniforms as needed

        glClear(GL_COLOR_BUFFER_BIT);

        // rendering happens drawing the screenfilling quad and using the front faces and direction as input
        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Note: Only used to pass update events through to the GPU Volume
// Upate the volume bricks after a tf/iso change or setting the bricksize
// only updates the cache, should not be called when bricksize change. call setVolumeBricksSize in that case
void GPURenderer::updateVolumeBricks()
{
    m_pGPUVolume->updateBrickCache(m_renderConfig, m_opacitySumTable);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Note: Only used to pass update events through to the GPU Volume
// sets a new bricksize and updates the bricking volume (full update)
void GPURenderer::setVolumeBricksSize()
{
    m_pGPUVolume->brickSizeChanged(m_renderConfig, m_opacitySumTable);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Helper function to simplify shader creation
void GPURenderer::linkShaderProgram(GLuint& shader, GLuint vertexShader, GLuint fragmentShader)
{
    shader = glCreateProgram();
    glAttachShader(shader, vertexShader);
    glAttachShader(shader, fragmentShader);
    glLinkProgram(shader);

    glDetachShader(shader, vertexShader);
    glDetachShader(shader, fragmentShader);
}
}
