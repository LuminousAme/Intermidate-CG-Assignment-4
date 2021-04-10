//Titan Engine, by Atlas X Games 
// Material.cpp - source file for the class that represents materials on 3D objects

//precompile header
#include "Titan/ttn_pch.h"
//include the header
#include "Titan/Graphics/Material.h"

namespace Titan {
	//default constructor
	TTN_Material::TTN_Material() 
		: m_Shininess(0), m_HeightInfluence(1.0f), m_hasAmbientLighting(true), m_hasSpecularLighting(true), 
		m_hasOutline(false), m_outlineSize(0.0f), m_useDiffuseRamp(false), m_useSpecularRamp(false), m_UseAlbedo(true), m_recievesShadows(true),
		m_emissiveStrenght(1.0f), m_useEmissive(false), m_useNormalMap(false), m_hasRimLighting(false)
	{

		//set the albedo to an all white texture by default
		m_Albedo = basicWhiteTexture;

		//set the specular to an all white texture by default
		m_SpecularMap = basicWhiteTexture;

		//set the cube map to an all white texture by default
		m_SkyboxTexture = basicWhiteSkybox;

		//set the height map to an all black texture by default 
		m_HeightMap = basicWhiteTexture;

		//set the diffuse ramp to an all white texture by default
		m_diffuseRamp = basicWhiteTexture;

		//set the specular ramp to an all white texture by default
		m_specularRamp = basicWhiteTexture;

		//set the emissive texture to an all white texture by default
		m_EmissiveText = basicWhiteTexture;

		//set the normal map to an all white texture by default
		m_NormalMap = basicWhiteTexture;
	}

	//default desctructor
	TTN_Material::~TTN_Material()
	{ }

	void TTN_Material::Init()
	{
		basicWhiteTexture = TTN_Texture2D::Create();
		basicWhiteTexture->Clear(glm::vec4(1.0f));

		basicWhiteSkybox = TTN_TextureCubeMap::Create();
		basicWhiteSkybox->Clear(glm::vec4(1.0f));
	}

	//sets the albedo texture
	void TTN_Material::SetAlbedo(TTN_Texture2D::st2dptr albedo)
	{
		m_Albedo = albedo;
	}

	//function to set wheter or not albedo texturing should be applied to the models
	void TTN_Material::SetUseAlbedo(bool useAlbedo)
	{
		m_UseAlbedo = useAlbedo;
	}

	//sets the shininess
	void TTN_Material::SetShininess(float shininess)
	{
		m_Shininess = shininess;
	}

	//sets the specular map texture
	void TTN_Material::SetSpecularMap(TTN_Texture2D::st2dptr specular)
	{
		m_SpecularMap = specular;
	}

	//sets a cube map texture for a skybox
	void TTN_Material::SetSkybox(TTN_TextureCubeMap::stcmptr Skybox)
	{
		m_SkyboxTexture = Skybox;
	}

	//sets the height map texture
	void TTN_Material::SetHeightMap(TTN_Texture2D::st2dptr height)
	{
		m_HeightMap = height;
	}

	//sets a multipliers for how how influence the height map should have
	void TTN_Material::SetHeightInfluence(float influence)
	{
		m_HeightInfluence = influence;
	}

	//Sets wheter or not this material has ambient lighting
	void TTN_Material::SetHasAmbient(bool hasAmbient)
	{
		m_hasAmbientLighting = hasAmbient;
	}

	//Sets wheter or not this material has specular lighting
	void TTN_Material::SetHasSpecular(bool hasSpecular)
	{
		m_hasSpecularLighting = hasSpecular;
	}

	//Sets wheter or not this material has a line art like outline effect
	void TTN_Material::SetHasOutline(bool hasOutline)
	{
		m_hasOutline = hasOutline;
	}

	//Sets the size (from 0.0 to 1.0) of the line art outline
	void TTN_Material::SetOutlineSize(float outlineSize)
	{
		m_outlineSize = outlineSize;
	}

	//set the diffuse ramp for toon shading
	void TTN_Material::SetDiffuseRamp(TTN_Texture2D::st2dptr ramp)
	{
		m_diffuseRamp = ramp;
	}

	//set wheter or not the diffuse ramp should be sampled by the default shaders
	void TTN_Material::SetUseDiffuseRamp(bool useRamp)
	{
		m_useDiffuseRamp = useRamp;
	}

	//set the specular ramp for toon shading
	void TTN_Material::SetSpecularRamp(TTN_Texture2D::st2dptr ramp)
	{
		m_specularRamp = ramp;
	}

	//set wheter or not the specular ramp should be sampled by the default shaders
	void TTN_Material::SetUseSpecularRamp(bool useRamp)
	{
		m_useSpecularRamp = useRamp;
	}

	//sets wheter or not this material recieves shadows while shadow mapping
	void TTN_Material::SetRecievesShadows(bool recievesShadows)
	{
		m_recievesShadows = recievesShadows;
	}
}