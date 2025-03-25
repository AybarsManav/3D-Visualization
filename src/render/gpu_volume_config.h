#pragma once
#include <cstring>
#include <glm/vec3.hpp>

namespace render {

struct GPUVolumeConfig {
    int brickSize { 32 };
    bool useVolumeBricking { false };
};

// NOTE: should be replaced by C++20 three-way operator (aka spaceship operator) if we require C++ 20 support from Linux users (GCC10 / Clang10).
inline bool operator==(const GPUVolumeConfig& lhs, const GPUVolumeConfig& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(GPUVolumeConfig)) == 0;
}
inline bool operator!=(const GPUVolumeConfig& lhs, const GPUVolumeConfig& rhs)
{
    return !(lhs == rhs);
}

}