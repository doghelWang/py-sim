#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

enum class CmdType { RECT, CIRCLE, TEXT, CLEAR };

struct DrawCmd {
  CmdType type;
  float x, y, w, h, r;
  unsigned int color; // 0xAABBGGRR
  std::string text;   // For TEXT commands
};

struct AppState {
  std::mutex mtx;
  std::condition_variable cv;

  // Flags
  std::atomic<bool> is_running{false};
  std::atomic<bool> is_paused{false};
  std::atomic<bool> should_terminate{false};

  // Script Data
  char script_path[1024] = "../scripts/snake_game.py";
  std::vector<std::string> source_lines;

  // Graphics & Input
  std::vector<DrawCmd> draw_queue;
  std::map<std::string, bool>
      input_sticky; // Latched key presses ("up", "down")
  float mouse_x = 0, mouse_y = 0;
  bool mouse_down[3] = {false, false, false}; // Left, Right, Middle

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
