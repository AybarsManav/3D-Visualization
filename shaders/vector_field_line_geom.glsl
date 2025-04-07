#version 330
// in come the two vertices of a line segment
layout(lines) in;

// out go the four vertices of a quad
layout(triangle_strip, max_vertices = 4) out;

// uniform containing the data size (xy) and the line width (z)
uniform vec4 visualParams;

void main()
{
	// get the two vertices of the line into NDC
	vec2 startNDC = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
	vec2 endNDC   = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;

	// direction of the line in NDC
	vec2 dir = normalize(endNDC - startNDC);
	
	// vector representing the orthogonal offset to the line in NDC
	vec2 orthogonal = (visualParams.z / max(visualParams.x, visualParams.y)) * vec2( -dir.y, dir.x );

	// emit the four vertices of the quad by offsetting the line vertices by the orthogonal
	gl_Position = vec4( (startNDC + orthogonal) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw );
	EmitVertex();
	
	gl_Position = vec4( (startNDC - orthogonal) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw );
	EmitVertex();
	
	gl_Position = vec4( (endNDC + orthogonal) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw );
	EmitVertex();
	
	gl_Position = vec4( (endNDC - orthogonal) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw );
	EmitVertex();
	
	EndPrimitive();
}