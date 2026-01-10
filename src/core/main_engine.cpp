#include "core/main_engine.h"

std::vector<std::shared_ptr<Scene>> MainEngine::scenes;
size_t MainEngine::currSceneIdx = 0;
std::unordered_map<unsigned int, bool> MainEngine::keyPresses;

void MainEngine::CharacterCallback(GLFWwindow* window, unsigned int key, int scancode, int action, int mods)
{
    std::shared_ptr<Camera> camera = GetCurrScene()->GetCurrCamera();
    if(GetCurrScene()->GetCameras().at(0) == camera) {
        if(key == GLFW_KEY_W) {
            camera->SetPosition(camera->GetPosition() + camera->GetForward() * movementSense);
        }
        if(key == GLFW_KEY_S) {
            camera->SetPosition(camera->GetPosition() - camera->GetForward() * movementSense);
        }
        if(key == GLFW_KEY_D) {
            camera->SetPosition(camera->GetPosition() + camera->GetRight() * movementSense);
        }
        if(key == GLFW_KEY_A) {
            camera->SetPosition(camera->GetPosition() - camera->GetRight() * movementSense);
        }
    }

    if(keyPresses.find(key) == keyPresses.end()) {
        keyPresses.insert(std::pair<int, bool>(key, false));
    }

    if(action == GLFW_PRESS) {
        keyPresses.insert_or_assign(key, true);
    }
    else if(action == GLFW_RELEASE) {
        keyPresses.insert_or_assign(key, false);
    }
}

void MainEngine::MouseCallback(GLFWwindow* window, int button, int action, int mods) {
    std::shared_ptr<Camera> camera = GetCurrScene()->GetCurrCamera();
    if(GetCurrScene()->GetCameras().at(0) == camera) {
        if(button == GLFW_MOUSE_BUTTON_RIGHT) {
            if(action == GLFW_PRESS) {
                mouseDragging = true;
                double xPos, yPos;
                glfwGetCursorPos(window, &xPos, &yPos);
                lastMousePos = glm::vec2(xPos, yPos);

                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            }
            else if(action == GLFW_RELEASE) {
                mouseDragging = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }
}

void MainEngine::runSimulation() {
    auto models = GetCurrScene()->GetModels();
    for(auto curr : models) {
        curr->GetTransform()->position += gravity * glm::vec3(0, -1, 0) * 0.1f;
    }
}
