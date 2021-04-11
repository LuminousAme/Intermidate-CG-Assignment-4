//Dam Defense, by Atlas X Games
//Game.cpp, the source file for the class that represents the main gameworld scene

//import the class
#include "Game.h"
#include "glm/ext.hpp"

//default constructor
Game::Game()
	: TTN_Scene()
{
}

//sets up the scene
void Game::InitScene()
{
	//load in the scene's assets
	SetUpAssets();

	//set up the other data
	SetUpOtherData();

	//create the entities
	SetUpEntities();

	RestartData();

	TTN_Scene::InitScene();
}

//updates the scene every frame
void Game::Update(float deltaTime)
{
	engine.GetListener();

	//call the sound update
	GameSounds(deltaTime);

	if (!m_hasGivenLowHealthWarning && Dam_health < 25.0f) {
		m_hasGivenLowHealthWarning = true;
		m_DialogueLowHealth->SetNextPostion(glm::vec3(0.0f));
		m_DialogueLowHealth->PlayFromQueue();
	}

	if (!m_paused) {
		//subtract from the input delay
		if (m_InputDelay >= 0.0f) m_InputDelay -= deltaTime;

		//look through all of the cannonballs updating them
		for (int i = 0; i < cannonBalls.size(); i++) {
			//if the cannonball is just falling, make it move the way it should
			if (glm::normalize(Get<TTN_Physics>(cannonBalls[i].first).GetLinearVelocity()) == glm::vec3(0.0f, -1.0f, 0.0f)) {
				Get<TTN_Physics>(cannonBalls[i].first).AddForce(cannonBallForce * playerDir);
			}

			//if a cannonball has passed the water plane but not yet splashed, play the splash effect
			if (!cannonBalls[i].second && Get<TTN_Transform>(cannonBalls[i].first).GetGlobalPos().y <=
				Get<TTN_Transform>(water).GetGlobalPos().y - 0.1f) {
				//sets the position where the sound should play
				glm::vec3 temp = Get<TTN_Transform>(cannonBalls[i].first).GetGlobalPos();
				temp.x *= -1.0f;
				temp.y = Get<TTN_Transform>(water).GetGlobalPos().y;
				m_splashSounds->SetNextPostion(temp);
				//and plays the splash sound
				m_splashSounds->PlayFromQueue();

				//play a splash particle effect
				CreateSplash(Get<TTN_Transform>(cannonBalls[i].first).GetGlobalPos());

				//and mark the cannonball as having splashed
				cannonBalls[i].second = true;
			}
		}

		//delete any cannonballs that're way out of range
		DeleteCannonballs();

		//allow the player to rotate
		PlayerRotate(deltaTime);

		//switch to the cannon's normal static animation if it's firing animation has ended
		StopFiring();

		//if the player is on shoot cooldown, decrement the time remaining on the cooldown
		if (playerShootCooldownTimer >= 0.0f) playerShootCooldownTimer -= deltaTime;

		//update the enemy wave spawning
		WaveUpdate(deltaTime);
		//collision check
		Collisions();
		//damage function, contains cooldoown
		Damage(deltaTime);

		//goes through the boats vector
		for (int i = 0; i < boats.size(); i++) {
			//sets gravity to 0
			Get<TTN_Physics>(boats[i]).GetRigidBody()->setGravity(btVector3(0.0f, 0.0f, 0.0f));
			Get<EnemyComponent>(boats[i]).SetDifficulty(difficulty);
		}

		//go through all the entities with enemy compontents
		auto enemyView = GetScene()->view<EnemyComponent>();
		for (auto entity : enemyView) {
			//and run their update
			Get<EnemyComponent>(entity).Update(deltaTime);
		}

		//update the special abilities
		BirdUpate(deltaTime);
		FlamethrowerUpdate(deltaTime);

		//increase the total time of the scene to make the water animated correctly
		water_time += deltaTime / 2.5f;
	}

	//game over stuff
	if (Dam_health <= 0.0f) {
		m_gameOver = true;
		printf("GAME OVER");
	}
	if (m_currentWave > lastWave && !m_arcade) {
		m_gameWin = true;
	}

	//shop function
	Shop(deltaTime);

	/*
	if (m_applyWarmLut) {
		m_colorCorrectEffect->SetShouldApply(true);
		m_colorCorrectEffect->SetCube(TTN_AssetSystem::GetLUT("Warm LUT"));
		//and make sure the cool and customs luts are set not to render
		m_applyCoolLut = false;
		m_applyCustomLut = false;
	}
	else {
		//if it's been turned of set the effect not to render
		m_colorCorrectEffect->SetShouldApply(false);
	}*/

	//update the sound
	engine.Update();

	//call the update on ImGui
	ImGui();

	//get fps
	//std::cout << "FPS: " << std::to_string(1.0f / deltaTime) << std::endl;
	//don't forget to call the base class' update
	TTN_Scene::Update(deltaTime);
}

//render the terrain and water
void Game::PostRender()
{
	//disable blending so the gBuffer can draw properlly
	glDisable(GL_BLEND);

	//bind the geometry buffer
	gBuffer->Bind();

	//render the terrain
	{
		//bind the shader
		shaderProgramTerrain->Bind();

		//vert shader
		//bind the height map texture
		terrainMap->Bind(0);
		TTN_AssetSystem::GetTexture2D("Normal Map")->Bind(1);

		//pass the scale uniform
		shaderProgramTerrain->SetUniform("u_scale", terrainScale);
		//pass the mvp uniform
		glm::mat4 mvp = Get<TTN_Camera>(camera).GetProj();
		glm::mat4 viewMat = glm::inverse(Get<TTN_Transform>(camera).GetGlobal());
		mvp *= viewMat;
		mvp *= Get<TTN_Transform>(terrain).GetGlobal();
		shaderProgramTerrain->SetUniformMatrix("MVP", mvp);
		//pass the model uniform
		shaderProgramTerrain->SetUniformMatrix("Model", Get<TTN_Transform>(terrain).GetGlobal());
		//and pass the normal matrix uniform
		shaderProgramTerrain->SetUniformMatrix("NormalMat",
			glm::mat3(glm::inverse(glm::transpose(Get<TTN_Transform>(terrain).GetGlobal()))));

		//frag shader
		//bind the textures
		sandText->Bind(2);
		rockText->Bind(3);
		grassText->Bind(4);

		m_mats[0]->GetDiffuseRamp()->Bind(10);
		m_mats[0]->GetSpecularMap()->Bind(11);

		//set if the albedo textures should be used
		shaderProgramTerrain->SetUniform("u_UseDiffuse", (int)m_mats[0]->GetUseAlbedo());

		//render the terrain
		terrainPlain->GetVAOPointer()->Render();
	}

	//render the water
	{
		//bind the shader
		shaderProgramWater->Bind();

		//vert shader
		//pass the mvp uniform
		glm::mat4 mvp = Get<TTN_Camera>(camera).GetProj();
		glm::mat4 viewMat = glm::inverse(Get<TTN_Transform>(camera).GetGlobal());
		mvp *= viewMat;
		mvp *= Get<TTN_Transform>(water).GetGlobal();
		shaderProgramWater->SetUniformMatrix("MVP", mvp);
		//pass the model uniform
		shaderProgramWater->SetUniformMatrix("Model", Get<TTN_Transform>(water).GetGlobal());
		//and pass the normal matrix uniform
		shaderProgramWater->SetUniformMatrix("NormalMat",
			glm::mat3(glm::inverse(glm::transpose(Get<TTN_Transform>(water).GetGlobal()))));

		//pass in data about the water animation
		float time = WaterManager::GetTime();
		float steepness = WaterManager::GetSteepness();
		int waveNums = WaterManager::GetNumberOfWaves();
		float speed = WaterManager::GetSpeed();

		shaderProgramWater->SetUniform("time", time);
		shaderProgramWater->SetUniform("u_Q", steepness);
		shaderProgramWater->SetUniform("u_numOfWaves", waveNums);
		shaderProgramWater->SetUniform("u_speed", speed);

		float amplitudes[16];
		float freqs[16];
		glm::vec3 directions[16];

		for (int i = 0; i < 16; i++) {
			if (i < WaterManager::GetNumberOfWaves()) {
				amplitudes[i] = WaterManager::m_waveAcutalAmplitude[i];
				freqs[i] = WaterManager::m_waveFrequency[i];
				directions[i] = WaterManager::m_waveDirection[i];
			}
			else {
				amplitudes[i] = 0.0f;
				freqs[i] = 0.0f;
				directions[i] = glm::vec3(0.0f);
			}
		}

		shaderProgramWater->SetUniform("u_amplitude", amplitudes[0], 16);
		shaderProgramWater->SetUniform("u_frequency", freqs[0], 16);
		shaderProgramWater->SetUniform("u_direction", directions[0], 16);

		//frag shader
		//bind the textures
		waterText->Bind(0);
		WaterManager::BindVoronoiAsColor(1);

		//send lighting from the scene
		shaderProgramWater->SetUniform("u_UseDiffuse", (int)m_mats[0]->GetUseAlbedo());

		//render the water (just use the same plane as the terrain)
		terrainPlain->GetVAOPointer()->Render();

		WaterManager::UnbindVoronoi(1);
	}

	//unbind the geometry buffer
	gBuffer->Unbind();

	//enable blending again so it works on everything else
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//and do the rest of the titan side rendering
	TTN_Scene::PostRender();
}

#pragma region INPUTS
//function to use to check for when a key is being pressed down for the first frame
void Game::KeyDownChecks()
{
	//if the game is not paused and the input delay is over
	if (!m_paused && m_InputDelay <= 0.0f && !firstFrame) {
		//and they press the 1 key, try to activate the flamethrower
		if (TTN_Application::TTN_Input::GetKeyDown(TTN_KeyCode::One)) {
			Flamethrower();
		}

		//and they press the 2 key, try to activate the bird bomb
		if (TTN_Application::TTN_Input::GetKeyDown(TTN_KeyCode::Two)) {
			BirdBomb();
		}
	}

	//if they try to press the escape key, pause or unpause the game
	if (TTN_Application::TTN_Input::GetKeyDown(TTN_KeyCode::Esc) && (!shopping || (shopping && pauseRender))) {
		m_InputDelay = m_InputDelayTime;
		m_paused = !m_paused;
		TTN_Scene::SetPaused(m_paused);
	}

	//control to make the CG controls appear or disappear
	if (TTN_Application::TTN_Input::GetKey(TTN_KeyCode::C) && TTN_Application::TTN_Input::GetKeyDown(TTN_KeyCode::G)) {
		showCGControls = !showCGControls;
	}
}

//function to cehck for when a key is being pressed
void Game::KeyChecks()
{
}

//function to check for when a key has been released
void Game::KeyUpChecks()
{
}

//function to check for when a mouse button has been pressed down for the first frame
void Game::MouseButtonDownChecks()
{
}

//function to check for when a mouse button is being pressed
void Game::MouseButtonChecks()
{
	//if the game is not paused and the input delay is over
	if (!m_paused && m_InputDelay <= 0.0f) {
		//if the cannon is not in the middle of firing, fire when the player is pressing the left mouse button
		if (Get<TTN_MorphAnimator>(cannon).getActiveAnim() == 0 && playerShootCooldownTimer <= 0.0f &&
			TTN_Application::TTN_Input::GetMouseButton(TTN_MouseButton::Left)) {
			//play the firing animation
			Get<TTN_MorphAnimator>(cannon).SetActiveAnim(1);
			Get<TTN_MorphAnimator>(cannon).getActiveAnimRef().Restart();
			//create a new cannonball
			CreateCannonball();
			//reset the cooldown
			playerShootCooldownTimer = playerShootCooldown;
			//and play the smoke particle effect
			Get<TTN_Transform>(smokePS).SetPos(Get<TTN_Transform>(cannon).GetGlobalPos() + (1.75f / 10.0f) * playerDir);
			Get<TTN_ParticeSystemComponent>(smokePS).GetParticleSystemPointer()->
				SetEmitterRotation(glm::vec3(rotAmmount.y, -rotAmmount.x, 0.0f));
			Get<TTN_ParticeSystemComponent>(smokePS).GetParticleSystemPointer()->Burst(150);
			m_cannonFiringSounds->SetNextPostion(glm::vec3(0.0f));
			m_cannonFiringSounds->PlayFromQueue();
		}
	}
}

//function to check for when a mouse button has been released
void Game::MouseButtonUpChecks()
{
}
#pragma endregion

