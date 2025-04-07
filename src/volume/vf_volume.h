#pragma once
#include "volume.h"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <vector>
#include "texture.h"

namespace volume {
struct VectorFieldVoxel {
    glm::vec4 dir_scalar;
}; 

class VectorFieldVolume {
public:
    // DO NOT REMOVE
    InterpolationMode interpolationMode { InterpolationMode::NearestNeighbour };

public:
    VectorFieldVolume(const Volume& volume);
    void normalizeDirScalar();

    std::vector<glm::vec4> getData() const;
    glm::ivec3 getDims() const;

    glm::vec2 getVectorDirectionInterpolated(const glm::vec3& pos) const;

    inline float getMaxMagnitude() const { return m_maxMagnitude; };

protected:

    const glm::ivec3 m_dim;
    float m_maxMagnitude;
    std::vector<VectorFieldVoxel> m_data;
};
}
