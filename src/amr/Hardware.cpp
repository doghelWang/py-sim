#include "amr/Hardware.hpp"
#include "amr/SimulatorCore.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace amr {

// ---------------------------------------------------------
// Inner Classes Implementation
// ---------------------------------------------------------

class SimHardware::SimAxis : public IAxis {
public:
  void SetMode(AxisMode mode) override { mode_ = mode; }

  void SetLimits(float min_pos, float max_pos, float max_vel) override {
    min_pos_ = min_pos;
    max_pos_ = max_pos;
    max_vel_ = max_vel;
  }

  void SetTarget(float val) override {
    target_val_ = val; // Interpretation depends on mode

    switch (mode_) {
    case AxisMode::POSITION:
      target_pos_ = val;
      // Clamp target if limits set? For now just raw set.
      is_moving_ = true;
      break;
    case AxisMode::VELOCITY:
      current_vel_ = val;
      // In vel mode, target_pos_ is not used directly
      break;
    case AxisMode::TORQUE:
      // TODO: Simulate torque
      break;
    }
  }

  void Stop() override {
    if (mode_ == AxisMode::POSITION) {
      target_pos_ = current_pos_;
    } else {
      current_vel_ = 0.0f;
    }
    is_moving_ = false;
  }

  AxisStatusExtended GetStatus() const override {
    AxisStatusExtended st;
    st.actual_pos = current_pos_;
    st.actual_vel = current_vel_;
    st.is_moving = is_moving_ || (std::abs(current_vel_) > 0.001f);
    st.in_position = !st.is_moving; // Simplification
    return st;
  }

  // Simulation Step
  void Update(float dt) {
    if (dt < 1e-6f)
      return;

    if (mode_ == AxisMode::POSITION && is_moving_) {
      float diff = target_pos_ - current_pos_;
      float dist = std::abs(diff);

      // Simple Trapezoidal Profile:
      // 1. Accel/Decel towards target velocity
      float target_vel = (diff > 0 ? max_vel_ : -max_vel_);

      // 2. Decel logic: Calculate stopping distance
      float accel = 200.0f; // units/s^2 (Default accel)
      float stop_dist = (current_vel_ * current_vel_) / (2.0f * accel);

      if (dist <= stop_dist) {
        // Decelerate
        target_vel = 0;
      }

      // 3. Update current velocity
      float vel_error = target_vel - current_vel_;
      float dv = accel * dt;
      if (std::abs(vel_error) < dv) {
        current_vel_ = target_vel;
      } else {
        current_vel_ += (vel_error > 0 ? dv : -dv);
      }

      // 4. Update position
      float ds = current_vel_ * dt;
      if (std::abs(ds) >= dist) {
        current_pos_ = target_pos_;
        current_vel_ = 0;
        is_moving_ = false;
      } else {
        current_pos_ += ds;
      }
    } else if (mode_ == AxisMode::VELOCITY) {
      current_pos_ += current_vel_ * dt;
      is_moving_ = (std::abs(current_vel_) > 0.001f);
    }

    // Clamp position limits
    if (min_pos_ != max_pos_) { // If limits set
      current_pos_ = std::max(min_pos_, std::min(max_pos_, current_pos_));
    }
  }

  // Legacy support helpers
  void ForcePos(float pos) {
    current_pos_ = pos;
    target_pos_ = pos;
    is_moving_ = false;
  }

private:
  AxisMode mode_ = AxisMode::POSITION;
  float current_pos_ = 0.0f;
  float current_vel_ = 0.0f;

  float target_pos_ = 0.0f;
  float target_val_ = 0.0f;

  float min_pos_ = 0.0f;
  float max_pos_ = 0.0f;
  float max_vel_ = 100.0f; // Default

  bool is_moving_ = false;
};

class SimHardware::SimChassis : public IChassis {
public:
  void SetVelocity(const Twist &cmd) override { cmd_ = cmd; }

  Odometry GetOdometry() const override {
    // Fetch ground truth from World Engine
    return SimulatorCore::Instance().GetOdometry();
  }

  void ResetOdometry() override { SimulatorCore::Instance().ResetOdometry(); }

  void Update(float dt) {
    // Push Physics to World Engine
    SimulatorCore::Instance().UpdateChassis(cmd_, dt);
  }

private:
  Twist cmd_;
};

