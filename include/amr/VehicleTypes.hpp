#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace amr {

// ---------------------------------------------------------
// Basic Types
// ---------------------------------------------------------

// Speed Command (Robot Base)
struct Twist {
  float linear_x = 0.0f;  // Forward/Backward (m/s)
  float linear_y = 0.0f;  // Strafe Left/Right (m/s)
  float angular_z = 0.0f; // Rotate CCW (rad/s)
};

// Odometry Feedback
struct Odometry {
  double x = 0.0;     // Global X (m)
  double y = 0.0;     // Global Y (m)
  double theta = 0.0; // Global Heading (rad)

  double vx = 0.0;    // Local Velocity X (m/s)
  double vy = 0.0;    // Local Velocity Y (m/s)
  double omega = 0.0; // Angular Velocity (rad/s)
};

// Lidar Data
struct LidarScan {
  std::vector<float> ranges;      // Distance in meters
  std::vector<float> intensities; // Signal strength (optional)
  float angle_min = 0.0f;         // Start angle (rad)
  float angle_max = 0.0f;         // End angle (rad)
  float angle_increment = 0.0f;   // Angular resolution (rad)
  float range_min = 0.0f;
  float range_max = 0.0f;
};

// Color for Light Strips
struct RGB {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  static RGB Red() { return {255, 0, 0}; }
  static RGB Green() { return {0, 255, 0}; }
  static RGB Blue() { return {0, 0, 255}; }
  static RGB White() { return {255, 255, 255}; }
  static RGB Off() { return {0, 0, 0}; }
};

// Axis Extended Status
struct AxisStatusExtended {
  float actual_pos = 0.0f;
  float actual_vel = 0.0f;
  float torque = 0.0f;
  bool is_moving = false;
  bool in_position = false;
  bool error = false;
};

// Axis Control Mode
enum class AxisMode { POSITION, VELOCITY, TORQUE };

} // namespace amr
