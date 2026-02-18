#include "gui/HardwareView.hpp"
#include "amr/AmrController.hpp"
#include "amr/AppModel.hpp"
#include "imgui.h"
#include <cmath>

namespace gui {

static amr::AppModel &Model() { return amr::AppModel::Instance(); }

HardwareView::HardwareView() {
  // Populate RPi Pins (simplified as in previous code)
  m_pins.resize(40);
  for (int i = 0; i < 40; ++i) {
    m_pins[i].phys_pin = i + 1;
    m_pins[i].gpio_id = -1;
    m_pins[i].amr_di_idx = -1;
    m_pins[i].amr_do_idx = -1;
    m_pins[i].name = "Pin " + std::to_string(i + 1);
  }
  // Specific mappings
  m_pins[37].name = "GPIO 20 (ESTOP)";
  m_pins[37].gpio_id = 20;
  m_pins[37].amr_di_idx = 6;
  m_pins[39].name = "GPIO 21 (HOME)";
  m_pins[39].gpio_id = 21;
  m_pins[39].amr_di_idx = 7;
  m_pins[10].name = "GPIO 17 (DO0)";
  m_pins[10].gpio_id = 17;
  m_pins[10].amr_do_idx = 0;
}

void HardwareView::Render() {
  if (ImGui::BeginTabBar("HwTabs")) {
    if (ImGui::BeginTabItem("RPi 5 Control")) {
      DrawRPiVisualizer();
      DrawIOPanel();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Axis Status")) {
      DrawAxisControl();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Config")) {
      ImGui::BeginChild("ConfigEditors");

      // Mechanism Editor
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                         "Mechanisms (Axes/Actuators)");
      auto &mechs = Model().GetMechanisms();
      for (int i = 0; i < (int)mechs.size(); ++i) {
        ImGui::PushID(i);
        ImGui::SetNextItemWidth(100);
        char buf[32];
        strncpy(buf, mechs[i].name.c_str(), 31);
        if (ImGui::InputText("##Name", buf, 32))
          mechs[i].name = buf;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragInt("Axis", &mechs[i].axis_map, 1, 0, 10);
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
          mechs.erase(mechs.begin() + i);
          i--;
        }
        ImGui::PopID();
      }
      if (ImGui::Button("Add Mechanism")) {
        amr::Mechanism m;
        m.id = (int)mechs.size();
        m.name = "Axis" + std::to_string(m.id);
        mechs.push_back(m);
      }

      ImGui::Separator();

      // Parameter Editor
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Global Parameters");
      auto &params = Model().GetGlobalParams();
      for (int i = 0; i < (int)params.size(); ++i) {
        ImGui::PushID(i + 1000);
        ImGui::SetNextItemWidth(100);
        char buf[32];
        strncpy(buf, params[i].name.c_str(), 31);
        if (ImGui::InputText("##PName", buf, 32))
          params[i].name = buf;
        ImGui::SameLine();
        ImGui::DragFloat("Value", &params[i].value);
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
          params.erase(params.begin() + i);
          i--;
        }
        ImGui::PopID();
      }
      if (ImGui::Button("Add Parameter")) {
        params.push_back({"NewParam", 0.0f});
      }

      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void HardwareView::DrawRPiVisualizer() {
  ImGui::BeginChild("RPiView", ImVec2(0, 220), true);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImVec2 p = ImGui::GetCursorScreenPos();

  dl->AddRectFilled(ImVec2(p.x + 10, p.y + 10), ImVec2(p.x + 310, p.y + 160),
                    IM_COL32(40, 160, 60, 255), 10.0f);
  dl->AddText(ImVec2(p.x + 100, p.y + 70), IM_COL32(200, 255, 200, 100),
              "Raspberry Pi 5");

  ImVec2 header_start = ImVec2(p.x + 150, p.y + 20);
  float step = 12.0f;
  for (int i = 0; i < 40; ++i) {
    int row = i % 2;
    int col = i / 2;
    ImVec2 pin_p =
        ImVec2(header_start.x + col * step, header_start.y + row * step);
    ImU32 col_pin = IM_COL32(200, 180, 100, 255);
    if (m_pins[i].amr_di_idx != -1 && Model().GetDI(m_pins[i].amr_di_idx))
      col_pin = IM_COL32(255, 50, 50, 255);
    if (m_pins[i].amr_do_idx != -1 && Model().GetDO(m_pins[i].amr_do_idx))
      col_pin = IM_COL32(50, 255, 50, 255);
    dl->AddCircleFilled(pin_p, 3.5f, col_pin);
    m_pins[i].socket_pos = pin_p;
  }

  // E-Stop/Home buttons on the right
  ImGui::SetCursorScreenPos(ImVec2(p.x + 330, p.y + 30));
  if (ImGui::Button("E-STOP (DI6)", ImVec2(100, 40)))
    Model().SetDI(6, !Model().GetDI(6));
  ImGui::SetCursorScreenPos(ImVec2(p.x + 330, p.y + 80));
  if (ImGui::Button("HOME (DI7)", ImVec2(100, 40)))
    Model().SetDI(7, !Model().GetDI(7));

  ImGui::EndChild();
}

void HardwareView::DrawIOPanel() {
  ImGui::BeginChild("IOMonitor", ImVec2(0, 0), true);

  if (ImGui::CollapsingHeader("Digital Inputs (0-31)",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    for (int i = 0; i < 32; ++i) {
      bool v = Model().GetDI(i);
      ImGui::PushID(i);
      if (ImGui::Checkbox("##DI", &v))
        Model().SetDI(i, v);

      // Check for safety label
      std::string label = "";
      if (i == amr::AmrController::Instance().GetPinForAction(
                   amr::InputAction::ESTOP))
        label = "[E-STOP]";
      else if (i == amr::AmrController::Instance().GetPinForAction(
                        amr::InputAction::PAUSE_TOGGLE))
        label = "[PAUSE]";
      else if (i == amr::AmrController::Instance().GetPinForAction(
                        amr::InputAction::HOME_ALL))
        label = "[HOME]";

      if (ImGui::IsItemHovered()) {
        if (!label.empty())
          ImGui::SetTooltip("DI %d: %s", i, label.c_str());
        else
          ImGui::SetTooltip("DI %d", i);
      }
      if (!label.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "%s",
                           label.substr(0, 4).c_str());
      }
      ImGui::PopID();
      if ((i + 1) % 4 != 0)
        ImGui::SameLine(); // Fewer columns for better label visibility
    }
  }

  ImGui::Separator();

  if (ImGui::CollapsingHeader("Digital Outputs (0-31)",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    for (int i = 0; i < 32; ++i) {
      bool v = Model().GetDO(i);
      ImU32 col = v ? IM_COL32(0, 255, 0, 200) : IM_COL32(50, 50, 50, 200);
      ImGui::ColorButton(("##DO" + std::to_string(i)).c_str(),
                         ImGui::ColorConvertU32ToFloat4(col), 0,
                         ImVec2(20, 20));
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("DO %d: %s", i, v ? "ON" : "OFF");
      if ((i + 1) % 8 != 0)
        ImGui::SameLine();
    }
    ImGui::NewLine();
  }

  ImGui::EndChild();
}

void HardwareView::DrawAxisControl() {
  ImGui::BeginChild("AxisCtrl", ImVec2(0, 0), true);
  for (int i = 0; i < 3; ++i) {
    auto st = Model().GetHardware()->GetAxis(i)->GetStatus();
    ImGui::Text("AXIS %d: Pos=%.1f Vel=%.1f %s", i, st.actual_pos,
                st.actual_vel, st.is_moving ? "MOVING" : "IDLE");
    if (ImGui::Button(("Move+##" + std::to_string(i)).c_str()))
      Model().AxisMove(i, st.actual_pos + 50, 25);
    ImGui::SameLine();
    if (ImGui::Button(("Move-##" + std::to_string(i)).c_str()))
      Model().AxisMove(i, st.actual_pos - 50, 25);
    ImGui::Separator();
  }
  ImGui::EndChild();
}

} // namespace gui
