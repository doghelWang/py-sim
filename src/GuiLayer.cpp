#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "GuiLayer.hpp"
#include "PythonEngine.hpp"
#include "amr/AppModel.hpp"
#include "imgui.h"
#include "stb_image_write.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

// For glReadPixels
#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// Helper for brevity
static amr::AppModel &Model() { return amr::AppModel::Instance(); }

// ---------------------------------------------------------
// API Docs for Reference
// ---------------------------------------------------------
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
    {"log_message", "log_message(msg)", "Log to System Log.",
     "host_api.log_message('Hi')"},
    {"get_param", "get_param(name)", "Get Global Param.",
     "val = host_api.get_param('Fast_Speed')"},
    {"sleep_ms", "sleep_ms(ms)", "Pause execution.", "host_api.sleep_ms(1000)"},
    {"compute_prime", "compute_prime(n)", "CPU heavy task.",
     "p = host_api.compute_prime(5000)"},
    {"write_file", "write_file(path, content)", "Write text file.",
     "host_api.write_file('out.txt', 'Hello')"},
    {"read_file", "read_file(path)", "Read text file.",
     "txt = host_api.read_file('in.txt')"},
    {"http_get", "http_get(host, path)", "HTTP GET request.",
     "res = host_api.http_get('example.com', '/')"},
    {"take_screenshot", "take_screenshot(filename)", "Save screenshot.",
     "host_api.take_screenshot('snap.png')"},
    {"draw_rect", "draw_rect(x, y, w, h, r, g, b)", "Draw Rectangle.",
     "host_api.draw_rect(10, 10, 50, 50, 255, 0, 0)"},
    {"draw_circle", "draw_circle(x, y, r, r, g, b)", "Draw Circle.",
     "host_api.draw_circle(100, 100, 20, 0, 255, 0)"},
    {"draw_text", "draw_text(x, y, msg, r, g, b)", "Draw Text.",
     "host_api.draw_text(10, 10, 'Hi', 255, 255, 255)"},
    {"spawn_particles", "spawn_particles(x, y, count, r, g, b)",
     "Spawn Particle Effect.",
     "host_api.spawn_particles(50, 50, 10, 255, 200, 50)"},
    {"is_key_down", "is_key_down(key)", "Check key press.",
     "if host_api.is_key_down('up'): ..."},
    {"get_mouse_pos", "get_mouse_pos()", "Get Mouse X,Y.",
     "x, y = host_api.get_mouse_pos()"},
    {"is_mouse_down", "is_mouse_down(btn)", "Check mouse btn (0/1/2).",
     "if host_api.is_mouse_down(0): ..."},
    {"set_do", "set_do(port, val)", "Set Digital Output (0-7).",
     "host_api.set_do(0, True)"},
    {"get_di", "get_di(port)", "Get Digital Input (0-7).",
     "val = host_api.get_di(0)"},
    {"set_reg", "set_reg(id, val)", "Set Internal Register (0-31).",
     "host_api.set_reg(0, 123.4)"},
    {"get_reg", "get_reg(id)", "Get Internal Register (0-31).",
     "val = host_api.get_reg(0)"},
    {"axis_move", "axis_move(id, pos, vel)", "Move Axis (0=X,1=Y,2=Z).",
     "host_api.axis_move(0, 100.0, 5.0)"},
    {"set_paused", "set_paused(bool)", "Pause/Resume System.",
     "host_api.set_paused(True)"},
    {"reset_system", "reset_system()", "Reset All IO/Motion.",
     "host_api.reset_system()"}};

void GuiLayer::SetupStyle() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 5.0f;
  style.FrameRounding = 4.0f;
  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.70f, 0.70f, 0.70f, 0.31f);
  colors[ImGuiCol_Button] = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
}

// ---------------------------------------------------------
// Helper: Load Script Context
// ---------------------------------------------------------
void LoadScriptContent() {
  Model().SetSourceLines({});
  std::ifstream file(Model().GetScriptPath());
  if (file.is_open()) {
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
    Model().SetSourceLines(lines);
    file.close();
  }
}

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

