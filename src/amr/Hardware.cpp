#include "amr/Hardware.hpp"
#include "amr/ServiceContext.hpp"
#include "amr/PhysicsContext.hpp" // Replaces SimulatorCore.hpp
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
      is_moving_ = true;
      break;
    case AxisMode::VELOCITY:
      current_vel_ = val;
      break;
    case AxisMode::TORQUE:
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
    st.in_position = !st.is_moving;
    return st;
  }

  // Simulation Step
  void Update(float dt) {
    if (dt < 1e-6f)
      return;

    if (mode_ == AxisMode::POSITION && is_moving_) {
      float diff = target_pos_ - current_pos_;
      float dist = std::abs(diff);
      float target_vel = (diff > 0 ? max_vel_ : -max_vel_);
      float accel = 200.0f; 
      float stop_dist = (current_vel_ * current_vel_) / (2.0f * accel);

      if (dist <= stop_dist) {
        target_vel = 0;
      }

      float vel_error = target_vel - current_vel_;
      float dv = accel * dt;
      if (std::abs(vel_error) < dv) {
        current_vel_ = target_vel;
      } else {
        current_vel_ += (vel_error > 0 ? dv : -dv);
      }

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

    if (min_pos_ != max_pos_) { 
      current_pos_ = std::max(min_pos_, std::min(max_pos_, current_pos_));
    }
  }

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
  float max_vel_ = 100.0f; 
  bool is_moving_ = false;
};

class SimHardware::SimChassis : public IChassis {
public:
  void SetVelocity(const Twist &cmd) override { cmd_ = cmd; }

  Odometry GetOdometry() const override {
    auto phys = ServiceContext::Instance().Get<PhysicsContext>();
    return phys ? phys->GetOdometry() : Odometry{};
  }

  void ResetOdometry() override { 
      auto phys = ServiceContext::Instance().Get<PhysicsContext>();
      if (phys) phys->ResetOdometry();
  }

  void Update(float dt) {
    // Note: AppModel::UpdatePhysics calls PhysicsContext::UpdateChassis directly with cached twist.
    // So this might be redundant IF SimHardware::Update(dt) is called before or after logic.
    // However, AppModel updates Chassis via PhysicsContext directly in `AppModel::UpdatePhysics`.
    // SimHardware Update is called by AmrController::Update.
    // If we use SimHardware to drive physics, we should call PhysicsContext here.
    // BUT AppModel::UpdatePhysics handles calling safety and physics.
    // Let's decide: Who drives physics? AppModel or Hardware?
    // Current AppModel logic: `phys->UpdateChassis(target_twist_, dt);`
    // So redundant call here is unnecessary or conflicting.
    // For now, let's make this a no-op or check if we want Hardware to own this.
    // Ideally Hardware just REPORTS state. Physics Engine drives state based on cmd.
    // So Update here is just "Reading" state? No, SetVelocity set the cmd.
    // If AppModel sets Twist on PhysicsContext directly, SimChassis::SetVelocity is ignored?
    // Legacy: AppModel sets target_twist_, calls SimulatorCore.
    // Hardware is mostly ignored for chassis control in Legacy.
  }

private:
  Twist cmd_;
};

class SimHardware::SimLidar : public ILidar {
public:
  LidarScan GetScanData() const override {
    auto phys = ServiceContext::Instance().Get<PhysicsContext>();
    if (!phys) return LidarScan{};
    
    Odometry pose = phys->GetOdometry();
    return phys->RayCast(pose);
  }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }
private:
  bool enabled_ = true;
};

class SimHardware::SimSystemIO : public ISystemIO {
public:
  void SetLightStrip(int id, const RGB &color) {}
  void PlayAudioClip(const std::string &name) {
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

IChassis *SimHardware::GetChassis() { return chassis_; }
ILidar *SimHardware::GetLidar() { return lidar_; }
ISystemIO *SimHardware::GetSystemIO() { return sys_io_; }
IAxis *SimHardware::GetAxis(int index) {
  if (index < 0) return nullptr;
  while (index >= (int)extended_axes_.size()) {
    extended_axes_.push_back(new SimAxis());
  }
  return extended_axes_[index];
}

void SimHardware::SetDO(int pin, bool value) {
  if (pin < 0 || pin >= 32) return;
  do_[pin] = value;
}
bool SimHardware::GetDO(int pin) const {
  if (pin < 0 || pin >= 32) return false;
  return do_[pin];
}
bool SimHardware::GetDI(int pin) const {
  if (pin < 0 || pin >= 32) return false;
  return di_[pin];
}
void SimHardware::SetDI(int pin, bool value) {
  if (pin < 0 || pin >= 32) return;
  di_[pin] = value;
}

void SimHardware::AxisMove(int axis, float pos, float vel) {
  if (axis < 0) return;
  while (axis >= (int)extended_axes_.size()) {
    extended_axes_.push_back(new SimAxis());
  }
  auto *ax = extended_axes_[axis];
  ax->SetMode(AxisMode::POSITION);   
  ax->SetLimits(-10000, 10000, vel); 
  ax->SetTarget(pos);
}

void SimHardware::AxisStop(int axis) {
  if (axis < 0) return;
  while (axis >= (int)extended_axes_.size()) {
    extended_axes_.push_back(new SimAxis());
  }
  extended_axes_[axis]->Stop();
}

void SimHardware::SetAxisData(int axis, float pos) {
  if (axis < 0) return;
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
  for (auto *ax : extended_axes_) {
    ax->Update(dt);
  }
  // Chassis Update handled by AppModel -> PhysicsContext
}

} // namespace amr
