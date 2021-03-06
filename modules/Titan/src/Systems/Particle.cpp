//Titan Engine, by Atlas X Games
// Partilce.cpp - source file for the class that represents a particle system

//precompile header, this file uses GLM/gtx/transform.hpp
#include "Titan/ttn_pch.h"
//include the header
#include "Titan/Systems/Particle.h"

//code refernce: https://www.youtube.com/watch?v=GK0jHlv3e3w&t=515s

namespace Titan {
	//default constructor
	TTN_ParticleSystem::TTN_ParticleSystem()
	{
		m_rotation = glm::vec3(0.0f);
		m_emitterShape = TTN_ParticleEmitterShape::SPHERE;
		m_EmitterAngle = 15.0f;
		m_EmitterScale = glm::vec3(1.0f);
		m_emissionRate = 5.0f;
		m_particle = TTN_ParticleTemplate();
		m_duration = 5.0f;
		m_loop = true;
		m_paused = false;
		m_emissionTimer = 0.0f;

		m_maxParticlesCount = 1000;
		m_durationRemaining = m_duration;
		m_activeParticleIndex = m_maxParticlesCount - 1;
		m_vao = TTN_VertexArrayObject::Create();

		//reverse memory space for all the particle data
		SetUpData();

		//set up function pointers
		readGraphVelo = &defaultReadGraph;
		readGraphColor = &defaultReadGraph;
		readGraphRotation = &defaultReadGraph;
		readGraphScale = &defaultReadGraph;
		readGraphAccelleration = &defaultReadGraph;

		SetUpRenderingStuff();
	}

	//constructor that takes in data
	TTN_ParticleSystem::TTN_ParticleSystem(size_t maxParticles, float emissionRate, TTN_ParticleTemplate particleTemplate,
		float duration, bool loop)
		: m_maxParticlesCount(maxParticles), m_emissionRate(emissionRate), m_particle(particleTemplate),
		m_duration(duration), m_loop(loop)
	{
		m_paused = false;

		//reverse memory space for all the particle data
		SetUpData();

		//setup the rest of the data
		m_durationRemaining = m_duration;
		m_activeParticleIndex = m_maxParticlesCount - 1;
		m_vao = TTN_VertexArrayObject::Create();
		m_rotation = glm::vec3(0.0f);
		m_emitterShape = TTN_ParticleEmitterShape::SPHERE;
		m_EmitterAngle = 15.0f;
		m_EmitterScale = glm::vec3(0.0f);
		m_emissionTimer = 0.0f;

		//set up function pointers
		readGraphVelo = &defaultReadGraph;
		readGraphColor = &defaultReadGraph;
		readGraphRotation = &defaultReadGraph;
		readGraphScale = &defaultReadGraph;
		readGraphAccelleration = &defaultReadGraph;

		SetUpRenderingStuff();

		VertexPosVBO->LoadData(m_particle._mesh->GetVertexPositions().data(), m_particle._mesh->GetVertexPositions().size());
		VertexNormVBO->LoadData(m_particle._mesh->GetVertexNormals().data(), m_particle._mesh->GetVertexNormals().size());
		VertexUVVBO->LoadData(m_particle._mesh->GetVertexUvs().data(), m_particle._mesh->GetVertexUvs().size());
	}

	TTN_ParticleSystem::~TTN_ParticleSystem()
	{
		delete[] Positions;
		delete[] StartColors;
		delete[] EndColors;
		delete[] StartVelocities;
		delete[] EndVelocities;
		delete[] StartScales;
		delete[] EndScales;
		delete[] StartAccelerations;
		delete[] EndAccelerations;
		delete[] acceleratingVelocity;
		delete[] timeAlive;
		delete[] lifeTimes;
		delete[] Active;

		delete[] particle_col;
		delete[] particle_pos;
		delete[] particle_scale;
	}

