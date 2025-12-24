#pragma once
#include <memory>
#include <vector>

namespace gui {
class IView;
}

class GuiLayer {
public:
  static void SetupStyle(float scale = 1.0f);
  static void Render(void *window_ptr);

  // Helper to trigger script generation (shared across views)
  static void RequestScriptGeneration();
};
