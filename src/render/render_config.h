#pragma once
#include "volume/vf_volume.h"
#include <array>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cstring>
#include <cmath>
#include "volume/vf_volume.h"
#include "imgui.h"

namespace render {

enum class RenderMode {
    RenderSlicer = 0,
    RenderMIP = 1,
    RenderIso = 2,
    RenderComposite = 3,
    RenderVectorfield = 4
};

enum class VectorGeometry {
    None = 0, 
    Hedgehog = 1, 
    Integration = 2
};

enum class LineMode {
    VF_StreamLines,
    VF_PathLines
};

enum class LineMethod {
    VF_Euler,
    VF_RK2
};

struct RenderConfig_VectorField {

    // modes
    bool isScalarActive { true };
    bool isLicActive { false };
    VectorGeometry geometryOverlay { VectorGeometry::None };

    // shared
    glm::ivec3 volumeDims { glm::ivec3(0) };
    int timeStep { 0 };

    // scalar
    //glm::bvec4 visualComponents { glm::bvec4(true, true, false, false) };
    glm::bvec3 vectorComponents { glm::bvec3(true, true, false) };
    glm::bvec2 scalarComponents { glm::bvec2(false, false) };
    float scalarOverlayAlpha { 0.5f };

    // lic
    int licKernelWidth { 10 };

    // hedgehog
    bool hedgehogFixedSize { false };
    float hedgehogBaseLength { 8.0f };
    int hedgehogSampling { 10 };

    // Field Lines
    int numLines { 10 };
    int lineWidth { 2 };
    
    ImVec4 geoColor = ImVec4(0.5f, 0.0f, 0.5f, 0.75f);

    // 0: Euler, 1: RK2
    LineMethod method { LineMethod::VF_Euler };
    
    // 0: Streamlines, 1: Pathlines
    LineMode mode { LineMode::VF_StreamLines };

    float stepSize { 0.5f };
    int numStepsTime { 50 };
    float stoppingMagnitude { 0.1f };
    int maxSteps { 2500 };

    glm::vec4 seedPoint { 0.5f, 0.5f, -1.0f, -1.0f };

    bool hedgehogsNeedUpdate(const RenderConfig_VectorField& rhs) const
    {
        return hedgehogFixedSize != rhs.hedgehogFixedSize
            || fabs(hedgehogBaseLength - rhs.hedgehogBaseLength) > 0.01f
            || hedgehogSampling != rhs.hedgehogSampling
            || timeStep != rhs.timeStep
            || any(epsilonNotEqual(glm::vec4(geoColor.x, geoColor.y, geoColor.z, geoColor.w), glm::vec4(rhs.geoColor.x, rhs.geoColor.y, rhs.geoColor.z, rhs.geoColor.w), 0.001f));
    }

    bool fieldLinesNeedUpdate(const RenderConfig_VectorField& rhs) const
    {
        return method != rhs.method
            || numLines != rhs.numLines
            || lineWidth != rhs.lineWidth
            || any(epsilonNotEqual(glm::vec4(geoColor.x, geoColor.y, geoColor.z, geoColor.w), glm::vec4(rhs.geoColor.x, rhs.geoColor.y, rhs.geoColor.z, rhs.geoColor.w), 0.001f))
            || mode != rhs.mode
            || fabs(stepSize - rhs.stepSize) > 0.001f
            || numStepsTime != rhs.numStepsTime
            || fabs(stoppingMagnitude - rhs.stoppingMagnitude) > 0.001f
            || maxSteps != rhs.maxSteps
            || any(epsilonNotEqual(seedPoint, rhs.seedPoint, 0.001f))
            || timeStep != rhs.timeStep;
    }
    
    bool operator!=(const RenderConfig_VectorField& rhs) const
    {
        return isScalarActive != rhs.isScalarActive
            || isLicActive != rhs.isLicActive
            || geometryOverlay != rhs.geometryOverlay
            || volumeDims != rhs.volumeDims
            || vectorComponents != rhs.vectorComponents
            || scalarComponents != rhs.scalarComponents
            || fabs(scalarOverlayAlpha - rhs.scalarOverlayAlpha) > 0.01f
            || licKernelWidth != rhs.licKernelWidth
            || hedgehogsNeedUpdate(rhs)
            || fieldLinesNeedUpdate(rhs);
    }

    RenderConfig_VectorField& operator=(const RenderConfig_VectorField& rhs)
    {
        numLines = rhs.numLines;
        lineWidth = rhs.lineWidth;
        geoColor = rhs.geoColor;
        isScalarActive = rhs.isScalarActive;
        isLicActive = rhs.isLicActive;
        geometryOverlay = rhs.geometryOverlay;
        volumeDims = rhs.volumeDims;
        timeStep = rhs.timeStep;
        vectorComponents = rhs.vectorComponents;
        scalarComponents = rhs.scalarComponents;
        scalarOverlayAlpha = rhs.scalarOverlayAlpha;
        licKernelWidth = rhs.licKernelWidth;
        hedgehogFixedSize = rhs.hedgehogFixedSize;
        hedgehogBaseLength = rhs.hedgehogBaseLength;
        hedgehogSampling = rhs.hedgehogSampling;
        method = rhs.method;
        mode = rhs.mode;
        stepSize = rhs.stepSize;
        numStepsTime = rhs.numStepsTime;
        maxSteps = rhs.maxSteps;
        stoppingMagnitude = rhs.stoppingMagnitude;
        seedPoint = rhs.seedPoint;

        return *this;
    }

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

    // vector field
    RenderConfig_VectorField vectorFieldParameters;

    // 1D transfer function.
    std::array<glm::vec4, 256> tfColorMap;
    // Used to convert from a value to an index in the color map.
    // index = (value - start) / range * tfColorMap.size();
    float tfColorMapIndexStart;
    float tfColorMapIndexRange;
    GLuint tfTexId; 

    // 2D transfer function.
//    float TF2DIntensity;
//    float TF2DRadius;
//    glm::vec4 TF2DColor;
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