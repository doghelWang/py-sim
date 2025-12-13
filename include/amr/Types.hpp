#pragma once

#include <string>
#include <vector>

namespace amr {

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

// Visual Sequencer Types
enum class BlockType {
  MOVE_AXIS,
  WAIT,
  DRILL_OP,
  LOG_MSG,
  HOME_AXIS,
  SET_DO,
  WAIT_DI,
  LOOP_START,
  LOOP_END,
  SET_REG,
  MATH_REG,
  IF_REG,
  CONFIG_SAFETY
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
