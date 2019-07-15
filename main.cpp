#include "stdafx.h"
#include "classes/system/Shader.h"
#include "classes/system/Scene.h"
#include "classes/system/FPSController.h"
#include "classes/level/Dungeon.h"
#include "classes/buffers/QuadBuffer.h"
#include "classes/objects/Hero.h"
#include "classes/objects/Bullet.h"

bool Pause;
bool keys[1024] = {0};
glm::vec2 MouseSceneCoord;
int WindowWidth = 800, WindowHeight = 600;
bool EnableVsync = 1;
GLFWwindow* window;
stFPSController FPSController;
tVoidFunc DrawFunc = NULL;

MShader Shader;
MScene Scene;
MDungeon Dungeon;

deque<glm::vec4> NewBullets;
vector<MBullet*> Bullets;
vector<MBullet*>::iterator itv;
MQuadBuffer BulletBuffer;

MHero* Hero;
MQuadBuffer QuadBuffer;
glm::vec2 QuadVelocity = glm::vec2(1.0, 1.0);
glm::vec2 QuadSize = glm::vec2(10, 10);
glm::vec3 QuadColor = glm::vec3(0, 1, 0);

void CreatePhysicQuad();
void MoveKeysPressed();
bool CheckOpenglSupport();
bool InitApp();
void ProcessBullets();
void DrawDungeon();
void DrawLoad();
void RenderStep();
void ClearApp();

void CreateHero() {
	Hero->Close();
	QuadBuffer.Clear();
	NRectangle2* r = Dungeon.GetLevelExitPrev();
	Hero->Create(glm::vec2(r->Position.x * 20 + 20, r->Position.y * 20 + 20 ), QuadSize, QuadColor, Dungeon.GetScale(), 
		Dungeon.GetWorld(), b2_dynamicBody, OT_HERO, OT_WALL | OT_EXIT | OT_CHEST | OT_ENEMY, false);
	QuadBuffer.AddQuad(Hero->GetColorQuad());
	QuadBuffer.Relocate();
}

static void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

static void mousepos_callback(GLFWwindow* window, double x, double y) {
	MouseSceneCoord = Scene.WindowPosToWorldPos(x, y);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if(!DrawFunc) return;
	if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		glm::vec2 Direction;
		Direction = glm::normalize(MouseSceneCoord - Hero->GetColorQuad()->GetCenter());
		NewBullets.push_back(glm::vec4(Hero->GetColorQuad()->GetCenter().x, Hero->GetColorQuad()->GetCenter().y, Direction.x, Direction.y));
	}
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return;
	}
	
	if(key == 'P' && action == GLFW_PRESS) {
		Pause = !Pause;
	}
	
	if(key == 'E' && action == GLFW_PRESS) {
		if(Hero->GetNearLevelExitNext())
		{
			Dungeon.Generate();
			CreateHero();
			return;
		}
		Hero->OpenChest();
	}
	
	if(action == GLFW_PRESS)
    	keys[key] = true;
    else if (action == GLFW_RELEASE)
    	keys[key] = false;
}

void MoveKeysPressed() {
	b2Vec2 CurrentVelocity = b2Vec2(0.0f, 0.0f);
	if(keys[GLFW_KEY_LEFT] || keys[GLFW_KEY_RIGHT] || keys[GLFW_KEY_UP] || keys[GLFW_KEY_DOWN]) {
		if(keys[GLFW_KEY_LEFT]) {
			CurrentVelocity.x = -QuadVelocity.x;
		}
		if(keys[GLFW_KEY_RIGHT]) {
			CurrentVelocity.x = QuadVelocity.x;
		}
		if(keys[GLFW_KEY_UP]) {
			CurrentVelocity.y = QuadVelocity.y;
		}
		if(keys[GLFW_KEY_DOWN]) {
			CurrentVelocity.y = -QuadVelocity.y;
		}
	}
	Hero->SetVelocity(CurrentVelocity); 
}

bool CheckOpenglSupport() {
	//get opengl data (here was CRASH on suspisious notebook)
	string OpenGLVersion = (char*)glGetString(GL_VERSION);
	string OpenGLVendor = (char*)glGetString(GL_VENDOR);
	string OpenGLRenderer = (char*)glGetString(GL_RENDERER);
	string ShadersVersion = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	LogFile<<"Window: OpenGL version: "<<OpenGLVersion<<endl;
	LogFile<<"Window: OpenGL vendor: "<<OpenGLVendor<<endl;
	LogFile<<"Window: OpenGL renderer: "<<OpenGLRenderer<<endl;
	LogFile<<"Window: Shaders version: "<<ShadersVersion<<endl;
	
	float VersionOGL, VersionSHD;
	sscanf(OpenGLVersion.c_str(), "%f", &VersionOGL);
	if(VersionOGL < 3.0f) {
		LogFile<<"Window: Old version of OpenGL. Sorry"<<endl;
		return false;
	}
	sscanf(ShadersVersion.c_str(), "%f", &VersionSHD);
	if(VersionSHD < 3.3f) {
		LogFile<<"Window: Old version of shaders. Sorry"<<endl;
		return false;
	}
}