// ---------------------------------------------------------
// 脚本代码生成器 (Script Generator)
// 将可视化的 Block 序列转换为 Python 代码字符串
// ---------------------------------------------------------
void GenerateScript() {
  // 1. 获取所有块 (Visual Sequencing)
  // 编辑器是列表式的，直接使用 Vector 顺序
  auto blocks = Model().GetBlocks();
  // std::sort removed - List order is execution order

  std::string code = "import time\nimport threading\nimport sys\nimport os\n";
  code +=
      "sys.path.append(os.getcwd() + '/../src')\n"; // Hack for finding host_api
  code += "try:\n";
  code += "    import host_api\n";
  code += "except ImportError:\n";
  code += "    # Mock for testing\n";
  code += "    class HostApi:\n";
  code += "        def log_message(self, msg): print(msg)\n";
  code += "        def axis_move(self, a, p, v): print(f'Move {a} {p} {v}')\n";
  code += "        def axis_is_moving(self, a): return False\n";
  code += "        def sleep_ms(self, ms): time.sleep(ms/1000.0)\n";
  code += "        def set_do(self, p, v): print(f'DO {p} {v}')\n";
  code += "        def get_di(self, p): return False\n";
  code += "        def set_reg(self, r, v): pass\n";
  code += "        def get_reg(self, r): return 0.0\n";
  code += "        def get_param(self, n): return 0.0\n";
  code += "        def set_paused(self, p): pass\n";
  code += "    host_api = HostApi()\n\n";

  // Safety Monitor Thread (Example)
  // Safety Configuration
  // Safety Configuration -> def init()
  code += "def init():\n";
  code += "    host_api.log_message('[Sys] Initializing Safety Config...')\n";
  code += "    # Clear previous safety (Important if script is re-run logic, "
          "though AppModel has ResetSafetyConfig)\n";
  // Actually, wait, host_api doesn't expose ResetSafetyConfig.
  // Should we add it? Or rely on overwriting?
  // MapInput overwrites. But to clear unused ones?
  // User asked for init(). Let's assume MapInput is enough or we add
  // reset_safety(). Let's add reset_safety call if we expose it? No, let's just
  // generate configure_input. The user's request implies structure.

  for (const auto &b : blocks) {
    if (b.type == amr::BlockType::CONFIG_SAFETY) {
      int pin = (int)b.param1;
      int action = (int)b.param2;
      int flags = (int)b.param3;
      bool invert = (flags & 1);
      bool edge = (flags & 2);

      std::string line = "    host_api.configure_input(" + std::to_string(pin) +
                         ", " + std::to_string(action) + ", " +
                         (invert ? "True" : "False") + ", " +
                         (edge ? "True" : "False") + ")\n";
      code += line;
    }
  }
  code += "    host_api.log_message('[Sys] Safety Configured.')\n\n";

  code += "def main():\n";
  code += "    host_api.log_message('Starting AMR Logic...')\n";

  // Note: Model access for blocks is mainly UI thread, assume safe for
  // generation
  int indent_level = 1;

  auto GetVal = [](float val, const std::string &ref) -> std::string {
    if (!ref.empty())
      return "host_api.get_param('" + ref + "')";
    char buf[32];
    snprintf(buf, 32, "%.2f", val);
    return std::string(buf);
  };

  // const auto &blocks = Model().GetBlocks(); // Blocks are now sorted
  const auto &mechanisms = Model().GetMechanisms();

  for (const auto &block : blocks) {
    if ((block.type == amr::BlockType::LOOP_END ||
         block.type == amr::BlockType::IF_REG) &&
        indent_level > 1 && block.type == amr::BlockType::LOOP_END)
      indent_level--;

    std::string indent = "";
    for (int i = 0; i < indent_level; ++i)
      indent += "    ";

    switch (block.type) {
    case amr::BlockType::MOVE_AXIS: {
      int mech_id = (int)block.param1;
      int axis = mech_id;
      for (const auto &m : mechanisms) {
        if (m.id == mech_id) {
          axis = m.axis_map;
          break;
        }
      }
      std::string pos_val = GetVal(block.param2, block.param2_ref);
      std::string vel_val = GetVal(block.param3, block.param3_ref);

      if (!block.param3_ref.empty()) {
        code += indent + "if host_api.get_param('" + block.param3_ref +
                "') <= 0:\n";
        code += indent + "    host_api.log_message('Twarn: Vel=0 Check " +
                block.param3_ref + "')\n";
      } else {
        if (block.param3 <= 0)
          code += indent + "host_api.log_message('Twarn: Vel=0 direct')\n";
      }

      code += indent + "host_api.axis_move(" + std::to_string(axis) + ", " +
              pos_val + ", " + vel_val + ")\n";
      code += indent + "while host_api.axis_is_moving(" + std::to_string(axis) +
              "):\n";
      code += indent + "    host_api.sleep_ms(10)\n";
      break;
    }
    case amr::BlockType::WAIT:
      code += indent + "host_api.sleep_ms(" +
              std::to_string((int)block.param1) + ")\n";
      break;
    case amr::BlockType::HOME_AXIS: {
      int mech_id = (int)block.param1;
      int axis = 0;
      for (const auto &m : mechanisms)
        if (m.id == mech_id)
          axis = m.axis_map;
      code += indent + "host_api.log_message('[AMR] Homing Mech " +
              std::to_string(mech_id) + "...')\n";
      code += indent + "host_api.axis_move(" + std::to_string(axis) +
              ", 0.0, 10.0)\n";
      code += indent + "while host_api.axis_is_moving(" + std::to_string(axis) +
              "):\n";
      code += indent + "    host_api.sleep_ms(10)\n";
      break;
    }
    case amr::BlockType::SET_DO:
      code += indent + "host_api.set_do(" + std::to_string((int)block.param1) +
              ", " + (block.param2 > 0 ? "True" : "False") + ")\n";
      break;
    case amr::BlockType::WAIT_DI:
      code += indent + "host_api.log_message('[AMR] Waiting for DI " +
              std::to_string((int)block.param1) + "...')\n";
      code += indent + "while host_api.get_di(" +
              std::to_string((int)block.param1) +
              ") != " + (block.param2 > 0 ? "True" : "False") + ":\n";
      code += indent + "    host_api.sleep_ms(10)\n";
      break;
    case amr::BlockType::SET_REG:
      code += indent + "host_api.set_reg(" + std::to_string((int)block.param1) +
              ", " + GetVal(block.param2, block.param2_ref) + ")\n";
      break;
    case amr::BlockType::MATH_REG:
      code += indent + "val = host_api.get_reg(" +
              std::to_string((int)block.param1) + ")\n";
      code +=
          indent + "val += " + GetVal(block.param2, block.param2_ref) + "\n";
      code += indent + "host_api.set_reg(" + std::to_string((int)block.param1) +
              ", val)\n";
      break;
    case amr::BlockType::IF_REG: {
      int op = (int)block.param3;
      std::string op_str = (op == 1) ? ">" : ((op == 2) ? "<" : "==");
      code += indent + "if host_api.get_reg(" +
              std::to_string((int)block.param1) + ") " + op_str + " " +
              GetVal(block.param2, block.param2_ref) + ":\n";
      indent_level++;
      break;
    }
    case amr::BlockType::LOOP_START:
      code += indent + "for _i in range(" + std::to_string((int)block.param1) +
              "):\n";
      indent_level++;
      break;
    case amr::BlockType::LOOP_END:
      break;
    case amr::BlockType::LOG_MSG:
      code += indent + "host_api.log_message('" + block.str_param + "')\n";
      break;
    default:
      break;
    }
  }

  code += "    host_api.log_message('Program Complete.')\n\n";
  code += "\nif __name__ == '__main__':\n";
  code += "    init()\n"; // Ensure init is called first
  code += "    main()\n";

  std::ofstream out("../scripts/visual_prog.py");
  out << code;
  out.close();
  Model().SetScriptPath("../scripts/visual_prog.py");
  LoadScriptContent();
}

