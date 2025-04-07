#version 330
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 u_modelViewProjection;

out vec2 TexCoord;

void main()
{
	gl_Position = u_modelViewProjection * vec4(aPos.x, aPos.y, 0.0, 1.0);
	TexCoord = vec2(aTexCoord.x, aTexCoord.y);
}