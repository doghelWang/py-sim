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
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 12.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;

  // Set Colors (Dark Theme)
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

void GuiLayer::Render(void *window_ptr) {
  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("MainLayout", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

  // Top Bar
  ImGui::BeginChild("TopBar", ImVec2(0, 60), true);
  {
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
        PythonEngine::StartWorker(); // Using new API
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
    // ... (Export API button logic unchanged)
  }
  ImGui::EndChild();

  ImGui::Columns(2, "MainColumns");
  if (g_app.is_running)
    ImGui::SetColumnWidth(0, io.DisplaySize.x * 0.60f);

  // Source View (Left)
  ImGui::Text("SOURCE EXPLORER");
  ImGui::BeginChild("SourceView", ImVec2(0, io.DisplaySize.y - 250), true);
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
          // Simple Syntax Highlighting Checks
          bool colored = false;
          // Check for comments
          size_t first_char = line_content.find_first_not_of(" \t");
          if (first_char != std::string::npos &&
              line_content[first_char] == '#') {
            ImGui::PushStyleColor(
                ImGuiCol_Text, IM_COL32(100, 200, 100, 255)); // Green Comments
            colored = true;
          } else {
            // Check keywords
            for (const auto &kw : keywords) {
              if (line_content.find(kw) != std::string::npos) { // Crude check
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    IM_COL32(200, 100, 200, 255)); // Pink Keywords
                colored = true;
                break;
              }
            }
          }

          if (!g_app.is_paused ||
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

  ImGui::NextColumn();

  // Variables View (Right) & Log (Bottom) - Reuse previous logic (truncated for
  // brevity in this prompt, but full content assumed)
  // ... [Variables View logic from previous file]
  ImGui::Text("DATA INSPECTOR");
  ImGui::BeginChild("VarsView", ImVec2(0, io.DisplaySize.y - 250), true);
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
          ImGui::TableNextColumn();
          ImGui::Text("%s", val.c_str());
        }
        ImGui::EndTable();
      }
    }
  }
  ImGui::EndChild();

  ImGui::Columns(1);
  ImGui::Separator();

  ImGui::Text("SYSTEM LOG");
  ImGui::BeginChild("LogView", ImVec2(0, 0), true);
  {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    ImGui::TextUnformatted(g_app.console_log.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
      ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();

  ImGui::End();
}
