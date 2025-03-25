#pragma once
#include <cstring> // memcmp  // macOS change TH
#include <glm/vec3.hpp>

namespace render {

struct GPUMeshConfig { 
    int blockSize {8};
    bool useEmptySpaceSkipping {false};
};

// NOTE(Mathijs): should be replaced by C++20 three-way operator (aka spaceship operator) if we require C++ 20 support from Linux users (GCC10 / Clang10).
inline bool operator==(const GPUMeshConfig& lhs, const GPUMeshConfig& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(GPUMeshConfig)) == 0;
}
inline bool operator!=(const GPUMeshConfig& lhs, const GPUMeshConfig& rhs)
{
    return !(lhs == rhs);
}

}