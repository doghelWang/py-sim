#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
// Default to no SSL to avoid link errors
#endif

#include "AppState.hpp"
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

void log_message(const std::string &msg) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  g_app.console_log += "[PYTHON] " + msg + "\n";
  if (g_app.console_log.size() > 5000)
    g_app.console_log =
        g_app.console_log.substr(g_app.console_log.size() - 4000);
}
// Utility
void sleep_ms(int ms) {
  if (ms <= 0)
    return;
  py::gil_scoped_release release;

  auto start = std::chrono::steady_clock::now();
  auto end = start + std::chrono::milliseconds(ms);

  while (std::chrono::steady_clock::now() < end) {
    if (g_app.should_terminate.load())
      return;

    {
      std::unique_lock<std::mutex> lock(g_app.mtx);
      if (g_app.is_paused) {
        g_app.console_log += "[DEBUG] sleep_ms: Entered Pause State.\n";
        auto pause_start = std::chrono::steady_clock::now();
        g_app.cv.wait(
            lock, [] { return !g_app.is_paused || g_app.should_terminate; });
        auto pause_end = std::chrono::steady_clock::now();
        g_app.console_log += "[DEBUG] sleep_ms: Resumed from Pause.\n";
        // Extend end time by duration paused
        end += (pause_end - pause_start);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

void set_paused(bool paused) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  g_app.console_log +=
      (paused ? "[DEBUG] set_paused(True)\n" : "[DEBUG] set_paused(False)\n");
  g_app.is_paused = paused;
  if (!paused)
    g_app.cv.notify_all();
}

int compute_prime(int n) {
  if (n < 2)
    return 0;
  int current = 2, count = 0;
  while (count < n) {
    bool is_p = true;
    for (int i = 2; i * i <= current; ++i)
      if (current % i == 0) {
        is_p = false;
        break;
      }
    if (is_p)
      count++;
    current++;
  }
  return current - 1;
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
  {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    g_app.screenshot_filename = filename;
    g_app.screenshot_requested = true;
  }
  while (true) {
    {
      std::lock_guard<std::mutex> lock(g_app.mtx);
      if (!g_app.screenshot_requested)
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

// --- Graphics API ---
void draw_rect(float x, float y, float w, float h, int r, int g, int b) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  unsigned int col = (255 << 24) | (b << 16) | (g << 8) | r;
  g_app.draw_queue.push_back({CmdType::RECT, x, y, w, h, 0, col});
}
void draw_circle(float x, float y, float radius, int r, int g, int b) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  unsigned int col = (255 << 24) | (b << 16) | (g << 8) | r;
  g_app.draw_queue.push_back({CmdType::CIRCLE, x, y, 0, 0, radius, col});
}
void draw_text(float x, float y, const std::string &text, int r, int g, int b) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  unsigned int col = (255 << 24) | (b << 16) | (g << 8) | r;
  g_app.draw_queue.push_back({CmdType::TEXT, x, y, 0, 0, 0, col, text});
}
void clear_screen() {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  g_app.draw_queue.clear();
}
bool is_key_down(const std::string &key) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  if (g_app.input_sticky.count(key)) {
    bool v = g_app.input_sticky[key];
    g_app.input_sticky[key] = false;
    return v;
  }
  return false;
}
std::tuple<float, float> get_mouse_pos() {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return std::make_tuple(g_app.mouse_x, g_app.mouse_y);
}
bool is_mouse_down(int b) {
  if (b < 0 || b > 2)
    return false;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return g_app.mouse_down[b];
}
void spawn_particles(float x, float y, int count, int r, int g, int b) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  unsigned int col = (255 << 24) | (b << 16) | (g << 8) | r;
  for (int i = 0; i < count; ++i) {
    Particle p;
    p.x = x;
    p.y = y;
    p.vx = ((rand() % 100) / 10.0f) - 5.0f;
    p.vy = ((rand() % 100) / 10.0f) - 5.0f;
    p.life = 1.0f;
    p.color = col;
    g_app.particles.push_back(p);
  }
}
void screen_shake(float i) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  g_app.shake_timer = i;
}