class SimHardware::SimLidar : public ILidar {
public:
  LidarScan GetScanData() const override {
    // RayCast from Logic World
    Odometry pose = SimulatorCore::Instance().GetOdometry();
    return SimulatorCore::Instance().RayCast(pose);
  }

  void SetEnabled(bool enabled) override { enabled_ = enabled; }

private:
  bool enabled_ = true;
};

class SimHardware::SimSystemIO : public ISystemIO {
public:
  void SetLightStrip(int id, const RGB &color) override {
    // Visualization Only - Store if needed?
  }

  void PlayAudioClip(const std::string &name) override {
    std::cout << "[SimAudio] Playing: " << name << std::endl;
  }
};

// ---------------------------------------------------------
// SimHardware Implementation
// ---------------------------------------------------------

SimHardware::SimHardware() {
  chassis_ = new SimChassis();
  lidar_ = new SimLidar();
  sys_io_ = new SimSystemIO();

  // Create 3 default axes for backward compatibility (X, Y, Z)
  for (int i = 0; i < 3; ++i) {
    extended_axes_.push_back(new SimAxis());
  }

  di_.resize(32, false);
  do_.resize(32, false);
}

SimHardware::~SimHardware() {
  delete chassis_;
  delete lidar_;
  delete sys_io_;
  for (auto *ax : extended_axes_)
    delete ax;
}

// --- Composite Accessors ---
IChassis *SimHardware::GetChassis() { return chassis_; }
ILidar *SimHardware::GetLidar() { return lidar_; }
ISystemIO *SimHardware::GetSystemIO() { return sys_io_; }
IAxis *SimHardware::GetAxis(int index) {
  if (index < 0)
    return nullptr;
  while (index >= (int)extended_axes_.size()) {
    extended_axes_.push_back(new SimAxis());
  }
  return extended_axes_[index];
}

// --- Legacy Legacy Methods (Mapped to New Inner Classes) ---

void SimHardware::SetDO(int pin, bool value) {
  if (pin < 0 || pin >= 32)
    return;
  do_[pin] = value;
}

bool SimHardware::GetDO(int pin) const {
  if (pin < 0 || pin >= 32)
    return false;
  return do_[pin];
}

bool SimHardware::GetDI(int pin) const {
  if (pin < 0 || pin >= 32)
    return false;
  return di_[pin];
}

void SimHardware::SetDI(int pin, bool value) {
  if (pin < 0 || pin >= 32)
    return;
  di_[pin] = value;
}

void SimHardware::AxisMove(int axis, float pos, float vel) {
  if (axis < 0)
    return;
  while (axis >= (int)extended_axes_.size()) {
    extended_axes_.push_back(new SimAxis());
  }

  auto *ax = extended_axes_[axis];
  ax->SetMode(AxisMode::POSITION);   // Assume legacy is Pos mode
  ax->SetLimits(-10000, 10000, vel); // Dynamic update of max vel
  ax->SetTarget(pos);
}

void SimHardware::AxisStop(int axis) {
  if (axis < 0)
    return;
  while (axis >= (int)extended_axes_.size()) {
    extended_axes_.push_back(new SimAxis());
  }
  extended_axes_[axis]->Stop();
}

void SimHardware::SetAxisData(int axis, float pos) {
  if (axis < 0)
    return;
  while (axis >= (int)extended_axes_.size()) {
    extended_axes_.push_back(new SimAxis());
  }
  extended_axes_[axis]->ForcePos(pos);
}

bool SimHardware::IsAxisMoving(int axis) const {
  if (axis >= 0 && axis < (int)extended_axes_.size()) {
    return extended_axes_[axis]->GetStatus().is_moving;
  }
  return false;
}

float SimHardware::GetAxisPos(int axis) const {
  if (axis >= 0 && axis < (int)extended_axes_.size()) {
    return extended_axes_[axis]->GetStatus().actual_pos;
  }
  return 0.0f;
}

void SimHardware::Update(float dt) {
  // 1. Update Sub-modules
  for (auto *ax : extended_axes_) {
    ax->Update(dt);
  }

  // Decoupled: Chassis is only driven by direct Twist commands from
  // AppModel/HostApi No longer mapping Axis 0,1,2 to linear/angular.

  if (chassis_)
    chassis_->Update(dt);

  // 2. Lidar update? Mostly static or driven by WorldSimulator injection
}

} // namespace amr
