//include titan's precompile header
#include "Titan/ttn_pch.h"
#include "Titan/Graphics/Post/Framebuffer.h"
#include "Titan/Graphics/IlluminationBuffer.h"

namespace Titan {
	void TTN_IlluminationBuffer::Init(unsigned width, unsigned height)
	{
		//composite buffer
		int index = int(m_buffers.size());
		m_buffers.push_back(TTN_Framebuffer::Create());
		m_buffers[index]->AddColorTarget(GL_RGB8);
		m_buffers[index]->AddDepthTarget();
		m_buffers[index]->Init(width, height);

		// illum buffer
		index = int(m_buffers.size());
		m_buffers.push_back(TTN_Framebuffer::Create());
		m_buffers[index]->AddColorTarget(GL_RGBA16F);
		m_buffers[index]->AddDepthTarget();
		m_buffers[index]->Init(width, height);

		// illum buffer 2 (for ping pong)
		index = int(m_buffers.size());
		m_buffers.push_back(TTN_Framebuffer::Create());
		m_buffers[index]->AddColorTarget(GL_RGBA16F);
		m_buffers[index]->AddDepthTarget();
		m_buffers[index]->Init(width, height);

		//load directional TTN_GBuffer shader
		index = int(m_shaders.size());
		m_shaders.push_back(TTN_Shader::Create());
		m_shaders[index]->LoadShaderStageFromFile("shaders/Post/ttn_passthrough_vert.glsl", GL_VERTEX_SHADER);
		m_shaders[index]->LoadShaderStageFromFile("shaders/ttn_gBuffer_directional_frag.glsl", GL_FRAGMENT_SHADER);
		m_shaders[index]->Link();

		//load ambient TTN_GBuffer shader
		index = int(m_shaders.size());
		m_shaders.push_back(TTN_Shader::Create());
		m_shaders[index]->LoadShaderStageFromFile("shaders/Post/ttn_passthrough_vert.glsl", GL_VERTEX_SHADER);
		m_shaders[index]->LoadShaderStageFromFile("shaders/ttn_gBuffer_ambient_frag.glsl", GL_FRAGMENT_SHADER);
		m_shaders[index]->Link();

		//load passthrough shader
		index = int(m_shaders.size());
		m_shaders.push_back(TTN_Shader::Create());
		m_shaders[index]->LoadShaderStageFromFile("shaders/Post/ttn_passthrough_vert.glsl", GL_VERTEX_SHADER);
		m_shaders[index]->LoadShaderStageFromFile("shaders/Post/ttn_passthrough_frag.glsl", GL_FRAGMENT_SHADER);
		m_shaders[index]->Link();

		//allocates sun buffer
		m_sunBuffer.AllocateMemory(sizeof(TTN_DirectionalLight));

		//if sun enabled, send data
		if (m_sunEnabled) {
			m_sunBuffer.SendData(reinterpret_cast<void*>(&m_sun), sizeof(TTN_DirectionalLight));
		}

		//make diffuse and specular ramps
		m_diffuseRamp = m_diffuseRamp->Create();
		m_diffuseRamp->Clear();
		m_specularRamp = m_specularRamp->Create();
		m_specularRamp->Clear();

		m_useDiffuseRamp = false;
		m_useSpecularRamp = false;

		TTN_PostEffect::Init(width, height);
	}

	void TTN_IlluminationBuffer::Setup()
	{
		//load the mesh
		/*s_sphereMesh = TTN_ObjLoader::LoadFromFile("lightVolumeSphere.obj");
		s_sphereMesh->SetUpVao();*/
		s_coneMesh = TTN_ObjLoader::LoadFromFile("cone.obj");
		s_coneMesh->SetUpVao();
		/*	s_cubeMesh = TTN_ObjLoader::LoadFromFile("cube.obj");
			s_cubeMesh->SetUpVao();*/

			//load the shaders
		s_pointLightShader = TTN_Shader::Create();
		s_pointLightShader->LoadShaderStageFromFile("shaders/ttn_gBuffer_point_vert.glsl", GL_VERTEX_SHADER);
		s_pointLightShader->LoadShaderStageFromFile("shaders/ttn_gBuffer_point_frag.glsl", GL_FRAGMENT_SHADER);
		s_pointLightShader->Link();

		s_lightVolumeShader = TTN_Shader::Create();
		s_lightVolumeShader->LoadShaderStageFromFile("shaders/ttn_gBuffer_point_vert.glsl", GL_VERTEX_SHADER);
		s_lightVolumeShader->LoadShaderStageFromFile("shaders/ttn_point_light_volume_frag.glsl", GL_FRAGMENT_SHADER);
		s_lightVolumeShader->Link();
	}

