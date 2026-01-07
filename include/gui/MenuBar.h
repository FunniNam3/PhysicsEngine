#pragma once

#include "imgui.h"
#include "core/main_engine.h"

class MenuBar{
  public:
  void ShowMenuBar(MainEngine *engine, bool &ShowDetail, bool &ShowView, bool &ShowHierarchy, bool &showPreferences);
  private:
};