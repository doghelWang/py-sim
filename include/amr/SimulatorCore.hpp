#pragma once

#include "amr/Types.hpp"
#include "amr/VehicleTypes.hpp"
#include <mutex>
#include <vector>

namespace amr {


class SimulatorCore {
public:
  static SimulatorCore &Instance();

  void Update(float dt);

  // --- World Engine (Physics & Kinematics) ---
  void UpdateChassis(const Twist &cmd, float dt);
  Odometry GetOdometry() const;
  void ResetOdometry();
  void ResetObstacles();

  // --- Sensor Simulation ---
  LidarScan RayCast(const Odometry &pose) const;

  // --- Map Engine ---
  void AddObstacle(float x, float y, float w, float h);
  const std::vector<Rect> &GetObstacles() const;

  // --- Visuals (Legacy Support) ---
  void PushDrawCmd(const DrawCmd &cmd);
  void ClearDrawQueue();
  std::vector<DrawCmd> GetDrawQueue() const;

  // Particles
  void SpawnParticles(float x, float y, int count, int color);
  std::vector<Particle> &GetParticles(); // Mutable for rendering

  // Effects
  void SetShakeTimer(float force);
  float GetShakeTimer() const;

private:
  SimulatorCore();
  ~SimulatorCore();

  mutable std::mutex mtx_;

  // Visuals
  std::vector<DrawCmd> draw_queue_;
  std::vector<Particle> particles_;
  float shake_timer_ = 0.0f;

  // Physics State
  Odometry odom_; // Robot true pose in world

  // Map Data
  std::vector<Rect> obstacles_;
};

} // namespace amr