// ---------------------------------------------------------
// UI: Hardware Config Tab
// ---------------------------------------------------------
// ---------------------------------------------------------
// UI: Hardware Config Tab
// ---------------------------------------------------------
void DrawHardwareConfig() {
  ImGui::Text("AMR HARDWARE CONFIGURATION");

  ImGui::Columns(2, "HwCols");

  // Mechanisms
  ImGui::BeginChild("MechList", ImVec2(0, 300), true);
  ImGui::Text("Mechanisms (Actuators)");
  ImGui::Separator();

  static char new_mech_name[32] = "NewMech";
  static int new_mech_axis = 0;
  ImGui::InputText("Name", new_mech_name, 32);
  ImGui::Combo("Map Axis", &new_mech_axis, "X-Axis\0Y-Axis\0Z-Axis\0\0");

  if (ImGui::Button("Add Mechanism")) {
    // Direct mutable access to vector - no lock if we are the only writer in UI
    // AppModel doesn't expose internal lock unless we wrap this into
    // "AddMechanism".
    auto &mechs = Model().GetMechanisms();
    amr::Mechanism m;
    m.id = mechs.size();
    m.name = new_mech_name;
    m.axis_map = new_mech_axis;
    mechs.push_back(m);
  }

  ImGui::Separator();
  {
    auto &mechs = Model().GetMechanisms();
    for (int i = 0; i < mechs.size(); ++i) {
      ImGui::Text("%d: %s (Axis %d)", mechs[i].id, mechs[i].name.c_str(),
                  mechs[i].axis_map);
      ImGui::SameLine();
      if (ImGui::Button(("Del##" + std::to_string(i)).c_str())) {
        mechs.erase(mechs.begin() + i);
        i--;
      }
    }
  }
  ImGui::EndChild();

  ImGui::NextColumn();

  // Global Params
  ImGui::BeginChild("ParamList", ImVec2(0, 300), true);
  ImGui::Text("Global Parameters");
  ImGui::Separator();

  static char new_param_name[32] = "ParamName";
  static float new_param_val = 0.0f;
  ImGui::InputText("Name##P", new_param_name, 32);
  ImGui::DragFloat("Value##P", &new_param_val);
  if (ImGui::Button("Add Param")) {
    auto &params = Model().GetGlobalParams();
    amr::GlobalParam p;
    p.name = new_param_name;
    p.value = new_param_val;
    params.push_back(p);
  }

  ImGui::Separator();
  {
    auto &params = Model().GetGlobalParams();
    for (int i = 0; i < params.size(); ++i) {
      ImGui::PushID(i);
      ImGui::Text("%s", params[i].name.c_str());
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      ImGui::DragFloat("##Val", &params[i].value);
      ImGui::PopItemWidth();
      ImGui::SameLine();
      if (ImGui::Button("X")) {
        params.erase(params.begin() + i);
        i--;
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  ImGui::Columns(1);
}

// ---------------------------------------------------------
// Save / Load / Config Helpers
// ---------------------------------------------------------
// ---------------------------------------------------------
// Save / Load / Config Helpers
// ---------------------------------------------------------
static char save_filename[64] = "amr_demo.txt";

void SaveProject() {
  std::ofstream out("../scripts/" + std::string(save_filename));
  if (!out.is_open())
    return;

  // Save Mechanisms
  out << "[MECHANISMS]\n";
  {
    const auto &mechs = Model().GetMechanisms();
    for (const auto &m : mechs) {
      out << m.id << "," << m.name << "," << m.axis_map << "\n";
    }
  }

  // Save Params
  out << "[PARAMS]\n";
  {
    const auto &params = Model().GetGlobalParams();
    for (const auto &p : params) {
      out << p.name << "," << p.value << "\n";
    }
  }

  // Save Blocks
  out << "[BLOCKS]\n";
  {
    const auto &blocks = Model().GetBlocks();
    for (const auto &b : blocks) {
      std::string r2 = b.param2_ref.empty() ? "_" : b.param2_ref;
      std::string r3 = b.param3_ref.empty() ? "_" : b.param3_ref;
      std::string sp = b.str_param.empty() ? "_" : b.str_param;
      out << (int)b.type << "," << b.param1 << "," << b.param2 << ","
          << b.param3 << "," << r2 << "," << r3 << "," << sp << "\n";
    }
  }
  out.close();
  Model().LogMessage("[Sys] Project Saved to scripts/" +
                     std::string(save_filename) + "\n");
}

// ---------------------------------------------------------
// Helper: Apply Safety Config from Blocks (Eager Load) - REMOVED
// User requested script-driven config only.
// ---------------------------------------------------------

void LoadProject() {
  std::ifstream in("../scripts/" + std::string(save_filename));
  if (!in.is_open()) {
    Model().LogMessage("[Sys] Error: Could not open scripts/" +
                       std::string(save_filename) + "\n");
    return;
  }

  // Clear Model
  Model().GetMechanisms().clear();
  Model().GetGlobalParams().clear();
  Model().GetBlocks().clear();
  Model().SetNextBlockId(0);

  std::string line;
  int mode = 0; // 0=None, 1=Mech, 2=Param, 3=Block

  while (std::getline(in, line)) {
    if (line.empty())
      continue;
    if (line == "[MECHANISMS]") {
      mode = 1;
      continue;
    }
    if (line == "[PARAMS]") {
      mode = 2;
      continue;
    }
    if (line == "[BLOCKS]") {
      mode = 3;
      continue;
    }

    std::stringstream ss(line);
    std::string segment;
    std::vector<std::string> seglist;
    while (std::getline(ss, segment, ','))
      seglist.push_back(segment);

    if (mode == 1 && seglist.size() >= 3) {
      amr::Mechanism m;
      m.id = std::stoi(seglist[0]);
      m.name = seglist[1];
      m.axis_map = std::stoi(seglist[2]);
      Model().GetMechanisms().push_back(m);
    } else if (mode == 2 && seglist.size() >= 2) {
      amr::GlobalParam p;
      p.name = seglist[0];
      p.value = std::stof(seglist[1]);
      Model().GetGlobalParams().push_back(p);
    } else if (mode == 3 && seglist.size() >= 7) {
      amr::VisualBlock b;
      b.id = Model().AllocateBlockId();
      b.type = (amr::BlockType)std::stoi(seglist[0]);
      b.param1 = std::stof(seglist[1]);
      b.param2 = std::stof(seglist[2]);
      b.param3 = std::stof(seglist[3]);
      b.param2_ref = (seglist[4] == "_") ? "" : seglist[4];
      b.param3_ref = (seglist[5] == "_") ? "" : seglist[5];
      b.str_param = (seglist[6] == "_") ? "" : seglist[6];
      Model().GetBlocks().push_back(b);
    }
  }
  // ApplySafetyFromBlocks(); // Removed call
}

// ---------------------------------------------------------
// Visual Editor Update
// ---------------------------------------------------------
// ---------------------------------------------------------
// Combined Editor & Config
// ---------------------------------------------------------
// ---------------------------------------------------------
// Visual Editor Update
// ---------------------------------------------------------
// ---------------------------------------------------------
// Combined Editor & Config
// ---------------------------------------------------------
void DrawEditorAndConfig() {
  ImGui::Columns(3, "EdCols");

  // COL 1: Palette & Mechanisms
  // ---------------------------
  ImGui::BeginChild("Col1Up", ImVec2(0, 300), true);
  ImGui::Text("Palette");
  ImGui::Separator();
  auto AddBlock = [&](amr::BlockType t, const char *name, float p1 = 0,
                      float p2 = 0, float p3 = 0) {
    if (ImGui::Button(name, ImVec2(130, 30))) {
      amr::VisualBlock b;
      b.id = Model().AllocateBlockId();
      b.type = t;
      b.param1 = p1;
      b.param2 = p2;
      b.param3 = p3;
      Model().GetBlocks().push_back(b);
    }
  };
  ImGui::TextDisabled("Motion");
  AddBlock(amr::BlockType::MOVE_AXIS, "+ Control Mech");
  AddBlock(amr::BlockType::WAIT, "+ Wait (ms)");
  AddBlock(amr::BlockType::HOME_AXIS, "+ Home Mech");

  ImGui::Separator();
  ImGui::TextDisabled("I/O");
  AddBlock(amr::BlockType::SET_DO, "+ Set DO");
  AddBlock(amr::BlockType::WAIT_DI, "+ Wait DI");
  AddBlock(amr::BlockType::CONFIG_SAFETY, "+ Safety Cfg");

  ImGui::Separator();
  ImGui::TextDisabled("Logic");
  AddBlock(amr::BlockType::SET_REG, "+ Set Reg");
  AddBlock(amr::BlockType::MATH_REG, "+ Math (+=)");
  AddBlock(amr::BlockType::IF_REG, "+ If Reg");
  AddBlock(amr::BlockType::LOOP_START, "+ Loop Start");
  AddBlock(amr::BlockType::LOOP_END, "+ Loop End");
  AddBlock(amr::BlockType::LOG_MSG, "+ Log Msg");
  ImGui::EndChild();

  ImGui::BeginChild("Col1Down", ImVec2(0, 0), true);
  ImGui::Text("Mechanisms");
  ImGui::Separator();
  static char new_mech_name[32] = "NewMech";
  static int new_mech_axis = 0;
  ImGui::PushItemWidth(100);
  ImGui::InputText("##MName", new_mech_name, 32);
  ImGui::Combo("##MAxis", &new_mech_axis, "X-Axis\0Y-Axis\0Z-Axis\0\0");
  ImGui::PopItemWidth();
  if (ImGui::Button("Add Mech")) {
    auto &mechs = Model().GetMechanisms();
    amr::Mechanism m;
    m.id = mechs.size();
    m.name = new_mech_name;
    m.axis_map = new_mech_axis;
    mechs.push_back(m);
  }
  ImGui::Separator();
  {
    auto &mechs = Model().GetMechanisms();
    for (int i = 0; i < mechs.size(); ++i) {
      ImGui::Text("%d: %s (Ax%d)", mechs[i].id, mechs[i].name.c_str(),
                  mechs[i].axis_map);
      ImGui::SameLine();
      if (ImGui::Button(("X##M" + std::to_string(i)).c_str())) {
        mechs.erase(mechs.begin() + i);
        i--;
      }
    }
  }
  ImGui::EndChild();
  ImGui::NextColumn();

  // COL 2: Workspace
  // ----------------
  ImGui::BeginChild("Workspace", ImVec2(0, 0), true);
  ImGui::Text("Program Flow");
  ImGui::Separator();
  {
    auto &blocks = Model().GetBlocks();
    auto &mechs = Model().GetMechanisms();

    for (auto &b : blocks) {
      ImGui::PushID(b.id);
      ImGui::BeginGroup();
      ImGui::Button("::");
      ImGui::SameLine();
      if (b.type == amr::BlockType::MOVE_AXIS) {
        ImGui::Text("CONTROL");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##Mech", "Select...")) {
          for (auto &m : mechs) {
            bool sel = ((int)b.param1 == m.id);
            if (ImGui::Selectable(m.name.c_str(), sel))
              b.param1 = (float)m.id;
          }
          ImGui::EndCombo();
        }
        if (!mechs.empty() && (int)b.param1 < mechs.size()) {
          ImGui::SameLine();
          ImGui::Text("(%s)", mechs[(int)b.param1].name.c_str());
        }
        ImGui::SameLine();
        ImGui::Text("Pos");
        ImGui::SameLine();
        ImGui::PushItemWidth(60);
        ImGui::DragFloat("##P", &b.param2);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Text("Vel");
        ImGui::SameLine();
        ImGui::PushItemWidth(60);
        ImGui::DragFloat("##V", &b.param3);
        ImGui::PopItemWidth();
      } else if (b.type == amr::BlockType::WAIT) {
        ImGui::Text("WAIT (ms)");
        ImGui::SameLine();
        ImGui::PushItemWidth(80);
        ImGui::DragFloat("##W", &b.param1);
        ImGui::PopItemWidth();
      } else if (b.type == amr::BlockType::LOG_MSG) {
        ImGui::Text("LOG");
        ImGui::SameLine();
        char buf[64];
        strncpy(buf, b.str_param.c_str(), 64);
        ImGui::PushItemWidth(120);
        if (ImGui::InputText("##MSG", buf, 64))
          b.str_param = buf;
        ImGui::PopItemWidth();
      } else if (b.type == amr::BlockType::HOME_AXIS) {
        ImGui::Text("HOME MECH");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##HM", "Select...")) {
          for (auto &m : mechs)
            if (ImGui::Selectable(m.name.c_str(), (int)b.param1 == m.id))
              b.param1 = (float)m.id;
          ImGui::EndCombo();
        }
        if ((int)b.param1 < mechs.size()) {
          ImGui::SameLine();
          ImGui::Text("(%s)", mechs[(int)b.param1].name.c_str());
        }
      } else if (b.type == amr::BlockType::SET_DO) {
        ImGui::Text("SET DO");
        ImGui::SameLine();
        int p = (int)b.param1;
        ImGui::PushItemWidth(70);
        if (ImGui::Combo("##P", &p,
                         "DO-0\0DO-1\0DO-2\0DO-3\0DO-4\0DO-5\0DO-6\0DO-7\0\0"))
          b.param1 = (float)p;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        bool v = (b.param2 > 0);
        if (ImGui::Checkbox("##V", &v))
          b.param2 = v ? 1.0f : 0.0f;
      } else if (b.type == amr::BlockType::WAIT_DI) {
        ImGui::Text("WAIT DI");
        ImGui::SameLine();
        int p = (int)b.param1;
        ImGui::PushItemWidth(70);
        if (ImGui::Combo("##P", &p,
                         "DI-0\0DI-1\0DI-2\0DI-3\0DI-4\0DI-5\0DI-6\0DI-7\0\0"))
          b.param1 = (float)p;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        bool v = (b.param2 > 0);
        if (ImGui::Checkbox("##V", &v))
          b.param2 = v ? 1.0f : 0.0f;
      } else if (b.type == amr::BlockType::SET_REG) {
        ImGui::Text("SET R");
        ImGui::SameLine();
        int r = (int)b.param1;
        ImGui::PushItemWidth(40);
        if (ImGui::Combo("##R", &r, "R0\0R1\0R2\0R3\0R4\0R5\0R6\0R7\0\0"))
          b.param1 = (float)r;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Text("=");
        ImGui::SameLine();
        ImGui::PushItemWidth(60);
        ImGui::DragFloat("##V", &b.param2);
        ImGui::PopItemWidth();
      } else if (b.type == amr::BlockType::MATH_REG) {
        ImGui::Text("MATH R");
        ImGui::SameLine();
        int r = (int)b.param1;
        ImGui::PushItemWidth(40);
        if (ImGui::Combo("##R", &r, "R0\0R1\0R2\0R3\0R4\0R5\0R6\0R7\0\0"))
          b.param1 = (float)r;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Text("+=");
        ImGui::SameLine();
        ImGui::PushItemWidth(60);
        ImGui::DragFloat("##MV", &b.param2);
        ImGui::PopItemWidth();
      } else if (b.type == amr::BlockType::IF_REG) {
        ImGui::Text("IF R");
        ImGui::SameLine();
        int r = (int)b.param1;
        ImGui::PushItemWidth(40);
        if (ImGui::Combo("##R", &r, "R0\0R1\0R2\0R3\0R4\0R5\0R6\0R7\0\0"))
          b.param1 = (float)r;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        int op = (int)b.param3;
        ImGui::PushItemWidth(40);
        if (ImGui::Combo("##OP", &op, "==\0>\0<\0\0"))
          b.param3 = (float)op;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(60);
        ImGui::DragFloat("##IV", &b.param2);
        ImGui::PopItemWidth();
      } else if (b.type == amr::BlockType::LOOP_START) {
        ImGui::Text("LOOP START");
        ImGui::SameLine();
        ImGui::Text("Count:");
        ImGui::SameLine();
        ImGui::PushItemWidth(50);
        ImGui::DragFloat("##C", &b.param1, 1, 1, 100, "%.0f");
        ImGui::PopItemWidth();
      } else if (b.type == amr::BlockType::LOOP_END) {
        ImGui::Text("LOOP END");
      } else if (b.type == amr::BlockType::CONFIG_SAFETY) {
        ImGui::Text("SAFETY CFG");
        ImGui::SameLine();

        // Pin Selection
        int p = (int)b.param1;
        ImGui::PushItemWidth(50);
        if (ImGui::Combo("##P", &p,
                         "DI0\0DI1\0DI2\0DI3\0DI4\0DI5\0DI6\0DI7\0\0")) {
          b.param1 = (float)p;
          // ApplySafetyFromBlocks(); // Removed
        }
        ImGui::PopItemWidth();

        // Action Selection (None, EStop, PauseTog, Home)
        int action = (int)b.param2;
        ImGui::PushItemWidth(80);
        if (ImGui::Combo("##ACT", &action, "None\0E-Stop\0Pause\0Home\0\0")) {
          b.param2 = (float)action;
          // ApplySafetyFromBlocks(); // Removed
        }
        ImGui::PopItemWidth();

        // Flags: Invert & Edge
        bool invert = (b.param3 > 0); // Stored as 1.0 or 0.0 in param3 (needs
                                      // workaround for 4th param)
        // Wait, VisualBlock only has param1, param2, param3. We need 4 values:
        // Pin, Action, Invert, Edge. Let's pack Invert & Edge into param3.
        // param3 = invert + edge*2.

        int flags = (int)b.param3;
        bool inv = (flags & 1);
        bool edge = (flags & 2);

        if (ImGui::Checkbox("Inv", &inv)) {
          flags = (inv ? 1 : 0) | (edge ? 2 : 0);
          b.param3 = (float)flags;
          // ApplySafetyFromBlocks(); // Removed
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Edge", &edge)) {
          flags = (inv ? 1 : 0) | (edge ? 2 : 0);
          b.param3 = (float)flags;
          // ApplySafetyFromBlocks(); // Removed
        }
      } else {
        ImGui::Text("Block %d (Type %d)", b.id, (int)b.type);
      }
      ImGui::SameLine();
      if (ImGui::Button("X")) {
        b.type = (amr::BlockType)-1;
      }
      ImGui::EndGroup();
      ImGui::PopID();
    }
    // Cleanup deleted blocks
    for (int i = 0; i < blocks.size(); ++i) {
      if ((int)blocks[i].type == -1) {
        blocks.erase(blocks.begin() + i);
        i--;
      }
    }
  }
  ImGui::EndChild();
  ImGui::NextColumn();

  // COL 3: Params & CodeGen
  // ----------------------
  ImGui::BeginChild("Col3Up", ImVec2(0, 300), true);
  ImGui::Text("Global Params");
  ImGui::Separator();
  static char new_param_name[32] = "ParamName";
  static float new_param_val = 0.0f;
  ImGui::PushItemWidth(80);
  ImGui::InputText("##PN", new_param_name, 32);
  ImGui::SameLine();
  ImGui::DragFloat("##PV", &new_param_val);
  ImGui::PopItemWidth();
  if (ImGui::Button("Add Param")) {
    auto &params = Model().GetGlobalParams();
    amr::GlobalParam p;
    p.name = new_param_name;
    p.value = new_param_val;
    params.push_back(p);
  }
  ImGui::Separator();
  {
    auto &params = Model().GetGlobalParams();
    for (int i = 0; i < params.size(); ++i) {
      ImGui::Text("%s:", params[i].name.c_str());
      ImGui::SameLine();
      ImGui::PushItemWidth(60);
      ImGui::DragFloat(("##V" + std::to_string(i)).c_str(), &params[i].value);
      ImGui::PopItemWidth();
      ImGui::SameLine();
      if (ImGui::Button(("X##P" + std::to_string(i)).c_str())) {
        params.erase(params.begin() + i);
        i--;
      }
    }
  }
  ImGui::EndChild();

  ImGui::BeginChild("Col3Down", ImVec2(0, 0), true);
  ImGui::Text("Operations");
  ImGui::Separator();
  ImGui::Text("Project File:");
  ImGui::PushItemWidth(120);
  ImGui::InputText("##Fname", save_filename, 64);
  ImGui::PopItemWidth();

  if (ImGui::Button("SAVE PROJ"))
    SaveProject();
  ImGui::SameLine();
  if (ImGui::Button("LOAD DEMO"))
    LoadProject();

  ImGui::Separator();

  if (ImGui::Button("GENERATE SCRIPT", ImVec2(-1, 40))) {
    GenerateScript();
    Model().LogMessage("[Sys] Script Generated. Go to Monitor tab to Run.\n");
  }
  ImGui::EndChild();
  ImGui::Columns(1);
}

// ---------------------------------------------------------
// Helpers (Machine, IO, Canvas, Control) - Restored & Ordered
// ---------------------------------------------------------

#include <chrono>
#include <thread>

using namespace amr;

void DrawMachine(ImDrawList *draw_list, ImVec2 p, ImVec2 size) {
  // Dynamic Scaling
  float margin = 20.0f;
  float avail_h = size.y - margin * 2;
  float avail_w = size.x - margin * 2;

  // 比例因子。参考高度 = 300px
  float scale = avail_h / 300.0f;
  if (scale > 1.0f)
    scale = 1.0f;
  if (scale < 0.5f)
    scale = 0.5f;

  float rail_y = p.y + size.y - 50.0f * scale;
  float rail_len = avail_w;
  float start_x = p.x + margin;

  // 绘制 X 轴导轨 (Draw X-Rail)
  draw_list->AddRectFilled(ImVec2(start_x, rail_y),
                           ImVec2(start_x + rail_len, rail_y + 10 * scale),
                           IM_COL32(100, 100, 100, 255));
  draw_list->AddText(ImVec2(start_x, rail_y + 15 * scale),
                     IM_COL32(200, 200, 200, 255), "X-Axis Rail");

  // 计算 X 轴龙门架位置
  float x_pct = Model().GetAxisPos(0) / 300.0f;
  if (x_pct < 0)
    x_pct = 0;
  if (x_pct > 1)
    x_pct = 1;
  float gantry_x = start_x + (x_pct * rail_len);
  float gantry_h = 200.0f * scale;
  float gantry_w = 20.0f * scale;
  draw_list->AddRectFilled(ImVec2(gantry_x - gantry_w / 2, rail_y - gantry_h),
                           ImVec2(gantry_x + gantry_w / 2, rail_y),
                           IM_COL32(200, 150, 50, 255));

  // 计算 Y 轴滑块位置
  float y_pct = Model().GetAxisPos(1) / 200.0f;
  if (y_pct < 0)
    y_pct = 0;
  if (y_pct > 1)
    y_pct = 1;
  float carriage_y = rail_y - (y_pct * gantry_h);
  float carriage_sz = 15.0f * scale;
  draw_list->AddRectFilled(
      ImVec2(gantry_x - 20 * scale, carriage_y - carriage_sz),
      ImVec2(gantry_x + 20 * scale, carriage_y + carriage_sz),
      IM_COL32(50, 150, 255, 255));

  // 计算 Z 轴钻头深度
  float z_pct = Model().GetAxisPos(2) / 50.0f;
  float drill_len = (20.0f + (z_pct * 30.0f)) * scale;
  draw_list->AddRectFilled(
      ImVec2(gantry_x - 5 * scale, carriage_y + carriage_sz),
      ImVec2(gantry_x + 5 * scale, carriage_y + carriage_sz + drill_len),
      IM_COL32(200, 50, 50, 255));

  char buf[64];
  snprintf(buf, 64, "X: %.1f", Model().GetAxisPos(0));
  draw_list->AddText(ImVec2(gantry_x - 15, rail_y + 30 * scale),
                     IM_COL32(255, 255, 255, 255), buf);
  snprintf(buf, 64, "Y: %.1f", Model().GetAxisPos(1));
  draw_list->AddText(ImVec2(gantry_x + 25 * scale, carriage_y - 5),
                     IM_COL32(255, 255, 255, 255), buf);
  snprintf(buf, 64, "Z: %.1f", Model().GetAxisPos(2));
  draw_list->AddText(
      ImVec2(gantry_x + 10 * scale, carriage_y + 15 + drill_len + 5),
      IM_COL32(255, 255, 255, 255), buf);
}

void DrawIOPanel() {
  ImGui::Text("DIGITAL I/O MONITOR");
  ImGui::Separator();

  // Inputs
  ImGui::BeginChild("Inputs", ImVec2(0, 80), true);
  ImGui::Text("Inputs (DI):");
  ImGui::Columns(4, "DICols", false);
  for (int i = 0; i < 8; ++i) {
    bool v = Model().GetDI(i);
    if (ImGui::Checkbox(("DI-" + std::to_string(i)).c_str(), &v)) {
      Model().SetDI(i, v); // Simulate Input Toggle
    }
    ImGui::SameLine();
    ImGui::TextDisabled(v ? "(ON)" : "(OFF)");
    ImGui::NextColumn();
  }
  ImGui::Columns(1);
  ImGui::EndChild();

  // Outputs
  ImGui::BeginChild("Outputs", ImVec2(0, 80), true);
  ImGui::Text("Outputs (DO):");
  ImGui::Columns(4, "DOCols", false);
  for (int i = 0; i < 8; ++i) {
    bool v = Model().GetDO(i);
    ImGui::Text("DO-%d:", i);
    ImGui::SameLine();
    if (v)
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "[ON]");
    else
      ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "[OFF]");
    ImGui::NextColumn();
  }
  ImGui::Columns(1);
  ImGui::EndChild();

  // Registers
  ImGui::BeginChild("Regs", ImVec2(0, 150), true);
  ImGui::Text("Internal Registers (R0-R31):");
  ImGui::Columns(4, "RegCols", true);
  for (int i = 0; i < 32; ++i) {
    ImGui::Text("R%02d: %.2f", i, Model().GetReg(i));
    ImGui::NextColumn();
  }
  ImGui::Columns(1);
  ImGui::EndChild();
}

