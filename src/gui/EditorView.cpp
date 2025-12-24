#define _CRT_SECURE_NO_WARNINGS
#include "gui/EditorView.hpp"
#include "amr/AppModel.hpp"
#include "amr/ServiceContext.hpp"
#include "amr/ScriptExecutor.hpp"
#include "GuiLayer.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <fstream>
#include <string>

namespace gui {

static amr::AppModel &Model() { return amr::AppModel::Instance(); }

// Helper
static void ParamRow(const char *label, float *v, float step = 1.0f) {
  ImGui::Text("%s:", label);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  std::string id = std::string("##") + label;
  ImGui::DragFloat(id.c_str(), v, step);
}


EditorView::EditorView() {}

void EditorView::Render() {
  ImGui::Text("Programming Workspace");
  auto &blocks = Model().GetBlocks();
  auto &mechs = Model().GetMechanisms();
  auto &params = Model().GetGlobalParams();

  // -- Toolbar --
  ImGui::BeginGroup(); // Toolbar Group
  
  // Row 1: File & Script
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("FILE:");
  ImGui::SameLine();
  
  std::string cur_path = Model().GetScriptPath();
  ImGui::SetNextItemWidth(180);
  if (ImGui::BeginCombo(
          "##Script",
          cur_path.substr(cur_path.find_last_of('/') + 1).c_str())) {
    if (ImGui::Selectable("visual_prog.py"))
      Model().SetScriptPath("scripts/visual_prog.py");
    if (ImGui::Selectable("snake_game.py"))
      Model().SetScriptPath("scripts/snake_game.py");
    if (ImGui::Selectable("particle_demo.py"))
      Model().SetScriptPath("scripts/particle_demo.py");
    if (ImGui::Selectable("forker_demo.py"))
      Model().SetScriptPath("scripts/forker_demo.py");
    if (ImGui::Selectable("ctu_demo.py"))
      Model().SetScriptPath("scripts/ctu_demo.py");
    if (ImGui::Selectable("ctu_full_test.py"))
      Model().SetScriptPath("scripts/ctu_full_test.py");
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  if (ImGui::Button("Import Demo", ImVec2(100, 0))) {
    std::ifstream ifs("scripts/amr_demo.txt");
    if (ifs.is_open()) {
       blocks.clear(); mechs.clear(); params.clear();
       std::string line, section;
       while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        if (line[0] == '[') { section = line; continue; }
        if (section == "[MECHANISMS]") {
          amr::Mechanism m; char name[32];
          if (sscanf(line.c_str(), "%d,%[^,],%d", &m.id, name, &m.axis_map) == 3) {
            m.name = name; mechs.push_back(m);
          }
        } else if (section == "[PARAMS]") {
          char name[32]; float val;
          if (sscanf(line.c_str(), "%[^,],%f", name, &val) == 2) params.push_back({name, val});
        } else if (section == "[BLOCKS]") {
           /* Simplified import for brevity, assuming minimal demo needs */
        }
       }
    }
  }

  // Row 2: Control
  ImGui::Dummy(ImVec2(0, 4));
  ImGui::TextDisabled("EXEC:");
  ImGui::SameLine();
  
  if (Model().IsRunning()) {
    if (Model().IsPaused()) {
      if (ImGui::Button("RESUME", ImVec2(80, 0))) Model().SetPaused(false);
    } else {
      if (ImGui::Button("PAUSE", ImVec2(80, 0))) Model().SetPaused(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("STOP", ImVec2(80, 0))) Model().RequestTermination();
  } else {
    if (ImGui::Button("RUN", ImVec2(80, 0))) {
      auto executor = amr::ServiceContext::Instance().Get<amr::ScriptExecutor>();
      if(executor) executor->RunScript(Model().GetScriptPath());
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("GENERATE", ImVec2(80, 0))) GuiLayer::RequestScriptGeneration();

  // Row 3: Safety
  ImGui::Dummy(ImVec2(0, 4));
  ImGui::TextDisabled("SAFE:");
  ImGui::SameLine();
  if (ImGui::Button("CLEAR SAFE", ImVec2(100, 0))) Model().ClearSafety();
  ImGui::SameLine();
  if (ImGui::Button("FULL RESET", ImVec2(100, 0))) Model().FullReset();

  ImGui::EndGroup(); // Close Toolbar Group
  ImGui::Separator();

  int indent_ui = 0;
  for (int i = 0; i < (int)blocks.size(); ++i) {
    auto &b = blocks[i];
    if (b.type == amr::BlockType::LOOP_END)
      indent_ui = std::max(0, indent_ui - 1);

    if (indent_ui > 0) ImGui::Indent(indent_ui * 20.0f);

    if (RenderBlock(b, i)) break;

    if (indent_ui > 0) ImGui::Unindent(indent_ui * 20.0f);

    if (b.type == amr::BlockType::LOOP_START)
      indent_ui++;
  }
}

bool EditorView::RenderBlock(amr::VisualBlock &b, int index) {
  auto &blocks = Model().GetBlocks();
  auto &mechs = Model().GetMechanisms();

  ImGui::PushID(b.id);
  
  // -- Modern Palette (Card Style) --
  ImVec4 block_col = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
  if (b.type == amr::BlockType::MOVE_AXIS || b.type == amr::BlockType::HOME_AXIS ||
      b.type == amr::BlockType::AXIS_MOVE || b.type == amr::BlockType::MOVE ||
      b.type == amr::BlockType::AGV_MOVE_VEL)
    block_col = ImVec4(1.0f, 0.42f, 0.21f, 1.0f); // #FF6B35 Orange
  
  if (b.type == amr::BlockType::WAIT || b.type == amr::BlockType::DELAY || 
      b.type == amr::BlockType::LOOP_START || b.type == amr::BlockType::LOOP_END ||
      b.type == amr::BlockType::IF_REG)
    block_col = ImVec4(0.18f, 0.77f, 0.71f, 1.0f); // #2EC4B6 Teal

  if (b.type == amr::BlockType::SET_DO || b.type == amr::BlockType::WAIT_DI ||
      b.type == amr::BlockType::CONFIG_SAFETY || b.type == amr::BlockType::SET_REG)
    block_col = ImVec4(0.0f, 0.66f, 0.59f, 1.0f); // #00A896 Green
  
  if (b.type == amr::BlockType::MSG || b.type == amr::BlockType::LOG_MSG)
    block_col = ImVec4(0.62f, 0.31f, 0.87f, 1.0f); // #9D4EDD Purple

  // -- Height Calculation --
  float header_h = 24.0f;
  float content_h = 32.0f;
  if (b.type == amr::BlockType::AGV_MOVE_VEL) content_h = 60.0f; // 2 rows
  if (b.type == amr::BlockType::MOVE_AXIS || b.type == amr::BlockType::AXIS_MOVE) content_h = 32.0f;

  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f); // Margin
  ImGui::BeginChild("Block", ImVec2(0, header_h + content_h), true,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
  
  // Custom Background Rendering
  ImDrawList* draw = ImGui::GetWindowDrawList();
  ImVec2 p_min = ImGui::GetWindowPos();
  ImVec2 p_max = ImVec2(p_min.x + ImGui::GetWindowWidth(), p_min.y + ImGui::GetWindowHeight());
  
  // 1. Header Rect (Colored)
  draw->AddRectFilled(p_min, ImVec2(p_max.x, p_min.y + header_h), ImGui::ColorConvertFloat4ToU32(block_col), 6.0f, ImDrawFlags_RoundCornersTop);
  // 2. Body Rect (Dark)
  draw->AddRectFilled(ImVec2(p_min.x, p_min.y + header_h), p_max, IM_COL32(40, 40, 45, 255), 6.0f, ImDrawFlags_RoundCornersBottom);
  // 3. Border (Subtle)
  draw->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 30), 6.0f);

  // -- Header Content --
  ImGui::SetCursorPos(ImVec2(6, 2));
  ImGui::TextColored(ImVec4(0,0,0,1), "::  %d", b.id);
  ImGui::SameLine();
  if(b.type == amr::BlockType::AGV_MOVE_VEL) ImGui::TextColored(ImVec4(0,0,0,0.7f), "AGV VELOCITY");
  else if(b.type == amr::BlockType::WAIT) ImGui::TextColored(ImVec4(0,0,0,0.7f), "WAIT");
  else if(b.type == amr::BlockType::LOG_MSG) ImGui::TextColored(ImVec4(0,0,0,0.7f), "LOG");

  // Drag Handle
  ImGui::SetCursorPos(ImVec2(0, 0));
  ImGui::InvisibleButton("HeaderDrag", ImVec2(ImGui::GetWindowWidth()-30, header_h));
  if (ImGui::BeginDragDropSource()) {
    m_dragging_idx = index;
    ImGui::SetDragDropPayload("BLOCK_SEQ", &m_dragging_idx, sizeof(int));
    ImGui::Text("Move Block %d", b.id);
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginDragDropTarget()) {
     if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("BLOCK_SEQ")) {
       int src = *(const int *)payload->Data;
       if (src < blocks.size()) {
         auto tmp = blocks[src];
         blocks.erase(blocks.begin() + src);
         blocks.insert(blocks.begin() + index, tmp);
         ImGui::EndDragDropTarget();
         ImGui::EndChild();
         ImGui::PopStyleVar(2);
         ImGui::PopID();
         return true; // Mutation
       }
     }
     ImGui::EndDragDropTarget();
  }

