#pragma once
#include <memory>
#include <vector>

namespace gui {
class IView;
}

class GuiLayer {
public:
  static void SetupStyle();
  static void Render(void *window_ptr);

  // Helper to trigger script generation (shared across views)
  static void RequestScriptGeneration();
};
