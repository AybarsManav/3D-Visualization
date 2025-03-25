#version 330
out vec4 FragColor;

in vec2 TexCoords;

// texture sampler
uniform sampler2D u_texture;

void main()
{
    FragColor = texture(u_texture, TexCoords);
}