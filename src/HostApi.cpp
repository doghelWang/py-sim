#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
// Default to no SSL to avoid link errors
#endif

#include "amr/AppModel.hpp"
#include "httplib.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <random>
#include <thread>

namespace py = pybind11;
using namespace amr;

static AppModel &Model() { return AppModel::Instance(); }

// ---------------------------------------------------------
// Helper: System Command Execution
// ---------------------------------------------------------
std::string exec_cmd(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
    return "popen() failed!";
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    result += buffer.data();
  return result;
}

// ---------------------------------------------------------
// API Implementation Functions
// ---------------------------------------------------------

// Utility
void log_message(const std::string &msg) { Model().LogMessage(msg); }

// ----------------------------------------------------------------------
// 系统 API 实现 (System API)
// ----------------------------------------------------------------------

// 毫秒级延时
void sys_sleep_ms(int ms) {
  // 如果是主线程调用（非脚本），直接睡眠
  // 如果是脚本线程，需要检查暂停/终止请求
  auto start = std::chrono::high_resolution_clock::now();
  int elapsed = 0;
  while (elapsed < ms) {
    if (Model().ShouldTerminate()) {
      break;
    }
    // 检查暂停
    Model().WaitForResume();

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    auto now = std::chrono::high_resolution_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
                  .count();
  }
}

void write_file(const std::string &path, const std::string &content) {
  std::ofstream out(path);
  if (out.is_open()) {
    out << content;
  }
}

std::string read_file(const std::string &path) {
  std::ifstream in(path);
  if (in.is_open()) {
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  }
  return "";
}

std::vector<float> get_random_data(int count) {
  std::vector<float> data;
  static std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<float> dis(0.0f, 100.0f);
  for (int i = 0; i < count; ++i)
    data.push_back(dis(gen));
  return data;
}

// --- Automation API ---

std::string http_get(const std::string &host, const std::string &path) {
  httplib::Client cli(host);
  auto res = cli.Get(path);
  if (res && res->status == 200)
    return res->body;
  return "";
}

std::string http_post(const std::string &host, const std::string &path,
                      const std::string &body) {
  httplib::Client cli(host);
  auto res = cli.Post(path, body, "text/plain");
  if (res && res->status == 200)
    return res->body;
  return "";
}

std::string run_cmd(const std::string &cmd) { return exec_cmd(cmd.c_str()); }

