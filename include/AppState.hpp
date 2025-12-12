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

struct Particle {
  float x, y;
  float vx, vy;
  float life; // 1.0 = full life, 0.0 = dead
  unsigned int color;
};

struct Axis {
  float current_pos = 0.0f;
  float target_pos = 0.0f;
  float current_vel = 0.0f;
  float max_vel = 10.0f; // Units per frame? Or per sec? Let's say pixels/update
  bool is_moving = false;
};

// Phase 7-8: Visual Sequencer Types
enum class BlockType {
  MOVE_AXIS,
  WAIT,
  DRILL_OP,
  LOG_MSG,
  HOME_AXIS,  // New
  SET_DO,     // New
  WAIT_DI,    // New
  LOOP_START, // New
  LOOP_END,   // New
  SET_REG,    // New
  MATH_REG,   // New
  IF_REG      // New
};

struct VisualBlock {
  int id; // Unique ID
  BlockType type;
  float param1 = 0.0f;    // Generic params (Axis ID, Time, etc)
  std::string param1_ref; // Name of global param if used
  float param2 = 0.0f;    // (Pos, etc)
  std::string param2_ref; // Name of global param if used
  float param3 = 0.0f;    // (Vel, etc)
  std::string param3_ref; // Name of global param if used
  std::string str_param;  // (Log msg)
};

struct Mechanism {
  int id;
  std::string name;
  std::string type; // "Linear", "Rotary"
  int axis_map;     // 0-2, or -1
  int home_di;      // -1 if none
  int limit_di;     // -1 if none
  float min_pos;
  float max_pos;
};

struct GlobalParam {
  std::string name;
  float value;
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

  // Visuals
  std::vector<Particle> particles;
  float shake_timer = 0.0f;

  // Automation / Screenshot
  std::string screenshot_filename;
  bool screenshot_requested = false;

  // Motion Control (Soft-PLC)
  Axis axes[3]; // 0=X, 1=Y, 2=Z

  // I/O System (Phase 8)
  bool digital_inputs[8] = {false};
  bool digital_outputs[8] = {false};

  // Registers (Phase 9)
  float registers[32] = {0.0f};

  // AMR Config (Phase 11)
  std::vector<Mechanism> mechanisms;
  std::vector<GlobalParam> global_params;

  // Visual Program
  std::vector<VisualBlock> visual_program;
  int next_block_id = 1;

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
