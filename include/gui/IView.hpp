#pragma once
#include "imgui.h"

namespace gui {

class IView {
public:
  virtual ~IView() = default;
  virtual void Render() = 0;
  virtual const char *GetName() const = 0;
};

} // namespace gui
