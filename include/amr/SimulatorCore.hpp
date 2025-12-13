#pragma once

#include "amr/Types.hpp"
#include <mutex>
#include <vector>

namespace amr {

class SimulatorCore {
public:
  static SimulatorCore &Instance();

  void Update(float dt);

  // Visuals
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

  std::mutex mtx_;
  std::vector<DrawCmd> draw_queue_;
  std::vector<Particle> particles_;
  float shake_timer_ = 0.0f;
};

} // namespace amr
