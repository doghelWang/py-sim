#include "GuiLayer.hpp"
#include "amr/AppModel.hpp"
#include "gui/EditorView.hpp"
#include "gui/HardwareView.hpp"
#include "gui/SimulatorView.hpp"
#include "imgui.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Global view instances (Keep it simple for now as it's a static layer)
static std::vector<std::unique_ptr<gui::IView>> g_views;

static amr::AppModel &Model() { return amr::AppModel::Instance(); }

void GuiLayer::SetupStyle() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 2.0f;
  style.FrameRounding = 3.0f;
  style.ScrollbarRounding = 10.0f;
  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.40f, 0.40f, 0.90f, 0.40f);
  colors[ImGuiCol_Button] = ImVec4(0.30f, 0.30f, 0.60f, 0.60f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.40f, 1.00f, 0.80f);
  colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
}

void GuiLayer::Render(void *window_ptr) {
  if (g_views.empty()) {
    g_views.push_back(std::make_unique<gui::SimulatorView>());
    g_views.push_back(std::make_unique<gui::HardwareView>());
    g_views.push_back(std::make_unique<gui::EditorView>());
  }

  static float left_width = 300.0f;
  static float right_width = 400.0f;
  static float bottom_height = 150.0f;
  static bool show_debug = true;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

  ImGui::Begin("AMR Controller Studio", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoNavFocus);

  ImVec2 full_size = ImGui::GetContentRegionAvail();

  // --- Left Panel: Hardware ---
  ImGui::BeginChild("LeftPanel",
                    ImVec2(left_width, show_debug ? full_size.y - bottom_height
                                                  : full_size.y),
                    true);
  g_views[1]->Render();
  ImGui::EndChild();

  ImGui::SameLine();

  // Splitter 1
  ImGui::Button("##S1", ImVec2(4, -1));
  if (ImGui::IsItemActive())
    left_width += ImGui::GetIO().MouseDelta.x;

  ImGui::SameLine();

  // --- Center Panel: Editor ---
  float center_width = full_size.x - left_width - right_width - 8;
  ImGui::BeginChild("CenterPanel",
                    ImVec2(center_width, show_debug
                                             ? full_size.y - bottom_height
                                             : full_size.y),
                    true);
  g_views[2]->Render();
  ImGui::EndChild();

  ImGui::SameLine();

  // Splitter 2
  ImGui::Button("##S2", ImVec2(4, -1));
  if (ImGui::IsItemActive())
    right_width -= ImGui::GetIO().MouseDelta.x;

  ImGui::SameLine();

  // --- Right Panel: Simulator ---
  ImGui::BeginChild(
      "RightPanel",
      ImVec2(0, show_debug ? full_size.y - bottom_height : full_size.y), true);
  g_views[0]->Render();
  ImGui::EndChild();

  // --- Bottom Panel: Debug Console ---
  if (show_debug) {
    ImGui::SetCursorPos(ImVec2(0, full_size.y - bottom_height + 4));
    ImGui::BeginChild("BottomPanel", ImVec2(full_size.x, 0), true);
    if (ImGui::Button("X", ImVec2(20, 20)))
      show_debug = false;
    ImGui::SameLine();
    ImGui::Text("Debug Console");
    ImGui::Separator();

    auto &logs = Model().GetLogs();
    for (const auto &log : logs) {
      ImGui::TextUnformatted(log.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
      ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
  } else {
    ImGui::SetCursorPos(ImVec2(0, full_size.y - 25));
    if (ImGui::Button("Show Console", ImVec2(120, 20)))
      show_debug = true;
  }

  ImGui::End();

  // --- Input Forwarding ---
  ImGuiIO &io = ImGui::GetIO();
  Model().SetMousePos(io.MousePos.x, io.MousePos.y);
  Model().SetMouseDown(0, io.MouseDown[0]);
  Model().SetMouseDown(1, io.MouseDown[1]);
  Model().SetInputSticky("LEFT", io.KeysDown[ImGuiKey_LeftArrow]);
  Model().SetInputSticky("RIGHT", io.KeysDown[ImGuiKey_RightArrow]);
  Model().SetInputSticky("UP", io.KeysDown[ImGuiKey_UpArrow]);
  Model().SetInputSticky("DOWN", io.KeysDown[ImGuiKey_DownArrow]);
  Model().SetInputSticky("SPACE", io.KeysDown[ImGuiKey_Space]);

  ImGui::PopStyleVar(3);
}

// Logic for script generation (kept static here for accessibility by views)
void GuiLayer::RequestScriptGeneration() {
  auto blocks = Model().GetBlocks();
  const auto &mechanisms = Model().GetMechanisms();

  std::string code = "import time\nimport threading\nimport sys\nimport os\n";
  code += "sys.path.append(os.getcwd() + '/../src')\ntry:\n    import "
          "host_api\nexcept ImportError:\n    class HostApi:\n        def "
          "log_message(self, msg): print(msg)\n        def axis_move(self, a, "
          "p, v): print(f'Move {a} {p} {v}')\n        def axis_is_moving(self, "
          "a): return False\n        def sleep_ms(self, ms): "
          "time.sleep(ms/1000.0)\n        def set_do(self, p, v): print(f'DO "
          "{p} {v}')\n        def get_di(self, p): return False\n        def "
          "set_reg(self, r, v): pass\n        def get_reg(self, r): return "
          "0.0\n        def get_param(self, n): return 0.0\n        def "
          "set_paused(self, p): pass\n    host_api = HostApi()\n\n";

  // init()
  code += "def init():\n    host_api.log_message('[Sys] Initializing Safety "
          "Config...')\n";
  for (const auto &b : blocks) {
    if (b.type == amr::BlockType::CONFIG_SAFETY) {
      int pin = (int)b.param1, action = (int)b.param2, flags = (int)b.param3;
      code += "    host_api.configure_input(" + std::to_string(pin) + ", " +
              std::to_string(action) + ", " + ((flags & 1) ? "True" : "False") +
              ", " + ((flags & 2) ? "True" : "False") + ")\n";
    }
  }
  code += "    host_api.log_message('[Sys] Safety Configured.')\n\n";

  // main()
  code += "def main():\n    host_api.log_message('Starting AMR Logic...')\n";
  int indent_level = 1;
  auto GetVal = [](float val, const std::string &ref) -> std::string {
    if (!ref.empty())
      return "host_api.get_param('" + ref + "')";
    return std::to_string(val);
  };

  for (const auto &block : blocks) {
    if (block.type == amr::BlockType::LOOP_END && indent_level > 1)
      indent_level--;
    std::string indent = "";
    for (int i = 0; i < indent_level; ++i)
      indent += "    ";

    switch (block.type) {
    case amr::BlockType::MOVE_AXIS: {
      int axis = (int)block.param1;
      for (const auto &m : mechanisms)
        if (m.id == (int)block.param1)
          axis = m.axis_map;
      code += indent + "host_api.axis_move(" + std::to_string(axis) + ", " +
              GetVal(block.param2, block.param2_ref) + ", " +
              GetVal(block.param3, block.param3_ref) + ")\n";
      code += indent + "while host_api.axis_is_moving(" + std::to_string(axis) +
              "): host_api.sleep_ms(10)\n";
      break;
    }
    case amr::BlockType::WAIT:
      code += indent + "host_api.sleep_ms(" +
              std::to_string((int)block.param1) + ")\n";
      break;
    case amr::BlockType::SET_DO:
      code += indent + "host_api.set_do(" + std::to_string((int)block.param1) +
              ", " + (block.param2 > 0 ? "True" : "False") + ")\n";
      break;
    case amr::BlockType::LOOP_START:
      code += indent + "for _i in range(" + std::to_string((int)block.param1) +
              "):\n";
      indent_level++;
      break;
    case amr::BlockType::HOME_AXIS: {
      int axis = (int)block.param1;
      for (const auto &m : mechanisms)
        if (m.id == (int)block.param1)
          axis = m.axis_map;
      code += indent + "host_api.axis_move(" + std::to_string(axis) +
              ", 0.0, 10.0)\n";
      code += indent + "while host_api.axis_is_moving(" + std::to_string(axis) +
              "): host_api.sleep_ms(10)\n";
      break;
    }
    case amr::BlockType::WAIT_DI:
      code += indent + "while host_api.get_di(" +
              std::to_string((int)block.param1) +
              ") != " + (block.param2 > 0 ? "True" : "False") +
              ": host_api.sleep_ms(10)\n";
      break;
    case amr::BlockType::SET_REG:
      code += indent + "host_api.set_reg(" + std::to_string((int)block.param1) +
              ", " + GetVal(block.param2, block.param2_ref) + ")\n";
      break;
    case amr::BlockType::LOOP_END:
      break;
    case amr::BlockType::LOG_MSG:
      code += indent + "host_api.log_message('" + block.str_param + "')\n";
      break;
    case amr::BlockType::RESET_ENV:
      code += indent + "host_api.reset_obstacles()\n";
      break;
    case amr::BlockType::SPAWN_OBSTACLE:
      code += indent + "host_api.add_obstacle(" + std::to_string(block.param1) +
              ", " + std::to_string(block.param2) + ", " +
              std::to_string(block.param3) + ", 50)\n";
      break;
    case amr::BlockType::IF_REG:
      code += indent + "if host_api.get_reg(" +
              std::to_string((int)block.param1) + ") > " +
              std::to_string(block.param2) + ":\n";
      indent_level++;
      break;
    case amr::BlockType::AGV_MOVE_VEL:
      code += indent + "host_api.set_twist(" + std::to_string(block.param1) +
              ", " + std::to_string(block.param2) + ", " +
              std::to_string(block.param3) + ")\n";
      break;
    case amr::BlockType::GAME_SPAWN_PARTICLES:
      code += indent + "host_api.spawn_particles(" +
              std::to_string(block.param1) + ", " +
              std::to_string(block.param2) + ", 10, 255, 100, 0)\n";
      break;
    case amr::BlockType::GAME_SHAKE:
      code += indent + "host_api.screen_shake(" + std::to_string(block.param1) +
              ")\n";
      break;
    case amr::BlockType::GAME_DRAW_TEXT:
      code += indent + "host_api.draw_text(100, 100, '" + block.str_param +
              "', 255, 255, 255)\n";
      break;
    default:
      break;
    }
  }

  code += "    host_api.log_message('Program Complete.')\n\nif __name__ == "
          "'__main__':\n    init()\n    main()\n";

  std::ofstream out("../scripts/visual_prog.py");
  out << code;
  out.close();
  Model().SetScriptPath("../scripts/visual_prog.py");
}