	//set up the shaders for the particle system
	void TTN_ParticleSystem::InitParticleShader()
	{
		s_particleShaderProgram = TTN_Shader::Create();
		s_particleShaderProgram->LoadShaderStageFromFile("shaders/ttn_particle_vert.glsl", GL_VERTEX_SHADER);
		s_particleShaderProgram->LoadShaderStageFromFile("shaders/ttn_particle_frag.glsl", GL_FRAGMENT_SHADER);
		s_particleShaderProgram->Link();

		//init the default particle texture too
		s_defaultWhiteTexture = TTN_Texture2D::LoadFromFile("textures/ttn_particle_default.png");

		//also do set up for the 2D sprite renderer 
		s_spriteParticleShader = TTN_Shader::Create();
		s_spriteParticleShader->LoadShaderStageFromFile("shaders/ttn_sprite_particle_vert.glsl", GL_VERTEX_SHADER);
		s_spriteParticleShader->LoadShaderStageFromFile("shaders/ttn_sprite_particle_geo.glsl", GL_GEOMETRY_SHADER);
		s_spriteParticleShader->LoadShaderStageFromFile("shaders/ttn_sprite_particle_frag.glsl", GL_FRAGMENT_SHADER);
		s_spriteParticleShader->Link();

		s_spriteVertexPosVBO = TTN_VertexBuffer::Create();
		s_spriteVertexPosVBO->LoadData(&s_particleSpriteVertexPos, 1);
	}

	//sets up the particle system as a cone
	void TTN_ParticleSystem::MakeConeEmitter(float angle, glm::vec3 emitterRotation)
	{
		m_EmitterAngle = angle;
		m_rotation = glm::radians(emitterRotation);
		m_emitterShape = TTN_ParticleEmitterShape::CONE;
	}

	//sets up the particle system as a cirlce
	void TTN_ParticleSystem::MakeCircleEmitter(glm::vec3 emitterRotation)
	{
		m_rotation = glm::radians(emitterRotation);
		m_emitterShape = TTN_ParticleEmitterShape::CIRCLE;
	}
	
	//sets up the particle system as a sphere
	void TTN_ParticleSystem::MakeSphereEmitter()
	{
		m_emitterShape = TTN_ParticleEmitterShape::SPHERE;
	}

	//sets up the particle system as a cube
	void TTN_ParticleSystem::MakeCubeEmitter(glm::vec3 scale, glm::vec3 emitterRotation)
	{
		m_EmitterScale = scale;
		m_rotation = glm::radians(emitterRotation);
		m_emitterShape = TTN_ParticleEmitterShape::CUBE;
	}

	//sets the angle of a cone emitter
	void TTN_ParticleSystem::SetEmitterAngle(float angle)
	{
		m_EmitterAngle = angle;
	}

	//set the scale of a cube emitter
	void TTN_ParticleSystem::SetEmitterScale(glm::vec3 scale)
	{
		m_EmitterScale = scale;
	}

	//set how long the particle system effect will last
	void TTN_ParticleSystem::SetDuration(float duration)
	{
		m_duration = duration;
		m_durationRemaining = m_duration;
	}

	//set wheter or not the effect should loop
	void TTN_ParticleSystem::SetShouldLoop(bool shouldLoop)
	{
		m_loop = shouldLoop;
	}

	//set the particle template it copies from
	void TTN_ParticleSystem::SetParticleTemplate(TTN_ParticleTemplate particleTemplate)
	{
		m_particle = particleTemplate;

		VertexPosVBO->LoadData(m_particle._mesh->GetVertexPositions().data(), m_particle._mesh->GetVertexPositions().size());
		VertexNormVBO->LoadData(m_particle._mesh->GetVertexNormals().data(), m_particle._mesh->GetVertexNormals().size());
		VertexUVVBO->LoadData(m_particle._mesh->GetVertexUvs().data(), m_particle._mesh->GetVertexUvs().size());
	}

	//set the rate at which particles are emitted (particles/second)
	void TTN_ParticleSystem::SetEmissionRate(float emissionRate)
	{
		m_emissionRate = emissionRate;
	}

	//set the rotation of the emitter for cone, circle, and cube emitters
	void TTN_ParticleSystem::SetEmitterRotation(glm::vec3 rotation)
	{
		m_rotation = glm::radians(rotation);
	}

	//sets wheter or not the particle system is paused
	void TTN_ParticleSystem::SetPaused(bool paused)
	{
		m_paused = paused;
	}

	//sets the function pointer for the readgraph used in lerping velocity
	void TTN_ParticleSystem::VelocityReadGraphCallback(float(*function)(float))
	{
		readGraphVelo = function;
	}

	//sets the function pointer for the readgraph used in lerping color
	void TTN_ParticleSystem::ColorReadGraphCallback(float(*function)(float))
	{
		readGraphColor = function;
	}

	//sets the function pointer for the readgraph used in lerping color
	void TTN_ParticleSystem::RotationReadGraphCallback(float(*function)(float))
	{
		readGraphRotation = function;
	}

	void TTN_ParticleSystem::ScaleReadGraphCallback(float(*function)(float))
	{
		readGraphScale = function;
	}

