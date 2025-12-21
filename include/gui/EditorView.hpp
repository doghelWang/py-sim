#pragma once
#include "amr/AppModel.hpp"
#include "gui/IView.hpp"
#include <string>
#include <vector>

namespace gui {

class EditorView : public IView {
public:
  EditorView();
  void Render() override;
  const char *GetName() const override { return "Editor"; }

private:
  void DrawPalette();
  void DrawWorkspace();
  void DrawCodeViewer();
  bool RenderBlock(amr::VisualBlock &b, int index);

  int m_dragging_idx = -1;
};

} // namespace gui
