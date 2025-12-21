#include "gui/EditorView.hpp"
#include "GuiLayer.hpp"
#include "PythonEngine.hpp"
#include "imgui.h"
#include <cfloat>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace gui {

static amr::AppModel &Model() { return amr::AppModel::Instance(); }

EditorView::EditorView() : m_dragging_idx(-1) {}

void EditorView::Render() {
  ImGui::Columns(2, "EditorCols");
  static bool first_run = true;
  if (first_run) {
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.35f);
    first_run = false;
  }
  DrawPalette();
  ImGui::NextColumn();

  ImGui::Separator();

  static int active_tab = 0;
  if (ImGui::Button("Blocks", ImVec2(100, 0)))
    active_tab = 0;
  ImGui::SameLine();
  if (ImGui::Button("Source Code", ImVec2(100, 0)))
    active_tab = 1;

  ImGui::Separator();

  if (active_tab == 0) {
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    DrawWorkspace();
    ImGui::EndChild();
  } else {
    ImGui::BeginChild("CodeViewer", ImVec2(0, 0), true);
    auto lines = Model().GetSourceLines();
    if (!lines.empty()) {
      for (const auto &line : lines) {
        ImGui::TextUnformatted(line.c_str());
      }
    } else {
      ImGui::Text("No source loaded. Please select a script or generate code.");
    }
    ImGui::EndChild();
  }
  ImGui::Columns(1);
}

