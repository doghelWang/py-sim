#include "GuiLayer.hpp"
#include "AppState.hpp"
#include "PythonEngine.hpp"
#include "imgui.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Basic Syntax Coloring Tokens
const std::vector<std::string> keywords = {
    "def ", "class ", "import ", "from ", "if ",     "else:", "elif ",
    "for ", "while ", "return",  "try:",  "except:", "print"};

void GuiLayer::SetupStyle() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 5.0f;
  style.FrameRounding = 4.0f;
  // ... (rest of style setup is fine, omitted for brevity if unchanged, but for
  // overwrite I must include all)
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 12.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.70f, 0.70f, 0.70f, 0.31f);
  colors[ImGuiCol_Button] = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
}

void LoadScriptContent() {
  g_app.source_lines.clear();
  std::ifstream file(g_app.script_path);
  if (!file.is_open())
    return;
  std::string line;
  while (std::getline(file, line))
    g_app.source_lines.push_back(line);
}

void DrawCanvas() {
  ImGui::Text("GAME OUTPUT");
  ImGui::BeginChild("GameCanvas", ImVec2(0, 0), true);

  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  ImVec2 p = ImGui::GetCursorScreenPos();

  ImVec2 size = ImGui::GetContentRegionAvail();
  draw_list->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                           IM_COL32(50, 50, 50, 255));

  {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    for (const auto &cmd : g_app.draw_queue) {
      float x = p.x + cmd.x;
      float y = p.y + cmd.y;

      if (cmd.type == CmdType::RECT) {
        draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + cmd.w, y + cmd.h),
                                 cmd.color);
      } else if (cmd.type == CmdType::CIRCLE) {
        draw_list->AddCircleFilled(ImVec2(x, y), cmd.r, cmd.color);
      }
    }
  }
  ImGui::EndChild();
}

void CaptureInput() {
  // Sticky Input: Set true if key pressed, consumer must clear it.
  std::lock_guard<std::mutex> lock(g_app.mtx);
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_W))
    g_app.input_sticky["up"] = true;
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_S))
    g_app.input_sticky["down"] = true;
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_A))
    g_app.input_sticky["left"] = true;
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_D))
    g_app.input_sticky["right"] = true;
  // Debounce/Limiting is handled by the consumer clearing flags
}

void GuiLayer::Render(void *window_ptr) {
  ImGuiIO &io = ImGui::GetIO();
  CaptureInput();

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("MainLayout", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

  // Top Bar
  ImGui::BeginChild("TopBar", ImVec2(0, 60), true);
  { // Block for scope
    ImGui::TextDisabled("CONTROLS");
    ImGui::SameLine();

    static char script_buf[1024];
    if (script_buf[0] == 0)
      strncpy(script_buf, g_app.script_path, 1024);
    ImGui::PushItemWidth(300);
    if (ImGui::InputText("##Path", script_buf, 1024))
      strncpy(g_app.script_path, script_buf, 1024);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("LOAD"))
      LoadScriptContent();
    ImGui::SameLine();

    if (!g_app.is_running) {
      if (ImGui::Button("RUN SCRIPT", ImVec2(100, 0))) {
        g_app.should_terminate = false;
        g_app.is_paused = false;
        LoadScriptContent();
        PythonEngine::StartWorker();
      }
    } else {
      if (g_app.is_paused) {
        if (ImGui::Button("RESUME", ImVec2(100, 0))) {
          std::lock_guard<std::mutex> lock(g_app.mtx);
          g_app.is_paused = false;
          g_app.cv.notify_all();
        }
      } else {
        if (ImGui::Button("PAUSE", ImVec2(100, 0)))
          g_app.is_paused = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("STOP", ImVec2(100, 0))) {
        g_app.should_terminate = true;
        {
          std::lock_guard<std::mutex> lock(g_app.mtx);
          g_app.is_paused = false;
        }
        g_app.cv.notify_all();
      }
    }
  }
  ImGui::EndChild();

  // Layout Logic
  float bottom_height = 200.0f; // Height for Logs
  float main_area_height =
      io.DisplaySize.y - 60 - bottom_height; // TopBar (60) + Bottom

  ImGui::Columns(2, "MainColumns");
  if (g_app.is_running)
    ImGui::SetColumnWidth(0, io.DisplaySize.x * 0.60f);

  // Left Column
  ImGui::BeginChild("LeftCol", ImVec2(0, main_area_height), false);

  // Source View (Top Half of Left)
  ImGui::Text("SOURCE EXPLORER");
  ImGui::BeginChild("SourceView", ImVec2(0, main_area_height * 0.5f), true);
  {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    if (g_app.source_lines.empty()) {
      ImGui::TextDisabled("No script loaded.");
    } else {
      for (int i = 0; i < g_app.source_lines.size(); i++) {
        int line_num = i + 1;
        bool is_current = (g_app.is_running && line_num == g_app.current_line);
        std::string line_content = g_app.source_lines[i];

        if (is_current) {
          ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
          ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 230, 100, 255));
          ImGui::BeginChild("LineHigh",
                            ImVec2(0, ImGui::GetTextLineHeightWithSpacing()),
                            false);
          ImGui::Text("> %03d: %s", line_num, line_content.c_str());
          ImGui::EndChild();
          ImGui::PopStyleColor(2);
          if (g_app.is_paused)
            ImGui::SetScrollHereY(0.5f);
        } else {
          bool colored = false;
          size_t first_char = line_content.find_first_not_of(" \t");
          if (first_char != std::string::npos &&
              line_content[first_char] == '#') {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 200, 100, 255));
            colored = true;
          } else {
            for (const auto &kw : keywords) {
              if (line_content.find(kw) != std::string::npos) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      IM_COL32(200, 100, 200, 255));
                colored = true;
                break;
              }
            }
          }

          if (!g_app.is_running || !g_app.is_paused ||
              std::abs(line_num - g_app.current_line) <= 5) {
            ImGui::Text("  %03d: %s", line_num, line_content.c_str());
          } else if (std::abs(line_num - g_app.current_line) == 6) {
            ImGui::TextDisabled("  ...");
          }
          if (colored)
            ImGui::PopStyleColor();
        }
      }
    }
  }
  ImGui::EndChild();

  // Game Canvas (Bottom Half of Left)
  DrawCanvas(); // This uses "GameCanvas" child, which will fit in remaining
                // space of LeftCol?
  // Wait, DrawCanvas uses BeginChild(0,0). If we are in LeftCol, it takes
  // remaining space of LeftCol. Correct.

  ImGui::EndChild(); // End LeftCol

  ImGui::NextColumn();

  // Right Column: Variables
  ImGui::Text("DATA INSPECTOR");
  ImGui::BeginChild("VarsView", ImVec2(0, main_area_height), true);
  {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    if (ImGui::CollapsingHeader("Live Variables",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::BeginTable("table_vars", 2,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();
        for (auto const &[key, val] : g_app.locals) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s", key.c_str());
          ImGui::TableNextColumn();              // Value
          ImGui::TextWrapped("%s", val.c_str()); // Wrap long values
        }
        ImGui::EndTable();
      }
    }
  }
  ImGui::EndChild();

  ImGui::Columns(1);
  ImGui::Separator();

  // Bottom Log Area
  ImGui::Text("SYSTEM LOG");
  ImGui::BeginChild("LogView", ImVec2(0, 0),
                    true); // Takes remaining space (at bottom)
  {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    ImGui::TextUnformatted(g_app.console_log.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
      ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();

  ImGui::End();
}
