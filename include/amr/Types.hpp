#pragma once

#include <map>
#include <string>
#include <vector>

namespace amr {

enum class CmdType { RECT, CIRCLE, TEXT, CLEAR };
enum class AgvType { BASIC, FORKER, CTU };

struct DrawCmd {
  CmdType type;
  float x, y, w, h, r;
  unsigned int color; // 0xAABBGGRR
  std::string text;   // For TEXT commands
};

struct Particle {
  float x, y;
  float vx, vy;
  float life;
  unsigned int color;
};

struct Axis {
  float current_pos = 0.0f;
  float target_pos = 0.0f;
  float current_vel = 0.0f;
  float max_vel = 10.0f;
  bool is_moving = false;
};

struct Rect {
  float x, y, w, h;
};

// Visual Sequencer Types
enum class BlockType {
  MOVE_AXIS,
  WAIT,
  HOME_AXIS,
  SET_DO,
  WAIT_DI,
  IF_REG,
  SET_REG,
  MATH_REG,
  LOOP_START,
  LOOP_END,
  LOG_MSG,
  CONFIG_SAFETY,
  SPAWN_OBSTACLE,
  RESET_ENV,
  AGV_MOVE_VEL,
  GAME_SPAWN_PARTICLES,
  GAME_SHAKE,
  GAME_DRAW_TEXT,
  MOVE,
  DELAY,
  AXIS_MOVE,
  MSG
};

// Input Actions for Safety Config (Match AppModel/AmrController)
enum class InputAction { NONE = 0, ESTOP = 1, PAUSE_TOGGLE = 2, HOME_ALL = 3 };

struct InputConfig {
  InputAction action = InputAction::NONE;
  bool invert = false;
  bool edge_trigger = false;
};

struct VisualBlock {
  int id;
  BlockType type;
  float param1 = 0.0f;
  std::string param1_ref;
  float param2 = 0.0f;
  std::string param2_ref;
  float param3 = 0.0f;
  std::string param3_ref;
  std::string str_param;
  float pos_x = 0.0f;
  float pos_y = 0.0f;

  // Script Parser additions
  std::map<std::string, float> params;
  std::string message;
};

struct Mechanism {
  int id;
  std::string name;
  std::string type;
  int axis_map;
  int home_di;
  int limit_di;
  float min_pos;
  float max_pos;
};

struct GlobalParam {
  std::string name;
  float value;
};

} // namespace amr
