#pragma once

#include "game_object.h"
#include "components/material.h"
#include "components/transform.h"
#include "components/light.h"
#include "scene.h"
#include "GLFW/glfw3.h"
#include <unordered_map>

class MainEngine
{
private:
    std::string name;
    static std::vector<std::shared_ptr<Scene>> scenes;
    static std::unordered_map<unsigned int, bool> keyPresses;
    static size_t currSceneIdx;

    bool changedScene = true;

    static void TestInit2() {
        const auto scene = std::make_shared<Scene>();

        // Camera
        const auto camera = std::make_shared<Camera>();

        camera->SetPosition(glm::vec3(10, 12, -19));
        camera->SetRotation(glm::vec3(20, -25, 0));

        scene->GetCameras().emplace_back(camera);

        // Make cubes
        constexpr size_t n = 1;
        constexpr glm::vec3 pos[n] = {
            {0.0f, 10.0f, 0.0f},
        };
        for (int i = 0; i < n; i++)
        {
            scene->AddModel();
            const auto& obj = scene->GetModels().at(i);

            std::dynamic_pointer_cast<Transform>(obj->components[TRANSFORM])->position = pos[i];
        }

        // Make floor

        scene->AddModel("square.obj", "floor");
        const auto &obj = scene->GetModels().at(1);

        std::dynamic_pointer_cast<Transform>(obj->components[TRANSFORM])->position = {0.f, 0.f, 0.f};
        std::dynamic_pointer_cast<Transform>(obj->components[TRANSFORM])->rotation = {-90.f, 0.f, 0.f};
        std::dynamic_pointer_cast<Transform>(obj->components[TRANSFORM])->scale = {100.f, 100.f, 100.f};

        // Make Light
        const auto light1 = std::make_shared<GameObject>();
        const auto lightTransform1 = std::make_shared<Transform>(
            glm::vec3(0.0f, 20.0f, -30.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f));

        const auto lightComp1 = std::make_shared<PointLight>(
            glm::vec3(0.5f, 0.5f, 0.5f),
                1.0f);

        light1->components[TRANSFORM] = lightTransform1;
        light1->components[LIGHT] = lightComp1;

        scene->GetLights().emplace_back(light1);

        scenes.push_back(scene);
    }

public:
    static std::vector<std::shared_ptr<Scene>>& GetScenes() { return scenes; }
    static std::shared_ptr<Scene>& GetCurrScene() { return scenes[currSceneIdx]; }
    static std::unordered_map<unsigned int, bool>& GetKeyPresses() { return keyPresses; }

    std::chrono::duration<float> timeDelta;

    float cameraSense = 0.1f;
    float movementSense = 1.f;
    glm::vec3 gravity = {0, -9.81f, 0};

    bool mouseDragging = false;
    glm::vec2 lastMousePos = glm::vec2(0.0f);

    bool simulationRunning = false;

    MainEngine()
    {
        TestInit2();
    }


    bool HasChangedScene() const { return changedScene; }
    void ChangedSceneAcknowledged() { changedScene = false; }
    void ChangeScene(const int idx) { currSceneIdx = idx; changedScene = true; }
    void CycleScene() { currSceneIdx = (currSceneIdx+1) % scenes.size(); changedScene = true; }

    std::string& getName() {
        return name;
    }

    static void SetScenes(const std::vector<std::shared_ptr<Scene> > &_scenes) {
        scenes = _scenes;
    }

    void setName(const std::string &_name) {
        name = _name;
    }

    void CharacterCallback(GLFWwindow *window, unsigned int key, int scancode, int action, int mods);

    void MouseCallback(GLFWwindow *window, int button, int action, int mods);

    void runSimulation();
};