	void TTN_ParticleSystem::accelerationReadGraph(float(*function)())
	{
	}

	//updates the particle system
	void TTN_ParticleSystem::Update(float deltaTime)
	{
		//only run if the particle system is not paused
		if (!m_paused) {
			//only emit new particles if it still has durtation remainig or doesn't but is looping
			if ((m_durationRemaining > 0.0f || (m_durationRemaining <= 0.0f && m_loop)) && (m_elapsedTime < m_stopTime || m_stopTime < 0.0f))
			{
				//emit new particles
				m_emissionTimer += deltaTime;

				while (m_emissionTimer > 1.0f / m_emissionRate) {
					Emit();
					m_emissionTimer -= 1.0f / m_emissionRate;
				}

				size_t NumOfNewParticles = static_cast<size_t>((double)m_emissionRate * (double)deltaTime);
				for (size_t i = 0; i < NumOfNewParticles; i++) {
					Emit();
				}

				m_elapsedTime += deltaTime;
				m_durationRemaining -= deltaTime;
			}
			//if it doesn't have duration left but should loop then loop it
			if (m_durationRemaining <= 0.0f && m_loop) {
				m_durationRemaining = m_duration;
			}

			//iterate over all the particles
			for (size_t i = 0; i < m_maxParticlesCount; i++) {
				//if it's not alive just skip it
				if (!Active[i])
					continue;
				//if it's gone through it's lifetime, deactive it and skip to the next one
				if (timeAlive[i] >= lifeTimes[i]) {
					Active[i] = false;
					continue;
				}

				//update how long the particle has been alive
				timeAlive[i] += deltaTime;

				//get a t value for interpolation
				float t = std::clamp(timeAlive[i] / lifeTimes[i], 0.0f, 1.0f);

				//update acceleterating velocity 
				acceleratingVelocity[i] += glm::mix(StartAccelerations[i], EndAccelerations[i], t) * deltaTime;

				
				//update the position of the particlce based on the interpolation of the velocities
				Positions[i] += glm::mix(StartVelocities[i], EndVelocities[i], readGraphVelo(t)) * deltaTime + acceleratingVelocity[i] * deltaTime + 
					glm::mix(StartAccelerations[i], EndAccelerations[i], t) * (deltaTime * deltaTime);
			}
		}
	}

