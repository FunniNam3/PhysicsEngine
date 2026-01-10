#pragma once

#include "imgui.h"
#include "core/main_engine.h"


class PreferencesWindow
{
    bool showPreferences = true;
  public:
    void ShowPreferencesWindow(MainEngine* mainEngine);
};