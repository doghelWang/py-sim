#include "amr/SimulatorCore.hpp"
#include <cstdlib>

namespace amr {

SimulatorCore &SimulatorCore::Instance() {
  static SimulatorCore instance;
  return instance;
}

SimulatorCore::SimulatorCore() {}

SimulatorCore::~SimulatorCore() {}

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
  for (int r_idx = 0; r_idx < particles_.size(); ++r_idx) {
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

float SimulatorCore::GetShakeTimer() const {
  // std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
  // Atomic read of float is usually fine, but strictly needs lock.
  return shake_timer_;
}

} // namespace amr