	//renders all the active particles
	void TTN_ParticleSystem::Render(glm::vec3 ParentGlobalPos, glm::mat4 view, glm::mat4 projection)
	{
		if (m_isSprites) {
			glDepthMask(GL_FALSE);

			//bind the shader
			s_spriteParticleShader->Bind();

			//set uniforms
			glm::mat4 temp_model = glm::translate(ParentGlobalPos) * glm::toMat4(glm::quat(glm::vec3(0.0f, 0.0f, 0.0f))) * glm::scale(glm::vec3(1.0f));
			glm::mat4 tempVP = projection;
			tempVP *= view;
			s_spriteParticleShader->SetUniformMatrix("u_model", temp_model);
			s_spriteParticleShader->SetUniformMatrix("u_vp", tempVP);
			s_spriteParticleShader->SetUniformMatrix("u_modelView", view * temp_model);

			//bind the albedo texture from the mat
			if (m_spriteParticleTexture != nullptr) {
				m_spriteParticleTexture->Bind(0);
			}
			//if it doesn't have a texture in the mat set a default white texture
			else {
				s_defaultWhiteTexture->Bind(0);
			}

			size_t numOfActiveParticles = 0;
			//go through all the particles and set up their data for rendering
			for (size_t i = 0; i < m_maxParticlesCount; i++) {
				//if the particle isn't active, just skip to the next one
				if (!Active[i])
					continue;

				//get a t value for interpolation
				float t = std::clamp(timeAlive[i] / lifeTimes[i], 0.0f, 1.0f);

				//interpolate the color
				glm::vec4 temp_col = glm::mix(StartColors[i], EndColors[i], readGraphColor(t));
				//interpolate the scale
				float temp_scale = glm::mix(StartScales[i], EndScales[i], readGraphScale(t));
				//get the global position of the particle
				glm::vec3 temp_pos = Positions[i];

				//save the color
				particle_col[numOfActiveParticles] = temp_col;
				particle_pos[numOfActiveParticles] = temp_pos;
				particle_scale[numOfActiveParticles] = temp_scale;

				numOfActiveParticles++;
			}
			//if there are particles to acutally be rendered, render them, if not just exit the function
			if (numOfActiveParticles > 0) {
				//manually set up the buffers and vao since titan doesn't currently have the infastructure to render instanced stuff automatically

				ColorInstanceBuffer->LoadData(particle_col, numOfActiveParticles);

				PositionInstanceBuffer->LoadData(particle_pos, numOfActiveParticles);

				ScaleInstanceBuffer->LoadData(particle_scale, numOfActiveParticles);

				s_spriteVAO->Bind();

				glDrawArraysInstanced(GL_POINTS, 0, 1, numOfActiveParticles);

				s_spriteVAO->UnBind();
			}

			glDepthMask(GL_TRUE);
			//unbind the shader
			s_spriteParticleShader->UnBind();
		}
		else {
			//bind the shader
			s_particleShaderProgram->Bind();

			//set uniforms
			glm::mat4 temp_model = glm::translate(ParentGlobalPos) * glm::toMat4(glm::quat(glm::vec3(0.0f, 0.0f, 0.0f))) * glm::scale(glm::vec3(1.0f));
			glm::mat4 tempMVP = projection;
			tempMVP *= view;
			s_particleShaderProgram->SetUniformMatrix("u_model", temp_model);
			s_particleShaderProgram->SetUniformMatrix("u_mvp", tempMVP * temp_model);
			s_particleShaderProgram->SetUniformMatrix("u_normalMat", glm::mat3(glm::transpose(glm::inverse(temp_model))));

			//bind the albedo texture from the mat
			if (m_particle._mat->GetAlbedo() != nullptr) {
				m_particle._mat->GetAlbedo()->Bind(0);
			}
			//if it doesn't have a texture in the mat set a default white texture
			else {
				s_defaultWhiteTexture->Bind(0);
			}

			size_t numOfActiveParticles = 0;
			//go through all the particles and set up their data for rendering
			for (size_t i = 0; i < m_maxParticlesCount; i++) {
				//if the particle isn't active, just skip to the next one
				if (!Active[i])
					continue;

				//get a t value for interpolation
				float t = std::clamp(timeAlive[i] / lifeTimes[i], 0.0f, 1.0f);

				//interpolate the color
				glm::vec4 temp_col = glm::mix(StartColors[i], EndColors[i], readGraphColor(t));
				//interpolate the scale
				float temp_scale = glm::mix(StartScales[i], EndScales[i], readGraphScale(t));
				//get the global position of the particle
				glm::vec3 temp_pos = Positions[i];

				//save the color
				particle_col[numOfActiveParticles] = temp_col;
				particle_pos[numOfActiveParticles] = temp_pos;
				particle_scale[numOfActiveParticles] = temp_scale;

				numOfActiveParticles++;
			}
			//if there are particles to acutally be rendered, render them, if not just exit the function
			if (numOfActiveParticles > 0) {
				//manually set up the buffers and vao since titan doesn't currently have the infastructure to render instanced stuff automatically

				ColorInstanceBuffer->LoadData(particle_col, numOfActiveParticles);

				PositionInstanceBuffer->LoadData(particle_pos, numOfActiveParticles);

				ScaleInstanceBuffer->LoadData(particle_scale, numOfActiveParticles);

				m_vao->RenderInstanced(numOfActiveParticles, m_particle._mesh->GetVertexPositions().size());
			}
		}

	}