bool InitApp() {
	LogFile<<"Starting application"<<endl;    
    glfwSetErrorCallback(error_callback);
    
    if(!glfwInit()) return false;
    window = glfwCreateWindow(WindowWidth, WindowHeight, "TestApp", NULL, NULL);
    if(!window) {
        glfwTerminate();
        return false;
    }
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mousepos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwMakeContextCurrent(window);
    if(glfwExtensionSupported("WGL_EXT_swap_control")) {
    	LogFile<<"Window: V-Sync supported. V-Sync: "<<EnableVsync<<endl;
		glfwSwapInterval(EnableVsync);//0 - disable, 1 - enable
	}
	else LogFile<<"Window: V-Sync not supported"<<endl;
    LogFile<<"Window created: width: "<<WindowWidth<<" height: "<<WindowHeight<<endl;

	//glew
	GLenum Error = glewInit();
	if(GLEW_OK != Error) {
		LogFile<<"Window: GLEW Loader error: "<<glewGetErrorString(Error)<<endl;
		return false;
	}
	LogFile<<"GLEW initialized"<<endl;
	
	if(!CheckOpenglSupport()) return false;

	//shaders
	if(!Shader.CreateShaderProgram("shaders/main.vertexshader.glsl", "shaders/main.fragmentshader.glsl")) return false;
	if(!Shader.PrepareShaderValues()) return false;
	LogFile<<"Shaders loaded"<<endl;

	//scene
	if(!Scene.Initialize(&WindowWidth, &WindowHeight)) return false;
	LogFile<<"Scene initialized"<<endl;

	//randomize
    srand(time(NULL));
    LogFile<<"Randomized"<<endl;
    
    //create dungeon
    if(!Dungeon.Initialize(30, 30, 20.0f, 20.0f)) return false;
    Dungeon.Generate();
	
	//create hero and bullet
	Hero = new MHero;
	QuadBuffer = MQuadBuffer(GL_STREAM_DRAW);
	BulletBuffer = MQuadBuffer(GL_STREAM_DRAW);
	CreateHero();
	Dungeon.SetHero(Hero);
	
	//draw func
	DrawFunc = &DrawDungeon;
	Pause = false;
    
    return true;
}

void ProcessBullets() {
	while(!NewBullets.empty()) {
		Bullets.push_back(new MBullet);
		Bullets.back()->Create(glm::vec2(NewBullets.front()[0] - 4, NewBullets.front()[1] - 4), glm::vec2(8, 8), glm::vec3(1,0,1), Dungeon.GetScale(), 
			Dungeon.GetWorld(), b2_dynamicBody, OT_BULLET, OT_WALL | OT_ENBODY, true);
		BulletBuffer.AddQuad(Bullets.back()->GetColorQuad());
		Bullets.back()->SetVelocity(b2Vec2(NewBullets.front()[2], NewBullets.front()[3]));
		Bullets.back()->SetDamage(5);
		NewBullets.pop_front();
	}
	itv = Bullets.begin();
	while(itv != Bullets.end()) {
		if((*itv)->GetNeedRemove()) {
			BulletBuffer.RemoveQuad((*itv)->GetColorQuad());
			(*itv)->Close();
			delete *itv;
			*itv = NULL;
			itv = Bullets.erase(itv);
		}
		else {
			(*itv)->Update();
			++ itv;
		}
	}
}

void DrawDungeon() {
	if(!Pause) {
		MoveKeysPressed();
		Dungeon.WorldStep();
		Dungeon.ProcessEnemies();
		Hero->Update();
		if(!FPSController.SummaryFrameTime) Hero->ProcessContactEnemies();//DPS
		ProcessBullets();
		QuadBuffer.UpdateAll();
		BulletBuffer.UpdateAll();
	}
	
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(Shader.ProgramId);
	glUniformMatrix4fv(Shader.MVPId, 1, GL_FALSE, Scene.GetDynamicMVP());
	
	glEnableVertexAttribArray(SHR_LAYOUT_VERTEX);
	glEnableVertexAttribArray(SHR_LAYOUT_COLOR);
	
	Dungeon.Draw();
	QuadBuffer.DrawAll();
	BulletBuffer.DrawAll();
	
	glDisableVertexAttribArray(SHR_LAYOUT_VERTEX);
	glDisableVertexAttribArray(SHR_LAYOUT_COLOR);
}

void DrawLoad() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderStep() {
	(*DrawFunc)();
}

void ClearApp() {
	LogFile<<"Closing application. Free memory"<<endl;
	for(itv = Bullets.begin(); itv != Bullets.end(); ++itv) {
		delete (*itv);
	}
	Bullets.clear();
	if(Hero) {
		Hero->Close();
		delete Hero;
	}
	Dungeon.Close();
	BulletBuffer.Close();
	QuadBuffer.Close();
	Shader.Close();
}

int main(int argc, char** argv) {
	if(!InitApp()) {
		ClearApp();
		glfwTerminate();
		return 0;
	}
	FPSController.Initialize(glfwGetTime());
	while(!glfwWindowShouldClose(window)) {
		FPSController.FrameStep(glfwGetTime());
    	FPSController.FrameCheck();
		RenderStep();
        glfwSwapBuffers(window);
        glfwPollEvents();
	}
	ClearApp();
    glfwTerminate();
	
	return 0;
}
