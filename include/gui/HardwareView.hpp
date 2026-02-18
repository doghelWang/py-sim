#pragma once
#include "amr/AppModel.hpp"
#include "gui/IView.hpp"
#include <string>
#include <vector>

namespace gui {

struct RPiPin {
  int phys_pin;
  std::string name;
  int gpio_id;
  int amr_di_idx;
  int amr_do_idx;
  ImVec2 socket_pos;
};

class HardwareView : public IView {
public:
  HardwareView();
  void Render() override;
  const char *GetName() const override { return "Hardware"; }

private:
  void DrawRPiVisualizer();
  void DrawAxisControl();
  void DrawIOPanel();

  std::vector<RPiPin> m_pins;
};

} // namespace gui