	void TTN_IlluminationBuffer::ApplyEffect(TTN_GBuffer::sgbufptr gBuffer)
	{
		//blit the gbuffer's depth over
		glm::ivec2 windowSize = TTN_Backend::GetWindowSize();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer->GetHandle());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_buffers[1]->GetHandle());
		glBlitFramebuffer(0, 0, windowSize.x, windowSize.y, 0, 0, windowSize.x, windowSize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, GL_NONE);

		//don't write to the light's depth buffer
		glDepthMask(GL_FALSE);

		//send direcitonal light data
		m_sunBuffer.SendData(reinterpret_cast<void*>(&m_sun), sizeof(TTN_DirectionalLight));

		//directional light
		if (m_sunEnabled) {
			//binds directional light shader
			m_shaders[TTN_Lights::DIRECTIONAL]->Bind();
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("u_CamPos", m_camPos);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("rimColor", m_rimColor);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("emissivePower", m_emissiveStrenght);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("u_SplitRanges", m_splitRanges[0], 4);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniformMatrix("u_lightViewProj", m_lightSpaceViewProj[0], 4);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniformMatrix("u_vp", m_viewMat);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("u_useAmbientLight", (int)m_useAmbient);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("u_useSpecularLight", (int)m_useSpecular);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("u_useShadows", (int)m_useShadowMapping);

			//bind sun uniform
			m_sunBuffer.Bind(0);

			//bind the gBuffer for lighting
			gBuffer->BindLighting();

			//bind the shadow buffer's depth
			m_shadowBuffer->BindDepthAsTexture(30);

			//bind the ramps
			m_diffuseRamp->Bind(9);
			m_specularRamp->Bind(10);
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("u_UseDiffuseRamp", int(m_useDiffuseRamp));
			m_shaders[TTN_Lights::DIRECTIONAL]->SetUniform("u_useSpecularRamp", int(m_useSpecularRamp));

			//binds and draws to illumination buffer
			m_buffers[1]->RenderToFSQ();

			gBuffer->UnbindLighting();

			//unbuinds the uniform buffers
			m_sunBuffer.Unbind(0);

			//unbind shader
			m_shaders[TTN_Lights::DIRECTIONAL]->UnBind();

			//copy to spare buffer
			m_shaders[m_shaders.size() - 1]->Bind();
			m_buffers[1]->BindColorAsTexture(0, 0);

			m_buffers[2]->RenderToFSQ();

			m_buffers[1]->UnbindTexture(0);
			m_shaders[m_shaders.size() - 1]->UnBind();
		}

	
		//point lights 
		for (int i = 0; i < m_lights.size(); i++) {
			//glCullFace(GL_FRONT);

			s_pointLightShader->Bind();
			s_pointLightShader->SetUniform("u_CamPos", m_camPos);
			s_pointLightShader->SetUniform("u_windowWidth", float(windowSize.x));
			s_pointLightShader->SetUniform("u_windowHeight", float(windowSize.y));
			m_diffuseRamp->Bind(9);
			m_specularRamp->Bind(10);
			s_pointLightShader->SetUniform("u_useAmbientLight", (int)m_useAmbient);
			s_pointLightShader->SetUniform("u_useSpecularLight", (int)m_useSpecular);
			s_pointLightShader->SetUniform("u_UseDiffuseRamp", int(m_useDiffuseRamp));
			s_pointLightShader->SetUniform("u_useSpecularRamp", int(m_useSpecularRamp));
			//bind the gBuffer for lighting
			gBuffer->BindLighting();
			m_buffers[1]->Bind();
			m_buffers[2]->BindColorAsTexture(0, 15);

			s_pointLightShader->SetUniform("u_lightPos", m_lights[i].GetPosition());
			s_pointLightShader->SetUniform("u_lightColor", m_lights[i].GetColor());
			s_pointLightShader->SetUniform("u_ambStr", m_lights[i].GetAmbientStrength());
			s_pointLightShader->SetUniform("u_specStr", m_lights[i].GetSpecularStrength());
			s_pointLightShader->SetUniform("u_AttenConst", m_lights[i].GetConstantAttenuation());
			s_pointLightShader->SetUniform("u_AttenLine", m_lights[i].GetLinearAttenuation());
			s_pointLightShader->SetUniform("u_AttenQuad", m_lights[i].GetQuadraticAttenuation());
			
			//set the position and scale
			s_volumeTrans.SetPos(m_lights[i].GetPosition());
			s_volumeTrans.SetScale(glm::vec3(m_lights[i].GetRadius()));
			
			//make the mvp matrix
			glm::mat4 mvp = m_vp * s_volumeTrans.GetGlobal();
			s_pointLightShader->SetUniformMatrix("MVP", mvp);
			
			s_sphereMesh->GetVAOPointer()->Render();

			m_buffers[1]->Unbind();
			m_buffers[2]->UnbindTexture(0);
			s_pointLightShader->UnBind();
			gBuffer->UnbindLighting();
			//glEnable(GL_CULL_FACE);
			//glCullFace(GL_BACK);

			//copy to spare buffer
			m_shaders[m_shaders.size() - 1]->Bind();
			m_buffers[1]->BindColorAsTexture(0, 0);

			m_buffers[2]->RenderToFSQ();

			m_buffers[1]->UnbindTexture(0);
			m_shaders[m_shaders.size() - 1]->UnBind();
		}


		//write to the depth buffer again
		glDepthMask(GL_TRUE);

		//ambient lighting and adding it to the existing images

		//bind ambient shader
		m_shaders[TTN_Lights::AMBIENT]->Bind();

		//binds direcitonal light data
		m_sunBuffer.Bind(0);

		//binds for lighting
		gBuffer->BindLighting();
		m_buffers[1]->BindColorAsTexture(0, 4);
		m_buffers[0]->BindColorAsTexture(0, 5);

		m_shaders[TTN_Lights::AMBIENT]->SetUniform("u_useAmbientLight", (int)m_useAmbient);
		m_shaders[TTN_Lights::AMBIENT]->SetUniform("u_exposure", m_exposure);

		m_buffers[0]->RenderToFSQ();

		m_buffers[0]->UnbindTexture(5);
		m_buffers[1]->UnbindTexture(4);

		gBuffer->UnbindLighting();

		//unbinds uniform buffer
		m_sunBuffer.Unbind(0);
		m_shaders[TTN_Lights::AMBIENT]->UnBind();
	}

	void TTN_IlluminationBuffer::DrawIllumBuffer()
	{
		m_shaders[m_shaders.size() - 1]->Bind();
		m_buffers[1]->BindColorAsTexture(0, 0);

		TTN_Framebuffer::DrawFullScreenQuad();

		m_buffers[1]->UnbindTexture(0);

		m_shaders[m_shaders.size() - 1]->UnBind();
	}

	void TTN_IlluminationBuffer::SetViewMat(glm::mat4 view)
	{
		m_viewMat = view;
	}

	void TTN_IlluminationBuffer::SetLightSpaceMatrices(glm::mat4 mats[])
	{
		m_lightSpaceViewProj[0] = mats[0];
		m_lightSpaceViewProj[1] = mats[1];
		m_lightSpaceViewProj[2] = mats[2];
		m_lightSpaceViewProj[3] = mats[3];
	}

	void TTN_IlluminationBuffer::SetSplitRanges(float splits[])
	{
		m_splitRanges[0] = splits[0];
		m_splitRanges[1] = splits[1];
		m_splitRanges[2] = splits[2];
		m_splitRanges[3] = splits[3];
	}

	void TTN_IlluminationBuffer::SetFarClip(float farClip)
	{
		m_farClip = farClip;
	}

	void TTN_IlluminationBuffer::SetCamPos(glm::vec3 camPos)
	{
		m_camPos = camPos;
	}

	void TTN_IlluminationBuffer::SetShadowBuffer(TTN_Framebuffer::sfboptr shadowBuffer)
	{
		m_shadowBuffer = shadowBuffer;
	}

	TTN_DirectionalLight& TTN_IlluminationBuffer::GetSunRef()
	{
		return m_sun;
	}

	void TTN_IlluminationBuffer::SetSun(TTN_DirectionalLight newSun)
	{
		m_sun = newSun;
	}

	void TTN_IlluminationBuffer::SetSun(glm::vec4 lightDir, glm::vec4 lightCol)
	{
		m_sun.m_lightDirection = lightDir;
		m_sun.m_lightColor = lightCol;
	}

	void TTN_IlluminationBuffer::EnableSun(bool enabled)
	{
		m_sunEnabled = enabled;
	}

	//renders any point lights to the currently bound framebuffer
	void TTN_IlluminationBuffer::RenderPointLightVolumes()
	{
		//bind the shader
		s_lightVolumeShader->Bind();

		//set the polygon mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		//render all of the spheres
		for (int i = 0; i < m_lights.size(); i++) {
			if (m_lights[i].GetVolumeShouldRender()) {
				//set the position and scale
				s_volumeTrans.SetPos(m_lights[i].GetPosition());
				s_volumeTrans.SetScale(glm::vec3(m_lights[i].GetRadius()));

				//make the mvp matrix
				glm::mat4 mvp = m_vp * s_volumeTrans.GetGlobal();
				s_lightVolumeShader->SetUniformMatrix("MVP", mvp);

				//set the fragment shader uniforms
				s_lightVolumeShader->SetUniform("u_lightColor", m_lights[i].GetColor());
				s_lightVolumeShader->SetUniform("u_alpha", m_lights[i].GetVolumeTransparency());

				//render
				//s_sphereMesh->GetVAOPointer()->Render();
				s_coneMesh->GetVAOPointer()->Render();
				//s_cubeMesh->GetVAOPointer()->Render();
			}
		}

		//reset the polygon mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		//unbind the shader
		s_lightVolumeShader->UnBind();
	}
}