// --- Motion Control API ---

void axis_move(int axis, float pos, float vel) {
  if (axis < 0 || axis > 2)
    return;
  std::lock_guard<std::mutex> lock(g_app.mtx);

  if (vel <= 0.0001f) {
    g_app.console_log +=
        "[Error] axis_move: 速度为0，请检查参数! Motion ignored.\n";
    return;
  }

  g_app.axes[axis].target_pos = pos;
  g_app.axes[axis].max_vel = vel;
  g_app.axes[axis].is_moving = true;
}

float axis_get_pos(int axis) {
  if (axis < 0 || axis > 2)
    return 0.0f;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return g_app.axes[axis].current_pos;
}

bool axis_is_moving(int axis) {
  if (axis < 0 || axis > 2)
    return false;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return g_app.axes[axis].is_moving;
}

// --- I/O API ---

void set_do(int port, bool val) {
  if (port < 0 || port > 7)
    return;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  g_app.digital_outputs[port] = val;
}

bool get_di(int port) {
  if (port < 0 || port > 7)
    return false;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return g_app.digital_inputs[port];
}

bool get_do(int port) {
  if (port < 0 || port > 7)
    return false;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return g_app.digital_outputs[port];
}

// --- Register API --
void set_reg(int id, float val) {
  if (id < 0 || id > 31)
    return;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  g_app.registers[id] = val;
}

float get_reg(int id) {
  if (id < 0 || id > 31)
    return 0.0f;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return g_app.registers[id];
}

float get_param(const std::string &name) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  for (const auto &p : g_app.global_params) {
    if (p.name == name)
      return p.value;
  }
  return 0.0f; // Not found fallback
}

// Module
PYBIND11_EMBEDDED_MODULE(host_api, m) {
  m.def("log_message", &log_message);
  m.def("sleep_ms", &sleep_ms);
  m.def("compute_prime", &compute_prime);
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

  // Hardcoded Logic for Demo Safety Monitor
  m.def("demo_safety_check", []() {
    // Logic:
    // DI-6 -> Pause (Hold to Pause). Rising Edge=Pause, Falling Edge=Resume.
    // DI-7 -> Home (Vel=2)
    py::gil_scoped_release release;
    std::lock_guard<std::mutex> lock(g_app.mtx);

    // DI-6 Logic
    static bool di6_was_high = false;
    bool di6_now = g_app.digital_inputs[6];

    if (di6_now && !di6_was_high) {
      // Rising Edge -> Pause
      g_app.is_paused = true;
      g_app.console_log += "[Sys] DI-6 Active -> Pausing...\n";
    } else if (!di6_now && di6_was_high) {
      // Falling Edge -> Resume
      g_app.is_paused = false;
      g_app.cv.notify_all();
      g_app.console_log += "[Sys] DI-6 Released -> Resuming...\n";
    }
    di6_was_high = di6_now;

    // DI-7: Home All
    if (g_app.digital_inputs[7]) {
      // Move all axes to 0 at vel 2
      for (int i = 0; i < 2; ++i) { // Demo only uses 2 axes
        g_app.axes[i].target_pos = 0;
        g_app.axes[i].max_vel = 2.0f;
        g_app.axes[i].is_moving = true;
      }
    }
  });

  m.def("reset_system", []() {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    // Clear IO
    for (int i = 0; i < 8; ++i) {
      g_app.digital_outputs[i] = false;
      g_app.digital_inputs[i] = false;
    }
    // Zero Axes
    for (int i = 0; i < 3; ++i) {
      g_app.axes[i].target_pos = 0;
      g_app.axes[i].current_pos = 0;
      g_app.axes[i].is_moving = false;
    }
    // Resume if paused
    g_app.is_paused = false;
    g_app.cv.notify_all();
    g_app.console_log += "[Sys] System Reset.\n";
  });
}
