#version 420
//gbuffer directional
layout(location = 0) in vec2 inUV;

uniform vec3 u_lightColor;
uniform float u_alpha;

out vec4 frag_color;

void main() {
	//pass out the frag color
	frag_color = vec4(u_lightColor, u_alpha);
}