  // Close Button
  ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 24, 0));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
  if (ImGui::Button("x", ImVec2(24, 24))) {
     blocks.erase(blocks.begin() + index);
     ImGui::EndChild();
     ImGui::PopStyleColor(2);
     ImGui::PopStyleVar(2);
     ImGui::PopID();
     return true;
  }
  ImGui::PopStyleColor(2);

  // -- Body Content (Parameters) --
  ImGui::SetCursorPos(ImVec2(8, header_h + 4));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

  if (b.type == amr::BlockType::AGV_MOVE_VEL) {
      if (b.params.empty()) { b.params["vx"]=b.param1; b.params["vy"]=b.param2; b.params["wz"]=b.param3; }
      ImGui::PushItemWidth(50);
      ImGui::Text("VX"); ImGui::SameLine(); ImGui::DragFloat("##vx", &b.params["vx"], 0.1f); 
      ImGui::SameLine(); ImGui::Text("VY"); ImGui::SameLine(); ImGui::DragFloat("##vy", &b.params["vy"], 0.1f);
      ImGui::Text("WZ"); ImGui::SameLine(); ImGui::DragFloat("##wz", &b.params["wz"], 0.1f);
      ImGui::PopItemWidth();
  } 
  else if (b.type == amr::BlockType::WAIT || b.type == amr::BlockType::DELAY) {
      float ms = (b.type == amr::BlockType::DELAY) ? b.params["ms"] : b.param1;
      ImGui::Text("Duration(ms):"); ImGui::SameLine(); 
      ImGui::SetNextItemWidth(80);
      if(ImGui::DragFloat("##ms", &ms, 10.f)) { b.params["ms"]=ms; b.param1=ms; }
  }
  else if (b.type == amr::BlockType::AXIS_MOVE || b.type == amr::BlockType::MOVE_AXIS) {
      int axis_id = (b.type == amr::BlockType::AXIS_MOVE) ? (int)b.params["axis"] : (int)b.param1;
      ImGui::SetNextItemWidth(80);
      std::string current = "Axis " + std::to_string(axis_id);
      for(auto& m: mechs) if(m.id == axis_id) current = m.name;
      
      if(ImGui::BeginCombo("##ax", current.c_str())) {
          for(auto& m : mechs) if(ImGui::Selectable(m.name.c_str())) { b.params["axis"]=(float)m.id; b.param1=(float)m.id; }
          ImGui::EndCombo();
      }
      ImGui::SameLine();
      float pos = (b.type == amr::BlockType::AXIS_MOVE) ? b.params["pos"] : b.param2;
      ImGui::SetNextItemWidth(60);
      if(ImGui::DragFloat("##p", &pos)) { b.params["pos"]=pos; b.param2=pos; }
  }
  else if (b.type == amr::BlockType::LOG_MSG) {
      char buf[128]; strncpy(buf, b.str_param.c_str(), 127);
      ImGui::SetNextItemWidth(-10);
      if(ImGui::InputText("##log", buf, 128)) b.str_param = buf;
  }
  else if (b.type == amr::BlockType::LOOP_START) {
      int count = (int)b.param1;
      ImGui::Text("Loop Count:"); ImGui::SameLine();
      if(ImGui::InputInt("##cnt", &count)) b.param1 = (float)count;
  }
  else {
      ImGui::Text("Type: %d", (int)b.type);
  }

  ImGui::PopStyleColor(); // Text
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopID();
  return false;
}

void EditorView::DrawCodeViewer() {}

} // namespace gui
