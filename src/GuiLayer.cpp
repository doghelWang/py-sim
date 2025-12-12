#include "GuiLayer.hpp"
#include "AppState.hpp"
#include "PythonEngine.hpp"
#include "imgui.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Basic Syntax Coloring Tokens
const std::vector<std::string> keywords = {
    "def ", "class ", "import ", "from ", "if ",     "else:", "elif ",
    "for ", "while ", "return",  "try:",  "except:", "print"};

struct ApiDoc {
  std::string name;
  std::string signature;
  std::string description;
  std::string example;
};

const std::vector<ApiDoc> api_docs = {
    {"log_message", "log_message(msg: str)",
     "Prints a message to the System Log.", "host_api.log_message('Hello')"},
    {"sleep_ms", "sleep_ms(ms: int)", "Pauses execution for N milliseconds.",
     "host_api.sleep_ms(100)"},
    {"draw_rect", "draw_rect(x, y, w, h, r, g, b)",
     "Draws a colored rectangle.",
     "host_api.draw_rect(10, 10, 50, 50, 255, 0, 0)"},
    {"draw_circle", "draw_circle(x, y, radius, r, g, b)",
     "Draws a colored circle.",
     "host_api.draw_circle(100, 100, 20, 0, 255, 0)"},
    {"draw_text", "draw_text(x, y, text, r, g, b)", "Draws text at position.",
     "host_api.draw_text(10, 10, 'Score: 0', 255, 255, 255)"},
    {"clear_screen", "clear_screen()", "Clears the game canvas.",
     "host_api.clear_screen()"},
    {"is_key_down", "is_key_down(key: str) -> bool",
     "Checks if key (up, down, left, right, w, a, s, d) was pressed.",
     "if host_api.is_key_down('up'): ..."},
    {"get_mouse_pos", "get_mouse_pos() -> (x, y)",
     "Returns mouse position relative to canvas.",
     "mx, my = host_api.get_mouse_pos()"},
    {"is_mouse_down", "is_mouse_down(btn: int) -> bool",
     "Checks mouse button (0=Left, 1=Right).",
     "if host_api.is_mouse_down(0): ..."},
    {"compute_prime", "compute_prime(n: int) -> int",
     "Calculates Nth prime number (CPU intensive).",
     "p = host_api.compute_prime(100)"},
    {"write_file", "write_file(path, content)", "Writes string to file.",
     "host_api.write_file('log.txt', 'data')"},
    {"read_file", "read_file(path) -> str", "Reads file content.",
     "data = host_api.read_file('config.txt')"},
    {"get_random_data", "get_random_data(count) -> [float]",
     "Returns list of random floats.", "data = host_api.get_random_data(10)"}};

void GuiLayer::SetupStyle() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 5.0f;
  style.FrameRounding = 4.0f;
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

// Scans ../scripts/ directory
std::vector<std::string> ScanScripts() {
  std::vector<std::string> files;
  std::string path = "../scripts";
  try {
    if (std::filesystem::exists(path)) {
      for (const auto &entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().extension() == ".py") {
          files.push_back(entry.path().string());
        }
      }
    }
  } catch (...) {
  }
  return files;
}

void DrawCanvas() {
  ImGui::Text("GAME OUTPUT");
  ImGui::BeginChild("GameCanvas", ImVec2(0, 0), true);

  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  ImVec2 p = ImGui::GetCursorScreenPos();

  ImVec2 size = ImGui::GetContentRegionAvail();
  draw_list->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                           IM_COL32(50, 50, 50, 255));

  // Capture Mouse
  ImGuiIO &io = ImGui::GetIO();
  {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    g_app.mouse_x = io.MousePos.x - p.x;
    g_app.mouse_y = io.MousePos.y - p.y;
    g_app.mouse_down[0] = io.MouseDown[0];
    g_app.mouse_down[1] = io.MouseDown[1];
    g_app.mouse_down[2] = io.MouseDown[2];
  }

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
      } else if (cmd.type == CmdType::TEXT) {
        draw_list->AddText(ImVec2(x, y), cmd.color, cmd.text.c_str());
      }
    }
  }
  ImGui::EndChild();
}

void CaptureInput() {
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
}

void GuiLayer::Render(void *window_ptr) {
  ImGuiIO &io = ImGui::GetIO();
  CaptureInput();

  static std::vector<std::string> script_list = ScanScripts();
  static int selected_script_idx = -1;

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

    ImGui::PushItemWidth(200);
    const char *preview =
        (selected_script_idx >= 0 && selected_script_idx < script_list.size())
            ? script_list[selected_script_idx].c_str()
            : "Select Script...";

    if (ImGui::BeginCombo("##Scripts", preview)) {
      for (int n = 0; n < script_list.size(); n++) {
        const bool is_selected = (selected_script_idx == n);
        if (ImGui::Selectable(script_list[n].c_str(), is_selected)) {
          selected_script_idx = n;
          strncpy(g_app.script_path, script_list[n].c_str(), 1024);
          LoadScriptContent();
        }
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("REFRESH"))
      script_list = ScanScripts();

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

  float bottom_height = 200.0f;
  float main_area_height = io.DisplaySize.y - 60 - bottom_height;

  ImGui::Columns(2, "MainColumns");
  if (g_app.is_running)
    ImGui::SetColumnWidth(0, io.DisplaySize.x * 0.60f);

  // Left Column: Split API List and Source
  ImGui::BeginChild("LeftCol", ImVec2(0, main_area_height), false);

  ImGui::Text("SOURCE EXPLORER");
  // Split Source Explorer into API List (25%) and Source (75%)
  ImGui::BeginChild("SourceContainer", ImVec2(0, main_area_height * 0.5f),
                    true);

  ImGui::Columns(2, "SourceSplit");
  ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.25f);

  // 1. API List
  ImGui::TextDisabled("API LIST");
  ImGui::BeginChild("ApiList", ImVec2(0, 0), false);
  for (const auto &doc : api_docs) {
    if (ImGui::Selectable(doc.name.c_str())) {
      ImGui::OpenPopup("ApiPopup");
    }

    // Hover Tooltip (Fall back if click doesn't work well)
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", doc.signature.c_str());
      ImGui::Text("%s", doc.description.c_str());
      ImGui::EndTooltip();
    }

    // Click Popup (Persistent Bubble)
    if (ImGui::BeginPopupContextItem()) { // Right click also works
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", doc.signature.c_str());
      ImGui::Separator();
      ImGui::TextWrapped("%s", doc.description.c_str());
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "Example:");
      ImGui::TextWrapped("%s", doc.example.c_str());
      ImGui::EndPopup();
    }
  }
  ImGui::EndChild();

  ImGui::NextColumn();

  // 2. Source Code
  ImGui::TextDisabled("CODE VIEW");
  ImGui::BeginChild("CodeView", ImVec2(0, 0), false);
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
  ImGui::EndChild(); // CodeView

  ImGui::Columns(1); // End SourceSplit
  ImGui::EndChild(); // End SourceContainer

  DrawCanvas();

  ImGui::EndChild(); // End LeftCol

  ImGui::NextColumn();

  // Right Column
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
          ImGui::TableNextColumn();
          ImGui::TextWrapped("%s", val.c_str());
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