void EditorView::DrawPalette() {
  ImGui::BeginChild("Palette", ImVec2(0, 0), true);
  ImGui::Text("Interface Blocks");
  ImGui::Separator();

  auto AddBtn = [&](amr::BlockType t, const char *label, ImU32 col) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(col));
    if (ImGui::Button(label, ImVec2(-FLT_MIN, 40))) {
      amr::VisualBlock b;
      b.id = Model().AllocateBlockId();
      b.type = t;
      Model().GetBlocks().push_back(b);
    }
    ImGui::PopStyleColor();
  };

  ImGui::Columns(2, "PaletteCols", false);

  ImGui::TextDisabled("Motion");
  AddBtn(amr::BlockType::MOVE_AXIS, "Move Axis", IM_COL32(255, 120, 0, 255));
  AddBtn(amr::BlockType::HOME_AXIS, "Home Mech", IM_COL32(255, 120, 0, 255));

  ImGui::NextColumn();
  ImGui::TextDisabled("Environment");
  AddBtn(amr::BlockType::SPAWN_OBSTACLE, "Spawn Wall",
         IM_COL32(100, 200, 50, 255));
  AddBtn(amr::BlockType::RESET_ENV, "Reset Env", IM_COL32(100, 200, 50, 255));
  AddBtn(amr::BlockType::AGV_MOVE_VEL, "AGV Vel", IM_COL32(100, 200, 50, 255));

  ImGui::NextColumn();
  ImGui::TextDisabled("I/O");
  AddBtn(amr::BlockType::SET_DO, "Set DO", IM_COL32(150, 50, 150, 255));
  AddBtn(amr::BlockType::WAIT_DI, "Wait DI", IM_COL32(150, 50, 150, 255));
  AddBtn(amr::BlockType::CONFIG_SAFETY, "SafetyCfg",
         IM_COL32(150, 50, 150, 255));

  ImGui::NextColumn();
  ImGui::TextDisabled("Logic");
  AddBtn(amr::BlockType::WAIT, "Delay", IM_COL32(50, 150, 255, 255));
  AddBtn(amr::BlockType::LOOP_START, "Loop Start", IM_COL32(50, 150, 255, 255));
  AddBtn(amr::BlockType::LOOP_END, "Loop End", IM_COL32(50, 150, 255, 255));
  AddBtn(amr::BlockType::IF_REG, "If Reg", IM_COL32(50, 150, 255, 255));
  AddBtn(amr::BlockType::LOG_MSG, "Log", IM_COL32(50, 150, 255, 255));

  ImGui::NextColumn();
  ImGui::TextDisabled("FX");
  AddBtn(amr::BlockType::GAME_SPAWN_PARTICLES, "Particles",
         IM_COL32(200, 50, 200, 255));
  AddBtn(amr::BlockType::GAME_SHAKE, "Shake", IM_COL32(200, 50, 200, 255));

  ImGui::Columns(1);
  ImGui::EndChild();
}
void EditorView::DrawWorkspace() {
  // ImGui::BeginChild("Workspace", ImVec2(0, 0), true); // This is now handled
  // by the Render() function's tab logic
  ImGui::Text("Programming Workspace");
  auto &blocks = Model().GetBlocks();
  auto &mechs = Model().GetMechanisms();
  auto &params = Model().GetGlobalParams();

  if (ImGui::Button("GENERATE", ImVec2(100, 0))) {
    GuiLayer::RequestScriptGeneration();
  }
  ImGui::SameLine();

  // Lifecycle Controls
  if (Model().IsRunning()) {
    if (Model().IsPaused()) {
      if (ImGui::Button("RESUME", ImVec2(80, 0)))
        Model().SetPaused(false);
    } else {
      if (ImGui::Button("PAUSE", ImVec2(80, 0)))
        Model().SetPaused(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("STOP", ImVec2(80, 0)))
      Model().RequestTermination();
  } else {
    if (ImGui::Button("RUN", ImVec2(100, 0))) {
      PythonEngine::StartWorker();
    }
  }
  ImGui::SameLine();

  if (ImGui::Button("CLEAR SAFETY", ImVec2(120, 0))) {
    Model().ClearSafety();
  }
  ImGui::SameLine();
  if (ImGui::Button("FULL RESET", ImVec2(100, 0))) {
    Model().FullReset();
  }
  ImGui::SameLine();

  if (ImGui::Button("Import amr_demo.txt", ImVec2(150, 0))) {
    std::ifstream ifs("../scripts/amr_demo.txt");
    if (ifs.is_open()) {
      blocks.clear();
      mechs.clear();
      params.clear();
      std::string line, section;
      while (std::getline(ifs, line)) {
        if (line.empty())
          continue;
        if (line[0] == '[') {
          section = line;
          continue;
        }

        if (section == "[MECHANISMS]") {
          amr::Mechanism m;
          char name[32];
          if (sscanf(line.c_str(), "%d,%[^,],%d", &m.id, name, &m.axis_map) ==
              3) {
            m.name = name;
            mechs.push_back(m);
          }
        } else if (section == "[PARAMS]") {
          char name[32];
          float val;
          if (sscanf(line.c_str(), "%[^,],%f", name, &val) == 2) {
            params.push_back({name, val});
          }
        } else if (section == "[BLOCKS]") {
          auto split = [](const std::string &s) {
            std::vector<std::string> v;
            size_t p = 0, n;
            while ((n = s.find(',', p)) != std::string::npos) {
              v.push_back(s.substr(p, n - p));
              p = n + 1;
            }
            v.push_back(s.substr(p));
            return v;
          };
          auto parts = split(line);
          if (parts.size() >= 7) {
            amr::VisualBlock b;
            b.id = Model().AllocateBlockId();
            int tid = std::stoi(parts[0]);
            // Mapping logic (Sync with previous version's IDs)
            if (tid == 0)
              b.type = amr::BlockType::MOVE_AXIS;
            else if (tid == 5)
              b.type = amr::BlockType::WAIT;
            else if (tid == 6)
              b.type = amr::BlockType::HOME_AXIS;
            else if (tid == 12)
              b.type = amr::BlockType::CONFIG_SAFETY;
            else
              b.type = amr::BlockType::LOG_MSG;

            b.param1 = std::stof(parts[1]);
            b.param2 = std::stof(parts[2]);
            b.param3 = std::stof(parts[3]);
            if (parts[4] != "_")
              b.param1_ref = parts[4];
            if (parts[5] != "_")
              b.param2_ref = parts[5];
            if (parts[6] != "_")
              b.param3_ref = parts[6];
            blocks.push_back(b);
          }
        }
      }
    }
  }

  // --- Script Selector ---
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  std::string cur_path = Model().GetScriptPath();
  if (ImGui::BeginCombo(
          "##Script",
          cur_path.substr(cur_path.find_last_of('/') + 1).c_str())) {
    if (ImGui::Selectable("visual_prog.py"))
      Model().SetScriptPath("../scripts/visual_prog.py");
    if (ImGui::Selectable("snake_game.py"))
      Model().SetScriptPath("../scripts/snake_game.py");
    if (ImGui::Selectable("particle_demo.py"))
      Model().SetScriptPath("../scripts/particle_demo.py");
    if (ImGui::Selectable("forker_demo.py"))
      Model().SetScriptPath("../scripts/forker_demo.py");
    if (ImGui::Selectable("ctu_demo.py"))
      Model().SetScriptPath("../scripts/ctu_demo.py");
    if (ImGui::Selectable("ctu_full_test.py"))
      Model().SetScriptPath("../scripts/ctu_full_test.py");
    ImGui::EndCombo();
  }
  ImGui::Separator();

  int indent_ui = 0;
  for (int i = 0; i < (int)blocks.size(); ++i) {
    auto &b = blocks[i];
    // Handle Loop Indentation (Visual only)
    if (b.type == amr::BlockType::LOOP_END)
      indent_ui = std::max(0, indent_ui - 1);

    if (indent_ui > 0)
      ImGui::Indent(indent_ui * 20.0f);

    // Render the block. If it returns true (structural change), break loop.
    if (RenderBlock(b, i))
      break;

    if (indent_ui > 0)
      ImGui::Unindent(indent_ui * 20.0f);

    if (b.type == amr::BlockType::LOOP_START)
      indent_ui++;
  }
}

void EditorView::DrawCodeViewer() {}

// Helper
static void ParamRow(const char *label, float *v, float step = 1.0f) {
  ImGui::Text("%s:", label);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-FLT_MIN);
  std::string id = std::string("##") + label;
  ImGui::DragFloat(id.c_str(), v, step);
}