void DrawTopBar() {
  ImGui::BeginChild("TopBar", ImVec2(0, 50), true);

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
  ImGui::Text("AMR STUDIO");
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::Text(" | Status: %s", Model().IsRunning()
                                   ? (Model().IsPaused() ? "PAUSED" : "RUNNING")
                                   : "STOPPED");
  ImGui::SameLine(300);

  // Controls
  if (!Model().IsRunning()) {
    if (ImGui::Button("RUN SCRIPT", ImVec2(100, 30))) {
      // Need a way to reset termination flag in Model if it's sticky?
      // Model doesn't explicit expose "ResetTermination".
      // Assuming StartWorker manages its state or Model().SetRunning(true).
      // PythonEngine sets is_running currently.
      PythonEngine::StartWorker();
    }
  } else {
    if (!Model().IsPaused()) {
      if (ImGui::Button("PAUSE", ImVec2(80, 30))) {
        Model().SetPaused(true);
      }
    } else {
      if (ImGui::Button("RESUME", ImVec2(80, 30))) {
        Model().SetPaused(false);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("STOP", ImVec2(80, 30))) {
      Model().RequestTermination();
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("RESET SYSTEM", ImVec2(120, 30))) {
    // Reset Logic
    for (int i = 0; i < 8; ++i) {
      Model().SetDI(i, false);
      Model().SetDO(i, false);
    }
    for (int i = 0; i < 3; ++i) {
      Model().SetAxisTarget(i, 0, 0);
      Model().SetAxisCurrentPos(i, 0); // Force position to 0
    }
    Model().SetPaused(false);
    Model().RequestTermination(); // Stop script
    Model().LogMessage("[Sys] System Reset Manually.");
  }

  ImGui::EndChild();
}

void DrawCanvas() {
  ImGui::BeginChild("Canvas", ImVec2(0, 300), true);

  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImVec2 size = ImGui::GetContentRegionAvail();

  draw_list->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                           IM_COL32(20, 20, 25, 255));

  {
    // Draw Machine
    DrawMachine(draw_list, p, size);

    // Draw Queue
    const auto &queue = Model().GetDrawQueue();
    for (const auto &cmd : queue) {
      if (cmd.type == amr::CmdType::RECT)
        draw_list->AddRectFilled(
            ImVec2(p.x + cmd.x, p.y + cmd.y),
            ImVec2(p.x + cmd.x + cmd.w, p.y + cmd.y + cmd.h), cmd.color);
      else if (cmd.type == amr::CmdType::CIRCLE)
        draw_list->AddCircleFilled(ImVec2(p.x + cmd.x, p.y + cmd.y), cmd.r,
                                   cmd.color);
      else if (cmd.type == amr::CmdType::TEXT)
        draw_list->AddText(ImVec2(p.x + cmd.x, p.y + cmd.y), cmd.color,
                           cmd.text.c_str());
    }

    // Particles
    auto &particles = Model().GetParticles();
    for (auto &part : particles) {
      if (part.life > 0)
        draw_list->AddCircleFilled(ImVec2(p.x + part.x, p.y + part.y), 3.0f,
                                   part.color);
    }
  }
  ImGui::EndChild();
}

// ---------------------------------------------------------
// Main Render
// ---------------------------------------------------------
void GuiLayer::Render(void *w) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  ImGui::Begin("AMR Controller Studio", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

  // Physics Simulation Loop (Required for Motion)
  {
    float dt = 1.0f / 60.0f; // Approx 60hz
    Model().UpdatePhysics(dt);
  }

  // 1. Top Bar
  DrawTopBar();

  // 2. 主内容区域 (Main Content Area)
  if (ImGui::BeginTabBar("MainTabs")) {

    // 选项卡：监控与控制 (Monitor & Control)
    if (ImGui::BeginTabItem("Monitor")) {
      ImGui::Columns(2, "MonitorCols");

      // 左侧：仿真画布 (Simulation Canvas)
      ImGui::BeginChild("SimView", ImVec2(0, 400), true);
      DrawCanvas();
      ImGui::EndChild();

      // 左下：IO 面板 (IO Panel)
      DrawIOPanel();

      ImGui::NextColumn();

      // 右侧：源代码查看器 (Source Code Viewer)
      ImGui::Text("Active Script: %s", Model().GetScriptPath().c_str());
      ImGui::BeginChild("CodeViewInd", ImVec2(0, 400), true);
      {
        auto lines = Model().GetSourceLines();
        bool running = Model().IsRunning();
        int cur_line = Model().GetCurrentLine();

        for (int i = 0; i < lines.size(); i++) {
          int line_num = i + 1;
          const std::string &line_content = lines[i];
          if (running && line_num == cur_line) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "> %03d: %s", line_num,
                               line_content.c_str());
          } else {
            ImGui::Text("  %03d: %s", line_num, line_content.c_str());
          }
        }
      }
      ImGui::EndChild();

      // Right Bottom: Logs
      ImGui::Text("System Log");
      ImGui::BeginChild("LogRegion", ImVec2(0, 0), true);
      {
        std::string log = Model().GetLog();
        ImGui::TextWrapped("%s", log.c_str());
        if (log.size() > 0)
          ImGui::SetScrollHereY(1.0f);
      }
      ImGui::EndChild();

      ImGui::Columns(1);
      ImGui::EndTabItem();
    }

    // TAB: EDITOR & CONFIG
    if (ImGui::BeginTabItem("Editor & Config")) {
      DrawEditorAndConfig();
      ImGui::EndTabItem();
    }

    // Removed old separate tabs

    ImGui::EndTabBar();
  }

  ImGui::End();
  ImGui::PopStyleVar();
}
