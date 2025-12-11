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
  // Inefficient prime finder to burn CPU
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

// ---------------------------------------------------------
// Module Definition
// ---------------------------------------------------------
PYBIND11_EMBEDDED_MODULE(host_api, m) {
  m.doc() = "Extended Host API";

  m.def("log_message", &log_message, "Log a message to the host console");
  m.def("sleep_ms", &sleep_ms, "Sleep for specified milliseconds");
  m.def("compute_prime", &compute_prime,
        "Find the Nth prime number (CPU intensive)");
  m.def("write_file", &write_file, "Write string content to a file");
  m.def("read_file", &read_file, "Read string content from a file");
  m.def("get_random_data", &get_random_data, "Get a list of random floats");
}
