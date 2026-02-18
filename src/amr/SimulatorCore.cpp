#include "amr/SimulatorCore.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace amr {

SimulatorCore &SimulatorCore::Instance() {
  static SimulatorCore instance;
  return instance;
}

SimulatorCore::SimulatorCore() {
  // Initialize Physics World
  odom_ = {0, 0, 0, 0, 0, 0};

  // Add Demo Obstacles (Walls)
  // Outer Bounds (10x10m area centered at 0,0?) let's make it map coordinates
  // Let's assume map is 0-800 pixels scale which maps to 0-8.0 meters?
  // Let's stick to Pixels for visualization consistency essentially, or Meters?
  // User requested "Physics", likely Meters is better suited for robotics,
  // but existing Visuals use Pixels (e.g. DrawRect x=100).
  // Let's treat 1.0 unit = 1.0 Meter.
  // So 800 pixels = 800 Meters? That's huge.
  // Or 100 pixels = 1 Meter. Let's say 1 Unit = 1 Pixel for now to match
  // current drawing logic simplicity. Wait, the "DrawMachine" uses 0-100 range
  // for axis. Let's stick to "World Units" where 1 Unit = 1 cm? or 1 Pixel.
  // Let's assume 1 Unit = 1 Pixel for now to strictly match existing Draw
  // commands.

  // Walls
  AddObstacle(100, 100, 20, 200); // Vertical wall
  AddObstacle(300, 100, 200, 20); // Horizontal wall
  AddObstacle(400, 300, 50, 50);  // Box
}

SimulatorCore::~SimulatorCore() {}

// --- World Engine ---

void SimulatorCore::UpdateChassis(const Twist &cmd, float dt) {
  std::lock_guard<std::mutex> lock(mtx_);

  // 0. Cap dt to prevent explosion (e.g. after long pause)
  if (dt > 0.1f)
    dt = 0.1f;
  if (dt < 1e-6f)
    return;

  // 1. Simple Kinematics (Omni/Mecanum Model)
  // Local Velocity
  // World Unity assumption: 100.0 units = 1 Meter. Input is m/s.
  // So we multiply by 100 to get units/s.
  float vx_local = cmd.linear_x * 100.0f;
  float vy_local = cmd.linear_y * 100.0f;
  float omega = cmd.angular_z; // rad/s, no scaling needed

  // Global Velocity (Rotate by Theta)
  float cos_t = std::cos((float)odom_.theta);
  float sin_t = std::sin((float)odom_.theta);

  float vx_global = vx_local * cos_t - vy_local * sin_t;
  float vy_global = vx_local * sin_t + vy_local * cos_t;

  // Integration
  double dx = vx_global * dt;
  double dy = vy_global * dt;
  odom_.x += dx;
  odom_.y += dy;
  odom_.theta += omega * dt;

  // Periodic Logging (Every 1s-ish or significant move)
  static float log_timer = 0;
  log_timer += dt;
  if (log_timer > 2.0f) {
    log_timer = 0;
    char buf[128];
    snprintf(buf, 128, "[Phys] Odom: x=%.1f, y=%.1f, th=%.2f (cmd_vx=%.1f)\n",
             odom_.x, odom_.y, odom_.theta, vx_local);
    // We can't call Model().LogMessage here directly because of circular header
    // dependency or locking? Actually SimulatorCore is lower than AppModel.
    // Let's use std::cout for now or find another way.
    // std::cout << buf << std::flush;
  }

  // Normalize Theta (-PI to PI)
  if (odom_.theta > M_PI)
    odom_.theta -= 2 * M_PI;
  if (odom_.theta < -M_PI)
    odom_.theta += 2 * M_PI;

  // Update Velocity Feedback
  odom_.vx = vx_local;
  odom_.vy = vy_local;
  odom_.omega = omega;
}

Odometry SimulatorCore::GetOdometry() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return odom_;
}

void SimulatorCore::ResetOdometry() {
  std::lock_guard<std::mutex> lock(mtx_);
  odom_ = Odometry{};
}

void SimulatorCore::AddObstacle(float x, float y, float w, float h) {
  std::lock_guard<std::mutex> lock(mtx_);
  obstacles_.push_back({x, y, w, h});
}

void SimulatorCore::ResetObstacles() {
  std::lock_guard<std::mutex> lock(mtx_);
  obstacles_.clear();
}

const std::vector<Rect> &SimulatorCore::GetObstacles() const {
  // Warning: returning reference to member requires external locking or careful
  // usage. For now assuming caller copies or it's safe enough for sim.
  return obstacles_;
}

// --- Sensor Simulation (RayCasting) ---

