#include "renderer.h"
#include <algorithm>
#include <algorithm> // std::fill
#include <cmath>
#include <functional>
#include <glm/common.hpp>
#include <glm/gtx/component_wise.hpp>
#include <iostream>
#include <tuple>

namespace render {

// The renderer is passed a pointer to the volume, gradinet volume, camera and an initial renderConfig.
// The camera being pointed to may change each frame (when the user interacts). When the renderConfig
// changes the setConfig function is called with the updated render config. This gives the Renderer an
// opportunity to resize the framebuffer.
Renderer::Renderer(
    const volume::Volume* pVolume,
    const volume::GradientVolume* pGradientVolume,
    const render::RayTraceCamera* pCamera,
    const RenderConfig& initialConfig)
    : m_pVolume(pVolume)
    , m_pGradientVolume(pGradientVolume)
    , m_pCamera(pCamera)
    , m_config(initialConfig)
{
    resizeImage(initialConfig.renderResolution);
}

// Set a new render config if the user changed the settings.
void Renderer::setConfig(const RenderConfig& config)
{
    if (config.renderResolution != m_config.renderResolution)
        resizeImage(config.renderResolution);

    m_config = config;
}

// Resize the framebuffer and fill it with black pixels.
void Renderer::resizeImage(const glm::ivec2& resolution)
{
    m_frameBuffer.resize(size_t(resolution.x) * size_t(resolution.y), glm::vec4(0.0f));
}

// Clear the framebuffer by setting all pixels to black.
void Renderer::resetImage()
{
    std::fill(std::begin(m_frameBuffer), std::end(m_frameBuffer), glm::vec4(0.0f));
}

// Return a VIEW into the framebuffer. This view is merely a reference to the m_frameBuffer member variable.
// This does NOT make a copy of the framebuffer.
gsl::span<const glm::vec4> Renderer::frameBuffer() const
{
    return m_frameBuffer;
}

// Main render function. It computes an image according to the current renderMode.
// Multithreading is enabled in Release/RelWithDebInfo modes. In Debug mode multithreading is disabled to make debugging easier.
void Renderer::render()
{
    resetImage();

    const glm::vec3 planeNormal = -glm::normalize(m_pCamera->forward());
    const glm::vec3 volumeCenter = glm::vec3(m_pVolume->dims()) / 2.0f;
    const Bounds bounds {glm::vec3(0.0f),  glm::vec3(m_pVolume->dims() - glm::ivec3(1))};

    // 0 = sequential (single-core), 1 = OMP (multi-core)
#ifdef NDEBUG
    // If NOT in debug mode then enable parallelism using the Open MP
#define PARALLELISM 1
#else
    // Disable multi threading in debug mode.
#define PARALLELISM 0
#endif

#if PARALLELISM == 1
    #pragma omp parallel for
#endif
    for (int x = 0; x < m_config.renderResolution.x; x++) {
        for (int y = 0; y < m_config.renderResolution.y; y++) {

            // Compute a ray for the current pixel.
            const glm::vec2 pixelPos = glm::vec2(x, y) / glm::vec2(m_config.renderResolution);
            Ray ray = m_pCamera->generateRay(pixelPos * 2.0f - 1.0f);

            // Compute where the ray enters and exists the volume.
            // If the ray misses the volume then we continue to the next pixel.
            if (!instersectRayVolumeBounds(ray, bounds))
                continue;

            // Get a color for the current pixel according to the current render mode.
            glm::vec4 color {};
            switch (m_config.renderMode) {
            case RenderMode::RenderSlicer: {
                color = traceRaySlice(ray, volumeCenter, planeNormal);
                break;
            }
            case RenderMode::RenderMIP: {
                color = traceRayMIP(ray, m_config.stepSize);
                break;
            }
            case RenderMode::RenderComposite: {
                color = traceRayComposite(ray, m_config.stepSize);
                break;
            }
            case RenderMode::RenderIso: {
                color = traceRayISO(ray, m_config.stepSize);
                break;
            }
            };
            // Write the resulting color to the screen.
            fillColor(x, y, color);

        }
    }
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// This function generates a view alongside a plane perpendicular to the camera through the center of the volume
//  using the slicing technique.
glm::vec4 Renderer::traceRaySlice(const Ray& ray, const glm::vec3& volumeCenter, const glm::vec3& planeNormal) const
{
    const float t = glm::dot(volumeCenter - ray.origin, planeNormal) / glm::dot(ray.direction, planeNormal);
    const glm::vec3 samplePos = ray.origin + ray.direction * t;
    const float val = m_pVolume->getSampleInterpolate(samplePos);
    return glm::vec4(glm::vec3(std::max(val / m_pVolume->maximum(), 0.0f)), 1.f);
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Function that implements maximum-intensity-projection (MIP) raycasting.
// It returns the color assigned to a ray/pixel given it's origin, direction and the distances
// at which it enters/exits the volume (ray.tmin & ray.tmax respectively).
// The ray must be sampled with a distance defined by the stepSize
glm::vec4 Renderer::traceRayMIP(const Ray& ray, float stepSize) const
{
    float maxVal = 0.0f;

    // Incrementing samplePos directly instead of recomputing it each frame gives a measureable speed-up.
    glm::vec3 samplePos = ray.origin + ray.tmin * ray.direction;
    const glm::vec3 increment = stepSize * ray.direction;
    for (float t = ray.tmin; t <= ray.tmax; t += stepSize, samplePos += increment) {
        const float val = m_pVolume->getSampleInterpolate(samplePos);
        maxVal = std::max(val, maxVal);
    }

    // Normalize the result to a range of [0 to mpVolume->maximum()].
    return glm::vec4(glm::vec3(maxVal) / m_pVolume->maximum(), 1.0f);
}

// ======= TODO: IMPLEMENT ========
// This function should find the position where the ray intersects with the volume's isosurface.
// If volume shading is DISABLED then simply return the isoColor.
// If volume shading is ENABLED then return the phong-shaded color at that location using the local gradient (from m_pGradientVolume).
//   Use the camera position (m_pCamera->position()) as the light position.
// Use the bisectionAccuracy function (to be implemented) to get a more precise isosurface location between two steps.
glm::vec4 Renderer::traceRayISO(const Ray& ray, float stepSize) const
{
    static constexpr glm::vec3 isoColor { 0.8f, 0.8f, 0.2f };
    glm::vec3 samplePos = ray.origin + ray.tmin * ray.direction;
    const glm::vec3 increment = stepSize * ray.direction;

    for (float t = ray.tmin; t <= ray.tmax; t += stepSize, samplePos += increment) {

        // get value at current position
        const float val = m_pVolume->getSampleInterpolate(samplePos);

        // if the value is greater or equal than the isoValue, stop
        if (val >= m_config.isoValue) {

            // find best time step t
            float accurateT = bisectionAccuracy(ray, t - stepSize, t, m_config.isoValue);
            samplePos = ray.origin + accurateT * ray.direction;

            if (m_config.volumeShading) {

                // compute and return phong shading
                auto phong = computePhongShading(glm::normalize(isoColor), m_pGradientVolume->getGradientInterpolate(samplePos), -glm::normalize(m_pCamera->position()), ray.direction);
                return glm::vec4(phong, 1.0f);
            }
            // return isoColor when volume shading is DISABLED
            return glm::vec4(isoColor, 1.0f);
        }
    }
    // no intersection with object found, return black
    return glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

// ======= TODO: IMPLEMENT ========
// Given that the iso value lies somewhere between t0 and t1, find a t for which the value
// closely matches the iso value (less than 0.01 difference). Add a limit to the number of
// iterations such that it does not get stuck in degerate cases.
float Renderer::bisectionAccuracy(const Ray& ray, float t0, float t1, float isoValue) const
{
    int maxiter = 100;

    glm::vec3 samplePos = ray.origin + t1 * ray.direction;
    float tnew = t1;
    float val = m_pVolume->getSampleInterpolate(samplePos);

    for (int iter = 0; iter < maxiter; iter++) {
        if (std::abs(val - isoValue) < 0.01) {

            // stop when closely matching the iso value
            return tnew;
        }

        // get new position by bisecting the range
        tnew = (t0 + t1) / 2.0f;
        samplePos = ray.origin + tnew * ray.direction;
        val = m_pVolume->getSampleInterpolate(samplePos);

        // update such that potential intersection stays between t0 and t1
        if (val < isoValue) {
            t0 = tnew;
        } else {
            t1 = tnew;
        }
    }
    return t1;
}

// ======= TODO: IMPLEMENT ========
// Compute Phong Shading given the voxel color (material color), the gradient, the light vector and view vector.
//
// you are allowed to modify the default parameters in the .h file, if needed. DO NOT modify them inside the function.

glm::vec3 Renderer::computePhongShading(const glm::vec3& color, const volume::GradientVoxel& gradient, const glm::vec3& L, const glm::vec3& V, float ambientCoefficient, float diffuseCoefficient, float specularCoefficient, int specularPower)
{

    // Normal from gradient
    glm::vec3 N = gradient.dir;
    if (glm::length(N) > 0.0f) {
        N = glm::normalize(N);
    }

    // Ensure the normal faces the viewer
    if (glm::dot(N, V) < 0.0f) {
        N = -N;
    }

    // Compute the diffuse component, clamping to zero if negative
    float dotLN = glm::dot(L, N);
    float diffuseTerm = std::max(dotLN, 0.0f);

    // Reflect L around N and compute specular term, also clamping to zero
    glm::vec3 R = glm::reflect(-L, N);
    float dotRV = glm::dot(R, V);
    float specularTerm = 0.0f;
    if (diffuseTerm > 0.0f) {
        specularTerm = std::max(dotRV, 0.0f);
    }

    // Calculate ambient, diffuse and specular
    glm::vec3 ambient = ambientCoefficient * color;
    glm::vec3 diffuse = diffuseCoefficient * diffuseTerm * color;
    glm::vec3 specular = specularCoefficient * glm::pow(specularTerm, float(specularPower)) * color;

    return ambient + diffuse + specular;
}

// ======= TODO: IMPLEMENT ========
// In this function, implement 1D transfer function raycasting.
// Use getTFValue to compute the color for a given volume value according to the 1D transfer function.
glm::vec4 Renderer::traceRayComposite(const Ray& ray, float stepSize) const
{

    float t = ray.tmin;
    const auto step = ray.direction * stepSize;
    auto pos = ray.origin + ray.direction * ray.tmin;
    glm::vec4 aggregated_samples = glm::vec4(0.0f);
    // we sample the volume until we exit the volume, or the opacity has become (pretty much) opaque.
    while (t <= ray.tmax && aggregated_samples.w < 0.99f) {
        const float sample = this->m_pVolume->getSampleInterpolate(pos);
        glm::vec4 color_sample = this->getTFValue(sample);

        if (m_config.volumeShading) {
            // if shading is enabled, compute phong with a lightsource coming from the camera
            auto gradient = m_pGradientVolume->getGradientInterpolate(pos);
            // we need to catch the case where the gradient is zero as that will result in NaNs for phong
            // similarly when the gradient moves away from the ray, we do not want to compute shading as that will negate the compositing effect
            if (glm::length(gradient.dir) > 0.0f && glm::dot(gradient.dir, ray.direction) > 0.0f) {
                auto phong = computePhongShading(glm::vec3(color_sample), gradient, ray.direction, ray.direction);
                color_sample = glm::vec4(phong, color_sample.w);
            }
        }

        // pre-multiply colors with its own alpha channel
        color_sample *= glm::vec4(glm::vec3(color_sample.w), 1.0f);

        // C'i = C'i-1 + (1 - A'i-1)*Ci
        // A'i = A'i-1 + (1 - A'i-1)*Ai
        aggregated_samples += (1.0f - aggregated_samples.w) * color_sample;
        t += stepSize;
        pos += step;
    }

    return aggregated_samples;
}

// ======= DO NOT MODIFY THIS FUNCTION ========
// Looks up the color+opacity corresponding to the given volume value from the 1D tranfer function LUT (m_config.tfColorMap).
// The value will initially range from (m_config.tfColorMapIndexStart) to (m_config.tfColorMapIndexStart + m_config.tfColorMapIndexRange) .
glm::vec4 Renderer::getTFValue(float val) const
{
    // Map value from [m_config.tfColorMapIndexStart, m_config.tfColorMapIndexStart + m_config.tfColorMapIndexRange) to [0, 1) .
    const float range01 = (val - m_config.tfColorMapIndexStart) / m_config.tfColorMapIndexRange;
    const size_t i = std::min(static_cast<size_t>(range01 * static_cast<float>(m_config.tfColorMap.size())), m_config.tfColorMap.size() - 1);
    return m_config.tfColorMap[i];
}


// This function computes if a ray intersects with the axis-aligned bounding box around the volume.
// If the ray intersects then tmin/tmax are set to the distance at which the ray hits/exists the
// volume and true is returned. If the ray misses the volume the the function returns false.
//
// If you are interested you can learn about it at.
// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
bool Renderer::instersectRayVolumeBounds(Ray& ray, const Bounds& bounds) const
{
    const glm::vec3 invDir = 1.0f / ray.direction;
    const glm::bvec3 sign = glm::lessThan(invDir, glm::vec3(0.0f));

    float tmin = (bounds.lowerUpper[sign[0]].x - ray.origin.x) * invDir.x;
    float tmax = (bounds.lowerUpper[!sign[0]].x - ray.origin.x) * invDir.x;
    const float tymin = (bounds.lowerUpper[sign[1]].y - ray.origin.y) * invDir.y;
    const float tymax = (bounds.lowerUpper[!sign[1]].y - ray.origin.y) * invDir.y;

    if ((tmin > tymax) || (tymin > tmax))
        return false;
    tmin = std::max(tmin, tymin);
    tmax = std::min(tmax, tymax);

    const float tzmin = (bounds.lowerUpper[sign[2]].z - ray.origin.z) * invDir.z;
    const float tzmax = (bounds.lowerUpper[!sign[2]].z - ray.origin.z) * invDir.z;

    if ((tmin > tzmax) || (tzmin > tmax))
        return false;

    ray.tmin = std::max(tmin, tzmin);
    ray.tmax = std::min(tmax, tzmax);
    return true;
}

// This function inserts a color into the framebuffer at position x,y
void Renderer::fillColor(int x, int y, const glm::vec4& color)
{
    const size_t index = static_cast<size_t>(m_config.renderResolution.x * y + x);
    m_frameBuffer[index] = color;
}
}