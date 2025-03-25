#pragma once
#include <GL/glew.h>
#include <array>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cstring>
#include <cmath>

namespace render {

enum class RenderMode {
    RenderSlicer = 0,
    RenderMIP = 1,
    RenderIso = 2,
    RenderComposite = 3
};

struct RenderConfig {
    RenderMode renderMode { RenderMode::RenderSlicer };
    glm::ivec2 renderResolution;
    float stepSize { 1.0f };

    bool volumeShading { false };
    bool clippingPlanes { false };

    bool useOpacityModulation {false };
    glm::vec4 illustrativeParams { glm::vec4(0.0, 1.0, 1.0, 1.0) };
    
    bool updateTF { false }; // Used in the main loop to know when the TF should be updated. Defined as a parameter instead of a callback since the TF is already in this struct and seperating it would make thing more complex in other parts of the code

    float isoValue { 95.0f };
    bool bisection { false };
    
    int renderStep { 3 };

    // 1D transfer function.
    std::array<glm::vec4, 256> tfColorMap;
    // Used to convert from a value to an index in the color map.
    // index = (value - start) / range * tfColorMap.size();
    float tfColorMapIndexStart;
    float tfColorMapIndexRange;
    GLuint tfTexId; 
};

// NOTE(Mathijs): should be replaced by C++20 three-way operator (aka spaceship operator) if we require C++ 20 support from Linux users (GCC10 / Clang10).
inline bool operator==(const RenderConfig& lhs, const RenderConfig& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(RenderConfig)) == 0;
}
inline bool operator!=(const RenderConfig& lhs, const RenderConfig& rhs)
{
    return !(lhs == rhs);
}

}