#pragma region SetUP STUFF
//sets up all the assets in the scene
void Game::SetUpAssets()
{
	//make the events
	m_cannonFiringSounds = TTN_AudioEventHolder::Create("Cannon Shot", "{01c9d609-b06a-4bb8-927d-01ee25b2b815}", 2);
	m_splashSounds = TTN_AudioEventHolder::Create("Splash", "{ca17eafa-bffe-4121-80a3-441a94ee2fe7}", 8);
	m_flamethrowerSound = TTN_AudioEventHolder::Create("Flamethrower", "{b52a7dfc-88df-47a9-9263-859e6564e161}", 1);
	m_jingle = TTN_AudioEventHolder::Create("Wave Complete", "{d28d68df-bb3e-4153-95b6-89fd2715a5a3}", 1);
	m_music = TTN_AudioEventHolder::Create("Music", "{239bd7d6-7e7e-47a7-a0f6-7afc6f1b35bc}", 1);

	m_enemyCannonSound = TTN_AudioEventHolder::Create("Enemy Cannon", "{20e8878c-a8a3-41e6-a5cc-200394f71009}", 8);
	m_enemyDeathSound = TTN_AudioEventHolder::Create("Enemy Death", "{06a2e720-3387-40c6-a8bf-34459a4a58d0}", 8);

	m_healSound = TTN_AudioEventHolder::Create("Heal Sound", "{7d588012-40ab-48a9-91d2-a8f4e2595ad7}", 1);
	m_cooldownReducedSound = TTN_AudioEventHolder::Create("Cooldown Reduced Sound", "{0295e5e9-3ffd-4f14-bad8-e48fb90f8f54}", 1);
	m_cannonUpgradeSound = TTN_AudioEventHolder::Create("Cannon Upgrade Sound", "{6abc8c3f-9bfd-40e2-8698-f838927cf0c0}", 1);

	m_birdBombSound = TTN_AudioEventHolder::Create("Bird Bomb", "{7b09167d-47be-4d90-908f-543d8347eba3}", 1);

	m_DialogueOpening = TTN_AudioEventHolder::Create("Opening Dialouge", "{c79fd2c3-4b97-470f-a2b2-03262612ce0d}", 1);
	m_DialougeWave2 = TTN_AudioEventHolder::Create("Wave 2 Dialouge", "{6ee1b2f1-4205-4a22-964c-58fc15c545ff}", 1);
	m_DialogueWave3 = TTN_AudioEventHolder::Create("Wave 3 Dialouge", "{9803a883-3ba6-406e-b828-a221514b472a}", 1);
	m_DialogueWave4 = TTN_AudioEventHolder::Create("Wave 4 Dialouge", "{ee746cdd-0ee8-4375-8208-f293436ea878}", 1);
	m_DialougeEnding = TTN_AudioEventHolder::Create("Ending Dialouge", "{ee746cdd-0ee8-4375-8208-f293436ea878}", 1);

	m_DialogueLowHealth = TTN_AudioEventHolder::Create("Low Health Warning", "{1a396596-b228-4a1a-a8b5-1a580dd7d9ae}", 1);
	m_DialougeFlamethrower = TTN_AudioEventHolder::Create("Dialogue Flame Thrower", "{08bfaf49-8b2a-4226-808f-b7d711b60a87}", 1);
	m_DialougeBirdBomb = TTN_AudioEventHolder::Create("Dialouge Bird Bomb", "{201b5f50-767c-4327-944c-49b17ce424d2}", 1);

	m_DialougeHittingABird = TTN_AudioEventHolder::Create("Dialogue Hitting a bird", "{ee38d21f-d09b-4c78-aa68-027f8cd7a991}", 1);
	m_DialougeKilling5Birds = TTN_AudioEventHolder::Create("Dialouge Killing 5 birds", "{cf5fc3c7-5391-42e9-b862-0795f4e29590}", 1);
	m_DialougeKilling10Birds = TTN_AudioEventHolder::Create("Dialogue Killing 10 Birds", "{4bfaf6a0-cfca-466b-af2c-eb8b3137171f}", 1);
	m_DialougeKilling25Birds = TTN_AudioEventHolder::Create("Dialouge Killing 25 BIrds", "{86fedd33-1384-4242-8df4-66e193126f38}", 1);
	
	m_DialougeKillingJerry = TTN_AudioEventHolder::Create("Killing Jerry", "{8ed61f3c-f5a5-4f4b-b616-736672a84e34}", 1);
	m_DialougeKillingJuilian = TTN_AudioEventHolder::Create("Killing Julian", "{3602d91a-014c-4346-808b-5feaffa769eb}", 1);
	m_DialougeKillingJulianWhileJerryIsAlive = TTN_AudioEventHolder::Create("Killing Julian While Jerry Is Alive", "{7a299c08-45d4-44d6-a512-71cc6b8f0699}", 1);

	//// SHADERS ////
	//grab the shaders
	shaderProgramTextured = TTN_AssetSystem::GetShader("Basic textured shader");
	shaderProgramSkybox = TTN_AssetSystem::GetShader("Skybox shader");
	shaderProgramTerrain = TTN_AssetSystem::GetShader("Terrain shader");
	shaderProgramWater = TTN_AssetSystem::GetShader("Water shader");
	shaderProgramAnimatedTextured = TTN_AssetSystem::GetShader("Animated textured shader");
	shaderDepth = TTN_AssetSystem::GetShader("Depth shader");

	////MESHES////
	//cannonMesh = TTN_ObjLoader::LoadAnimatedMeshFromFiles("models/cannon/cannon", 7);
	//skyboxMesh = TTN_ObjLoader::LoadFromFile("models/SkyboxMesh.obj");
	//sphereMesh = TTN_ObjLoader::LoadFromFile("models/IcoSphereMesh.obj");
	//flamethrowerMesh = TTN_ObjLoader::LoadFromFile("models/Flamethrower.obj");
	//flamethrowerMesh->SetUpVao();
	//boat1Mesh = TTN_ObjLoader::LoadFromFile("models/Boat 1.obj");
	//boat2Mesh = TTN_ObjLoader::LoadFromFile("models/Boat 2.obj");
	//boat3Mesh = TTN_ObjLoader::LoadFromFile("models/Boat 3.obj");
	//terrainPlain = TTN_ObjLoader::LoadFromFile("models/terrainPlain.obj");
	//terrainPlain->SetUpVao();
	//birdMesh = TTN_ObjLoader::LoadAnimatedMeshFromFiles("models/bird/bird", 2);
	//treeMesh[0] = TTN_ObjLoader::LoadFromFile("models/Tree1.obj");
	//treeMesh[1] = TTN_ObjLoader::LoadFromFile("models/Tree2.obj");
	//treeMesh[2] = TTN_ObjLoader::LoadFromFile("models/Tree3.obj");
	//damMesh = TTN_ObjLoader::LoadFromFile("models/Dam.obj");

	//grab the meshes
	cannonMesh = TTN_AssetSystem::GetMesh("Cannon mesh");
	skyboxMesh = TTN_AssetSystem::GetMesh("Skybox mesh");
	sphereMesh = TTN_AssetSystem::GetMesh("Sphere");
	flamethrowerMesh = TTN_AssetSystem::GetMesh("Flamethrower mesh");
	boat1Mesh = TTN_AssetSystem::GetMesh("Boat 1");
	boat2Mesh = TTN_AssetSystem::GetMesh("Boat 2");
	boat3Mesh = TTN_AssetSystem::GetMesh("Boat 3");
	terrainPlain = TTN_AssetSystem::GetMesh("Terrain plane");
	birdMesh = TTN_AssetSystem::GetMesh("Bird mesh");
	damMesh = TTN_AssetSystem::GetMesh("Dam mesh");
	enemyCannonMesh = TTN_AssetSystem::GetMesh("Enemy Cannon mesh");

	///TEXTURES////
	//grab textures
	cannonText = TTN_AssetSystem::GetTexture2D("Cannon texture");
	skyboxText = TTN_AssetSystem::GetSkybox("Skybox texture");
	terrainMap = TTN_AssetSystem::GetTexture2D("Terrain height map");
	sandText = TTN_AssetSystem::GetTexture2D("Sand texture");
	rockText = TTN_AssetSystem::GetTexture2D("Rock texture");
	grassText = TTN_AssetSystem::GetTexture2D("Grass texture");
	waterText = TTN_AssetSystem::GetTexture2D("Water texture");
	boat1Text = TTN_AssetSystem::GetTexture2D("Boat texture 1");
	boat2Text = TTN_AssetSystem::GetTexture2D("Boat texture 2");
	boat3Text = TTN_AssetSystem::GetTexture2D("Boat texture 3");
	flamethrowerText = TTN_AssetSystem::GetTexture2D("Flamethrower texture");
	birdText = TTN_AssetSystem::GetTexture2D("Bird texture");
	damText = TTN_AssetSystem::GetTexture2D("Dam texture");
	enemyCannonText = TTN_AssetSystem::GetTexture2D("Enemy Cannon texture");

	////MATERIALS////
	cannonMat = TTN_AssetSystem::GetMaterial("cannonMat");
	cannonMat->SetAlbedo(cannonText);
	cannonMat->SetNormalMap(TTN_AssetSystem::GetTexture2D("Cannon Normal Map"));
	cannonMat->SetUseNormalMap(true);
	cannonMat->SetShininess(128.0f);
	m_mats.push_back(cannonMat);

	enemyCannonMat = TTN_AssetSystem::GetMaterial("enemyCannonMat");
	enemyCannonMat->SetAlbedo(enemyCannonText);
	enemyCannonMat->SetNormalMap(TTN_AssetSystem::GetTexture2D("Enemy Cannon Normal Map"));
	enemyCannonMat->SetUseNormalMap(true);
	enemyCannonMat->SetHasRimLighting(true);
	enemyCannonMat->SetShininess(128.0f);
	m_mats.push_back(enemyCannonMat);

	boat1Mat = TTN_AssetSystem::GetMaterial("boat1Mat");
	boat1Mat->SetAlbedo(boat1Text);
	boat1Mat->SetNormalMap(TTN_AssetSystem::GetTexture2D("Boat Normal Map 1"));
	boat1Mat->SetUseNormalMap(true);
	boat1Mat->SetHasRimLighting(true);
	boat1Mat->SetShininess(128.0f);
	m_mats.push_back(boat1Mat);
	boat2Mat = TTN_AssetSystem::GetMaterial("boat2Mat");
	boat2Mat->SetAlbedo(boat2Text);
	boat2Mat->SetNormalMap(TTN_AssetSystem::GetTexture2D("Boat Normal Map 2"));
	boat2Mat->SetUseNormalMap(true);
	boat2Mat->SetHasRimLighting(true);
	boat2Mat->SetShininess(128.0f);
	m_mats.push_back(boat2Mat);
	boat3Mat = TTN_AssetSystem::GetMaterial("boat3Mat");
	boat3Mat->SetAlbedo(boat3Text);
	boat3Mat->SetNormalMap(TTN_AssetSystem::GetTexture2D("Boat Normal Map 3"));
	boat3Mat->SetUseNormalMap(true);
	boat3Mat->SetHasRimLighting(true);
	boat3Mat->SetShininess(128.0f);
	m_mats.push_back(boat3Mat);

	flamethrowerMat = TTN_AssetSystem::GetMaterial("flamethrowerMat");
	flamethrowerMat->SetAlbedo(flamethrowerText);
	flamethrowerMat->SetShininess(128.0f);
	m_mats.push_back(flamethrowerMat);

	skyboxMat = TTN_AssetSystem::GetMaterial("skyboxMat");
	skyboxMat->SetSkybox(skyboxText);
	smokeMat = TTN_Material::Create();
	smokeMat->SetAlbedo(nullptr); //do this to be sure titan uses it's default white texture for the particle

	fireMat = TTN_AssetSystem::GetMaterial("fireMat");
	fireMat->SetAlbedo(nullptr); //do this to be sure titan uses it's default white texture for the particle

	birdMat = TTN_AssetSystem::GetMaterial("birdMat");
	birdMat->SetAlbedo(birdText);
	m_mats.push_back(birdMat);

	damMat = TTN_AssetSystem::GetMaterial("damMat");
	damMat->SetAlbedo(damText);
	m_mats.push_back(damMat);

	lightHouseMat = TTN_AssetSystem::GetMaterial("LightHouseMat");
	lightHouseMat->SetAlbedo(TTN_AssetSystem::GetTexture2D("LightHouseText"));
	lightHouseMat->SetEmissive(TTN_AssetSystem::GetTexture2D("LightHouseEmissive"));
	lightHouseMat->SetUseEmissive(true);
	illBuffer->SetEmissiveStrenght(1.0f);
	m_mats.push_back(lightHouseMat);

	treeMat = TTN_AssetSystem::GetMaterial("TreeMat");
	treeMat->SetAlbedo(TTN_AssetSystem::GetTexture2D("Tree texture"));
	m_mats.push_back(treeMat);

	for (int i = 0; i < m_mats.size(); i++) {
		m_mats[i]->SetDiffuseRamp(TTN_AssetSystem::GetTexture2D("blue ramp"));
		m_mats[i]->SetSpecularRamp(TTN_AssetSystem::GetTexture2D("blue ramp"));
		m_mats[i]->SetUseDiffuseRamp(m_useDiffuseRamp);
		m_mats[i]->SetUseSpecularRamp(m_useSpecularRamp);
	}
}

