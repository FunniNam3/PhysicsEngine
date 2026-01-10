#pragma once

#include <iostream>
#include <string>
#include <array>
#include <unordered_set>
#include <memory>
#include <vector>

#include "components/component.h"
#include "components/transform.h"
#include "components/material.h"
#include "components/model.h"
#include "components/light.h"

class GameObject {
public:
    std::string name;
    // Contains all the compenents associated with the game object
    // If the game object does not have that component, the element where
    // that component should be is null.
    std::array<std::shared_ptr<Component>, Component::GetEnumSize()> components{};
    std::unordered_set<std::string> tags;
    std::string model_path;

    GameObject(const std::string model_path = "../resources/models/sphere.obj") : model_path(model_path), id(generateUniqueId()) {}
    const int id;

    Transform* GetTransform() const {
        return dynamic_cast<Transform*>(components[TRANSFORM].get());
    }

    // returns a reference to the component of the given type
    Component* GetComponent(COMPONENT_TYPE type) const {
        return components[type].get();
    }

    // returns a reference to the component by the index
    /*Component* GetComponent(size_t index) {
        return components[index].get();
    }*/

    // returns the amount of components
    int GetComponentCount() const {
        int count = 0;

        for(const std::shared_ptr<Component>& component : components) {
            if(component) {
                count++;
            }
        }
        return count;
    }

    // returns true if tag is present in the game object
    bool CompareTag(const std::string& tag) const {
        return tags.find(tag) != tags.end();
    }

    // adds a tag to the game object
    void AddTag(const std::string& tag) {
        tags.insert(tag);
    }

    // removes a tag from the game object
    void RemoveTag(const std::string& tag) {
        tags.erase(tag);
    }

    void AddComponent(const COMPONENT_TYPE component) {
        switch(component) {
            // case TRANSFORM:
            //     if(!components[TRANSFORM]) {
            //         components[TRANSFORM] = std::make_shared<Transform>();
            //     }
            //     break;
            case MATERIAL:
                if(!components[MATERIAL]) {
                    components[MATERIAL] = std::make_shared<Material>();
                }
                break;
            case MODEL:
                if(!components[MODEL]) {
                    components[MODEL] = std::make_shared<Model>();
                }
                break;
            case LIGHT:
                if(!components[LIGHT]) {
                    components[LIGHT] = std::make_shared<PointLight>();
                }
                break;
            default: ;
        }
    }

    // Don't call on transform
    void RemoveComponent(COMPONENT_TYPE type) {
        if(components[type] && type != TRANSFORM) {
            components[type] = nullptr;
        }
    }

    void CopyComponent(const std::shared_ptr<Component> component) {
        if(component) {
            COMPONENT_TYPE type = component->type;
            components[type] = component->Clone();
        }
    }

    bool operator==(const GameObject& other) const
    {
        return id == other.id;
    }
private:
    static int lastId;

    static int generateUniqueId() {
        return lastId++;
    }
};
