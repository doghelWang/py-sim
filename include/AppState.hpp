#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

struct AppState {
  std::mutex mtx;
  std::condition_variable cv;

  // Flags
  std::atomic<bool> is_running{false};
  std::atomic<bool> is_paused{false};
  std::atomic<bool> should_terminate{false};

  // Script Data
  char script_path[1024] = "../scripts/complex_test.py";
  std::vector<std::string> source_lines;

  // Execution State (Updated by Worker)
  std::string current_file;
  int current_line = -1;
  std::map<std::string, std::string> locals;

  // UI Settings
  std::set<std::string> watched_vars;
  std::string console_log;
};

// Global Instance Declaration
extern AppState g_app;