	//emits a single particle
	void TTN_ParticleSystem::Emit()
	{
		//setup the new particle's data
		//position
		{
			if (m_emitterShape == TTN_ParticleEmitterShape::CUBE) {
				float x = TTN_Random::RandomFloat(-(m_EmitterScale.x / 2), m_EmitterScale.x / 2);
				float y = TTN_Random::RandomFloat(-(m_EmitterScale.y / 2), m_EmitterScale.y / 2);
				float z = TTN_Random::RandomFloat(-(m_EmitterScale.z / 2), m_EmitterScale.z / 2);

				Positions[m_activeParticleIndex] = glm::vec3(x, y, z);
			}
			else {
				Positions[m_activeParticleIndex] = glm::vec3(0.0f);
			}
		}

		//colors
		{
			glm::vec4 Startcolor, EndColor;

			//calculate start color
			float t = TTN_Random::RandomFloat(0.0f, 1.0f);
			Startcolor = glm::mix(m_particle._StartColor, m_particle._StartColor2, t);

			//calculate end color
			t = TTN_Random::RandomFloat(0.0f, 1.0f);
			EndColor = glm::mix(m_particle._EndColor, m_particle._EndColor2, t);

			StartColors[m_activeParticleIndex] = Startcolor;
			EndColors[m_activeParticleIndex] = EndColor;
		}

		//velocities
		{
			glm::vec3 Dir = glm::vec3(0.0f);

			//calculate the direction
			//sphere emitter
			if (m_emitterShape == TTN_ParticleEmitterShape::SPHERE) {
				float x = TTN_Random::RandomFloat(-1.0f, 1.0f);
				float y = TTN_Random::RandomFloat(-1.0f, 1.0f);
				float z = TTN_Random::RandomFloat(-1.0f, 1.0f);

				Dir = glm::vec3(x, y, z);
				Dir = glm::normalize(Dir);
			}
			//circle emitter
			else if (m_emitterShape == TTN_ParticleEmitterShape::CIRCLE) {
				float x = TTN_Random::RandomFloat(-1.0f, 1.0f);
				float y = TTN_Random::RandomFloat(-1.0f, 1.0f);
				float z = 0.0f;

				Dir = glm::vec3(x, y, z);
				Dir = glm::normalize(Dir);

				//rotate it
				glm::quat rotQuat = glm::quat(m_rotation);
				glm::mat4 rotMat = glm::toMat4(rotQuat);

				Dir = glm::vec3(rotMat * glm::vec4(Dir, 1.0f));
			}
			//cone emitter
			else if (m_emitterShape == TTN_ParticleEmitterShape::CONE) {
				Dir = glm::vec3(0.0f, 1.0f, 0.0f);

				//rotate it by a random factor within give angle
				glm::vec3 coneRot = glm::vec3(TTN_Random::RandomFloat(-m_EmitterAngle, m_EmitterAngle), 0.0f, TTN_Random::RandomFloat(-m_EmitterAngle, m_EmitterAngle));

				glm::quat coneRotQuat = glm::quat(glm::radians(coneRot));
				glm::mat4 coneRotMat = glm::toMat4(coneRotQuat);

				Dir = glm::vec3(coneRotMat * glm::vec4(Dir, 1.0f));

				//rotate it
				glm::quat rotQuat = glm::quat(m_rotation);
				glm::mat4 rotMat = glm::toMat4(rotQuat);

				Dir = glm::vec3(rotMat * glm::vec4(Dir, 1.0f));
			}
			//cube emitter
			else if (m_emitterShape == TTN_ParticleEmitterShape::CUBE) {
				Dir = glm::vec3(0.0f, 1.0f, 0.0f);

				glm::quat rotQuat = glm::quat(m_rotation);
				glm::mat4 rotMat = glm::toMat4(rotQuat);

				Dir = glm::vec3(rotMat * glm::vec4(Dir, 1.0f));
			}

			float t = TTN_Random::RandomFloat(0.0f, 1.0f);
			float startSpeed = glm::mix(m_particle._startSpeed, m_particle._startSpeed2, t);
			t = TTN_Random::RandomFloat(0.0f, 1.0f);
			float endSpeed = glm::mix(m_particle._endSpeed, m_particle._endSpeed2, t);

			StartVelocities[m_activeParticleIndex] = Dir * startSpeed;
			EndVelocities[m_activeParticleIndex] = Dir * endSpeed;
		}

		//scales
		{
			float t = TTN_Random::RandomFloat(0.0f, 1.0f);
			float startScale = glm::mix(m_particle._StartSize, m_particle._StartSize2, t);
			t = TTN_Random::RandomFloat(0.0f, 1.0f);
			float endScale = glm::mix(m_particle._EndSize, m_particle._EndSize2, t);
			StartScales[m_activeParticleIndex] = startScale;
			EndScales[m_activeParticleIndex] = endScale;
		}

		//accelerations 
		{
			float t = TTN_Random::RandomFloat(0.0f, 1.0f);
			glm::vec3 startAccel = glm::mix(m_particle._startAcceleration, m_particle._startAcceleration2, t);
			t = TTN_Random::RandomFloat(0.0f, 1.0f);
			glm::vec3 endAccel = glm::mix(m_particle._endAcceleration, m_particle._endAccelertaion2, t);
			StartAccelerations[m_activeParticleIndex] = startAccel;
			EndAccelerations[m_activeParticleIndex] = endAccel;
			acceleratingVelocity[m_activeParticleIndex] = glm::vec3(0.0f);
		}

		//how long the particle has been alive and how long it should live (used to caculate t values)
		timeAlive[m_activeParticleIndex] = 0.0f;
		float t = TTN_Random::RandomFloat(0.0f, 1.0f);
		float lifetime = glm::mix(m_particle._lifeTime, m_particle._lifeTime2, t);
		lifeTimes[m_activeParticleIndex] = lifetime;

		//set the particle to be alive
		Active[m_activeParticleIndex] = true;

		//and move the index back so the next emit will use the next index
		m_activeParticleIndex = (m_activeParticleIndex - 1) % m_maxParticlesCount;
	}

