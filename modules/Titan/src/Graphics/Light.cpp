//Titan Engine, by Atlas X Games
// Light.cpp - source file for the class that represents lights in a scene

//precompile header
#include "Titan/ttn_pch.h"
//include the header
#include "Titan/Graphics/Light.h"

namespace Titan {
	//default constructor
	TTN_Light::TTN_Light()
		: m_Color(glm::vec3(1.0f)), m_AmbientStr(0.0f), m_SpecularStr(0.0f),
		m_ConstAttenutation(0.0f), m_LinearAttenuation(0.0f), m_QuadraticAttenuation(0.0f)
	{
		CalculateRadius();
	}

	//default destructor
	TTN_Light::~TTN_Light()
	{ }

	//constructor that sets all the data
	TTN_Light::TTN_Light(glm::vec3 color, float ambientStrength, float specularStrength, float constantAttenuation, float linearAttenuation, float quadraticAttenuation)
		: m_Color(color), m_AmbientStr(ambientStrength), m_SpecularStr(specularStrength),
		m_ConstAttenutation(constantAttenuation), m_LinearAttenuation(linearAttenuation), m_QuadraticAttenuation(quadraticAttenuation)
	{
		CalculateRadius();
		/*lerpDown = true;
		lerpUp = false;*/
	}

	//sets the light's colour
	void TTN_Light::SetColor(glm::vec3 color)
	{
		m_Color = color;
		CalculateRadius();
	}

	//sets the ambient strenght of the light
	void TTN_Light::SetAmbientStrength(float ambientStrength)
	{
		m_AmbientStr = ambientStrength;
	}

	//sets the specular strenght of the light
	void TTN_Light::SetSpecularStrength(float specularStrength)
	{
		m_SpecularStr = specularStrength;
	}

	//sets the constant attenuation value of the light
	void TTN_Light::SetConstantAttenuation(float constantAttenuation)
	{
		m_ConstAttenutation = constantAttenuation;
		CalculateRadius();
	}

	//sets the linear attenuation value of the light
	void TTN_Light::SetLinearAttenuation(float linearAttenuation)
	{
		m_LinearAttenuation = linearAttenuation;
		CalculateRadius();
	}

	//sets the quadratic attenuation value of the light
	void TTN_Light::SetQuadraticAttenuation(float quadraticAttenuation)
	{
		m_QuadraticAttenuation = quadraticAttenuation;
		CalculateRadius();
	}

	void TTN_Light::CalculateRadius()
	{
		float lightMax = glm::max(glm::max(m_Color.r, m_Color.g), m_Color.b);

		m_radius = -m_LinearAttenuation + glm::sqrt(m_LinearAttenuation * m_LinearAttenuation - 4 * m_QuadraticAttenuation *
			(m_ConstAttenutation - (256.0f / 5.f) * lightMax))
			/ (2 * m_QuadraticAttenuation);
	}
}