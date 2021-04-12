#version 420
//gbuffer directional
layout(location = 0) in vec2 inUV;

//get gbuffer data
layout (binding = 0) uniform sampler2D s_albedoTex;
layout (binding = 1) uniform sampler2D s_normalsTex;
layout (binding = 2) uniform sampler2D s_specularTex;
layout (binding = 3) uniform sampler2D s_positionTex;
layout (binding = 4) uniform sampler2D s_emissiveTex;

layout (binding = 15) uniform sampler2D s_illbuffer;

//toon shading
layout (binding = 9) uniform sampler2D s_DiffuseRamp;
uniform int u_UseDiffuseRamp = 0;
layout (binding = 10) uniform sampler2D s_SpecularRamp;
uniform int u_useSpecularRamp = 0;

uniform int u_useAmbientLight;
uniform int u_useSpecularLight;

uniform vec3 u_CamPos;

uniform float u_windowWidth;
uniform float u_windowHeight;

//point light data
uniform vec3 u_lightPos;
uniform vec3 u_lightColor;
uniform float u_ambStr;
uniform float u_specStr;
uniform float u_AttenConst;
uniform float u_AttenLine;
uniform float u_AttenQuad;

in vec4 gl_FragCoord;

out vec4 frag_color;

float ReMap(float oMin, float oMax, float nMin, float nMax, float val) {
	float t = (val - oMin) / (oMax - oMin);

	return mix(nMin, nMax, t);
}

// https://learnopengl.com/Advanced-Lighting/Advanced-Lighting
void main() {
	//figure out where we are in screen [0-1] screen space so we can sample the gBuffer
	vec2 screenUV = vec2(gl_FragCoord.x / u_windowWidth, gl_FragCoord.y / u_windowHeight);

    //albedo 
    vec4 textureColor = texture(s_albedoTex, screenUV);
    //normals
    vec3 inNormal = (normalize(texture(s_normalsTex,screenUV).rgb) * 2.0) - 1.0;
    //specular
    float texSpec = texture(s_specularTex,screenUV).r;
	//emissive
	float emissiveStrenght = texture(s_specularTex,screenUV).g;
	//rim
	float rimStrenght = texture(s_specularTex,screenUV).b;
    //positions
    vec3 fragPos = texture(s_positionTex,screenUV).rgb;

	// Diffuse
	vec3 N = normalize(inNormal);
	vec3 lightDir = normalize(u_lightPos - fragPos);
	float dif = max(dot(N, lightDir), 0.0);
	vec3 diffuse = dif * u_lightColor;// add diffuse intensity
	diffuse = mix(diffuse, texture(s_DiffuseRamp, vec2(dif, dif)).xyz, u_UseDiffuseRamp);
	diffuse = mix(vec3(0.0), diffuse, u_useAmbientLight * u_useSpecularLight);

	// Specular
	vec3 viewDir  = normalize(u_CamPos - fragPos);
	vec3 h        = normalize(lightDir + viewDir);

	float spec = pow(max(dot(N, h), 0.0), 4.0); // Shininess coefficient (can be a uniform)
	vec3 specular = u_specStr * texSpec * spec * u_lightColor; // Can also use a specular color
	specular = mix(specular, (texture(s_SpecularRamp, vec2(spec, spec)).xyz), u_useSpecularRamp);
	specular = mix(vec3(0.0), specular, u_useSpecularLight);

	//ambient
	vec3 ambient = (u_ambStr * u_lightColor);
	ambient = mix(vec3(0.0), ambient, u_useAmbientLight);

	//attenuation
	float dist = length(u_lightPos - fragPos);
	float attenuation = 1.0 / (
		u_AttenConst + 
		u_AttenLine * dist +
		u_AttenQuad * dist * dist);

	vec3 result = (0.5 * (ambient + diffuse + specular) * attenuation);

	//if the alpha is less than 31% than it is our clear colour
    if (textureColor.a < 0.31)
    {
		//so just set it to white so there isn't any lighting on the skybox
        result = vec3(1.0,1.0,1.0);
    }

	//pass out the frag color
	frag_color = vec4(result + texture(s_illbuffer, screenUV).rgb, textureColor.a);
}