	//emits a bunch of particles all at once
	void TTN_ParticleSystem::Burst(size_t numOfParticles)
	{
		for (size_t i = 0; i < numOfParticles; i++) {
			Emit();
		}
	}

	//sets up vao and vbos
	void TTN_ParticleSystem::SetUpRenderingStuff()
	{
		//create vbos
		VertexPosVBO = TTN_VertexBuffer::Create();
		VertexNormVBO = TTN_VertexBuffer::Create();
		VertexUVVBO = TTN_VertexBuffer::Create();
		ColorInstanceBuffer = TTN_VertexBuffer::Create();
		PositionInstanceBuffer = TTN_VertexBuffer::Create();
		ScaleInstanceBuffer = TTN_VertexBuffer::Create();
		//create the vao
		m_vao = TTN_VertexArrayObject::Create();

		//load the basic vertex buffers
		m_vao->AddVertexBuffer(VertexPosVBO, { BufferAttribute(0, 3, GL_FLOAT, false, sizeof(float) * 3, 0, AttribUsage::Position) });
		m_vao->AddVertexBuffer(VertexNormVBO, { BufferAttribute(1, 3, GL_FLOAT, false, sizeof(float) * 3, 0, AttribUsage::Normal) });
		m_vao->AddVertexBuffer(VertexUVVBO, { BufferAttribute(2, 2, GL_FLOAT, false, sizeof(float) * 2, 0, AttribUsage::Texture) });

		//load the instanced vertex buffers
		m_vao->AddVertexBuffer(ColorInstanceBuffer, { BufferAttribute(3, 4, GL_FLOAT, false, sizeof(float) * 4, 0, AttribUsage::Color, 1) });
		m_vao->AddVertexBuffer(PositionInstanceBuffer, { BufferAttribute(4, 3, GL_FLOAT, false, sizeof(float) * 3, 0, AttribUsage::User0, 1) });
		m_vao->AddVertexBuffer(ScaleInstanceBuffer, { BufferAttribute(5, 1, GL_FLOAT, false, sizeof(float), 0, AttribUsage::User1, 1) });
	}

	//sets up the data making sure it's zeroed out
	void TTN_ParticleSystem::SetUpData()
	{
		Positions = new glm::vec3[m_maxParticlesCount];
		StartColors = new glm::vec4[m_maxParticlesCount];
		EndColors = new glm::vec4[m_maxParticlesCount];
		StartVelocities = new glm::vec3[m_maxParticlesCount];
		EndVelocities = new glm::vec3[m_maxParticlesCount];
		StartScales = new float[m_maxParticlesCount];
		EndScales = new float[m_maxParticlesCount];
		StartAccelerations = new glm::vec3[m_maxParticlesCount];
		EndAccelerations = new glm::vec3[m_maxParticlesCount];
		acceleratingVelocity = new glm::vec3[m_maxParticlesCount];
		timeAlive = new float[m_maxParticlesCount];
		lifeTimes = new float[m_maxParticlesCount];
		Active = new bool[m_maxParticlesCount];

		particle_pos = new glm::vec3[m_maxParticlesCount];
		particle_col = new glm::vec4[m_maxParticlesCount];
		particle_scale = new float[m_maxParticlesCount];

		for (size_t i = 0; i < m_maxParticlesCount; i++) {
			Positions[i] = glm::vec3(0.0f);
			StartColors[i] = glm::vec4(0.0f);
			EndColors[i] = glm::vec4(0.0f);
			StartVelocities[i] = glm::vec3(0.0f);
			EndVelocities[i] = glm::vec3(0.0f);
			StartScales[i] = 0.0f;
			EndScales[i] = 0.0f;
			timeAlive[i] = 0.0f;
			lifeTimes[i] = 0.0f;
			StartAccelerations[i] = glm::vec3(0.0f);
			EndAccelerations[i] = glm::vec3(0.0f);
			acceleratingVelocity[i] = glm::vec3(0.0f);
			Active[i] = false;

			particle_pos[i] = glm::vec3(0.0f);
			particle_col[i] = glm::vec4(0.0f);
			particle_scale[i] = 0.0f;
		}
	}
}