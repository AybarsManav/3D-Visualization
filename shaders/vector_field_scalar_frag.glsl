#version 330
in vec2 TexCoord;
out vec4 FragColor;

// texture sampler
uniform sampler3D vector_field_texture;
uniform sampler2D tfTexture;

uniform vec4 visualProperties; // active timestep, opacity
uniform vec4 vectorComponents; // x, y, scalar 1, scalar 2
uniform vec4 scalarComponents; // x, y, scalar 1, scalar 2

vec4 RenderComponent(vec4 value)
{    
    if(any(greaterThan(vectorComponents, vec4(0.5)))) // direction is active
    {
        float sumVec = vectorComponents.x + vectorComponents.y;
        if(vectorComponents.z> 0.5f)
        {
            return texture(tfTexture,vec2(length(value.xy),0.5));
        }
        else if(sumVec > 1.5f)
        {
            return vec4(abs(value.x),abs(value.y),0,1);
        }
        else
        {
            float v = (abs(value.x) * vectorComponents.x + abs(value.y) * vectorComponents.y);
            return texture(tfTexture,vec2(v,0.5));
        }
    }
    else if(any(greaterThan(scalarComponents, vec4(0.5)))) // scalar is active
    {
        float sumVec = scalarComponents.x + scalarComponents.y;
        if(sumVec > 1.5f)
        {
            return vec4(abs(value.z),abs(value.w),0,1);
        }
        else
        {
            float v = abs(value.z) * scalarComponents.x + abs(value.w) * scalarComponents.y;
            return texture(tfTexture,vec2(v,0.5));
        }
    }
    else
    {
        return value;
    }

    return vec4(1);
}

void main()
{
    float timeStep = visualProperties.x;  

    vec4 vf_value = texture(vector_field_texture,vec3(TexCoord,timeStep));
    vf_value = RenderComponent(vf_value);

    vf_value.a = visualProperties.y; // set alpha

    FragColor = vf_value;
}