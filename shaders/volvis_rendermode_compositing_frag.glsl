#version 330
out vec4 FragColor;

in vec2 TexCoords;

// the front faces texture
uniform sampler2D frontFaces;

// the back faces texture
uniform sampler2D backFaces;

// the volume or volume cache when using indirection
uniform sampler3D volumeData;

// the volume indirection lookup
uniform sampler3D volumeIndexData;

// the transferfunction (2D for simplicity, values in y do not change, so it can be sampled with (norm intensity, 0.5)
uniform sampler2D transferFunction;

// this contains the voxels size in normalized coordinates + 0 if using regular texture and 1 when using bricking
uniform vec4 volumeInfo; // (voxelsize.x, voxelsize.y, voxelsize.z, use bricking?)
uniform vec2 volumeMaxValues; // 1/max intensity, 1/max gm

// contains various rendering options, here stepsize and its reciprocal (both in normalized volume space) and the stepsize in the original space, and the toggle to use shading
// Note: we give reciprocals to avoid divisions as they are more expensive than multiplications
// so if we can calculate a reciprocal once outside instead of potentially multiple times every thread it can save a lot of computation
uniform vec4 renderOptions; // (stepSize (adjusted to 0..1 volume coords), 1.0f / stepSize, stepSize (orginal value relative to default), use shading)
uniform vec4 gmParams; // kc, ks, ke, use opacity modulation


// Phong shading constants, defined globally to avoid repeated creation in phongShading function
const float ambientCoefficient = 0.1;
const float diffuseCoefficient = 0.6;
const float specularCoefficient = 0.5;
const int specularPower = 32;

// ======= DO NOT MODIFY THIS FUNCTION ========
// calculate Phong shading the same way as in the CPU renderer
// we give the sample color and return the shaded color
vec3 phongShading(vec3 color, vec4 normal, vec3 L, vec3 V)
{
    // If the gradient magnitude is zero, return black
    if (normal.a == 0.0) {
        return vec3(0.0);
    }

    // Ensure the normal is always oriented towards the viewer
    vec3 N = normalize(normal.xyz);
    if (dot(N, V) < 0.0) {
        N = -N;
    }

    // Phong reflection model (assuming white light, thus no contribution to the color)
    vec3 ambient = ambientCoefficient * color;
    vec3 diffuse = diffuseCoefficient * clamp(dot(L, N), 0.0, 1.0) * color;
    vec3 R = 2.0 * dot(L, N) * N - L;
    vec3 specular = specularCoefficient * pow(clamp(dot(R, V), 0.0, 1.0), specularPower) * color;

    return ambient + diffuse + specular;
}

// ======= TODO: IMPLEMENT ========
//
// Part of **1. Basic Volume Rendering**
//
// This function should calculate the gradient on the fly at the current samplePos
// Gives the voxelSize (in normalized volume coordinates) for the offset
// The return is a vec4 intended to contain the gradient magnitude in the fourth component though not strictly necessary
// Note: The function can be cpoied over to iso-surface shader once implemented
vec4 calculateGradient(vec3 samplePos, vec3 voxelSize)
{
    vec4 gradient = vec4(0.0f);

    return gradient;
}


// ======= TODO: IMPLEMENT ========
//
// Part of **1. Basic Volume Rendering**
//
// This function should calculate the compositing
// We give the ray set up for front to back following MIP
// In addition to the CPU version, here you must
// Use front to back compositing
// Implement early ray termination
// Implement the gradient magnitude opacity weighting following Rheingans and Ebert
// Implement the gradient calculation on-the-fly instead of from a precomputed volume (can be reused in iso surface shader)
//
// Part of **3. Volume Bricking**
//
// Update the code to use the sampling indirection using volumeIndexData
// Consider adding a function to separate the sampling that you can also add to the isosurface
void main()
{
    // start positions from the front face texture
    vec3 samplePos = texture(frontFaces, TexCoords).xyz;

    // ray direction from the direction texture
    vec3 direction = texture(backFaces, TexCoords).xyz - samplePos;


    // ======= TODO: IMPLEMENT ========
    //
    // Part of **3. Volume Bricking**
    //
    // fix potential size differences between the bricked volume and the original volume
    // The index volume is the size of the volume in bricks
    // If the volume is not an exact multiple of the brickSize this will create a hypothetical volume larger than the original one
    // e.g., volume = 10x10x10 and the bricksize is 6xx6x6
    //       the index volume will be 2x2x2
    //       now we can sample in normalized coordinates in that volume, but that means we are sampling in a hypothetical
    //       2x2x2 * 6x6x6 = 12x12x12 volume.
    // There are various ways to fix this. As we do not bother you with the geometry in this assignment
    //  a simple way to fix it is here in the shader by adjusting the samplePos and direction
    // NOTE: a different place would be in the gpu_optimization_vert.glsl shader
    // NOTE: not all needed information is provided. You need to add additional uniform(s)
    if(volumeInfo.w > 0.5f){
    }

    // we split the ray into the normalized direction and the length
    vec3 ray_direction = normalize(direction / volumeInfo.xyz);
    float ray_length = length(direction / volumeInfo.xyz);

    int numSteps = int(ray_length / renderOptions.z);
    vec3 ray_increment = ray_direction * volumeInfo.xyz * renderOptions.z;

    vec4 color = vec4(0.0f);
    for(int i = 0; i < numSteps; i++) {

        // ======= TODO: IMPLEMENT ========
        // replace this with your code
        color = vec4(samplePos, 0.0f);
    }

    // this sets the final color to the pixel
    FragColor = color;
}