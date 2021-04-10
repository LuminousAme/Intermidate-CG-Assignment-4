//Dam Defense, by Atlas X Games
//PauseMenu.h, the header file for the class that represents the pause menu
#pragma once

//include required features from titan
#include "Titan/Application.h"
#include "Titan/Utilities/Interpolation.h"

using namespace Titan;

class PauseMenu : public TTN_Scene {
public:
	//default constructor
	PauseMenu();

	//default destrcutor 
	~PauseMenu() = default;

	//sets up the scene
	void InitScene();

	//update the scene
	void Update(float deltaTime);

	//mouse input
	void MouseButtonDownChecks();

	bool GetShouldQuit() { return shouldQuit; }
	bool GetShouldResume() { return shouldResume; }
	bool GetShouldMenu() { return shouldMenu; }
	bool GetShouldOptions() { return shouldOptions; }
	bool GetRendering() { return isRendering; }

	void SetShouldResume(bool resume) { shouldResume = resume; }
	void SetShouldMenu(bool menu) { shouldMenu = menu; }
	void SetShouldOptions(bool option) { shouldOptions = option; }
	void SetRendering(bool rend) { isRendering = rend; }

private:
	//entities
	entt::entity cam;
	entt::entity background;
	//resume
	entt::entity resumeButton;
	entt::entity resumeText;
	//options
	entt::entity optionsButton;
	entt::entity optionsText;
	//quit
	entt::entity quitButton;
	entt::entity quitText;
	//main menu
	entt::entity menuButton;
	entt::entity menuText;

	//assets
	TTN_Texture2D::st2dptr textureBackground;
	TTN_Texture2D::st2dptr texturePaused;
	TTN_Texture2D::st2dptr textureButton1;
	TTN_Texture2D::st2dptr textureButton2;
	TTN_Texture2D::st2dptr textureResume;
	TTN_Texture2D::st2dptr textureOptions;
	TTN_Texture2D::st2dptr textureQuit;
	TTN_Texture2D::st2dptr textureMainMenu;
 
	//should quit the game
	bool shouldQuit;
	bool shouldResume;
	bool shouldMenu;
	bool shouldOptions;
	bool isRendering;
};