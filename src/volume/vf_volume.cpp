#include "vf_volume.h"
#include <algorithm>
#include <exception>
#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>
#include <gsl/span>
#include <iostream>

#include <random>

namespace volume {

static std::vector<VectorFieldVoxel> computeVectorFieldVolume(const Volume& volume)
{
    const auto dim = volume.dims(); // Volume dimensions
    const std::vector<float>& volume_data = volume.getData(); // Get data as a const reference

    // Pre-allocate output vector
    std::vector<VectorFieldVoxel> out(static_cast<size_t>(dim.x * dim.y * dim.z));

    // Iterate through the volume
    for (int z = 0; z < dim.z; z++) {
        for (int y = 0; y < dim.y; y++) {
            for (int x = 0; x < dim.x; x++) {
                // Compute the flat index in the volume data
                int idx = z * dim.x * dim.y + y * dim.x + x;

                // Extract direction and scalar values
                out[idx] = { glm::vec4(
                    volume_data[idx * 5],
                    volume_data[idx * 5 + 1],
                    volume_data[idx * 5 + 3],
                    volume_data[idx * 5 + 4]) };
            }
        }
    }
    return out;
}

template <typename T>
inline T bilinearInterpolation(const T& v00, const T& v10, const T& v01, const T& v11, const glm::vec2& coord)
{
    auto x0 = v00 * (1 - coord.x) + v10 * (coord.x);
    auto x1 = v01 * (1 - coord.x) + v11 * (coord.x);
    return x0 * (1 - coord.y) + x1 * (coord.y);
}

int getVoxelOffset(const VectorFieldVolume& volume, int x, int y, int time)
{
    glm::ivec3 dims = volume.getDims();
    return time * dims.y * dims.x + y * dims.x + x;
}

VectorFieldVolume::VectorFieldVolume(const Volume& volume)
    : m_dim(volume.dims())
    , m_data(computeVectorFieldVolume(volume))
{
    normalizeDirScalar();
}

void VectorFieldVolume::normalizeDirScalar()
{
    if (m_data.empty())
        return;

    glm::vec4 minValues(std::numeric_limits<float>::max());
    glm::vec4 maxValues(std::numeric_limits<float>::lowest());

// Step 1: Find min and max for each component
#pragma omp parallel
    {
        glm::vec4 localMin(std::numeric_limits<float>::max());
        glm::vec4 localMax(std::numeric_limits<float>::lowest());

#pragma omp for
        for (int i = 0; i < m_data.size(); ++i) {
            const glm::vec4& vec = m_data[i].dir_scalar;
            localMin = glm::min(localMin, vec);
            localMax = glm::max(localMax, vec);
        }
#pragma omp critical
        {
            minValues = glm::min(minValues, localMin);
            maxValues = glm::max(maxValues, localMax);
        }
    }

    // x and y should be scaled with the same factor
    minValues.x = glm::min(minValues.x, minValues.y);
    minValues.y = minValues.x;
    maxValues.x = glm::max(maxValues.x, maxValues.y);
    maxValues.y = maxValues.x;

    // if the min is positive use it as min, otherwise use the negative max
    minValues.x = glm::min(minValues.x, -maxValues.x);
    minValues.y = minValues.x;
    minValues.z = glm::min(minValues.z, -maxValues.z);
    minValues.w = glm::min(minValues.w, -maxValues.w);


    // Step 2: Normalize each component
    glm::vec4 scale = 1.0f / glm::max(-minValues, maxValues);
#pragma omp parallel for
    for (int i = 0; i < m_data.size(); ++i) {
        glm::vec4& vec = m_data[i].dir_scalar;
        for (int j = 0; j < 4; j++) {
            if (minValues[j] * maxValues[j] < 0.0f) {
                vec[j] = vec[j] * scale[j];
            } else {
                vec[j] = (vec[j] - minValues[j]) / (maxValues[j] - minValues[j]);
            }
        }
    }

    m_maxMagnitude = 0.0f;
    for (int i = 0; i < m_data.size(); ++i) {
        glm::vec2 vector = m_data[i].dir_scalar;
        m_maxMagnitude = std::max(m_maxMagnitude, glm::length(vector));
    }
}

std::vector<glm::vec4> VectorFieldVolume::getData() const
{
    std::vector<glm::vec4> result;
    result.reserve(m_data.size()); // Reserve space for efficiency.

    for (const auto& voxel : m_data) {
        result.push_back(voxel.dir_scalar); // Add the glm::vec4 directly.
    }

    return result;
}

glm::ivec3 VectorFieldVolume::getDims() const
{
    return m_dim;
}

/// <param name="rel_pos">x,y, time</param>
glm::vec2 VectorFieldVolume::getVectorDirectionInterpolated(const glm::vec3& rel_pos) const
{
    auto pos_px = rel_pos - 0.5f;
    auto i000 = glm::ivec3(glm::floor(pos_px)); // floor

    auto v000 = m_data[getVoxelOffset(*this, glm::clamp(i000.x + 0, 0, m_dim.x - 1), glm::clamp(i000.y + 0, 0, m_dim.y - 1), glm::clamp(i000.z + 0, 0, m_dim.z - 1))].dir_scalar;
    auto v100 = m_data[getVoxelOffset(*this, glm::clamp(i000.x + 1, 0, m_dim.x - 1), glm::clamp(i000.y + 0, 0, m_dim.y - 1), glm::clamp(i000.z + 0, 0, m_dim.z - 1))].dir_scalar;
    auto v010 = m_data[getVoxelOffset(*this, glm::clamp(i000.x + 0, 0, m_dim.x - 1), glm::clamp(i000.y + 1, 0, m_dim.y - 1), glm::clamp(i000.z + 0, 0, m_dim.z - 1))].dir_scalar;
    auto v110 = m_data[getVoxelOffset(*this, glm::clamp(i000.x + 1, 0, m_dim.x - 1), glm::clamp(i000.y + 1, 0, m_dim.y - 1), glm::clamp(i000.z + 0, 0, m_dim.z - 1))].dir_scalar;

    auto pos_rel = glm::vec2(pos_px.x, pos_px.y) - glm::vec2(i000.x, i000.y);

    auto v0 = bilinearInterpolation(v000, v100, v010, v110, pos_rel);

    auto v001 = m_data[getVoxelOffset(*this, glm::clamp(i000.x + 0, 0, m_dim.x - 1), glm::clamp(i000.y + 0, 0, m_dim.y - 1), glm::clamp(i000.z + 1, 0, m_dim.z - 1))].dir_scalar;
    auto v101 = m_data[getVoxelOffset(*this, glm::clamp(i000.x + 1, 0, m_dim.x - 1), glm::clamp(i000.y + 0, 0, m_dim.y - 1), glm::clamp(i000.z + 1, 0, m_dim.z - 1))].dir_scalar;
    auto v011 = m_data[getVoxelOffset(*this, glm::clamp(i000.x + 0, 0, m_dim.x - 1), glm::clamp(i000.y + 1, 0, m_dim.y - 1), glm::clamp(i000.z + 1, 0, m_dim.z - 1))].dir_scalar;
    auto v111 = m_data[getVoxelOffset(*this, glm::clamp(i000.x + 1, 0, m_dim.x - 1), glm::clamp(i000.y + 1, 0, m_dim.y - 1), glm::clamp(i000.z + 1, 0, m_dim.z - 1))].dir_scalar;

    auto v1 = bilinearInterpolation(v001, v101, v011, v111, pos_rel);

    auto v = glm::mix(v0, v1, pos_px.z - i000.z);

    return glm::vec2(v.x, v.y);
}

}