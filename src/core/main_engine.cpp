#include "core/main_engine.h"

std::vector<std::shared_ptr<Scene>> MainEngine::scenes;
int MainEngine::currSceneIdx = 0;
std::unordered_map<unsigned int, bool> MainEngine::keyPresses;
