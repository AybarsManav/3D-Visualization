#version 330
out vec4 FragColor;

in vec2 TexCoords;

// the front faces texture
uniform sampler2D frontFaces;

// the back faces texture
uniform sampler2D backFaces;

// the volume
uniform sampler3D volumeData;

// contains various rendering options, here stepsize and its reciprocal
uniform vec4 renderOptions; // (stepSize, 1.0f / stepSize, empty, empty)

// this contains the voxels size in normalized coordinates + the reciprocal of the max intensity of the volume
uniform vec4 volumeInfo; // (voxelsize.x, voxelsize.y, voxelsize.z, 1.0f/max vol intensity)

void main()
{
    // start positions from the front face texture
    vec3 samplePos = texture(frontFaces, TexCoords).xyz;

    // ray direction from the direction texture
    vec3 direction = texture(backFaces, TexCoords).xyz - samplePos;

    // we split the ray into the normalized direction and the length
    vec3 ray_direction = normalize(direction);
    float ray_length = length(direction);

    // calculate the number of steps to take (renderOptions.y = 1/stepsize to avoid division here)
    int numSteps = int(ray_length * renderOptions.y);
    vec3 ray_increment = ray_direction * renderOptions.x;

    // track max value
    float maxIntensity = 0.0f;
    for(int i = 0; i < numSteps; i++) {
    
        // sample the volume
        float intensity = float(texture(volumeData, samplePos).r);
        
        // update max value
        maxIntensity = max(intensity, maxIntensity);

        // move the ray forward
        samplePos += ray_increment;
    }
    
    // set the max tracked intensity, normalized with the 1 / maximum volume value
    FragColor = vec4(maxIntensity * volumeInfo.www, 1.0);
}