void take_screenshot(const std::string &filename) {
  Model().RequestScreenshot(filename);
  while (Model().IsScreenshotRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

// --- Graphics API ---
void draw_rect(float x, float y, float w, float h, int r, int g, int b) {
  unsigned int col = (255 << 24) | (b << 16) | (g << 8) | r;
  Model().PushDrawCmd({amr::CmdType::RECT, x, y, w, h, 0, col});
}
void draw_circle(float x, float y, float radius, int r, int g, int b) {
  unsigned int col = (255 << 24) | (b << 16) | (g << 8) | r;
  Model().PushDrawCmd({amr::CmdType::CIRCLE, x, y, 0, 0, radius, col});
}
void draw_text(float x, float y, const std::string &text, int r, int g, int b) {
  unsigned int col = (255 << 24) | (b << 16) | (g << 8) | r;
  Model().PushDrawCmd({amr::CmdType::TEXT, x, y, 0, 0, 0, col, text});
}
void clear_screen() { Model().ClearDrawQueue(); }
bool is_key_down(std::string key) {
  // Normalize to uppercase
  std::transform(key.begin(), key.end(), key.begin(), ::toupper);
  return Model().GetInputSticky(key);
}
std::tuple<float, float> get_mouse_pos() {
  auto p = Model().GetMousePos();
  return std::make_tuple(p.first, p.second);
}
bool is_mouse_down(int b) { return Model().IsMouseDown(b); }
void spawn_particles(float x, float y, int count, int r, int g, int b) {
  unsigned int col = (255 << 24) | (b << 16) | (g << 8) | r;
  Model().SpawnParticles(x, y, count, col);
}
void screen_shake(float i) { Model().SetShakeTimer(i); }

// --- Motion Control API ---

void axis_move(int axis, float pos, float vel) {
  if (vel <= 0.0001f) {
    Model().LogMessage(
        "[Error] axis_move: 速度为0，请检查参数! Motion ignored.\n");
    return;
  }
  Model().AxisMove(axis, pos, vel);
}

float axis_get_pos(int axis) { return Model().GetAxisPos(axis); }

bool axis_is_moving(int axis) { return Model().IsAxisMoving(axis); }

// --- I/O API ---

void set_do(int port, bool val) { Model().SetDO(port, val); }

bool get_di(int port) { return Model().GetDI(port); }

bool get_do(int port) { return Model().GetDO(port); }

// --- Register API --
void set_reg(int id, float val) { Model().SetReg(id, val); }

float get_reg(int id) { return Model().GetReg(id); }

float get_param(const std::string &name) { return Model().GetParam(name); }

void set_paused(bool paused) {
  Model().SetPaused(paused);
  Model().LogMessage(paused ? "[DEBUG] set_paused(True)\n"
                            : "[DEBUG] set_paused(False)\n");
}

// Module
PYBIND11_EMBEDDED_MODULE(host_api, m) {
  m.def("log_message", &log_message);
  m.def("sleep_ms", &sys_sleep_ms);
  // m.def("compute_prime", &compute_prime); // Removed
  m.def("write_file", &write_file);
  m.def("read_file", &read_file);
  m.def("get_random_data", &get_random_data);

  m.def("draw_rect", &draw_rect);
  m.def("draw_circle", &draw_circle);
  m.def("draw_text", &draw_text);
  m.def("clear_screen", &clear_screen);
  m.def("spawn_particles", &spawn_particles);
  m.def("screen_shake", &screen_shake);
  m.def("is_key_down", &is_key_down);
  m.def("get_mouse_pos", &get_mouse_pos);
  m.def("is_mouse_down", &is_mouse_down);

  // Automation
  m.def("http_get", &http_get, "Get(host, path)");
  m.def("http_post", &http_post, "Post(host, path, body)");
  m.def("exec_cmd", &run_cmd, "Run shell command");
  m.def("take_screenshot", &take_screenshot, "Capture screen to file");

  // Motion
  m.def("axis_move", &axis_move, "Move Axis (0-2) to Pos at Vel");
  m.def("axis_get_pos", &axis_get_pos, "Get Axis (0-2) Position");
  m.def("axis_is_moving", &axis_is_moving, "Check if Axis is moving");

  // I/O
  m.def("set_do", &set_do, "Set Digital Output (0-7)");
  m.def("get_di", &get_di, "Get Digital Input (0-7)");
  m.def("get_do", &get_do, "Get Digital Output (0-7)");

  // Registers
  m.def("set_reg", &set_reg, "Set Register (0-31)");
  m.def("get_reg", &get_reg, "Get Register (0-31)");

  // AMR Config
  m.def("get_param", &get_param, "Get Global Param value by Name");

  // System Control
  m.def("set_paused", &set_paused, "Pause/Resume System");

  // Safety Config
  m.def(
      "configure_input",
      [](int pin, int action, bool invert, bool edge) {
        // Map int to InputAction
        auto act = static_cast<amr::AppModel::InputAction>(action);
        Model().MapInput(pin, act, invert, edge);
        Model().LogMessage("[Sys] Input Mapped: Pin " + std::to_string(pin) +
                           " -> Action " + std::to_string(action));
      },
      "Configure DI Pin: pin, action(0=None,1=EStop,2=PauseTog,3=Home), "
      "invert, edge");

  // Hardcoded Logic for Demo Safety Monitor
  m.def("demo_safety_check", []() {
    // Legacy: This logic has been moved to AppModel::UpdateSafetyLogic()
  });

  m.def("reset_system", []() {
    // Clear IO
    for (int i = 0; i < 8; ++i) {
      Model().SetDO(i, false);
      // Model().SetDI(i, false); // DI is input, not set by system
    }
    // Zero Axes
    for (int i = 0; i < 3; ++i) {
      Model().AxisMove(i, 0,
                       0);             // Set target to 0, velocity 0 to stop
      Model().SetAxisCurrentPos(i, 0); // Reset current position
    }
    // Resume if paused
    Model().SetPaused(false);
    Model().LogMessage("[Sys] System Reset.\n");
  });
}
