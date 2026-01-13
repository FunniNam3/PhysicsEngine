#include "imgui.h"
#include "gui/AddObjectWindow.h"
#include "core/camera.h"
#include <glm/common.hpp>

void AddObjectWindow::showAddObjectWindow(bool &ShowAddObject) {
    // Set add object;
    ImGui::Begin("Add Object Window", nullptr);

    if (ImGui::Button("Game Object")) {
        objectType = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Light")) {
        objectType = 2;
    }
    ImGui::SameLine();
    if (ImGui::Button("Camera")) {
        objectType = 3;
    }

    switch(objectType){
        case 1:
            ImGui::Text("Game object selected");
            ImGui::Text("Location");
            ImGui::SameLine();
            char locationBuffer[128];
            strncpy(locationBuffer, location.c_str(), sizeof(locationBuffer));
            locationBuffer[127] = '\0';
            if (ImGui::InputText("##Location", locationBuffer, sizeof(locationBuffer))) {
                location = locationBuffer;
            }

            ImGui::Text("Name");
            ImGui::SameLine();
            char nameBuffer[128];
            strncpy(nameBuffer, name.c_str(), sizeof(nameBuffer));
            nameBuffer[127] = '\0';
            if(ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
                name = nameBuffer;
            }
            break;

        case 2:
            ImGui::Text("Light object selected");
            break;
        case 3:
            ImGui::Text("Camera object selected");
            break;
        default:
            ImGui::Text("No object type selected");
    }



    if(ImGui::Button("Add Object")) {
        switch (objectType) {
            case 1:
                if (!location.empty() && (location.find(".obj") != std::string::npos)) {
                    MainEngine::GetCurrScene()->AddModel(location, name);
                }
                break;
            case 2: {
                const auto light1 = std::make_shared<GameObject>();
                const auto lightTransform1 = std::make_shared<Transform>(
                    glm::vec3(0.0f, 0.0f, 3.0f),
                    glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 0.0f));

                const auto lightComp1 = std::make_shared<PointLight>(
                    glm::vec3(0.5f, 0.5f, 0.5f),
                        1.0f);

                light1->components[TRANSFORM] = lightTransform1;
                light1->components[LIGHT] = lightComp1;

                MainEngine::GetCurrScene()->GetLights().emplace_back(light1);
                break;
            }
            case 3: {
                auto camera = std::make_shared<Camera>();
                MainEngine::GetCurrScene()->GetCameras().emplace_back(camera);
                break;
            }

            default:
                ImGui::Text("Invalid object type selected");
                break;
        }
        ShowAddObject = false;
    }
    ImGui::End();
};
