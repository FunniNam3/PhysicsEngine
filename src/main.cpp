
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>

#include "gui/gui_engine.h"
#define TINYOBJLOADER_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <memory>
#include <filesystem>

#include "core/main_engine.h"
#include "render/render_engine.h"

#include "core/scene.h"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

GLFWwindow *window;

std::unique_ptr<GuiEngine> guiEngine;
std::unique_ptr<RenderEngine> renderEngine;

MainEngine mainEngine;

// Time delta calculations for physics
std::chrono::duration<float> timeDelta = std::chrono::duration<float>::zero();
auto start = std::chrono::steady_clock::now();
auto end   = std::chrono::steady_clock::now();

// Keyboard character callback function
void CharacterCallback(GLFWwindow* lWindow, int key, int scancode, int action,int mode)
{
	renderEngine->CharacterCallback(lWindow, key);
	mainEngine.CharacterCallback(lWindow, key, scancode, action, mode);
}

void MouseCallback(GLFWwindow* lWindow, int button, int action, int mods) {
	mainEngine.MouseCallback(lWindow, button, action, mods);
}

void FrameBufferSizeCallback(GLFWwindow* lWindow, int width, int height)
{
	renderEngine->FrameBufferSizeCallback(lWindow, width, height);
}


static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char *argv[])
{

	// GLFWwindow* window is shared between gui and render,
	// so let's declare it in main.
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
	    return -1;
    }
	glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_FALSE);
	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Physics Engine", nullptr, nullptr);
	if (!window) return -1;
	glfwMakeContextCurrent(window);

	// Initialize GLAD
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glfwSetKeyCallback(window, CharacterCallback);
	glfwSetMouseButtonCallback(window, MouseCallback);
	glfwSetFramebufferSizeCallback(window, FrameBufferSizeCallback);

	// Creates the assets folder if it doesn't already exist
	if (!std::filesystem::exists(std::filesystem::current_path() / "Assets")) {
		std::filesystem::create_directory(std::filesystem::current_path() / "Assets");
	}

	mainEngine = MainEngine();
	guiEngine = std::make_unique<GuiEngine>();
	renderEngine = std::make_unique<RenderEngine>(window, &mainEngine);

	// Generate Frame buffer for ShadowMapping
	GLuint depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);

	const GLuint shadowWidth = 1024, shadowHeight = 1024;

	GLuint depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowWidth, shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
	             nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Generate Framebuffer for actual rendering
	GLuint framebuffer;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	// Create a texture to attach to the framebuffer
	GLuint textureColorbuffer;
	glGenTextures(1, &textureColorbuffer);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

	// Create a renderbuffer for depth and stencil attachment (optional)
	GLuint rbo;
	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, WINDOW_WIDTH, WINDOW_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

	// Check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "ERROR: Framebuffer is not complete!" << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	int width, height;
	glfwGetWindowSize(window, &width, &height);

	guiEngine->init(window, &mainEngine);
	while ( glfwWindowShouldClose(window) == 0 )
	{

		glfwPollEvents();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        start = end;

		guiEngine->run();
		if(guiEngine->showView) {
			renderEngine->MapShadows(depthMap, shadowWidth, shadowHeight);
			renderEngine->Display(guiEngine->SendViewportInfo(), depthMap);
		}

        end = std::chrono::steady_clock::now();
        timeDelta = end - start;
        // std::cout << timeDelta.count() << std::endl;
		glfwSwapBuffers(window);

		if(mainEngine.mouseDragging) {
			auto currScene = MainEngine::GetCurrScene();
			if (!currScene) {
				std::cerr << "No current scene!" << std::endl;
				return -1;
			}
			auto camera = currScene->GetCurrCamera();
			if (!camera) {
				std::cerr << "No camera!" << std::endl;
				return -1;
			}


			double xPos, yPos;
			glfwGetCursorPos(window, &xPos, &yPos);

			double xOffset = xPos - mainEngine.lastMousePos.x;
			double yOffset = yPos - mainEngine.lastMousePos.y;

			glm::vec3 rotation = camera->GetEularRotation();

			camera->SetRotation(rotation + glm::vec3(yOffset, -xOffset, 0.0f) * mainEngine.cameraSense);

			glfwSetCursorPos(window, mainEngine.lastMousePos.x, mainEngine.lastMousePos.y);
		}
	}
	guiEngine->cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

	return 0;
}