// Helper: Line Segment Intersection
// Returns distance to intersection, or -1 if none
float IntersectRayRect(float rx, float ry, float dx, float dy,
                       const Rect &rect) {
  // Slab method or simply check 4 segments
  // Simple approach: Check intersection with 4 lines
  // Line 1: x,y -> x+w,y
  // Line 2: x,y -> x,y+h
  // Line 3: x+w,y -> x+w,y+h
  // Line 4: x,y+h -> x+w,y+h

  float min_dist = std::numeric_limits<float>::max();
  bool hit = false;

  auto check_line = [&](float x1, float y1, float x2, float y2) {
    float r_mag = std::sqrt(dx * dx + dy * dy); // usually 1.0 needed?
    // Let's use parametric: P + t*D = Q + u*S
    // Ray: R(t) = (rx, ry) + t*(dx, dy)
    // Seg: S(u) = (x1, y1) + u*(x2-x1, y2-y1)

    float sx = x2 - x1;
    float sy = y2 - y1;

    float denom = dx * sy - dy * sx;
    if (std::abs(denom) < 1e-6)
      return; // Parallel

    float t = ((x1 - rx) * sy - (y1 - ry) * sx) / denom;
    float u = ((x1 - rx) * dy - (y1 - ry) * dx) / denom;

    if (t > 0 && u >= 0 && u <= 1) {
      if (t < min_dist) {
        min_dist = t;
        hit = true;
      }
    }
  };

  check_line(rect.x, rect.y, rect.x + rect.w, rect.y);
  check_line(rect.x, rect.y, rect.x, rect.y + rect.h);
  check_line(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h);
  check_line(rect.x, rect.y + rect.h, rect.x + rect.w, rect.y + rect.h);

  return hit ? min_dist : -1.0f;
}

LidarScan SimulatorCore::RayCast(const Odometry &pose) const {
  std::lock_guard<std::mutex> lock(mtx_);
  LidarScan scan;
  const int rays = 360;
  scan.angle_min = 0;
  scan.angle_max = 2 * M_PI;
  scan.angle_increment = (2 * M_PI) / rays;
  scan.range_min = 1.0f;
  scan.range_max = 1000.0f; // 1000 pixels

  scan.ranges.resize(rays, scan.range_max); // precise resize with default max

  for (int i = 0; i < rays; ++i) {
    float angle = pose.theta + (i * scan.angle_increment);
    float dx = std::cos(angle);
    float dy = std::sin(angle);

    float closest = scan.range_max;

    for (const auto &rect : obstacles_) {
      float dist = IntersectRayRect(pose.x, pose.y, dx, dy, rect);
      if (dist > 0 && dist < closest) {
        closest = dist;
      }
    }
    scan.ranges[i] = closest;
  }

  return scan;
}

// --- Legacy Impl Below ---

void SimulatorCore::Update(float dt) {
  std::lock_guard<std::mutex> lock(mtx_);

  // Physics Step for Particles
  for (auto &p : particles_) {
    p.x += p.vx;
    p.y += p.vy;
    p.life -= dt;
  }

  // Remove dead particles
  // Helper to remove_if
  int w_idx = 0;
  for (int r_idx = 0; r_idx < (int)particles_.size(); ++r_idx) {
    if (particles_[r_idx].life > 0) {
      if (w_idx != r_idx) {
        particles_[w_idx] = particles_[r_idx];
      }
      w_idx++;
    }
  }
  particles_.resize(w_idx);

  // Shake
  if (shake_timer_ > 0) {
    shake_timer_ -= dt;
    if (shake_timer_ < 0)
      shake_timer_ = 0;
  }
}

void SimulatorCore::PushDrawCmd(const DrawCmd &cmd) {
  std::lock_guard<std::mutex> lock(mtx_);
  draw_queue_.push_back(cmd);
}

void SimulatorCore::ClearDrawQueue() {
  std::lock_guard<std::mutex> lock(mtx_);
  draw_queue_.clear();
}

std::vector<DrawCmd> SimulatorCore::GetDrawQueue() const {
  std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(mtx_));
  return draw_queue_;
}

void SimulatorCore::SpawnParticles(float x, float y, int count, int color) {
  std::lock_guard<std::mutex> lock(mtx_);
  for (int i = 0; i < count; ++i) {
    Particle p;
    p.x = x;
    p.y = y;
    p.vx = ((rand() % 100) / 10.0f) - 5.0f;
    p.vy = ((rand() % 100) / 10.0f) - 5.0f;
    p.life = 1.0f;
    p.color = static_cast<unsigned int>(color);
    particles_.push_back(p);
  }
}

std::vector<Particle> &SimulatorCore::GetParticles() {
  // WARNING: Not Thread Safe if modified externally
  // For rendering, ideally use a copy or lock.
  // Preserving current unsafe behavior for API compatibility in first pass.
  return particles_;
}

void SimulatorCore::SetShakeTimer(float force) {
  std::lock_guard<std::mutex> lock(mtx_);
  shake_timer_ = force;
}

float SimulatorCore::GetShakeTimer() const { return shake_timer_; }

} // namespace amr