//create the scene's initial entities
void Game::SetUpEntities()
{
	//entity for the camera
	{
		//create an entity in the scene for the camera
		camera = CreateEntity();
		SetCamEntity(camera);
		Attach<TTN_Transform>(camera);
		Attach<TTN_Camera>(camera);
		auto& camTrans = Get<TTN_Transform>(camera);
		camTrans.SetPos(glm::vec3(0.0f, 0.070f, -0.275f));
		camTrans.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
		camTrans.LookAlong(glm::vec3(0.0, 0.0, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		Get<TTN_Camera>(camera).CalcPerspective(60.0f, 1.78f, 0.01f, 30.0f);
		Get<TTN_Camera>(camera).View();
	}

	//entity for the skybox
	{
		skybox = CreateEntity();
		SetSkyboxEntity(skybox);

		//setup a mesh renderer for the skybox
		TTN_Renderer skyboxRenderer = TTN_Renderer(skyboxMesh, shaderProgramSkybox);
		skyboxRenderer.SetMat(skyboxMat);
		skyboxRenderer.SetRenderLayer(100);
		skyboxRenderer.SetCastShadows(false);
		//attach that renderer to the entity
		AttachCopy<TTN_Renderer>(skybox, skyboxRenderer);

		//setup a transform for the skybox
		TTN_Transform skyboxTrans = TTN_Transform(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f / 10.0f));
		//attach that transform to the entity
		AttachCopy<TTN_Transform>(skybox, skyboxTrans);
	}

	//entity for the cannon
	{
		cannon = CreateEntity("Cannon");

		//setup a mesh renderer for the cannon
		TTN_Renderer cannonRenderer = TTN_Renderer(cannonMesh, shaderProgramAnimatedTextured, cannonMat);
		//TTN_Material f = cannonRenderer.GetMat();

		//attach that renderer to the entity
		AttachCopy(cannon, cannonRenderer);

		//setup a transform for the cannon
		TTN_Transform cannonTrans = TTN_Transform(glm::vec3(0.0f, -0.4f / 10.0f, -0.25f / 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.3375f / 10.0f, 0.3375f / 10.0f, 0.2875f / 10.0f));
		//attach that transform to the entity
		AttachCopy(cannon, cannonTrans);

		//setup an animator for the cannon
		TTN_MorphAnimator cannonAnimator = TTN_MorphAnimator();
		//create an animation for the cannon when it's not firing
		TTN_MorphAnimation notFiringAnim = TTN_MorphAnimation({ 0 }, { 3.0f / 24 }, true); //anim 0
		//create an animation for the cannon when it is firing
		std::vector<int> firingFrameIndices = std::vector<int>();
		std::vector<float> firingFrameLengths = std::vector<float>();
		for (int i = 0; i < 7; i++) firingFrameIndices.push_back(i);
		firingFrameLengths.push_back(3.0f / 24.0f);
		firingFrameLengths.push_back(1.0f / 24.0f);
		firingFrameLengths.push_back(1.0f / 24.0f);
		firingFrameLengths.push_back(1.0f / 24.0f);
		firingFrameLengths.push_back(1.0f / 24.0f);
		firingFrameLengths.push_back(2.0f / 24.0f);
		firingFrameLengths.push_back(3.0f / 24.0f);
		TTN_MorphAnimation firingAnim = TTN_MorphAnimation(firingFrameIndices, firingFrameLengths, true); //anim 1
		//add both animatons to the animator
		cannonAnimator.AddAnim(notFiringAnim);
		cannonAnimator.AddAnim(firingAnim);
		//start on the not firing anim
		cannonAnimator.SetActiveAnim(0);
		//attach that animator to the entity
		AttachCopy(cannon, cannonAnimator);
	}

	//entity for the dam
	{
		dam = CreateEntity();

		//setup a mesh renderer for the dam
		TTN_Renderer damRenderer = TTN_Renderer(damMesh, shaderProgramTextured, damMat);
		//attach that renderer to the entity
		AttachCopy(dam, damRenderer);

		//setup a transform for the dam
		TTN_Transform damTrans = TTN_Transform(glm::vec3(0.0f, -10.0f / 10.0f, 3.0f / 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.7f / 10.0f, 0.7f / 10.0f, 0.3f / 10.0f));
		//attach that transform to the entity
		AttachCopy(dam, damTrans);
	}

	flamethrowers = std::vector<entt::entity>();
	//entities for flamethrowers
	for (int i = 0; i < 6; i++) {
		//flamethrower entities
		{
			flamethrowers.push_back(CreateEntity());

			//setup a mesh renderer for the cannon
			TTN_Renderer ftRenderer = TTN_Renderer(flamethrowerMesh, shaderProgramTextured);
			ftRenderer.SetMat(flamethrowerMat);
			//attach that renderer to the entity
			AttachCopy<TTN_Renderer>(flamethrowers[i], ftRenderer);

			//setup a transform for the flamethrower
			TTN_Transform ftTrans = TTN_Transform(glm::vec3(5.0f / 10.0f, -6.0f / 10.0f, 2.0f / 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.40f / 10.0f));
			if (i == 0) {
				ftTrans.SetPos(glm::vec3(-5.0f / 10.0f, -6.0f / 10.0f, 2.0f / 10.0f));
			}
			else if (i == 1) {
				ftTrans.SetPos(glm::vec3(15.0f / 10.0f, -6.0f / 10.0f, 2.0f / 10.0f));
			}
			else if (i == 2) {
				ftTrans.SetPos(glm::vec3(-15.0f / 10.0f, -6.0f / 10.0f, 2.0f / 10.0f));
			}
			else if (i == 3) {
				ftTrans.SetPos(glm::vec3(40.0f / 10.0f, -6.0f / 10.0f, 2.0f / 10.0f));
			}
			else if (i == 4) {
				ftTrans.SetPos(glm::vec3(-40.0f / 10.0f, -6.0f / 10.0f, 2.0f / 10.0f));
			}

			//attach that transform to the entity
			AttachCopy<TTN_Transform>(flamethrowers[i], ftTrans);
		}
	}

	//entity for the lighthouse
	{
		entt::entity lightHouse = CreateEntity();

		//setup a mesh renderer for the light house
		TTN_Renderer lightHouseRenderer = TTN_Renderer(TTN_AssetSystem::GetMesh("lightHouseMesh"), shaderProgramTextured, lightHouseMat);
		//attach that renderer to the entity
		AttachCopy(lightHouse, lightHouseRenderer);

		//setup a transform for the light house
		TTN_Transform lightHouseTrans = TTN_Transform(glm::vec3(10.0, -0.1f, 9.0f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.4f));
		//attach that transform to the entity
		AttachCopy(lightHouse, lightHouseTrans);
	}

	//entity for the lighthouse
	for (int i = 0; i < 48; i++) {
		entt::entity tree = CreateEntity();

		//setup a mesh renderer for a tree
		TTN_Renderer treeRenderer = TTN_Renderer(TTN_AssetSystem::GetMesh("Tree Mesh"), shaderProgramTextured, treeMat);
		//attach that renderer to the entity
		AttachCopy(tree, treeRenderer);

		//setup a transform for a tree
		TTN_Transform treeTrans;
		if (i == 0) treeTrans = TTN_Transform(glm::vec3(9.0f + 2.0f, -0.1f, 8.0f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 1) treeTrans = TTN_Transform(glm::vec3(-10.0f, -0.1f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 2) treeTrans = TTN_Transform(glm::vec3(-9.5f, -0.1f, 3.0f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 3) treeTrans = TTN_Transform(glm::vec3(10.1 + 2.0f, -0.1f, 4.5f), glm::vec3(0.0f, -180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 4) treeTrans = TTN_Transform(glm::vec3(8.9 + 2.0f, -0.2f, 1.2f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 5) treeTrans = TTN_Transform(glm::vec3(-9.1f, -0.1f, 7.6f), glm::vec3(0.0f, 72.0f, 0.0f), glm::vec3(0.075f));

		else if (i == 6) treeTrans = TTN_Transform(glm::vec3(8.5f + 1.8f, -0.1f, 8.0f), glm::vec3(0.0f, 170.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 7) treeTrans = TTN_Transform(glm::vec3(-10.2f, -0.1f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 8) treeTrans = TTN_Transform(glm::vec3(-9.3f, -0.1f, 3.0f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 9) treeTrans = TTN_Transform(glm::vec3(10.4 + 1.8f, -0.1f, 4.5f), glm::vec3(0.0f, -181.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 10) treeTrans = TTN_Transform(glm::vec3(8.2 + 1.8f, -0.2f, 1.2f), glm::vec3(0.0f, 179.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 11) treeTrans = TTN_Transform(glm::vec3(-9.4f, -0.1f, 7.6f), glm::vec3(0.0f, 72.0f, 0.0f), glm::vec3(0.075f));

		else if (i == 12) treeTrans = TTN_Transform(glm::vec3(9.0f + 2.1f, -0.1f, 8.3f), glm::vec3(0.0f, 190.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 13) treeTrans = TTN_Transform(glm::vec3(-10.0f, -0.1f, 4.7f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 14) treeTrans = TTN_Transform(glm::vec3(-9.5f, -0.1f, 3.2f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 15) treeTrans = TTN_Transform(glm::vec3(10.1 + 2.1f, -0.1f, 4.1f), glm::vec3(0.0f, -180.2f, 0.0f), glm::vec3(0.075f));
		else if (i == 16) treeTrans = TTN_Transform(glm::vec3(8.9 + 2.1f, -0.2f, 1.5f), glm::vec3(0.0f, 181.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 17) treeTrans = TTN_Transform(glm::vec3(-9.1f, -0.1f, 7.2f), glm::vec3(0.0f, 72.0f, 0.0f), glm::vec3(0.075f));

		else if (i == 18) treeTrans = TTN_Transform(glm::vec3(9.3f + 2.0f, -0.1f, 8.2f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 19) treeTrans = TTN_Transform(glm::vec3(-10.4f, -0.1f, 4.7f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 20) treeTrans = TTN_Transform(glm::vec3(-9.2f, -0.1f, 2.7f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 21) treeTrans = TTN_Transform(glm::vec3(10.4 + 2.0f, -0.1f, 4.8f), glm::vec3(0.0f, -173.9f, 0.0f), glm::vec3(0.075f));
		else if (i == 22) treeTrans = TTN_Transform(glm::vec3(9.3 + 2.0f, -0.2f, 1.4f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 23) treeTrans = TTN_Transform(glm::vec3(-9.5f, -0.1f, 7.1f), glm::vec3(0.0f, 72.0f, 0.0f), glm::vec3(0.075f));

		else if (i == 24) treeTrans = TTN_Transform(glm::vec3(9.0f + 1.75f, -0.1f, 8.0f - 2.0f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 25) treeTrans = TTN_Transform(glm::vec3(-10.0f, -0.1f, 5.0f + 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 26) treeTrans = TTN_Transform(glm::vec3(-9.5f, -0.1f, 3.0f - 1.0f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 27) treeTrans = TTN_Transform(glm::vec3(10.1 + 1.75f, -0.1f, 4.5f - 0.5f), glm::vec3(0.0f, -180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 28) treeTrans = TTN_Transform(glm::vec3(8.9 + 1.75f, -0.2f, 1.2f + 1.2f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 29) treeTrans = TTN_Transform(glm::vec3(-9.1f, -0.1f, 7.6f - 1.0f), glm::vec3(0.0f, 72.0f, 0.0f), glm::vec3(0.075f));

		else if (i == 30) treeTrans = TTN_Transform(glm::vec3(8.5f + 1.92f, -0.1f, 8.0f - 2.0f), glm::vec3(0.0f, 170.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 31) treeTrans = TTN_Transform(glm::vec3(-10.2f, -0.1f, 5.0f + 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 32) treeTrans = TTN_Transform(glm::vec3(-9.3f, -0.1f, 3.0f - 1.0f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 33) treeTrans = TTN_Transform(glm::vec3(10.4 + 1.92f, -0.1f, 4.5f - 0.5f), glm::vec3(0.0f, -181.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 34) treeTrans = TTN_Transform(glm::vec3(8.2 + 1.92f, -0.2f, 1.2f + 1.2f), glm::vec3(0.0f, 179.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 35) treeTrans = TTN_Transform(glm::vec3(-9.4f, -0.1f, 7.6f - 1.0f), glm::vec3(0.0f, 72.0f, 0.0f), glm::vec3(0.075f));

		else if (i == 36) treeTrans = TTN_Transform(glm::vec3(9.0f + 1.92f, -0.1f, 8.3f - 2.0f), glm::vec3(0.0f, 190.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 37) treeTrans = TTN_Transform(glm::vec3(-10.0f, -0.1f, 4.7f + 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 38) treeTrans = TTN_Transform(glm::vec3(-9.5f, -0.1f, 3.2f - 1.0f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 39) treeTrans = TTN_Transform(glm::vec3(10.1 + 1.92f, -0.1f, 4.1f - 0.5f), glm::vec3(0.0f, -180.2f, 0.0f), glm::vec3(0.075f));
		else if (i == 40) treeTrans = TTN_Transform(glm::vec3(8.9 + 1.92f, -0.2f, 1.5f + 1.2f), glm::vec3(0.0f, 181.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 41) treeTrans = TTN_Transform(glm::vec3(-9.1f, -0.1f, 7.2f - 1.0f), glm::vec3(0.0f, 72.0f, 0.0f), glm::vec3(0.075f));

		else if (i == 42) treeTrans = TTN_Transform(glm::vec3(9.3f + 1.5f, -0.1f, 8.2f - 2.0f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 43) treeTrans = TTN_Transform(glm::vec3(-10.4f, -0.1f, 4.7f - 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 44) treeTrans = TTN_Transform(glm::vec3(-9.2f, -0.1f, 2.7f - 1.0f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 45) treeTrans = TTN_Transform(glm::vec3(10.4 + 1.5f, -0.1f, 4.8f - 0.5f), glm::vec3(0.0f, -173.9f, 0.0f), glm::vec3(0.075f));
		else if (i == 46) treeTrans = TTN_Transform(glm::vec3(9.3 + 1.5f, -0.2f, 1.4f + 1.2f), glm::vec3(0.0f, 180.0f, 0.0f), glm::vec3(0.075f));
		else if (i == 47) treeTrans = TTN_Transform(glm::vec3(-9.5f, -0.1f, 7.1f - 1.0f), glm::vec3(0.0f, 72.0f, 0.0f), glm::vec3(0.075f));
		//attach that transform to the entity
		AttachCopy(tree, treeTrans);
	}

	//terrain entity
	{
		terrain = CreateEntity();

		//setup a transform for the terrain
		TTN_Transform terrainTrans = TTN_Transform(glm::vec3(0.0f, -15.0f / 10.0f, 35.0f / 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(100.0f / 10.0f));
		//attach that transform to the entity
		AttachCopy(terrain, terrainTrans);
	}

	//water
	{
		water = CreateEntity();

		//setup a transform for the water
		TTN_Transform waterTrans = TTN_Transform(glm::vec3(0.0f, -8.0f / 10.0f, 35.0f / 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(93.0f / 10.0f));
		//attach that transform to the entity
		AttachCopy(water, waterTrans);
	}

	//set the camera as the cannon's parent
	Get<TTN_Transform>(cannon).SetParent(&Get<TTN_Transform>(camera), camera);

	//prepare the vector of cannonballs
	cannonBalls = std::vector<std::pair<entt::entity, bool>>();
	//vector of boats
	boats = std::vector<entt::entity>();

	//vector of enemy cannons	q
	enemyCannons = std::vector<entt::entity>();

	//vector for flamethrower models and flame particles
	flames = std::vector<entt::entity>();
}

//sets up any other data the game needs to store
void Game::SetUpOtherData()
{
	Bombing = false;
	julianAlive = true;
	jerryAlive = true;
	//create the particle templates
	//smoke particle
	{
		smokeParticle = TTN_ParticleTemplate();
		smokeParticle.SetMat(smokeMat);
		smokeParticle.SetMesh(sphereMesh);
		smokeParticle.SetTwoLifetimes((playerShootCooldown - 0.1f), playerShootCooldown);
		smokeParticle.SetOneStartColor(glm::vec4(0.1f, 0.1f, 0.1f, 0.8f));
		smokeParticle.SetTwoEndColors(glm::vec4(0.25f, 0.25f, 0.25f, 0.0f), glm::vec4(0.5f, 0.5f, 0.5f, 0.25f));
		smokeParticle.SetOneStartSize(0.05f / 10.0f);
		smokeParticle.SetOneEndSize(0.05f / 10.0f);
		smokeParticle.SetTwoStartSpeeds(1.5f / 10.0f, 1.0f / 10.0f);
		smokeParticle.SetOneEndSpeed(0.05f / 10.0f);
		smokeParticle.SetOneStartAcceleration(glm::vec3(0.0f));
		smokeParticle.SetTwoEndAcellerations(glm::vec3(0.0f, 0.147f, 0.0f), glm::vec3(0.0f, 0.22f, 0.0f));
	}

	//fire particle template
	{
		fireParticle = TTN_ParticleTemplate();
		fireParticle.SetMat(fireMat);
		fireParticle.SetMesh(sphereMesh);
		fireParticle.SetOneEndColor(glm::vec4(1.0f, 0.2f, 0.0f, 0.0f));
		fireParticle.SetOneEndSize(4.0f / 10.0f);
		fireParticle.SetOneEndSpeed(6.0f / 10.0f);
		fireParticle.SetOneLifetime(2.0f);
		fireParticle.SetTwoStartColors(glm::vec4(1.0f, 0.35f, 0.1f, 1.0f), glm::vec4(1.0f, 0.60f, 0.0f, 1.0f));
		fireParticle.SetOneStartSize(0.5f / 10.0f);
		fireParticle.SetOneStartSpeed(8.5f / 10.0f);
	}

	//expolsion particle template
	{
		expolsionParticle = TTN_ParticleTemplate();
		expolsionParticle.SetMat(fireMat);
		expolsionParticle.SetMesh(sphereMesh);
		expolsionParticle.SetTwoEndColors(glm::vec4(1.0f, 0.35f, 0.1f, 0.0f), glm::vec4(1.0f, 0.60f, 0.1f, 0.0f));
		expolsionParticle.SetOneEndSize(2.5f / 10.0f);
		expolsionParticle.SetOneEndSpeed(0.05f / 10.0f);
		expolsionParticle.SetOneLifetime(1.9f);
		expolsionParticle.SetTwoStartColors(glm::vec4(1.0f, 0.35f, 0.1f, 1.0f), glm::vec4(1.0f, 0.60f, 0.1f, 1.0f));
		expolsionParticle.SetOneStartSize(0.8f / 10.0f);
		expolsionParticle.SetTwoStartSpeeds(0.01f / 10.0f, 12.0f / 10.0f);
	}

	//bird particle template
	{
		birdParticle = TTN_ParticleTemplate();
		birdParticle.SetMat(smokeMat);
		birdParticle.SetMesh(sphereMesh);
		birdParticle.SetTwoEndColors(glm::vec4(1.0f, 1.0f, 1.0f, 0.2f), glm::vec4(1.0f, 1.0f, 1.0f, 0.2f));
		birdParticle.SetOneEndSize(1.5f * 0.65f / 10.0f);
		birdParticle.SetOneEndSpeed(0.13f / 10.0f);
		birdParticle.SetTwoLifetimes(0.85f, 1.10f);
		birdParticle.SetTwoStartColors(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		birdParticle.SetOneStartSize(1.5f * 0.01f / 10.0f);
		birdParticle.SetOneStartSpeed(10.9f / 10.0f);
	}

	//muzzle flash particle template
	{
		gunParticle = TTN_ParticleTemplate();
		gunParticle.SetMat(smokeMat);
		gunParticle.SetMesh(sphereMesh);
		//	gunParticle.SetTwoEndColors(glm::vec4(0.5f, 0.5f, 0.5f, 0.1f), glm::vec4(0.5f, 0.5f, 0.5f, 0.1f)); //orange
	//gunParticle.SetTwoEndColors(glm::vec4(0.1f, 0.1f, 0.1f, 0.8f), glm::vec4(0.1f, 0.1f, 0.1f, 0.8f)); ///black
		gunParticle.SetTwoEndColors(glm::vec4(1.0f, 0.50f, 0.0f, 0.50f), glm::vec4(1.0f, 0.50f, 0.0f, 0.50f)); ///yellow
		gunParticle.SetOneEndSize(0.35f / 10.0f);
		gunParticle.SetOneEndSpeed(0.35f / 10.0f);
		gunParticle.SetTwoLifetimes(0.85f, 1.10f);
		gunParticle.SetTwoStartColors(glm::vec4(1.0f, 0.50f, 0.0f, 1.0f), glm::vec4(1.0f, 0.50f, 0.0f, 1.0f)); //orange
	//	gunParticle.SetTwoStartColors(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)); //yellow
		//gunParticle.SetTwoStartColors(glm::vec4(0.1f, 0.1f, 0.1f, 0.8f), glm::vec4(0.1f, 0.1f, 0.1f, 0.8f)); //black
		gunParticle.SetOneStartSize(0.20f / 10.0f);
		gunParticle.SetOneStartSpeed(10.0f / 10.0f);
	}

	//water splash particle template
	{
		splashparticle = TTN_ParticleTemplate();
		splashparticle.SetMat(smokeMat);
		splashparticle.SetMesh(sphereMesh);

		splashparticle.SetOneEndColor(glm::mix(glm::vec4(0.2f, 0.537f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.8f), 0.5f));
		splashparticle.SetOneStartColor(glm::mix(glm::vec4(0.2f, 0.537f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.8f), 0.5f));

		splashparticle.SetOneEndSize(0.65f / 10.0f);
		splashparticle.SetOneStartSize(0.65f / 10.0f);

		splashparticle.SetTwoStartSpeeds(1.0f, 2.0f);
		splashparticle.SetOneEndSpeed(0.0f);

		splashparticle.SetOneLifetime(2.0f);

		splashparticle.SetOneStartAcceleration(glm::vec3(0.0f, -1.8675, 0.0f));
		splashparticle.SetOneEndAcelleration(glm::vec3(0.0f, -1.8675, 0.0f));
	}

	//call the restart data function
	RestartData();

	glm::ivec2 windowSize = TTN_Backend::GetWindowSize();

	//setup the bloom effect
	m_bloomThreshold = m_bloomEffect->GetThreshold();
	m_numOfBloomPasses = m_bloomEffect->GetNumOfPasses();
	m_bloomBufferDivisor = m_bloomEffect->GetBlurDownScale();
	m_bloomRadius = m_bloomEffect->GetRadius();
	m_bloomStrength = m_bloomEffect->GetStrength();
	m_PostProcessingEffects.push_back(m_bloomEffect);

	//add color correction to the scene's list of effects to render
	m_PostProcessingEffects.push_back(m_colorCorrectEffect);

	//set all 3 effects to false
	m_applyWarmLut = false;
	m_applyCoolLut = false;
	m_applyCustomLut = false;

	//set the lighting bools
	m_noLighting = false;
	m_ambientOnly = false;
	m_specularOnly = false;
	m_ambientAndSpecular = true;
	m_ambientSpecularAndOutline = false;

	m_useTextures = true;

	for (int i = 0; i < m_mats.size(); i++)
		m_mats[i]->SetOutlineSize(m_outlineSize);
}

//restarts the game
void Game::RestartData()
{
	//player data
	rotAmmount = glm::vec2(0.0f);

	playerDir = glm::vec3(0.0f, 0.0f, 1.0f);
	playerShootCooldownTimer = playerShootCooldown;
	m_score = 0;
	m_InputDelay = m_InputDelayTime;

	//water and terrain data setup
	water_time = 0.0f;
	water_waveSpeed = -2.5f;
	water_waveBaseHeightIncrease = 0.0f;
	water_waveHeightMultiplier = 0.005f;
	water_waveLenghtMultiplier = -10.0f;

	//dam and flamethrower data setup
	Flaming = false;
	FlameTimer = 0.0f;
	FlameAnim = 0.0f;
	Dam_health = Dam_MaxHealth;

	//bird data setup
	BombTimer = 0.0f;

	//scene data setup
	TTN_Scene::SetGravity(glm::vec3(0.0f, -9.8f / 10.0f, 0.0f));
	m_paused = false;
	m_gameOver = false;
	m_gameWin = false;

	//shop stuff
	healAmount = 10.0f;
	healCounter = 0;
	cannonBuff = false;
	abilityCooldownBuff = false;
	upgradeAbilities = false;
	//bools for cost reset
	cannonScoreCost = false;
	abilityScoreCost = false;
	upgradeScoreCost = false;

	//enemy and wave data setup
	m_currentWave = 0;
	m_timeTilNextWave = m_timeBetweenEnemyWaves;
	m_timeUntilNextSpawn = m_timeBetweenEnemyWaves;
	m_boatsRemainingThisWave = m_enemiesPerWave;
	m_boatsStillNeedingToSpawnThisWave = m_boatsRemainingThisWave;
	m_rightSideSpawn = (bool)(rand() % 2);
	m_waveInProgress = false;
	m_firstWave = true;
	speedMod = 0.0f;
	damageMod = 0.0f;
	//delete all boats in scene
	auto enemyView = GetScene()->view<EnemyComponent>();
	std::vector<entt::entity> enemies = std::vector<entt::entity>();
	for (auto entity : enemyView) {
		enemies.push_back(entity);
	}

	std::vector<entt::entity>::iterator it = enemies.begin();
	while (it != enemies.end()) {
		//add a countdown until it deletes
		TTN_DeleteCountDown countdown = TTN_DeleteCountDown(0.01f);
		TTN_DeleteCountDown cannonCountdown = TTN_DeleteCountDown(0.008f);
		AttachCopy(*it, countdown);
		AttachCopy(Get<EnemyComponent>(*it).GetCannonEntity(), cannonCountdown);
		Get<TTN_MorphAnimator>(Get<EnemyComponent>(*it).GetCannonEntity()).SetActiveAnim(0);
		//mark it as dead
		Get<EnemyComponent>(*it).SetAlive(false);

		it = enemies.erase(it);
	}

	boats.clear();
	enemyCannons.clear();

	//delete all of the birds in the scene
	std::vector<entt::entity>::iterator itt = birds.begin();
	while (itt != birds.end()) {
		//delete the bird
		DeleteEntity(*itt);
		itt = birds.erase(itt);
	}

	for (int i = 0; i < birdNum; i++)
		MakeABird();

	//delete all of the particle systems in the scene
	auto particleView = GetScene()->view<TTN_ParticeSystemComponent>();
	std::vector<entt::entity> particles = std::vector<entt::entity>();
	for (auto entity : particleView) {
		particles.push_back(entity);
	}

	if (particles.size() > 0.0f) {
		std::vector<entt::entity>::iterator ittt = particles.begin();
		while (ittt != particles.end()) {
			//delete the particle system
			DeleteEntity(*ittt);
			itt = particles.erase(ittt);
		}
	}

	flames.clear();

	//entity for the smoke particle system (rather than recreating whenever we need it, we'll just make one
	//and burst again when we need to)
	{
		smokePS = CreateEntity();

		//setup a transfrom for the particle system
		TTN_Transform smokePSTrans = TTN_Transform(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f / 10.0f));
		//attach that transform to the entity
		AttachCopy(smokePS, smokePSTrans);

		//setup a particle system for the particle system
		TTN_ParticleSystem::spsptr ps = std::make_shared<TTN_ParticleSystem>(500, 0, smokeParticle, 0.0f, false);
		ps->MakeCircleEmitter(glm::vec3(0.0f));
		ps->VelocityReadGraphCallback(FastStart);
		ps->ColorReadGraphCallback(SlowStart);
		ps->MakeParticlesAsSprites(TTN_AssetSystem::GetTexture2D("Particle Sprite"));
		//setup a particle system component
		TTN_ParticeSystemComponent psComponent = TTN_ParticeSystemComponent(ps);
		//attach the particle system component to the entity
		AttachCopy(smokePS, psComponent);
	}

	//sets the buses to not be paused
	engine.GetBus("Music").SetPaused(false);
	engine.GetBus("SFX").SetPaused(false);

	//turn off all the instruments except the hihats
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("BangoPlaying", 0);
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("MarimbaPlaying", 0);
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("RecorderPlaying", 0);
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("TrumpetsPlaying", 0);
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("HihatsPlaying", 1);
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap1Playing", 0);
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap2Playing", 0);
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap3Playing", 0);
	engine.GetEvent(m_music->GetNextEvent()).SetParameter("BassDrumPlaying", 0);
	//and set the number of times the melody should play
	timesMelodyShouldPlay = rand() % 3 + 4;
	//and play the music
	m_music->SetNextPostion(glm::vec3(0.0f));
	m_music->PlayFromQueue();

	mousePos = TTN_Application::TTN_Input::GetMousePosition();
	firstFrame = true;
	m_score = 0;
	//reset prices
	healCost = 250; //score cost of heal
	cannonCost = 1000; //score cost of cannon powerup
	abilityCost = 750; //score cost of ability cd powerup
	upgradeCost = 750; //score cost of upgrade powerup

	m_DialogueOpening->SetNextPostion(glm::vec3(0.0f));
	m_DialogueOpening->PlayFromQueue();

	m_hasGivenLowHealthWarning = false;
	m_hasHitABirdThisRound = false;
	birdKillCount = 0;
}

#pragma endregion

#pragma region Player and CANNON Stuff
//called by update once a frame, allows the player to rotate
void Game::PlayerRotate(float deltaTime)
{
	//get the mouse position
	glm::vec2 tempMousePos = TTN_Application::TTN_Input::GetMousePosition();

	if (m_InputDelay <= 0.0f && !firstFrame) {
		//figure out how much the cannon and camera should be rotated
		glm::vec2 addAmmount = (tempMousePos - mousePos) * (mouseSensetivity / 5.f) * deltaTime;
		rotAmmount += addAmmount;

		//clamp the rotation to within 85 degrees of the base rotation in all the directions
		if (rotAmmount.x > 80.0f) rotAmmount.x = 80.0f;
		else if (rotAmmount.x < -80.0f) rotAmmount.x = -80.0f;
		if (rotAmmount.y > 32.5f) rotAmmount.y = 32.5f;
		else if (rotAmmount.y < -85.0f) rotAmmount.y = -85.0f;

		//reset the rotation
		Get<TTN_Transform>(camera).LookAlong(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//and rotate it by the ammount it should be rotated
		Get<TTN_Transform>(camera).RotateFixed(glm::vec3(rotAmmount.y, -rotAmmount.x, 0.0f));
		//clear the direction the player is facing, and rotate it to face the same along
		playerDir = glm::vec3(0.0f, 0.0f, 1.0f);
		playerDir = glm::vec3(glm::toMat4(glm::quat(glm::radians(glm::vec3(rotAmmount.y, -rotAmmount.x, 0.0f)))) * glm::vec4(playerDir, 1.0f));
		playerDir = glm::normalize(playerDir);
	}

	//save the next position to rotate properly next frame
	mousePos = tempMousePos;
	firstFrame = false;
}

//called by update, makes the cannon switch back to it's not firing animation when it's firing animation has ended
void Game::StopFiring()
{
	if (Get<TTN_MorphAnimator>(cannon).getActiveAnim() == 1 &&
		Get<TTN_MorphAnimator>(cannon).getActiveAnimRef().getIsDone()) {
		Get<TTN_MorphAnimator>(cannon).SetActiveAnim(0);
	}
}

//function to create a cannonball, used when the player fires
void Game::CreateCannonball()
{
	//create the cannonball
	{
		//create the entity
		cannonBalls.push_back(std::pair(CreateEntity(), false));

		//set up a renderer for the cannonball
		TTN_Renderer cannonBallRenderer = TTN_Renderer(sphereMesh, shaderProgramTextured, cannonMat);
		//attach that renderer to the entity
		AttachCopy(cannonBalls[cannonBalls.size() - 1].first, cannonBallRenderer);

		//set up a transform for the cannonball
		TTN_Transform cannonBallTrans = TTN_Transform();
		cannonBallTrans.SetPos(Get<TTN_Transform>(cannon).GetGlobalPos());
		cannonBallTrans.SetScale(glm::vec3(0.35f / 10.0f));
		//attach that transform to the entity
		AttachCopy(cannonBalls[cannonBalls.size() - 1].first, cannonBallTrans);

		//set up a physics body for the cannonball
		TTN_Physics cannonBallPhysBod = TTN_Physics(cannonBallTrans.GetPos(), glm::vec3(0.0f), cannonBallTrans.GetScale() + glm::vec3(0.1f / 10.0f),
			cannonBalls[cannonBalls.size() - 1].first);

		//attach that physics body to the entity
		AttachCopy(cannonBalls[cannonBalls.size() - 1].first, cannonBallPhysBod);
		//get the physics body and apply a force along the player's direction
		Get<TTN_Physics>(cannonBalls[cannonBalls.size() - 1].first).AddForce((cannonBallForce * playerDir));

		TTN_Tag ballTag = TTN_Tag("Ball"); //sets boat path number to ttn_tag
		AttachCopy<TTN_Tag>(cannonBalls[cannonBalls.size() - 1].first, ballTag);
	}
}

//function that will check the positions of the cannonballs each frame and delete any that're too low
void Game::DeleteCannonballs()
{
	//iterate through the vector of cannonballs, deleting the cannonball if it is at or below y = -50
	std::vector<std::pair<entt::entity, bool>>::iterator it = cannonBalls.begin();
	while (it != cannonBalls.end()) {
		if (Get<TTN_Transform>((*it).first).GetGlobalPos().y > -40.0f / 10.0f) {
			it++;
		}
		else {
			DeleteEntity((*it).first);
			it = cannonBalls.erase(it);
		}
	}
}

//function that will create an expolsion particle effect at a given input location
void Game::CreateExpolsion(glm::vec3 location)
{
	//we don't really need to save the entity number for any reason, so we just make the variable local
	entt::entity newExpolsion = CreateEntity(2.0f);

	//setup a transfrom for the particle system
	TTN_Transform PSTrans = TTN_Transform(location, glm::vec3(0.0f), glm::vec3(1.0f / 10.0f));
	//attach that transform to the entity
	AttachCopy(newExpolsion, PSTrans);
	glm::vec3 tempLoc = Get<TTN_Transform>(newExpolsion).GetGlobalPos();

	//setup a particle system for the particle system
	TTN_ParticleSystem::spsptr ps = std::make_shared<TTN_ParticleSystem>(500, 0, expolsionParticle, 0.0f, false);
	ps->MakeSphereEmitter();
	ps->VelocityReadGraphCallback(FastStart);
	ps->ColorReadGraphCallback(ZeroUntilHalfThenOne);
	ps->ScaleReadGraphCallback(ZeroOneZero);
	ps->MakeParticlesAsSprites(TTN_AssetSystem::GetTexture2D("Particle Sprite"));
	//setup a particle system component
	TTN_ParticeSystemComponent psComponent = TTN_ParticeSystemComponent(ps);
	//attach the particle system component to the entity
	AttachCopy(newExpolsion, psComponent);

	m_enemyDeathSound->SetNextPostion(glm::vec3(-1.0f, 1.0f, 1.0f) * location);
	m_enemyDeathSound->PlayFromQueue();

	//get a reference to that particle system and burst it
	Get<TTN_ParticeSystemComponent>(newExpolsion).GetParticleSystemPointer()->Burst(200);
}

void Game::CreateBirdExpolsion(glm::vec3 location)
{
	//we don't really need to save the entity number for any reason, so we just make the variable local
	entt::entity newExpolsion = CreateEntity(2.0f);

	//setup a transfrom for the particle system
	TTN_Transform PSTrans = TTN_Transform(location, glm::vec3(0.0f), glm::vec3(1.0f / 10.0f));
	//attach that transform to the entity
	AttachCopy(newExpolsion, PSTrans);
	glm::vec3 tempLoc = Get<TTN_Transform>(newExpolsion).GetGlobalPos();

	//setup a particle system for the particle system
	TTN_ParticleSystem::spsptr ps = std::make_shared<TTN_ParticleSystem>(25, 0, birdParticle, 0.0f, false);
	//ps->MakeCircleEmitter(glm::vec3(0.01f));
	ps->MakeSphereEmitter();
	ps->VelocityReadGraphCallback(FastStart);
	ps->ColorReadGraphCallback(SlowStart);
	ps->ScaleReadGraphCallback(ZeroOneZero);
	ps->MakeParticlesAsSprites(TTN_AssetSystem::GetTexture2D("Feather Sprite"));
	//setup a particle system component
	TTN_ParticeSystemComponent psComponent = TTN_ParticeSystemComponent(ps);
	//attach the particle system component to the entity
	AttachCopy(newExpolsion, psComponent);

	//get a reference to that particle system and burst it
	Get<TTN_ParticeSystemComponent>(newExpolsion).GetParticleSystemPointer()->Burst(5);
}

void Game::CreateMuzzleFlash(glm::vec3 location, entt::entity e, glm::vec3 direction, float directionMultipler, bool inverted)
{
	//we don't really need to save the entity number for any reason, so we just make the variable local
	entt::entity newExpolsion = CreateEntity(2.5f);

	//setup a transfrom for the particle system
	TTN_Transform PSTrans = TTN_Transform(location, glm::vec3(0.0f), glm::vec3(1.0f));

	//make a right vector from the direction and world up
	glm::vec3 right = glm::normalize(glm::cross(glm::normalize(direction), glm::vec3(0.0f, 1.0f, 0.0f)));

	//randomize which barrllel the shot is coming from
	int pos = rand() % 3;
	if (pos == 0) {
		PSTrans.SetPos(glm::vec3(location.x, location.y, location.z) + directionMultipler * glm::normalize(direction));
	}
	if (pos == 1) {
		PSTrans.SetPos(glm::vec3(location.x, location.y, location.z) + directionMultipler * glm::normalize(direction) + right * 0.06f);
	}
	if (pos == 2) {
		PSTrans.SetPos(glm::vec3(location.x, location.y, location.z) + directionMultipler * glm::normalize(direction) - right * 0.06f);
	}

	//get the angle at which it should be rotated along the y-axis in order to be facing in the correct direction
	float newAngle = glm::degrees(glm::acos(glm::dot(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)), glm::normalize(direction))));

	//attach that transform to the entity
	AttachCopy(newExpolsion, PSTrans);

	//setup a particle system for the particle system
	TTN_ParticleSystem::spsptr ps = std::make_shared<TTN_ParticleSystem>(25, 0, gunParticle, 0.0f, false);

	//if (!inverted)ps->MakeConeEmitter(10.0f, glm::vec3(-tempR.y, -tempR.x, tempR.z));//-75 x
	//else if (inverted)ps->MakeConeEmitter(10.0f, glm::vec3(tempR.y, -tempR.x, tempR.z));
	ps->MakeConeEmitter(10.0f, glm::vec3(-90.0f, newAngle, 0.0f));
	ps->VelocityReadGraphCallback(FastStart);
	ps->ColorReadGraphCallback(SlowStart);
	ps->ScaleReadGraphCallback(ZeroOneZero);
	ps->MakeParticlesAsSprites(TTN_AssetSystem::GetTexture2D("Particle Sprite"));
	//setup a particle system component
	TTN_ParticeSystemComponent psComponent = TTN_ParticeSystemComponent(ps);
	//attach the particle system component to the entity
	AttachCopy(newExpolsion, psComponent);

	//get a reference to that particle system and burst it
	Get<TTN_ParticeSystemComponent>(newExpolsion).GetParticleSystemPointer()->Burst(30);
	//Get<TTN_Transform>(newExpolsion).SetParent(&Get<TTN_Transform>(e),e);

	m_enemyCannonSound->SetNextPostion(glm::vec3(-1.0f, 1.0f, 1.0f) * location);
	m_enemyCannonSound->PlayFromQueue();
}

void Game::CreateSplash(glm::vec3 location)
{
	{
		//we can just make it a local variable
		entt::entity splashPS = CreateEntity(2.0f);

		//setup a transfrom for the particle system
		TTN_Transform splashPsTrans = TTN_Transform(location, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

		//attach that transform to the entity
		AttachCopy(splashPS, splashPsTrans);

		//setup a particle system for the particle system
		TTN_ParticleSystem::spsptr ps = std::make_shared<TTN_ParticleSystem>(100, 0, splashparticle, 10.0f, true);
		ps->MakeConeEmitter(15.0f, glm::vec3(0.0f, 0.0f, 0.0f));
		ps->MakeParticlesAsSprites(TTN_AssetSystem::GetTexture2D("Particle Sprite"));

		//setup a particle system component
		TTN_ParticeSystemComponent psComponent = TTN_ParticeSystemComponent(ps);
		//attach the particle system component to the entity
		AttachCopy(splashPS, psComponent);

		Get<TTN_ParticeSystemComponent>(splashPS).GetParticleSystemPointer()->Burst(100);
	}
}

//creates the flames for the flamethrower
void Game::Flamethrower() {
	//if the cooldown has ended
	if (FlameTimer <= 0.0f) {
		//reset cooldown
		FlameTimer = FlameThrowerCoolDown;
		//set the active flag to true
		Flaming = true;
		//and through and create the fire particle systems
		for (int i = 0; i < 6; i++) {
			//fire particle entities
			{
				flames.push_back(CreateEntity(FlameActiveTime + 1.0f));

				//setup a transfrom for the particle system
				TTN_Transform firePSTrans = TTN_Transform(Get<TTN_Transform>(flamethrowers[i]).GetGlobalPos() + glm::vec3(0.0f, 0.0f, 2.0f / 10.0f), glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(1.0f / 10.0f));

				//attach that transform to the entity
				AttachCopy(flames[i], firePSTrans);

				//setup a particle system for the particle system
				TTN_ParticleSystem::spsptr ps = std::make_shared<TTN_ParticleSystem>(300, 50, fireParticle, FlameActiveTime, true);
				ps->MakeConeEmitter(15.0f, glm::vec3(90.0f, 0.0f, 0.0f));
				ps->SetStopAfter(FlameActiveTime - 1.0f);
				ps->MakeParticlesAsSprites(TTN_AssetSystem::GetTexture2D("Particle Sprite"));

				//setup a particle system component
				TTN_ParticeSystemComponent psComponent = TTN_ParticeSystemComponent(ps);
				//attach the particle system component to the entity
				AttachCopy(flames[i], psComponent);
			}
		}

		//play the sound effect
		m_flamethrowerSound->SetNextPostion(glm::vec3(0.0f));
		m_flamethrowerSound->PlayFromQueue();

		if (TTN_Random::RandomFloat(0.0f, 1.0f) <= 0.1f) {
			m_DialougeFlamethrower->SetNextPostion(glm::vec3(0.0f));
			m_DialougeFlamethrower->PlayFromQueue();
		}
	}
	//otherwise nothing happens
	else {
		Flaming = false;
	}
}

//function to update the flamethrower logic
void Game::FlamethrowerUpdate(float deltaTime)
{
	//reduce the cooldown timer on the flamethrower
	FlameTimer -= deltaTime;

	//if the flamethrowers are active
	if (Flaming) {
		//increment flamethrower anim timer
		FlameAnim += deltaTime;

		//if it's reached the end of the animation
		if (FlameAnim >= FlameActiveTime) {
			//get rid of all the flames, reset the timer and set the active flag to false
			flames.clear();
			FlameAnim = 0.0f;
			Flaming = false;
		}

		//while it's flaming, iterate through the vector of boats, deleting the boat if it is at or below z = 27
		std::vector<entt::entity>::iterator it = boats.begin();
		while (it != boats.end()) {
			if (Get<TTN_Transform>(*it).GetPos().z >= 27.0f / 10.0f) {
				it++;
			}
			else {
				//remove the physics from it
				Remove<TTN_Physics>(*it);
				//add a countdown until it deletes
				TTN_DeleteCountDown countdown = TTN_DeleteCountDown(2.5f);
				TTN_DeleteCountDown cannonCountdown = TTN_DeleteCountDown(2.48f);
				AttachCopy(*it, countdown);
				AttachCopy(Get<EnemyComponent>(*it).GetCannonEntity(), cannonCountdown);
				Get<TTN_MorphAnimator>(Get<EnemyComponent>(*it).GetCannonEntity()).SetActiveAnim(0);
				//mark it as dead
				Get<EnemyComponent>(*it).SetAlive(false);

				//add to the player's score
				m_score += 50;

				//and remove it from the list of boats as it will be deleted soon
				std::vector<entt::entity>::iterator itt = enemyCannons.begin();
				while (itt != enemyCannons.end()) {
					if (*itt == Get<EnemyComponent>(*it).GetCannonEntity()) {
						itt = enemyCannons.erase(itt);
					}
					else
						itt++;
				}

				it = boats.erase(it);
				m_boatsRemainingThisWave--;
			}
		}
	}
}
#pragma endregion

#pragma region Enemy spawning and wave stuff
//spawn a boat on the left side of the map
void Game::SpawnBoatLeft()
{
	//boats
	{
		//create the entity
		boats.push_back(CreateEntity());
		int randomBoat = rand() % 3;
		//int randomBoat = 0;

		//create a renderer
		TTN_Renderer boatRenderer = TTN_Renderer(boat1Mesh, shaderProgramTextured, boat1Mat);
		//setup renderer for green boat
		if (randomBoat == 0) {
			boatRenderer = TTN_Renderer(boat1Mesh, shaderProgramTextured, boat1Mat);
			//boatRenderer = TTN_Renderer(boat1Mesh, shaderProgramAnimatedTextured, boat1Mat);
		}
		//setup renderer for red boat
		else if (randomBoat == 1) {
			boatRenderer = TTN_Renderer(boat2Mesh, shaderProgramTextured, boat2Mat);
		}
		//set up renderer for yellow boat
		else if (randomBoat == 2) {
			boatRenderer = TTN_Renderer(boat3Mesh, shaderProgramTextured, boat3Mat);
		}
		//attach the renderer to the boat
		AttachCopy<TTN_Renderer>(boats[boats.size() - 1], boatRenderer);

		//create a transform for the boat
		TTN_Transform boatTrans = TTN_Transform(glm::vec3(20.0f / 10.0f, 10.0f / 10.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f / 10.0f));
		//set up the transform for the green boat
		if (randomBoat == 0) {
			boatTrans.RotateFixed(glm::vec3(0.0f, 180.0f, 0.0f));
			boatTrans.SetScale(glm::vec3(0.25f / 10.0f, 0.25f / 10.0f, 0.25f / 10.0f));
			boatTrans.SetPos(glm::vec3(90.0f / 10.0f, -8.5f / 10.0f, 115.0f / 10.0f));
		}
		//setup transform for the red boat
		else if (randomBoat == 1) {
			boatTrans.RotateFixed(glm::vec3(0.0f, -90.0f, 0.0f));
			boatTrans.SetScale(glm::vec3(0.05f / 10.0f, 0.05f / 10.0f, 0.05f / 10.0f));
			boatTrans.SetPos(glm::vec3(90.0f / 10.0f, -8.0f / 10.0f, 115.0f / 10.0f));
		}
		//set up transform for the yellow boat
		else if (randomBoat == 2) {
			boatTrans.RotateFixed(glm::vec3(0.0f, 90.0f, 0.0f));
			boatTrans.SetScale(glm::vec3(0.15f / 10.0f, 0.15f / 10.0f, 0.15f / 10.0f));
			boatTrans.SetPos(glm::vec3(90.0f / 10.0f, -7.5f / 10.0f, 115.0f / 10.0f));
		}
		//attach the transform
		AttachCopy<TTN_Transform>(boats[boats.size() - 1], boatTrans);
		//AttachCopy(boats[boats.size() - 1], boatTrans);

		//create an attach a physics body
		glm::vec3 scale = glm::vec3(2.0f / 10.0f, 5.0f / 10.0f, 8.95f / 10.0f);
		if (randomBoat == 1) scale = glm::vec3(2.0f / 10.0f, 4.7f / 10.0f, 8.95f / 10.0f);
		if (randomBoat == 2) scale = glm::vec3(2.0f / 10.0f, 3.8f / 10.0f, 8.95f / 10.0f);
		TTN_Physics pbody = TTN_Physics(boatTrans.GetPos(), glm::vec3(0.0f), scale, boats[boats.size() - 1], TTN_PhysicsBodyType::DYNAMIC);
		pbody.SetLinearVelocity(glm::vec3(-25.0f / 10.0f, 0.0f, 0.0f));//-2.0f
		AttachCopy<TTN_Physics>(boats[boats.size() - 1], pbody);

		//creates and attaches a tag to the boat
		TTN_Tag boatTag = TTN_Tag("Boat");
		AttachCopy<TTN_Tag>(boats[boats.size() - 1], boatTag);

		//create and attach the enemy component to the boat
		int randPath = rand() % 3; // generates path number between 0-2 (left side paths, right side path nums are 3-5)
		//int randPath = 2; // generates path number between 0-2 (left side paths, right side path nums are 3-5)
		EnemyComponent en = EnemyComponent(boats[boats.size() - 1], this, randomBoat, randPath, 0.0f);
		en.SetSpeedMod(speedMod);
		AttachCopy(boats[boats.size() - 1], en);
	}

	//enemy ship cannons
	{
		enemyCannons.push_back(CreateEntity());

		//create a renderer
		TTN_Renderer cannonRenderer = TTN_Renderer(enemyCannonMesh, shaderProgramAnimatedTextured, enemyCannonMat);

		//attach that renderer to the entity
		AttachCopy<TTN_Renderer>(enemyCannons[enemyCannons.size() - 1], cannonRenderer);

		//transform component
		TTN_Transform cannonTrans = TTN_Transform(glm::vec3(4.0f, 3.0f, -14.0f), glm::vec3(0.0f), glm::vec3(1.0f));

		if (Get<EnemyComponent>(boats[boats.size() - 1]).GetBoatType() == 0) {//green
			cannonTrans.SetPos(glm::vec3(1.35f, 7.5f, -17.5f));
			cannonTrans.RotateFixed(glm::vec3(0.0f, 90.0f, 0.0f));
			cannonTrans.SetScale(glm::vec3(0.35f));
		}

		else if (Get<EnemyComponent>(boats[boats.size() - 1]).GetBoatType() == 1) {//ac carrier /red
			cannonTrans.SetPos(glm::vec3(8.0f, 18.0f, 40.0f));
			cannonTrans.RotateFixed(glm::vec3(0.0f, 90.0f, 0.0f));
			cannonTrans.SetScale(glm::vec3(1.95f));
		}

		else if (Get<EnemyComponent>(boats[boats.size() - 1]).GetBoatType() == 2) { //yellow
			cannonTrans.RotateFixed(glm::vec3(0.0f, 90.0f, 0.0f));
			cannonTrans.SetScale(glm::vec3(1.0f));
		}

		//attach transform to cannon
		AttachCopy<TTN_Transform>(enemyCannons[enemyCannons.size() - 1], cannonTrans);

		//create an animator
		TTN_MorphAnimator cannonAnimator = TTN_MorphAnimator();

		//create an animation for the cannon when it's not firing
		TTN_MorphAnimation notFiringAnim = TTN_MorphAnimation({ 0 }, { 3.0f / 37 }, true); //anim 0
		//create an animation for the cannon when it is firing
		std::vector<int> firingFrameIndices = std::vector<int>();
		std::vector<float> firingFrameLengths = std::vector<float>();
		for (int i = 0; i < 17; i++) firingFrameIndices.push_back(i);
		firingFrameLengths.push_back(4.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(3.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);//8
		firingFrameLengths.push_back(3.0f / 37.0f);//9
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(3.0f / 37.0f);//10
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(3.0f / 37.0f);//9
		firingFrameLengths.push_back(3.0f / 37.0f);//9
		firingFrameLengths.push_back(3.0f / 37.0f);//9
		TTN_MorphAnimation firingAnim = TTN_MorphAnimation(firingFrameIndices, firingFrameLengths, true); //anim 1
		//add both animatons to the animator
		cannonAnimator.AddAnim(notFiringAnim);
		cannonAnimator.AddAnim(firingAnim);
		//start on the not firing anim
		cannonAnimator.SetActiveAnim(0);
		//attach that animator to the entity
		AttachCopy(enemyCannons[enemyCannons.size() - 1], cannonAnimator);
	}

	Get<EnemyComponent>(boats[boats.size() - 1]).SetCannonEntity(enemyCannons[enemyCannons.size() - 1]);
}

//spawn a boat on the right side of the map
void Game::SpawnBoatRight() {
	{
		boats.push_back(CreateEntity());
		//gets the type of boat
		int randomBoat = rand() % 3;

		//create a renderer
		TTN_Renderer boatRenderer = TTN_Renderer(boat1Mesh, shaderProgramTextured, boat1Mat);
		//set up renderer for green boat
		if (randomBoat == 0) {
			boatRenderer = TTN_Renderer(boat1Mesh, shaderProgramTextured, boat1Mat);
		}
		//set up renderer for red boat
		else if (randomBoat == 1) {
			boatRenderer = TTN_Renderer(boat2Mesh, shaderProgramTextured, boat2Mat);
		}
		//set up renderer for yellow boat
		else if (randomBoat == 2) {
			boatRenderer = TTN_Renderer(boat3Mesh, shaderProgramTextured, boat3Mat);
		}
		//attach the renderer to the entity
		AttachCopy<TTN_Renderer>(boats[boats.size() - 1], boatRenderer);

		//create a transform for the boat
		//TTN_Transform boatTrans = TTN_Transform();
		TTN_Transform boatTrans = TTN_Transform(glm::vec3(21.0f / 10.0f, 10.0f / 10.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f / 10.0f));
		//set up the transform for the green boat
		if (randomBoat == 0) {
			boatTrans.RotateFixed(glm::vec3(0.0f, 0.0f, 0.0f));
			boatTrans.SetScale(glm::vec3(0.25f / 10.0f, 0.25f / 10.0f, 0.25f / 10.0f));
			boatTrans.SetPos(glm::vec3(-90.0f / 10.0f, -8.5f / 10.0f, 115.0f / 10.0f));
		}
		//set up the transform for the red boat
		else if (randomBoat == 1) {
			boatTrans.RotateFixed(glm::vec3(0.0f, 90.0f, 0.0f));
			boatTrans.SetScale(glm::vec3(0.05f / 10.0f, 0.05f / 10.0f, 0.05f / 10.0f));
			boatTrans.SetPos(glm::vec3(-90.0f / 10.0f, -8.0f / 10.0f, 115.0f / 10.0f));
		}
		//set up the transform the yellow boat
		else if (randomBoat == 2) {
			boatTrans.RotateFixed(glm::vec3(0.0f, -90.0f, 0.0f));
			boatTrans.SetScale(glm::vec3(0.15f / 10.0f, 0.15f / 10.0f, 0.15f / 10.0f));
			boatTrans.SetPos(glm::vec3(-90.0f / 10.0f, -7.5f / 10.0f, 115.0f / 10.0f));
		}
		//attach the transform
		//AttachCopy<TTN_Transform>(boats[boats.size() - 1], boatTrans);
		AttachCopy(boats[boats.size() - 1], boatTrans);

		//create and attach a physics body to the boats
		TTN_Physics pbody = TTN_Physics(boatTrans.GetPos(), glm::vec3(0.0f), glm::vec3(2.1f / 10.0f, 4.7f / 10.0f, 9.05f / 10.0f), boats[boats.size() - 1]);
		pbody.SetLinearVelocity(glm::vec3(25.0f / 10.0f, 0.0f, 0.0f));//-2.0f
		AttachCopy<TTN_Physics>(boats[boats.size() - 1], pbody);

		//creates and attaches a tag to the boat
		TTN_Tag boatTag = TTN_Tag("Boat");
		AttachCopy<TTN_Tag>(boats[boats.size() - 1], boatTag);

		//create and attach the enemy component to the boat
		int randPath = rand() % 3 + 3; // generates path number between 3-5 (right side paths, left side path nums are 0-2)
		//int randPath = 5; // generates path number between 3-5 (right side paths, left side path nums are 0-2)
		EnemyComponent en = EnemyComponent(boats[boats.size() - 1], this, randomBoat, randPath, 0.0f);
		en.SetSpeedMod(speedMod);
		AttachCopy(boats[boats.size() - 1], en);
	}

	//enemy ship cannons
	{
		enemyCannons.push_back(CreateEntity());

		//create a renderer
		TTN_Renderer cannonRenderer = TTN_Renderer(enemyCannonMesh, shaderProgramAnimatedTextured, enemyCannonMat);

		//attach that renderer to the entity
		AttachCopy<TTN_Renderer>(enemyCannons[enemyCannons.size() - 1], cannonRenderer);

		//transform component
		TTN_Transform cannonTrans = TTN_Transform(glm::vec3(4.0f, 2.0f, -18.0f), glm::vec3(0.0f), glm::vec3(1.0f));

		if (Get<EnemyComponent>(boats[boats.size() - 1]).GetBoatType() == 0) {//green
			cannonTrans.SetPos(glm::vec3(1.35f, 7.5f, -17.5f));
			cannonTrans.RotateFixed(glm::vec3(0.0f, 90.0f, 0.0f));
			cannonTrans.SetScale(glm::vec3(0.35f));
		}

		else if (Get<EnemyComponent>(boats[boats.size() - 1]).GetBoatType() == 1) {//ac carrier /red
			cannonTrans.SetPos(glm::vec3(8.0f, 18.0f, 40.0f));
			cannonTrans.RotateFixed(glm::vec3(0.0f, 90.0f, 0.0f));
			cannonTrans.SetScale(glm::vec3(1.95f));
		}

		else if (Get<EnemyComponent>(boats[boats.size() - 1]).GetBoatType() == 2) { //yellow
			cannonTrans.RotateFixed(glm::vec3(0.0f, 90.0f, 0.0f));
			cannonTrans.SetScale(glm::vec3(1.0f));
		}
		//attach transform to cannon
		AttachCopy<TTN_Transform>(enemyCannons[enemyCannons.size() - 1], cannonTrans);

		//create an animator
		TTN_MorphAnimator cannonAnimator = TTN_MorphAnimator();

		TTN_MorphAnimation notFiringAnim = TTN_MorphAnimation({ 0 }, { 3.0f / 37 }, true); //anim 0
		//create an animation for the cannon when it is firing
		std::vector<int> firingFrameIndices = std::vector<int>();
		std::vector<float> firingFrameLengths = std::vector<float>();
		for (int i = 0; i < 17; i++) firingFrameIndices.push_back(i);
		firingFrameLengths.push_back(4.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(3.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);
		firingFrameLengths.push_back(2.0f / 37.0f);//8
		firingFrameLengths.push_back(3.0f / 37.0f);//9
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(3.0f / 37.0f);//10
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(2.0f / 37.0f);//9
		firingFrameLengths.push_back(3.0f / 37.0f);//9
		firingFrameLengths.push_back(3.0f / 37.0f);//9
		TTN_MorphAnimation firingAnim = TTN_MorphAnimation(firingFrameIndices, firingFrameLengths, true); //anim 1
		//add both animatons to the animator
		cannonAnimator.AddAnim(notFiringAnim);
		cannonAnimator.AddAnim(firingAnim);
		//start on the not firing anim
		cannonAnimator.SetActiveAnim(0);
		//attach that animator to the entity
		AttachCopy(enemyCannons[enemyCannons.size() - 1], cannonAnimator);
	}

	Get<EnemyComponent>(boats[boats.size() - 1]).SetCannonEntity(enemyCannons[enemyCannons.size() - 1]);
}

//updates the waves
void Game::WaveUpdate(float deltaTime) {
	//if there are no more boats in this wave, begin the countdown to the next wave
	if (m_waveInProgress && m_boatsRemainingThisWave == 0 && m_timeTilNextWave <= 0.0f) {
		m_timeTilNextWave = m_timeBetweenEnemyWaves;
		m_timeUntilNextSpawn = m_timeBetweenEnemyWaves;
		m_hasHitABirdThisRound = false;
		playJingle = true;
		m_waveInProgress = false;
		Dam_health = round(Dam_health); // round the health at the end of the round
		//reset shop cost bools
		abilityScoreCost = false;
		cannonScoreCost = false;
		upgradeScoreCost = false;

		//play the dialouge
		if (m_currentWave == 1) {
			m_DialougeWave2->SetNextPostion(glm::vec3(0.0f));
			m_DialougeWave2->PlayFromQueue();
		}
		else if (m_currentWave == 2) {
			m_DialogueWave3->SetNextPostion(glm::vec3(0.0f));
			m_DialogueWave3->PlayFromQueue();
		}
		else if (m_currentWave == 3 && !m_arcade) {
			m_DialougeEnding->SetNextPostion(glm::vec3(0.0f));
			m_DialougeEnding->PlayFromQueue();
		}
		else if (m_currentWave == 3 && m_arcade) {
			m_DialogueWave4->SetNextPostion(glm::vec3(0.0f));
			m_DialogueWave4->PlayFromQueue();
		}
	}

	//if it is in the cooldown between waves, reduce the cooldown by deltaTime
	if (m_timeTilNextWave >= 0.0f && (!shopping)) {
		m_timeTilNextWave -= deltaTime;
	}

	//if the cooldown between waves has ended, begin the next wave
	else if (!m_waveInProgress && m_timeTilNextWave <= 0.0f && m_timeUntilNextSpawn >= 0.0f && (!shopping)) {
		m_currentWave++;
		m_boatsRemainingThisWave = m_enemiesPerWave * m_currentWave;
		m_boatsStillNeedingToSpawnThisWave = m_boatsRemainingThisWave;
		m_timeUntilNextSpawn = 0.0f;
		m_waveInProgress = true;
		m_firstWave = false;
		if (!(m_currentWave <= 1)) {
			damageMod++;
			if (damageMod >= 5.0f) {
				damageMod = 5.0f; //cap the damage growth
			}
			speedMod = speedMod + 1.8f;
			if (speedMod >= 12.0f) {
				speedMod = 12.0f; //cap the speed growth
			}
		}
		healCost = healCost * glm::floor(glm::pow(2.5f, 0.2f * float(m_currentWave)));
		//std::cout << "HEAL COST   "<< healCost << std::endl;
		abilityCost = abilityCost * glm::floor(glm::pow(2.5f, 0.2f * float(m_currentWave)));
		cannonCost = cannonCost * glm::floor(glm::pow(2.5f, 0.2f * float(m_currentWave)));
		upgradeCost = upgradeCost * glm::floor(glm::pow(2.5f, 0.2f * float(m_currentWave)));
	}

	//otherwise, check if it should spawn
	else {
		m_timeUntilNextSpawn -= deltaTime;
		//if it's time for the next enemy spawn
		if (m_timeUntilNextSpawn <= 0.0f && m_boatsStillNeedingToSpawnThisWave > 0) {
			//then spawn a new enemy and reset the timer
			if (m_rightSideSpawn)
				SpawnBoatRight();
			else
				SpawnBoatLeft();

			Get<TTN_Transform>(enemyCannons[enemyCannons.size() - 1]).SetParent(&Get<TTN_Transform>(boats[boats.size() - 1]), boats[boats.size() - 1]);

			m_rightSideSpawn = !m_rightSideSpawn;
			m_timeUntilNextSpawn = m_timeBetweenEnemySpawns;
			m_boatsStillNeedingToSpawnThisWave--;
		}
	}
}
#pragma endregion

#pragma region Collisions and Damage Stuff
//collision check
void Game::Collisions() {
	//collision checks
	//get the collisions from the base scene
	std::vector<TTN_Collision::scolptr> collisionsThisFrame = TTN_Scene::GetCollisions();

	//iterate through the collisions
	for (int i = 0; i < collisionsThisFrame.size(); i++) {
		//grab the entity numbers of the colliding entities
		entt::entity entity1Ptr = collisionsThisFrame[i]->GetBody1();
		entt::entity entity2Ptr = collisionsThisFrame[i]->GetBody2();

		//check if both entities still exist
		if (TTN_Scene::GetScene()->valid(entity1Ptr) && TTN_Scene::GetScene()->valid(entity2Ptr)) {
			bool cont = true;
			//if they do, then check they both have tags
			if (TTN_Scene::Has<TTN_Tag>(entity1Ptr) && TTN_Scene::Has<TTN_Tag>(entity2Ptr)) {
				//if they do, then do tag comparisons
	

				//if one is a boat and the other is a cannonball
				if (cont && ((Get<TTN_Tag>(entity1Ptr).getLabel() == "Boat" && Get<TTN_Tag>(entity2Ptr).getLabel() == "Ball") ||
					(Get<TTN_Tag>(entity1Ptr).getLabel() == "Ball" && Get<TTN_Tag>(entity2Ptr).getLabel() == "Boat"))) {
					//then iterate through the list of cannonballs until you find the one that's collided
					std::vector<std::pair<entt::entity, bool>>::iterator it = cannonBalls.begin();
					while (it != cannonBalls.end()) {
						if (entity1Ptr == (*it).first || entity2Ptr == (*it).first) {
							//and delete it
							DeleteEntity((*it).first);
							it = cannonBalls.erase(it);
						}
						else {
							it++;
						}
					}

					//and do the same with the boats, iteratoring through all of them until you find matching entity numbers
					std::vector<entt::entity>::iterator itt = boats.begin();
					while (itt != boats.end()) {
						if (entity1Ptr == *itt || entity2Ptr == *itt) {
							//play an expolsion at it's location
							glm::vec3 loc = Get<TTN_Transform>(*itt).GetGlobalPos();
							CreateExpolsion(loc);
							//make sure it's facing the direction it's moving
							if (Get<TTN_Physics>(*itt).GetLinearVelocity() != glm::vec3(0.0f)) Get<TTN_Transform>(*itt).LookAlong(
								glm::normalize(Get<TTN_Physics>(*itt).GetLinearVelocity()), glm::vec3(0.0f, 1.0f, 0.0f));
							//remove the physics from it
							Remove<TTN_Physics>(*itt);
							//add a countdown until it deletes
							TTN_DeleteCountDown countdown = TTN_DeleteCountDown(2.5f);
							TTN_DeleteCountDown cannonCountdown = TTN_DeleteCountDown(2.48f);
							AttachCopy(*itt, countdown);
							AttachCopy(Get<EnemyComponent>(*itt).GetCannonEntity(), cannonCountdown);
							Get<TTN_MorphAnimator>(Get<EnemyComponent>(*itt).GetCannonEntity()).SetActiveAnim(0);
							//mark it as dead
							Get<EnemyComponent>(*itt).SetAlive(false);

							//add to the player's score, based on the distance of the boat
							if (Get<TTN_Transform>(*itt).GetGlobalPos().z <= 30.0f / 10.0f)
								m_score += 50;
							else if (Get<TTN_Transform>(*itt).GetGlobalPos().z > 30.0f / 10.0f
								&& Get<TTN_Transform>(*itt).GetGlobalPos().z <= 70.0f / 10.0f)
								m_score += 100;
							else
								m_score += 200;

							//and remove it from the list of boats as it will be deleted soon
							std::vector<entt::entity>::iterator ittt = enemyCannons.begin();
							while (ittt != enemyCannons.end()) {
								if (*ittt == Get<EnemyComponent>(*itt).GetCannonEntity()) {
									ittt = enemyCannons.erase(ittt);
								}
								else
									ittt++;
							}

							//if the boat was the target for bird bomb
							if (Get<BirdComponent>(birds[0]).GetTarget() == *itt) {
								for (auto bird : birds) {
									//set the birds' target to null
									Get<BirdComponent>(bird).SetTarget(entt::null);
									//and start the cooldown on birdbomb
									Bombing = false;
									BombTimer = BirdBombCooldown;
								}
							}

							itt = boats.erase(itt);
							m_boatsRemainingThisWave--;
						}
						else {
							itt++;
						}
					}

					cont = false;
				}

				//if one is a bird  and they are not jerry or julian  and the other is a boat
				else if (cont && ((Get<TTN_Tag>(entity1Ptr).getLabel() == "Boat" && (Get<TTN_Tag>(entity2Ptr).getLabel() == "Bird" )) ||
					((Get<TTN_Tag>(entity1Ptr).getLabel() == "Bird") && Get<TTN_Tag>(entity2Ptr).getLabel() == "Boat"))) {
					//iterate through all of the boats through all of them until you find matching entity numbers
					

					std::vector<entt::entity>::iterator itt = boats.begin();
					while (itt != boats.end()) {
						if (entity1Ptr == *itt || entity2Ptr == *itt) {
							//play an expolsion at it's location
							glm::vec3 loc = Get<TTN_Transform>(*itt).GetGlobalPos();
							CreateExpolsion(loc);
							//remove the physics from it
							Remove<TTN_Physics>(*itt);
							//add a countdown until it deletes
							TTN_DeleteCountDown countdown = TTN_DeleteCountDown(2.5f);
							TTN_DeleteCountDown cannonCountdown = TTN_DeleteCountDown(2.48f);
							AttachCopy(*itt, countdown);
							AttachCopy(Get<EnemyComponent>(*itt).GetCannonEntity(), cannonCountdown);
							Get<TTN_MorphAnimator>(Get<EnemyComponent>(*itt).GetCannonEntity()).SetActiveAnim(0);
							//mark it as dead
							Get<EnemyComponent>(*itt).SetAlive(false);

							//add to the player's score, based on the distance of the boat
							if (Get<TTN_Transform>(*itt).GetGlobalPos().z <= 30.0f / 10.0f)
								m_score += 50;
							else if (Get<TTN_Transform>(*itt).GetGlobalPos().z > 30.0f / 10.0f
								&& Get<TTN_Transform>(*itt).GetGlobalPos().z <= 70.0f / 10.0f)
								m_score += 100;
							else
								m_score += 200;

							//and remove it from the list of boats as it will be deleted soon
							std::vector<entt::entity>::iterator ittt = enemyCannons.begin();
							while (ittt != enemyCannons.end()) {
								if (*ittt == Get<EnemyComponent>(*itt).GetCannonEntity()) {
									ittt = enemyCannons.erase(ittt);
								}
								else
									ittt++;
							}

							//if the boat was the target for bird bomb
							if (Get<BirdComponent>(birds[0]).GetTarget() == *itt) {
								for (auto bird : birds) {
									//set the birds' target to null
									Get<BirdComponent>(bird).SetTarget(entt::null);
									//and start the cooldown on birdbomb
									Bombing = false;
									BombTimer = BirdBombCooldown;
								}
							}

							itt = boats.erase(itt);
							m_boatsRemainingThisWave--;
						}
						else {
							itt++;
						}
					}

					cont = false;
				}

				else if (cont && ((Get<TTN_Tag>(entity1Ptr).getLabel() == "Ball" && (Get<TTN_Tag>(entity2Ptr).getLabel() == "Bird" && (Get<BirdComponent>(entity2Ptr).GetIsJerry() == false) && (Get<BirdComponent>(entity2Ptr).GetIsJulian() == false))) ||
					((Get<TTN_Tag>(entity1Ptr).getLabel() == "Bird" && (Get<BirdComponent>(entity1Ptr).GetIsJerry() == false) && (Get<BirdComponent>(entity1Ptr).GetIsJulian() == false)) && Get<TTN_Tag>(entity2Ptr).getLabel() == "Ball"))) {
					//iterate through all of the boats through all of them until you find matching entity numbers
					birdKillCount++;
					if (birdKillCount == 5) {
						m_DialougeKilling5Birds->SetNextPostion(glm::vec3(0.0f));
						m_DialougeKilling5Birds->PlayFromQueue();
					}
					else if (birdKillCount == 10) {
						m_DialougeKilling10Birds->SetNextPostion(glm::vec3(0.0f));
						m_DialougeKilling10Birds->PlayFromQueue();
					}
					else if (birdKillCount == 25) {
						m_DialougeKilling25Birds->SetNextPostion(glm::vec3(0.0f));
						m_DialougeKilling25Birds->PlayFromQueue();
					}
					else if (!m_hasHitABirdThisRound) {
						m_hasHitABirdThisRound = true;
						m_DialougeHittingABird->SetNextPostion(glm::vec3(0.0f));
						m_DialougeHittingABird->PlayFromQueue();
					}

					std::vector<entt::entity>::iterator btt = birds.begin();
					while (btt != birds.end()) {
						//if you find the bird
						if (entity1Ptr == *btt || entity2Ptr == *btt) {
							//play the particle effect and then have the bird delete soon
							CreateBirdExpolsion(Get<TTN_Transform>(*btt).GetPos());
							DeleteEntity(*btt);
							TTN_DeleteCountDown countdown = TTN_DeleteCountDown(0.001f);
							btt = birds.erase(btt);

							//make a new bird
							MakeABird();
							//and set it's target
							Get<BirdComponent>(birds[birds.size() - 1]).SetTarget(Get<BirdComponent>(birds[0]).GetTarget());

							//subtract score
							if (m_score > 50) {
								m_score = m_score - 50;
							}
						}
						else {
							btt++;
						}
					}

					cont = false;

				}
				
				//if one is jerry and other is a ball
				else if (cont && (((Get<TTN_Tag>(entity1Ptr).getLabel() == "Bird" && Get<BirdComponent>(entity1Ptr).GetIsJerry()) && Get<TTN_Tag>(entity2Ptr).getLabel() == "Ball")) ||
					(Get<TTN_Tag>(entity1Ptr).getLabel() == "Ball" && (Get<TTN_Tag>(entity2Ptr).getLabel() == "Bird" && Get<BirdComponent>(entity2Ptr).GetIsJerry()))) {
					m_DialougeKillingJerry->SetNextPostion(glm::vec3(0.0f));
					m_DialougeHittingABird->PlayFromQueue();

					//then iterate through the list of cannonballs until you find the one that's collided
					std::vector<std::pair<entt::entity, bool>>::iterator it = cannonBalls.begin();
					while (it != cannonBalls.end()) {
						if (entity1Ptr == (*it).first || entity2Ptr == (*it).first) {
							//and delete it
							DeleteEntity((*it).first);
							it = cannonBalls.erase(it);
						}
						else {
							it++;
						}
					}
					jerryAlive = false;
					std::vector<entt::entity>::iterator btt = birds.begin();
					while (btt != birds.end()) {
						//if you find the bird
						if (entity1Ptr == *btt || entity2Ptr == *btt) {
							//play the particle effect and then have the bird delete soon
							CreateBirdExpolsion(Get<TTN_Transform>(*btt).GetPos());
							DeleteEntity(*btt);
							TTN_DeleteCountDown countdown = TTN_DeleteCountDown(0.001f);
							btt = birds.erase(btt);

							//make a new bird
							MakeABird();
							//and set it's target
							Get<BirdComponent>(birds[birds.size() - 1]).SetTarget(Get<BirdComponent>(birds[0]).GetTarget());

							//subtract score
							if (m_score > 50) {
								m_score = m_score - 50;
							}
						}
						else {
							btt++;
						}
					}

					cont = false;
				}

				//if one is julian and jerry is alive and other is a ball
				else if (cont && (((Get<TTN_Tag>(entity1Ptr).getLabel() == "Bird" && Get<BirdComponent>(entity1Ptr).GetIsJulian() && jerryAlive) && Get<TTN_Tag>(entity2Ptr).getLabel() == "Ball")) ||
					(Get<TTN_Tag>(entity1Ptr).getLabel() == "Ball" && (Get<TTN_Tag>(entity2Ptr).getLabel() == "Bird" && Get<BirdComponent>(entity2Ptr).GetIsJulian() && jerryAlive))) {
					m_DialougeKillingJulianWhileJerryIsAlive->SetNextPostion(glm::vec3(0.0f));
					m_DialougeKillingJulianWhileJerryIsAlive->PlayFromQueue();
					
					//then iterate through the list of cannonballs until you find the one that's collided
					std::vector<std::pair<entt::entity, bool>>::iterator it = cannonBalls.begin();
					while (it != cannonBalls.end()) {
						if (entity1Ptr == (*it).first || entity2Ptr == (*it).first) {
							//and delete it
							DeleteEntity((*it).first);
							it = cannonBalls.erase(it);
						}
						else {
							it++;
						}
					}

					std::vector<entt::entity>::iterator btt = birds.begin();
					while (btt != birds.end()) {
						//if you find the bird
						if (entity1Ptr == *btt || entity2Ptr == *btt) {
							//play the particle effect and then have the bird delete soon
							CreateBirdExpolsion(Get<TTN_Transform>(*btt).GetPos());
							DeleteEntity(*btt);
							TTN_DeleteCountDown countdown = TTN_DeleteCountDown(0.001f);
							btt = birds.erase(btt);

							//make a new bird
							MakeABird();
							//and set it's target
							Get<BirdComponent>(birds[birds.size() - 1]).SetTarget(Get<BirdComponent>(birds[0]).GetTarget());

							//subtract score
							if (m_score > 50) {
								m_score = m_score - 50;
							}
						}
						else {
							btt++;
						}
					}

					cont = false;
				}

				//if one is julian and jerry is dead and other is a ball
				else if (cont && (((Get<TTN_Tag>(entity1Ptr).getLabel() == "Bird" && Get<BirdComponent>(entity1Ptr).GetIsJulian() && !jerryAlive) && Get<TTN_Tag>(entity2Ptr).getLabel() == "Ball")) ||
					(Get<TTN_Tag>(entity1Ptr).getLabel() == "Ball" && (Get<TTN_Tag>(entity2Ptr).getLabel() == "Bird" && Get<BirdComponent>(entity2Ptr).GetIsJulian() && !jerryAlive))) {
					m_DialougeKillingJuilian->SetNextPostion(glm::vec3(0.0f));
					m_DialougeKillingJuilian->PlayFromQueue();
					
					//then iterate through the list of cannonballs until you find the one that's collided
					std::vector<std::pair<entt::entity, bool>>::iterator it = cannonBalls.begin();
					while (it != cannonBalls.end()) {
						if (entity1Ptr == (*it).first || entity2Ptr == (*it).first) {
							//and delete it
							DeleteEntity((*it).first);
							it = cannonBalls.erase(it);
						}
						else {
							it++;
						}
					}

					std::vector<entt::entity>::iterator btt = birds.begin();
					while (btt != birds.end()) {
						//if you find the bird
						if (entity1Ptr == *btt || entity2Ptr == *btt) {
							//play the particle effect and then have the bird delete soon
							CreateBirdExpolsion(Get<TTN_Transform>(*btt).GetPos());
							DeleteEntity(*btt);
							TTN_DeleteCountDown countdown = TTN_DeleteCountDown(0.001f);
							btt = birds.erase(btt);

							//make a new bird
							MakeABird();
							//and set it's target
							Get<BirdComponent>(birds[birds.size() - 1]).SetTarget(Get<BirdComponent>(birds[0]).GetTarget());

							//subtract score
							if (m_score > 50) {
								m_score = m_score - 50;
							}
						}
						else {
							btt++;
						}
					}

					cont = false;
				}
			}
		}
	}
}

//damage cooldown and stuff
void Game::Damage(float deltaTime) {
	//iterator through all the boats
	std::vector<entt::entity>::iterator it = boats.begin();
	while (it != boats.end()) {
		//check if the boat is close enough to the dam to damage it
		if (Get<TTN_Transform>(*it).GetPos().z <= EnemyComponent::GetZTarget() + 2.0f * EnemyComponent::GetZTargetDistance())
			//if it is, damage it
			Dam_health = Dam_health - (damage + damageMod) * deltaTime * (difficulty / 100.f);
		//and move onto the next boat
		it++;
	}

	//attack anim
	std::vector<entt::entity>::iterator can = enemyCannons.begin(); //enemy cannon vector
	while (can != enemyCannons.end()) {
		if (Get<TTN_Transform>(*can).GetParentEntity() != entt::null) {
			if (Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).GetAttacking()) {
				Get<TTN_MorphAnimator>(*can).SetActiveAnim(1);
				glm::vec3 temp = Get<TTN_Transform>(*can).GetGlobalPos();
				glm::vec3 tempS = Get<TTN_Transform>(*can).GetScale();
				glm::vec3 tempR = Get<TTN_Transform>(*can).GetRotation();

				if (Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).GetMuzzleCD() <= 0.0f) {
					Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).SetMuzzleCD(muzzleFlashCD);
					float dirMultiplier = 0.0f;
					float heightIncrease = 0.0f;
					switch (Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).GetBoatType()) {
					case 0:
						dirMultiplier = 0.075f;
						heightIncrease = 0.030f;
						break;
					case 1:
						dirMultiplier = 0.075f;
						heightIncrease = 2.5f * 0.030f;
						break;
					case 2:
						dirMultiplier = 3.0f * 0.075f;
						heightIncrease = 2.5f * 0.030f;
						break;
					}

					int type = Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).GetBoatType();
					int path = Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).GetPath();
					bool inverted = false;

					if (path == 0 || path == 1 || path == 5) {
						inverted = true;
					}
					CreateMuzzleFlash(temp + glm::vec3(0.0f, heightIncrease, 0.0f), *can, Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).GetDirection(), dirMultiplier, inverted);
				}

				else {
					float temp = Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).GetMuzzleCD() - deltaTime;
					Get<EnemyComponent>((Get<TTN_Transform>(*can).GetParentEntity())).SetMuzzleCD(temp);
				}
			}
			can++;
		}
		else {
			DeleteEntity(*can);
			can = enemyCannons.erase(can);
		}
	}
}

#pragma endregion

void Game::GameSounds(float deltaTime)
{
	//check to make sure it's approraitely playing the paused or not paused theme
	if (m_paused != (bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("Paused")) {
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Paused", (int)m_paused);
	}

	//reset frame sensetive bools
	melodyFinishedThisFrame = false;
	fullMelodyFinishedThisFrame = false;

	//check if the melody should be switching
	if (timesMelodiesPlayed >= timesMelodyShouldPlay) {
		//generate a random number for the number of times it should play, 2 or 3
		timesMelodyShouldPlay = rand() % 3 + 4;
		//reset the counter for the number of times it has played
		timesMelodiesPlayed = 0;
		//and swap wheter it is currently playing the main or the off melody
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Main Melody",
			(int)(!((bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("Main Melody"))));
	}

	//if the time the melody has been playing has surpassed 6 seconds
	if (melodyTimeTracker >= 3.0f) {
		//take it back down
		melodyTimeTracker = std::fmod(melodyTimeTracker, 3.0f);
		//and add to the times the melodies has been played
		timesMelodiesPlayed++;
		//set the flag to say a melody has finished playing this frame to true
		melodyFinishedThisFrame = true;
		if (partialMelody) fullMelodyFinishedThisFrame = true;
		partialMelody = !partialMelody;
	}

	float percentBoatsRemaining = (float)m_boatsRemainingThisWave / (float)(m_enemiesPerWave * m_currentWave);
	//check if the bango should begin playing
	if (fullMelodyFinishedThisFrame && percentBoatsRemaining <= 0.85f &&
		!(bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("BangoPlaying")) {
		//if it should begin playing it
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("BangoPlaying", 1);

		//and make sure all of the drums are also playing
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("HihatsPlaying", 1);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap1Playing", 1);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap2Playing", 1);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap3Playing", 1);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("BassDrumPlaying", 1);
	}

	//check if the marimbra should begin playing
	if (fullMelodyFinishedThisFrame && percentBoatsRemaining <= 0.7f &&
		!(bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("MarimbaPlaying")) {
		//if it should begin playing it
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("MarimbaPlaying", 1);
	}

	//check if the recorder should begin playing
	if (fullMelodyFinishedThisFrame && percentBoatsRemaining <= 0.55f &&
		!(bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("RecorderPlaying")) {
		//if it should begin playing it
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("RecorderPlaying", 1);
	}

	//check if the trumpets should begin playing
	if (fullMelodyFinishedThisFrame && percentBoatsRemaining <= 0.4f && engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("TrumpetsPlaying")) {
		//if it should begin playing it
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("TrumpetsPlaying", 1);
	}

	//if only the hihats and all the claps are playing and it's been a lenght of the melody, start playing the bass drum
	if (fullMelodyFinishedThisFrame && ((bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("Clap3Playing")) &&
		!(bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("BassDrumPlaying")) {
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("BassDrumPlaying", 1);
	}

	//if only the hihats and clap 1 and 2 are playing and it's been a lenght of the melody, start playing the third clap
	if (fullMelodyFinishedThisFrame && ((bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("Clap2Playing")) &&
		!(bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("Clap3Playing")) {
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap3Playing", 1);
	}

	//if only the hihats and clap 1 are playing and it's been a lenght of the melody, start playing the second clap
	if (fullMelodyFinishedThisFrame && ((bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("Clap1Playing")) &&
		!(bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("Clap2Playing")) {
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap2Playing", 1);
	}

	//if only the hihats are playing and it's been a lenght of the melody, start playing the first clap
	if (fullMelodyFinishedThisFrame && ((bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("HihatsPlaying")) &&
		!(bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("Clap1Playing")) {
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap1Playing", 1);
	}

	//if none of the instruments are playing, and it's been a lenght of the melody since they last played, start playing them again
	if (!playJingle && melodyFinishedThisFrame && !((bool)engine.GetEvent(m_music->GetNextEvent()).GetParameterValue("HihatsPlaying"))) {
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("HihatsPlaying", 1);
	}

	//if the wave has ended this frame and the jingle should play, turn off all the instruments and play the jingle
	if (melodyFinishedThisFrame && playJingle) {
		//turn off each of the instruments
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("BangoPlaying", 0);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("MarimbaPlaying", 0);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("RecorderPlaying", 0);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("TrumpetsPlaying", 0);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("HihatsPlaying", 0);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap1Playing", 0);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap2Playing", 0);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("Clap3Playing", 0);
		engine.GetEvent(m_music->GetNextEvent()).SetParameter("BassDrumPlaying", 0);

		//reset the flag
		playJingle = false;
		//play the jingle
		m_jingle->SetNextPostion(glm::vec3(0.0f));
		m_jingle->PlayFromQueue();
		//reset the timer
		timeSinceJingleStartedPlaying = 0.0f;
	}

	melodyTimeTracker += deltaTime;
}

void Game::Shop(float deltaTime)
{
	if (shopping) {
		//heal from shop
		if ((healCounter > 0)) {
			//if (m_score < healCost) { //if score is less than the cost, do nothing
			//}
			//else {
			if (Dam_health < 100.f && Dam_health>90.f) { // if dam health is above 90 but below 100
				healAmount = abs(Dam_health - 100.f); //get remaining health
			}
			else // else normal heal amount
				healAmount = 10.f;

			Dam_health = Dam_health + healAmount; //heal
			//Dam_health = round(Dam_health);
			m_score = m_score - healCost;//score cost of heal
			//std::cout << Dam_health << std::endl;
			//}

			//sound effect
			m_healSound->SetNextPostion(glm::vec3(0.0f));
			m_healSound->PlayFromQueue();

			if (Dam_health > 25.0f) {
				m_hasGivenLowHealthWarning = false;
			}
		}

		//faster cannon
		if (cannonBuff) {
			playerShootCooldown = 0.45f;
			Get<TTN_MorphAnimator>(cannon).getAnimRefAtIndex(1).SetPlaybackSpeedFactor(1.5555555555f);
			//std::cout << "  CD LOWWW " << std::endl;
			if (!cannonScoreCost && m_score >= cannonCost) {
				m_score = m_score - cannonCost;//score cost of cannon powerup
				cannonScoreCost = true;

				//sound effect
				m_cannonUpgradeSound->SetNextPostion(glm::vec3(0.0f));
				m_cannonUpgradeSound->PlayFromQueue();
			}
		}

		else {
			playerShootCooldown = 0.7f;
			Get<TTN_MorphAnimator>(cannon).getAnimRefAtIndex(1).SetPlaybackSpeedFactor(1.0f);
			cannonScoreCost = false;
		}

		//if the player has lower ability cd from shop
		if (abilityCooldownBuff) {
			FlameThrowerCoolDown = 0.666666666f * 30.0f;
			BirdBombCooldown = 0.666666666f * 15.0f;
			if (!abilityScoreCost && m_score >= abilityCost) {
				m_score = m_score - abilityCost;//score cost of ability power up
				abilityScoreCost = true;

				//sound effect
				m_cooldownReducedSound->SetNextPostion(glm::vec3(0.0f));
				m_cooldownReducedSound->PlayFromQueue();
			}
		}
		else {
			FlameThrowerCoolDown = 30.0f;
			BirdBombCooldown = 15.0f;
			abilityScoreCost = false;
		}

		if (upgradeAbilities) {
			FlameActiveTime = 5.0f; //longer active flamethrower
			//std::cout << " UPPPPPPPPPPPPPPPGRADe" << std::endl;
			//std::cout << FlameActiveTime << std::endl;
			for (auto bird : birds) {
				Get<BirdComponent>(bird).SetDiveSpeed(50.0f / 10.0f);
				//Get<BirdComponent>(bird).SetDiveWeight
			}

			if (!upgradeScoreCost && m_score >= upgradeCost) {
				m_score = m_score - upgradeCost;//score cost of ability power up
				upgradeScoreCost = true;
				//sound effect
				m_cooldownReducedSound->SetNextPostion(glm::vec3(0.0f));
				m_cooldownReducedSound->PlayFromQueue();
			}
		}

		else
		{
			FlameActiveTime = 3.0f; // regular flame timer
			for (auto bird : birds) {
				Get<BirdComponent>(bird).SetDiveSpeed(25.0f / 10.0f);//regular dive speed
			}
			upgradeScoreCost = false;
		}
	}

	if (m_score < 0)
		m_score = 0;
}

//function for bird bomb, decides which ship to target and sends the birds after them
void Game::BirdBomb()
{
	//std::cout << (int)Bombing << std::endl;
	//if the bird bomb is not active and isn't on cooldown
	if (!Bombing && BombTimer <= 0.0f) {
		//set bombing to true
		Bombing = true;

		//get a streched out verison of the player's direction vector
		glm::vec3 bombingVector = playerDir * 10000.0f / 100.0f;

		//get the smallest angle between that vector and a ship
		float currentAngle = 1000.0f;
		entt::entity currentTarget = entt::null;

		//loop through all of the ships
		for (auto entity : boats) {
			//Get the position of the boat
			glm::vec3 boatPos = Get<TTN_Transform>(entity).GetGlobalPos();

			//get the angle between the vectors
			float newAngle = glm::degrees(std::abs(glm::acos(glm::dot(glm::normalize(bombingVector), glm::normalize(boatPos)))));

			//if the new angle is smaller, save it and the boat it's from
			if (newAngle < currentAngle) {
				currentAngle = newAngle;
				currentTarget = entity;
			}
			//if they're equal, check which one is closer
			else if (newAngle == currentAngle) {
				//project the new boat's position onto the player direction
				glm::vec3 ProjNew = (glm::dot(boatPos, bombingVector) / glm::length(bombingVector) * glm::length(bombingVector)) * bombingVector;

				//project the old boat's position onto the player direction
				glm::vec3 oldPos = Get<TTN_Transform>(currentTarget).GetGlobalPos();
				glm::vec3 ProjOld = (glm::dot(oldPos, bombingVector) / glm::length(bombingVector) * glm::length(bombingVector)) * bombingVector;

				//set the target to whichever one has the smaller lenght
				if (glm::length(ProjNew) < glm::length(ProjOld)) currentTarget = entity;
			}
		}

		//after looping through all of the boats, if the remaining angle is greater than a certain threshold assume no boats are being hit
		if (currentAngle > 130.0f) currentTarget = entt::null;

		//if the target is null, turn bombing to false as there were no valid targets for the birds to target
		if (currentTarget == entt::null) {
			Bombing = false;
			//std::cout << "No target found\n";
		}
		else {
			//sound effect
			m_birdBombSound->SetNextPostion(glm::vec3(0.0f));
			m_birdBombSound->PlayFromQueue();

			if (TTN_Random::RandomFloat(0.0f, 1.0f) <= 0.1f) {
				m_DialougeBirdBomb->SetNextPostion(glm::vec3(0.0f));
				m_DialougeBirdBomb->PlayFromQueue();
			}
		}

		//loop through and set the target for all of the birds
			for (auto bird : birds)
				Get<BirdComponent>(bird).SetTarget(currentTarget);
	}
}

//function to make a single bird entity
void Game::MakeABird()
{
	birds.push_back(CreateEntity());

	//create a renderer
	TTN_Renderer birdRenderer = TTN_Renderer(birdMesh, shaderProgramAnimatedTextured, birdMat);
	//attach that renderer to the entity
	AttachCopy(birds[birds.size() - 1], birdRenderer);

	//create an animator
	TTN_MorphAnimator birdAnimator = TTN_MorphAnimator();
	//create an animation for the bird flying
	TTN_MorphAnimation flyingAnim = TTN_MorphAnimation({ 0, 1 }, { 10.0f / 24.0f, 10.0f / 24.0f }, true); //anim 0
	birdAnimator.AddAnim(flyingAnim);
	birdAnimator.SetActiveAnim(0);
	//attach that animator to the entity
	AttachCopy(birds[birds.size() - 1], birdAnimator);

	//create a transform
	float randX = TTN_Random::RandomFloat(-60.0f / 10.0f, 60.0f / 10.0f);
	float randY = TTN_Random::RandomFloat(20.0f / 10.0f, 30.0f / 10.0f);
	float randZ = TTN_Random::RandomFloat(20.0f / 10.0f, 100.0f / 10.0f);
	TTN_Transform birdTrans = TTN_Transform(glm::vec3(randX, randY, randZ), glm::vec3(0.0f), glm::vec3(1.0f / 10.0f));
	//attach that transform to the entity
	AttachCopy(birds[birds.size() - 1], birdTrans);

	//set up a physics body for the bird
	TTN_Physics birdPhysBod = TTN_Physics(birdTrans.GetPos(), glm::vec3(0.0f), birdTrans.GetScale() * 3.5f, birds[birds.size() - 1], TTN_PhysicsBodyType::DYNAMIC);
	//attach the physics body to the entity
	AttachCopy(birds[birds.size() - 1], birdPhysBod);
	Get<TTN_Physics>(birds[birds.size() - 1]).SetHasGravity(false);

	//generate a random base velocity
	randX = TTN_Random::RandomFloat(-1.0f, 1.0f);
	randY = TTN_Random::RandomFloat(-0.5, 1.0f);
	randZ = TTN_Random::RandomFloat(-1.0f, 1.0f);
	if (randX == 0.0f && randZ == 0.0f) {
		randX += 0.01f;
		randZ += 0.01f;
	}
	glm::vec3 velocity = glm::normalize(glm::vec3(randX, randY, randZ));
	//set that velocity
	Get<TTN_Physics>(birds[birds.size() - 1]).SetLinearVelocity(birdBaseSpeed * velocity);
	//and make the bird face in that direction
	Get<TTN_Transform>(birds[birds.size() - 1]).LookAlong(velocity, glm::vec3(0.0f, 1.0f, 0.0f));

	//add a bird component
	BirdComponent bc = BirdComponent(birds[birds.size() - 1], this, birdNeighbourHoodDistance, birdBaseSpeed, birdDiveSpeed, birdAligmentWeight, birdCohensionWeight, birdSeperationWeight,
		birdCorrectionWeight, birdDiveWeight);
	AttachCopy(birds[birds.size() - 1], bc);

	//add a tag
	TTN_Tag birdTag = TTN_Tag("Bird"); //sets bird to ttn_tag
	AttachCopy<TTN_Tag>(birds[birds.size() - 1], birdTag);
}

void Game::BirdUpate(float deltaTime)
{
	if (BombTimer >= 0.0f) BombTimer -= deltaTime;

	//set the bird vector, turn off gravity for the birds and update the bird component
	for (auto bird : birds) {
		Get<BirdComponent>(bird).SetBirdsVector(birds);
		Get<TTN_Physics>(bird).GetRigidBody()->setGravity(btVector3(0.0f, 0.0f, 0.0f));
		Get<BirdComponent>(bird).Update(deltaTime);
	}
}

void Game::ImGui()
{
	if (showCGControls) {
		//ImGui controls for the CG requirements
		ImGui::Begin("CG Controls");

		if (ImGui::CollapsingHeader("Lighting and Shadow Mapping Controls")) {
			TTN_DirectionalLight tempSun = illBuffer->GetSunRef();

			if (ImGui::SliderFloat3("Directional Light Direction", glm::value_ptr(tempSun.m_lightDirection), -50.0f, 0.0f)) {
				SetSun(tempSun);
			}

			if (ImGui::ColorPicker3("Directional Light Color", glm::value_ptr(tempSun.m_lightColor))) {
				SetSun(tempSun);
			}

			if (ImGui::ColorPicker3("Directional Light Ambient Color", glm::value_ptr(tempSun.m_ambientColor))) {
				SetSun(tempSun);
			}

			if (ImGui::SliderFloat("Directional Light Ambient Power", &tempSun.m_ambientPower, 0.0f, 5.0f)) {
				SetSun(tempSun);
			}

			if (ImGui::SliderFloat("Directional Light - Light Ambient Power", &tempSun.m_lightAmbientPower, 0.0f, 5.0f)) {
				SetSun(tempSun);
			}

			if (ImGui::SliderFloat("Directional Light - Light Specular Power", &tempSun.m_lightSpecularPower, 0.0f, 5.0f)) {
				SetSun(tempSun);
			}

			if (ImGui::SliderFloat("Directional Light Min Shadow Bias", &tempSun.m_minShadowBias, 0.0f, 0.005f)) {
				SetSun(tempSun);
			}

			if (ImGui::SliderFloat("Directional Light Max Shadow Bias", &tempSun.m_maxShadowBias, 0.0f, 0.005f)) {
				SetSun(tempSun);
			}

			if (ImGui::SliderInt("Directional Light PCF Filter Passes", &tempSun.m_pcfFilterSamples, 1, 20)) {
				SetSun(tempSun);
			}

			float emissiveStr = illBuffer->GetEmissiveStrenght();
			if (ImGui::SliderFloat("Emissive Strenght", &emissiveStr, 0.0f, 10.0f)) {
				illBuffer->SetEmissiveStrenght(emissiveStr);
			}

			bool AmbientOn = illBuffer->GetUseAmbient();
			if (ImGui::Checkbox("Ambient Lighting On", &AmbientOn)) {
				illBuffer->SetUseAmbient(AmbientOn);
			}

			bool SpecularOn = illBuffer->GetUseSpecular();
			if (ImGui::Checkbox("Specular Lighting On", &SpecularOn)) {
				illBuffer->SetUseSpecular(SpecularOn);
			}

			bool ShadowingOn = illBuffer->GetuseShadowMapping();
			if (ImGui::Checkbox("Shadow Mapping On", &ShadowingOn)) {
				illBuffer->SetUseShadowMapping(ShadowingOn);
			}

			bool EmissiveOn = lightHouseMat->GetUseEmissive();
			if (ImGui::Checkbox("Emissive Lighting On", &EmissiveOn)) {
				lightHouseMat->SetUseEmissive(EmissiveOn);
			}

			bool RimOn = boat1Mat->GetHasRimLighting();
			if (ImGui::Checkbox("Rim Lighting On", &RimOn)) {
				boat1Mat->SetHasRimLighting(RimOn);
				boat2Mat->SetHasRimLighting(RimOn);
				boat3Mat->SetHasRimLighting(RimOn);
			}
		}

		if (ImGui::CollapsingHeader("Texture Controls")) {
			bool  texturesOn = m_mats[0]->GetUseAlbedo();
			if (ImGui::Checkbox("Use Textures", &texturesOn)) {
				for (int i = 0; i < m_mats.size(); i++) {
					m_mats[i]->SetUseAlbedo(texturesOn);
				}
			}
		}

		if (ImGui::CollapsingHeader("Color Correction Controls")) {
			bool mainlutIsActive = m_colorCorrectEffect->GetShouldApply();
			if (ImGui::Checkbox("Main Colour Correction On", &mainlutIsActive)) {
				m_colorCorrectEffect->SetShouldApply(mainlutIsActive);
			}

			ImGui::Text("Others Color correction effects can be toggled from the scenes they affect, and the options menu");
		}

		if (ImGui::CollapsingHeader("Bloom Controls")) {
			bool bloomShouldApply = m_bloomEffect->GetShouldApply();
			if (ImGui::Checkbox("Bloom On", &bloomShouldApply)) {
				m_bloomEffect->SetShouldApply(bloomShouldApply);
			}

			if (ImGui::SliderFloat("Threshold", &m_bloomThreshold, 0.0f, 1.0f)) {
				m_bloomEffect->SetThreshold(m_bloomThreshold);
			}

			if (ImGui::SliderInt("Number of blur passes", &m_numOfBloomPasses, 1, 20)) {
				m_bloomEffect->SetNumOfPasses(m_numOfBloomPasses);
			}

			int tempInt = m_bloomBufferDivisor;
			if (ImGui::SliderInt("Downscale divisor", &tempInt, 1, 20)) {
				m_bloomBufferDivisor = tempInt;
				m_bloomEffect->SetBlurDownScale(m_bloomBufferDivisor);
			}

			if (ImGui::SliderFloat("radius", &m_bloomRadius, 0.1f, 20.0f)) {
				m_bloomEffect->SetRadius(m_bloomRadius);
			}

			if (ImGui::SliderFloat("Strength (radial)", &m_bloomStrength, 0.1f, 20.0f)) {
				m_bloomEffect->SetStrength(m_bloomStrength);
			}

			if (ImGui::Button("Make Gaussian Blur")) {
				m_bloomEffect->SetBlurMode(TTN_BloomBlurModes::GAUSSIAN);
			}

			if (ImGui::Button("Make Box Blur")) {
				m_bloomEffect->SetBlurMode(TTN_BloomBlurModes::BOX);
			}

			if (ImGui::Button("Make Radial Blur")) {
				m_bloomEffect->SetBlurMode(TTN_BloomBlurModes::RADIAL);
			}
		}

		if (ImGui::CollapsingHeader("Toon Shading Controls")) {
			bool diffuseRampIsOn = illBuffer->GetUseDiffuseRamp();
			if (ImGui::Checkbox("Diffuse Ramp On", &diffuseRampIsOn)) {
				illBuffer->SetUseDiffuseRamp(diffuseRampIsOn);
			}

			bool specularRampIsOn = illBuffer->GetUseSpecularRamp();
			if (ImGui::Checkbox("Specular Ramp On", &specularRampIsOn)) {
				illBuffer->SetUseSpecularRamp(specularRampIsOn);
			}
		}

		ImGui::End();
	}

	ImGui::Begin("CG Assingment 4 Controls");
	
	ImGui::Text("Point Lights");

	std::vector<glm::vec3> lightPositions = std::vector<glm::vec3>(m_Lights.size());
	std::vector<glm::vec3> lightColors = std::vector<glm::vec3>(m_Lights.size());
	std::vector<float> lightAmbStr = std::vector<float>(m_Lights.size());
	std::vector<float> lightSpecStr = std::vector<float>(m_Lights.size());
	std::vector<float> lightConstAtten = std::vector<float>(m_Lights.size());
	std::vector<float> lightLineAtten = std::vector<float>(m_Lights.size());
	std::vector<float> lightQuadAtten = std::vector<float>(m_Lights.size());
	std::vector<bool> lightVolumeShouldRender = std::vector<bool>(m_Lights.size());
	std::vector<float> lightVolumeTransparency = std::vector<float>(m_Lights.size());

	for (int i = 0; i < m_Lights.size(); i++) {
		std::string lightName = "Point Light " + std::to_string(i);
		if (ImGui::CollapsingHeader(lightName.c_str())) {
			lightPositions[i] = Get<TTN_Light>(m_Lights[i]).GetPosition();
			std::string posName = "Position " + std::to_string(i);
			if (ImGui::SliderFloat3(posName.c_str(), glm::value_ptr(lightPositions[i]), -15.0f, 15.0f)) {
				Get<TTN_Light>(m_Lights[i]).SetPosition(lightPositions[i]);
			}

			lightColors[i] = Get<TTN_Light>(m_Lights[i]).GetColor();
			std::string colorName = "Color " + std::to_string(i);
			if (ImGui::ColorEdit3(colorName.c_str(), glm::value_ptr(lightColors[i]))) {
				Get<TTN_Light>(m_Lights[i]).SetColor(lightColors[i]);
			}

			lightAmbStr[i] = Get<TTN_Light>(m_Lights[i]).GetAmbientStrength();
			std::string ambStrName = "Ambient Strength " + std::to_string(i);
			if (ImGui::SliderFloat(ambStrName.c_str(), &lightAmbStr[i], 0.0f, 5.0f)) {
				Get<TTN_Light>(m_Lights[i]).SetAmbientStrength(lightAmbStr[i]);
			}

			lightSpecStr[i] = Get<TTN_Light>(m_Lights[i]).GetSpecularStrength();
			std::string specStrName = "Specular Strength " + std::to_string(i);
			if (ImGui::SliderFloat(specStrName.c_str(), &lightSpecStr[i], 0.0f, 5.0f)) {
				Get<TTN_Light>(m_Lights[i]).SetSpecularStrength(lightSpecStr[i]);
			}

			lightConstAtten[i] = Get<TTN_Light>(m_Lights[i]).GetConstantAttenuation();
			std::string constAttenName = "Attentuation Constant " + std::to_string(i);
			if (ImGui::SliderFloat(constAttenName.c_str(), &lightConstAtten[i], 0.0f, 10.0f)) {
				Get<TTN_Light>(m_Lights[i]).SetConstantAttenuation(lightConstAtten[i]);
			}

			lightLineAtten[i] = Get<TTN_Light>(m_Lights[i]).GetLinearAttenuation();
			std::string lineAttenName = "Attentuation Linear " + std::to_string(i);
			if (ImGui::SliderFloat(lineAttenName.c_str(), &lightLineAtten[i], 0.0f, 10.0f)) {
				Get<TTN_Light>(m_Lights[i]).SetLinearAttenuation(lightLineAtten[i]);
			}

			lightQuadAtten[i] = Get<TTN_Light>(m_Lights[i]).GetQuadraticAttenuation();
			std::string quadAttenName = "Attentuation Quadratic " + std::to_string(i);
			if (ImGui::SliderFloat(quadAttenName.c_str(), &lightQuadAtten[i], 0.0f, 10.0f)) {
				Get<TTN_Light>(m_Lights[i]).SetQuadraticAttenuation(lightQuadAtten[i]);
			}

			lightVolumeShouldRender[i] = Get<TTN_Light>(m_Lights[i]).GetVolumeShouldRender();
			bool temp = lightVolumeShouldRender[i];
			std::string volumeShouldrenderName = "Volume " + std::to_string(i) + " should render";
			if (ImGui::Checkbox(volumeShouldrenderName.c_str(), &temp)) {
				lightVolumeShouldRender[i] = temp;
				Get<TTN_Light>(m_Lights[i]).SetVolumeShouldRender(lightVolumeShouldRender[i]);
			}

			lightVolumeTransparency[i] = Get<TTN_Light>(m_Lights[i]).GetVolumeTransparency();
			std::string lightVolumeAlphaName = "Volume " + std::to_string(i) + "'s transparency";
			if (ImGui::SliderFloat(lightVolumeAlphaName.c_str(), &lightVolumeTransparency[i], 0.0f, 1.0f)) {
				Get<TTN_Light>(m_Lights[i]).SetVolumeTransparency(lightVolumeTransparency[i]);
			}
		}
	}

	if (ImGui::Button("Make new point light")) {
		entt::entity newLight = CreateEntity();
		m_Lights.push_back(newLight);

		TTN_Light lightLight = TTN_Light(glm::vec3(1.0f), 0.5f, 0.5f, 0.0f, 2.0f, 8.0f);
		lightLight.SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
		lightLight.SetVolumeShouldRender(true);

		AttachCopy(newLight, lightLight);
	}

	ImGui::Text("Tone Mapping");
	
	float exposure = illBuffer->GetExposure();
	if (ImGui::SliderFloat("Exposure", &exposure, 0.01f, 20.0f)) {
		illBuffer->SetExposure(exposure);
	}


	ImGui::End();
}