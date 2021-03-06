#version 420
//ambient gbuffer
layout(location = 0) in vec2 inUV;

struct DirectionalLight
{
	//Light direction (defaults to down, to the left, and a little forward)
	vec4 m_lightDirection;

	//Generic Light controls
	vec4 m_lightColor;

	//Ambience controls
	vec4 m_ambientColor;
	float m_ambientPower;
	
	//Power controls
	float m_lightAmbientPower;
	float m_lightSpecularPower;

	float m_minShadowBias;
	float m_maxShadowBias;
	int m_pcfFilterSamples;
};

layout (std140, binding = 0) uniform u_Lights
{
    DirectionalLight ambience;
};

uniform int u_useAmbientLight;
uniform float u_exposure;

layout (binding = 0) uniform sampler2D s_albedoTex;
layout (binding = 4) uniform sampler2D s_lightAccumTex;

out vec4 frag_color;

void main() {
    //albedo 
    vec4 textureColor = texture(s_albedoTex, inUV); 

    //lights 
    vec4 lightAccum = texture(s_lightAccumTex, inUV);

    //ambient calculation
    vec3 ambient = ambience.m_lightAmbientPower * ambience.m_ambientColor.rgb;
	ambient = mix(vec3(0.0), ambient, u_useAmbientLight);

    //add the ambient to the light accumulation 
    vec3 result = (ambient + lightAccum.rgb);
	//do tone mapping on it 
	result = vec3(1.0) - exp(-result * u_exposure);
	//gamma correction
	result = pow(result, vec3(1.0 / 2.2));

	//finally apply it to the texture
	result = result * textureColor.rgb;

    //and output the result
    frag_color = vec4(result, 1.0);
}