#include "AppState.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <random>
#include <thread>

namespace py = pybind11;

// ---------------------------------------------------------
// API Implementation Functions
// ---------------------------------------------------------

void log_message(const std::string &msg) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  g_app.console_log += "[PYTHON] " + msg + "\n";
}

void sleep_ms(int ms) {
  if (ms > 0)
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int compute_prime(int n) {
  if (n < 2)
    return 0;
  int count = 0;
  int current = 2;
  while (count < n) {
    bool is_prime = true;
    for (int i = 2; i <= std::sqrt(current); ++i) {
      if (current % i == 0) {
        is_prime = false;
        break;
      }
    }
    if (is_prime)
      count++;
    current++;
  }
  return current - 1;
}

void write_file(const std::string &path, const std::string &content) {
  std::ofstream out(path);
  if (out.is_open()) {
    out << content;
    out.close();
    log_message("File written: " + path);
  } else {
    log_message("Error writing file: " + path);
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
  for (int i = 0; i < count; ++i) {
    data.push_back(dis(gen));
  }
  return data;
}

// --- Graphics API ---

void draw_rect(float x, float y, float w, float h, int r, int g, int b) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  // Convert to uint 0xAABBGGRR
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
  DrawCmd cmd;
  cmd.type = CmdType::TEXT;
  cmd.x = x;
  cmd.y = y;
  cmd.color = col;
  cmd.text = text;
  g_app.draw_queue.push_back(cmd);
}

void clear_screen() {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  g_app.draw_queue.clear();
}

bool is_key_down(const std::string &key) {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  if (g_app.input_sticky.count(key)) {
    bool pressed = g_app.input_sticky[key];
    g_app.input_sticky[key] = false; // Consume
    return pressed;
  }
  return false;
}

std::tuple<float, float> get_mouse_pos() {
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return std::make_tuple(g_app.mouse_x, g_app.mouse_y);
}

bool is_mouse_down(int button) {
  if (button < 0 || button > 2)
    return false;
  std::lock_guard<std::mutex> lock(g_app.mtx);
  return g_app.mouse_down[button];
}

// ---------------------------------------------------------
// Module Definition
// ---------------------------------------------------------
PYBIND11_EMBEDDED_MODULE(host_api, m) {
  m.doc() = "Extended Host API";

  m.def("log_message", &log_message);
  m.def("sleep_ms", &sleep_ms);
  m.def("compute_prime", &compute_prime);
  m.def("write_file", &write_file);
  m.def("read_file", &read_file);
  m.def("get_random_data", &get_random_data);

  // Graphics
  m.def("draw_rect", &draw_rect);
  m.def("draw_circle", &draw_circle);
  m.def("draw_text", &draw_text);
  m.def("clear_screen", &clear_screen);
  m.def("is_key_down", &is_key_down);
  m.def("get_mouse_pos", &get_mouse_pos);
  m.def("is_mouse_down", &is_mouse_down);
}