bool EditorView::RenderBlock(amr::VisualBlock &b, int index) {
  auto &blocks = Model().GetBlocks();
  auto &mechs = Model().GetMechanisms();

  ImGui::PushID(b.id);

  // Color Coding
  ImVec4 block_col = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
  if (b.type == amr::BlockType::MOVE_AXIS ||
      b.type == amr::BlockType::HOME_AXIS ||
      b.type == amr::BlockType::AXIS_MOVE || b.type == amr::BlockType::MOVE)
    block_col = ImVec4(0.5f, 0.25f, 0.1f, 1.0f); // Orange
  if (b.type == amr::BlockType::WAIT || b.type == amr::BlockType::LOOP_START ||
      b.type == amr::BlockType::DELAY)
    block_col = ImVec4(0.1f, 0.3f, 0.5f, 1.0f); // Blue
  if (b.type == amr::BlockType::MSG || b.type == amr::BlockType::LOG_MSG)
    block_col = ImVec4(0.1f, 0.4f, 0.1f, 1.0f); // Green

  ImGui::PushStyleColor(ImGuiCol_ChildBg, block_col);
  float height = 50.0f;
  if (b.type == amr::BlockType::MOVE || b.type == amr::BlockType::AGV_MOVE_VEL)
    height = 100.0f;
  if (b.type == amr::BlockType::AXIS_MOVE)
    height = 80.0f;

  ImGui::BeginChild("Block", ImVec2(0, height), true,
                    ImGuiWindowFlags_NoScrollbar);

  // Header
  ImGui::Button("::", ImVec2(25, 25));
  if (ImGui::BeginDragDropSource()) {
    m_dragging_idx = index;
    ImGui::SetDragDropPayload("BLOCK_SEQ", &m_dragging_idx, sizeof(int));
    ImGui::Text("Moving Block %d", b.id);
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload("BLOCK_SEQ")) {
      int src = *(const int *)payload->Data;
      if (src < blocks.size()) {
        auto tmp = blocks[src];
        blocks.erase(blocks.begin() + src);
        blocks.insert(blocks.begin() + index, tmp);
        ImGui::EndDragDropTarget();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopID();
        return true; // Mutation!
      }
    }
    ImGui::EndDragDropTarget();
  }

  ImGui::SameLine();

  // Body
  if (b.type == amr::BlockType::MOVE ||
      b.type == amr::BlockType::AGV_MOVE_VEL) {
    ImGui::Text("SET VELOCITY");
    ImGui::Dummy(ImVec2(0, 2));
    if (b.type == amr::BlockType::AGV_MOVE_VEL && b.params.empty()) {
      b.params["vx"] = b.param1;
      b.params["vy"] = b.param2;
      b.params["wz"] = b.param3;
    }
    ParamRow("VX", &b.params["vx"], 0.1f);
    ParamRow("VY", &b.params["vy"], 0.1f);
    ParamRow("WZ", &b.params["wz"], 0.1f);
  } else if (b.type == amr::BlockType::AXIS_MOVE ||
             b.type == amr::BlockType::MOVE_AXIS) {
    ImGui::Text("AXIS MOVE");
    ImGui::Dummy(ImVec2(0, 2));

    int axis_id = (b.type == amr::BlockType::AXIS_MOVE) ? (int)b.params["axis"]
                                                        : (int)b.param1;
    std::string current_name = "Axis " + std::to_string(axis_id);
    for (auto &m : mechs)
      if (m.id == axis_id)
        current_name = m.name;

    ImGui::SetNextItemWidth(100);
    if (ImGui::BeginCombo("##Axis", current_name.c_str())) {
      for (auto &m : mechs) {
        if (ImGui::Selectable(m.name.c_str(), m.id == axis_id)) {
          b.params["axis"] = (float)m.id;
          b.param1 = (float)m.id;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    float pos =
        (b.type == amr::BlockType::AXIS_MOVE) ? b.params["pos"] : b.param2;
    ImGui::SetNextItemWidth(60);
    if (ImGui::DragFloat("Pos", &pos)) {
      b.params["pos"] = pos;
      b.param2 = pos;
    }
  } else if (b.type == amr::BlockType::DELAY ||
             b.type == amr::BlockType::WAIT) {
    ImGui::Text("WAIT (ms)");
    ImGui::SameLine();
    float ms = (b.type == amr::BlockType::DELAY) ? b.params["ms"] : b.param1;
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##ms", &ms, 10.0f)) {
      b.params["ms"] = ms;
      b.param1 = ms;
    }
  } else if (b.type == amr::BlockType::MSG ||
             b.type == amr::BlockType::LOG_MSG) {
    ImGui::Text("LOG MSG");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-35);
    char buf[128];
    if (b.type == amr::BlockType::MSG)
      strncpy(buf, b.message.c_str(), 127);
    else
      strncpy(buf, b.str_param.c_str(), 127);

    if (ImGui::InputText("##log", buf, 128)) {
      b.message = buf;
      b.str_param = buf;
    }
  } else {
    ImGui::Text("BLOCK ID: %d", (int)b.type);
  }

  // Close X
  ImGui::SameLine(ImGui::GetWindowWidth() - 35);
  if (ImGui::Button("X", ImVec2(25, 25))) {
    blocks.erase(blocks.begin() + index);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopID();
    return true; // Mutation!
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopID();
  return false;
}

} // namespace gui
