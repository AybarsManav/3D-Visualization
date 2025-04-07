#version 330
// output color
out vec4 FragColor;

// input texture coordinates containing the position in xy
in vec2 TexCoord;

// texture samplers for vector field and noise texture
uniform sampler3D vfTexture;
uniform sampler2D noiseTexture;

// Parameters provided by the CPU for the LIC algorithm
uniform vec4 visualProperties; // (active timestep adjusted to texture coordinates, 1 / dim x, 1 / dim y, kernel width)

// ======= TODO: IMPLEMENT ========
//
// Assignment 3, Part 3: Line Integral Convolution
void main()
{
    // For readability assign the uniform parameters to variables
    // The timestep to sample the 3D vector field volume
    float timeStep = visualProperties.x;
    // The kernel width for the LIC algorithm
    int kernelWidth= int(visualProperties.w);
    // The scale factor to bring the sampled vector into texture coordinate space
    vec2 scale = visualProperties.yz;

    // Our current position in xy is provided as the texture coordinate
    // Given that we store the timestep in the visualProperties uniform combine it with the TexCoord parameter to get our actual sample position
    vec3 samplePosition = vec3(TexCoord, timeStep);

    // We can sample the noise texture at the current position like this
    // The noise texture is a 2D texture, so we sample it with the xy coordinates
    // float greyValue = texture (noiseTexture, samplePosition.xy).s; // Not used

    // TODO: implement the smoothing according to the vector field
    // Use the kernelWidth as the number of steps to take in each direction (i.e., the actual width of your kernel should be 2*kernelWidth+1)
    // For simplicity, you can use a simple averaging kernel instead of a Gaussian kernel for smoothing along the vector field
    // To sample the noise texture, you need to calculate the positions along the kernel by advancing the vector field
    // Do this by sampling the vector field texture (vfTexture) at the current position and advancing samplePosition.xy according to the sampled vector
    // vfTexture is a 3D texture. The z coordinate is the timestep set in the GUI which is fixed for LIC. Thus you never have to update samplePosition.z.
    // Note that you need to scale the sampled vector by the scale factor before advancing the position
    // Remember that the kernel is centered around the current position, so you need to advance in both directions from the current position

    float acc = 0.0f;
    float weight = 0.0f;

    // Center
    acc += texture(noiseTexture, samplePosition.xy).r;
    weight += 1;

    // Forward direction
    vec3 currentPosition = samplePosition;
    for (int i = 0; i < kernelWidth; ++i) {
        vec2 dir = normalize(texture(vfTexture, currentPosition).xy); // Get the direction in the current sample pos
        currentPosition += vec3(dir * scale, timeStep);
        acc += texture(noiseTexture, currentPosition.xy).r; // No need to check the bounds because texture is setup with GL_CLAMP_TO_EDGE
        weight += 1;
    }

    // Backward direction
    currentPosition = samplePosition; // Reset current position to center
        for (int i = 0; i < kernelWidth; ++i) {
        vec2 dir = normalize(texture(vfTexture, currentPosition).xy); // Get the direction in the current sample pos
        currentPosition -= vec3(dir * scale, timeStep);
        acc += texture(noiseTexture, currentPosition.xy).r;  // No need to check the bounds because texture is setup with GL_CLAMP_TO_EDGE
        weight += 1;
    }

    acc = acc / weight;
    // When done we assign the grey value to the output color
    FragColor = vec4(acc, acc, acc, 1);
}