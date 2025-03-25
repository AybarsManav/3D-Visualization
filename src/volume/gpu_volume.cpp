#include "gpu_volume.h"
#include <glm/common.hpp>
#include <iostream>
#include <chrono>

#include <glm/gtx/component_wise.hpp>

namespace volume {

GPUVolume::GPUVolume(const Volume* volume)
    : m_volumeTexture(Texture(volume->getData(), volume->dims()))
    , m_indexTexture(Texture(std::vector<float>(0), glm::ivec3(1)))
    , m_pVolume(volume)
    , m_minMaxValues(std::vector<glm::vec2>())
    , m_brickVolumeSize(glm::ivec3(0))
    , m_brickVolume(std::vector<float>())
    , m_indexVolumeSize(glm::ivec3(0))
    , m_indexVolume(std::vector<glm::vec3>())
    , m_volumeDims(glm::vec3(-1))
    , m_brickSize(-1)
    , m_useBricking(false)
    , m_brickPadding(2)
{
    // The index texture should not interpolate offsets
    m_indexTexture.setInterpolationMode(GL_NEAREST);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// update the config
// Note: whenever this is called brickSizeChanged(...) should also be called
void GPUVolume::setVolumeConfig(const render::GPUVolumeConfig& config)
{
    m_volumeConfig = config;
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Full update of the min max data structure and cache neded when loading data or changing brick size
void GPUVolume::brickSizeChanged(render::RenderConfig renderConfig, std::array<float, 256>& opacitySumTable)
{ 
    // only update when the values changed
    if (m_brickSize != m_volumeConfig.brickSize || m_useBricking != m_volumeConfig.useVolumeBricking || m_volumeDims != m_pVolume->dims())
    {
        // set internal brick size, we always assume cubic bricks, i.e., m_brickSize * m_brickSize * m_brickSize
        m_brickSize = m_volumeConfig.brickSize;
        m_useBricking = m_volumeConfig.useVolumeBricking;
        m_volumeDims = m_pVolume->dims();

        updateMinMax();
        updateBrickCache(renderConfig, opacitySumTable);
    }
}

// ======= TODO: IMPLEMENT ========
//
// Part of **3. Volume Bricking**
//
// This function should calculate the minimum and maximum values per brick
// We store the values in the 1D std::vector m_minMaxValues with min and max in a glm::vec2
// This function is very similar to the min max calculation for the blocking you did in Part 2 but with the bricksize
// However, here you need to consider padding and you do not need to store position
//
// The bricksize is set in m_brickSize (as a single integer, as we want cubic bricks)
// To allow interpolation and gradient calculation without recalculating the indirection, we add padding to the bricks
// Use m_brickPadding as the size of the padding, in this implementation it is set to 2 and does not change but be generic
// You need to set m_indexVolumeSize (glm::vec3) as the size (in voxels) of the redirection index
void GPUVolume::updateMinMax()
{
    // we initialize a timer to test this method
    using clock = std::chrono::steady_clock;
    std::chrono::steady_clock::time_point start = clock::now();

    // ======= TODO: calculate m_indexVolumeSize  based on volume dimensions and m_brickSize
    m_indexVolumeSize = glm::vec3(1);

    // the number of bricks
    int numBricksLinear = m_indexVolumeSize.x * m_indexVolumeSize.y * m_indexVolumeSize.z;
    // resize m_minMaxValues to take one vec2(min, max) per brick
    m_minMaxValues.resize(numBricksLinear);

    // ======= TODO: replace this and calculate all actual min max values
    m_minMaxValues[0] = glm::vec2(0.0f, m_pVolume->maximum());
    // stop the timer
    using clock = std::chrono::steady_clock;
    std::chrono::steady_clock::time_point stop = clock::now();

    std::cout << "updateMinMax() executed in " << std::chrono::duration<double, std::milli>(stop - start).count() << "ms" << std::endl;
}

// ======= TODO: IMPLEMENT ========
//
// Part of **3. Volume Bricking**
//
// This function should calculate whether a brick is active or not
// and adds the offset to the index volume and copies the actual brick data into the cache
// It will be called whenever the volume is loaded, the brick size changes and when the transfer function or iso value changes
// 
// In principle this function is very similar to the updateActiveBlocks function you created for empty space skipping
// However, we also prepare the volume index and cache data for the GPU
// You need to update several variables for this to work downstream
// m_brickVolume: the actual volume cache should contain a brick at positions given by the offsets in the index
// m_brickVolumeSize: the size of the cache volume in 3D in voxels i.e., your result of findOptimalDimensions (num bricks in 3D) times the padded brick size
// m_indexVolume: the index redirection data should contain the offset into m_brickVolume in 3D (consider already normalizing these to save cycles on the GPU)
// NOTE: doing this correctly with padding can be tricky. If you get stuck debugging, consider without padding first.
// NOTE: you should have updated m_indexVolumeSize already in updateMinMax()
void GPUVolume::updateBrickCache(render::RenderConfig renderConfig, std::array<float, 256>& opacitySumTable)
{
    // we initialize a timer to test this method
    using clock = std::chrono::steady_clock;
    std::chrono::steady_clock::time_point start = clock::now();

    // no bricking means we just add a single item and the cache becomes the volume
    if (!m_volumeConfig.useVolumeBricking) { // if volume bricking is turned off just return the entire volume
        m_volumeTexture.update(m_pVolume->getData(), m_pVolume->dims());
        m_indexTexture.update(std::vector<glm::vec4> { glm::vec4(0) }, glm::ivec3(1));
        return;
    }


    // NOTE: m_indexVolume needs to be filled in the right order to work as a 3D volume on the GPU
    // (0,0,0) should be in index 0, (1,0,0) = 1, etc.
    // m_brickVolume can be filled in any way, as long as you keep correct offsets in your index
    // as long as you implement the redirected lookup on the GPU in the same way
    // we update the textures with the calculated values
    // NOTE: this might crash as long as m_brickVolumeSize and m_indexVolumeSize are not set correctly when enabling bricking
    m_volumeTexture.update(m_brickVolume, m_brickVolumeSize);
    m_indexTexture.update(m_indexVolume, m_indexVolumeSize);

    // stop the timer
    using clock = std::chrono::steady_clock;
    std::chrono::steady_clock::time_point stop = clock::now();

    std::cout << "updateCache() executed in " << std::chrono::duration<double, std::milli>(stop - start).count() << "ms" << std::endl;
}

// ======= TODO: IMPLEMENT ========
//
// Part of **3. Volume Bricking**
//
// This helper function should calculate the size of the cache volume in number of bricks in 3D
// The input N is the total number of bricks
// The input maxSize is the maximum texture size divided by the (padded) brick size
// The output is the number of bricks when tightly packing it into a 3D structure
// This is needed as the extent of textures on the GPU is limited
// There are various ways to do this, the goal is to find a 3D volume that has little empty space
// One that always works is taking the smallest cube that fits N, i.e., the next highest integer of the cubic root of N
// However this can be very wasteful
// E.g., N=10 bricks could be packed into a 5x2x1 cube without waste or the next cubic root 4x4x4 cuboid with six blocks of wasted space
// The goal of this function is to find a cuboid with little wasted space that fits into the texture extents in a short time
glm::ivec3 GPUVolume::findOptimalDimensions(int N)
{
    // we initialize a timer to test this method
    using clock = std::chrono::steady_clock;
    std::chrono::steady_clock::time_point start = clock::now();

    // TODO: calculate the optimal dimensions for the cache volume here
    glm::ivec3 optimalDimensions = glm::vec3(1, 1, 1);

    // stop the timer
    using clock = std::chrono::steady_clock;
    std::chrono::steady_clock::time_point stop = clock::now();

    std::cout << "findOptimalDimensions() with cube executed in " << std::chrono::duration<double, std::milli>(stop - start).count() << "ms" << std::endl;
    std::cout << "fitting cube uses " << (float)N/(optimalDimensions.x*optimalDimensions.y*optimalDimensions.z)*100.0f << "\% of available space" << std::endl;

    // return the optimal dimensions
    return optimalDimensions;
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// get the OpenGL id of the volume / cache texture
GLuint GPUVolume::getTexId() const 
{
    return m_volumeTexture.getTexId();
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// get the OpenGL id of the index texture
GLuint GPUVolume::getIndexTexId() const
{
    return m_indexTexture.getTexId();
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// on the GPU to switch between nearest and linear we just have to update the texture properties
void GPUVolume::updateInterpolation()
{
    if (interpolationMode == InterpolationMode::NearestNeighbour) {
        m_volumeTexture.setInterpolationMode(GL_NEAREST);
    } else {
        m_volumeTexture.setInterpolationMode(GL_LINEAR);
    }
}
}