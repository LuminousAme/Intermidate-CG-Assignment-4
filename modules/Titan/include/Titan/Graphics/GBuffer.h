#pragma once

//precompile header, this file uses GLM/glm.hpp and vector
#include "Titan/ttn_pch.h"
#include "Titan/Graphics/Post/Framebuffer.h"
#include "Titan/Graphics/Shader.h"

namespace Titan {
	enum class TTN_Target
	{
		ALBEDO,
		NORMAL,
		SPECULAR,
		POSITION,
		EMISSIVE
	};

	class TTN_GBuffer
	{
	public:
		//defines a special easier to use name for shared(smart) pointers to the class
		typedef std::shared_ptr<TTN_GBuffer> sgbufptr;
		
		//creates and returns a shared(smart) pointer to the class
		static inline sgbufptr Create() {
			return std::make_shared<TTN_GBuffer>();
		}

	public:
		//Initialize this effects (will be overriden in each derived class)
		void Init(unsigned width, unsigned height);

		//Binds the TTN_GBuffer
		void Bind();

		//Bind the lighting
		void BindLighting();

		//Clears the TTN_GBuffer
		void Clear();

		//Unbinds the TTN_GBuffer
		void Unbind();

		//Unbinds the lighting
		void UnbindLighting();

		//Draws out the buffers to the screen
		void DrawBuffersToScreen();

		//draws out only the position buffer
		void DrawPositionBuffer();
		//draws out only the normal buffer 
		void DrawNormalBuffer();
		//draws out only the albedo buffer
		void DrawAlbedoBuffer();
		//draws out only the emissive buffer
		void DrawEmissiveBuffer();

		//Reshape the framebuffer
		void Reshape(unsigned width, unsigned height);

		//Get the gbuffer handle
		GLuint GetHandle() { return m_gBuffer.GetHandle(); }

	private:
		TTN_Framebuffer m_gBuffer;
		TTN_Shader::sshptr m_passThrough;

		int m_windowWidth;
		int m_windowHeight;
	};
}