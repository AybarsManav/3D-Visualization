#version 330
layout(location = 0) in vec3 pos;

uniform mat4 u_modelViewProjection;
uniform mat4 u_model;
uniform vec3 cubeSize;

uniform samplerBuffer positionCube;
uniform isamplerBuffer blockActive;

out vec3 u_color;
out vec3 worldPos;

void main() {

    // Get the world space position 
    vec3 cubeOffset = texelFetch(positionCube, gl_InstanceID).xyz; 

    // get the active state
    int isActive = texelFetch(blockActive, gl_InstanceID).x;

    if (isActive > 0) {
        // moves the cube instance to the correct position and clamps the last row of cubes
        vec3 actualPos = clamp((pos + cubeOffset) * cubeSize, 0, 1);
        
        // calculate position in view space
        worldPos = (u_model * vec4(actualPos, 1.0)).xyz;
        gl_Position = u_modelViewProjection * vec4(actualPos, 1.0);
        
        // assign a color the corner
        // this would be an alternative place to fix the ratio mismatch
        // by adjusting the color before it goes to the fragment shaders
        u_color = actualPos;
    } else {
        // Set to a position outside the normalized device coordinates (NDC) [-1, 1] range
        // effectively discarding the vertex as you can't just discard vertices
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    }
}
