#include "amr/Hardware.hpp"
#include <cmath>
#include <iostream>

namespace amr {

SimHardware::SimHardware() {
  axes_.resize(3);
  di_.resize(8, false);
  do_.resize(8, false);
}

void SimHardware::SetDO(int pin, bool value) {
  if (pin < 0 || pin >= 8)
    return;
  do_[pin] = value;
}

bool SimHardware::GetDO(int pin) const {
  if (pin < 0 || pin >= 8)
    return false;
  return do_[pin];
}

bool SimHardware::GetDI(int pin) const {
  if (pin < 0 || pin >= 8)
    return false;
  return di_[pin];
}

void SimHardware::SetDI(int pin, bool value) {
  if (pin < 0 || pin >= 8)
    return;
  di_[pin] = value;
  // Note: AppModel handles logging for this now?
  // Hardware just stores state.
}

void SimHardware::AxisMove(int axis, float pos, float vel) {
  if (axis < 0 || axis >= 3)
    return;
  axes_[axis].target_pos = pos;
  axes_[axis].max_vel = vel;
  axes_[axis].is_moving = true;
}

void SimHardware::AxisStop(int axis) {
  if (axis < 0 || axis >= 3)
    return;
  // Stop means target = current
  axes_[axis].target_pos = axes_[axis].current_pos;
  axes_[axis].is_moving = false;
}

void SimHardware::SetAxisData(int axis, float pos) {
  if (axis < 0 || axis >= 3)
    return;
  axes_[axis].current_pos = pos;
  axes_[axis].target_pos = pos;
  axes_[axis].is_moving = false;
}

bool SimHardware::IsAxisMoving(int axis) const {
  if (axis < 0 || axis >= 3)
    return false;
  return axes_[axis].is_moving;
}

float SimHardware::GetAxisPos(int axis) const {
  if (axis < 0 || axis >= 3)
    return 0.0f;
  return axes_[axis].current_pos;
}

void SimHardware::Update(float dt) {
  // Physics Simulation
  for (int i = 0; i < 3; ++i) {
    if (axes_[i].is_moving) {
      float diff = axes_[i].target_pos - axes_[i].current_pos;
      float step = axes_[i].max_vel * dt;
      if (step < 0.0001f)
        step = 0.0001f;

      if (std::abs(diff) <= step) {
        axes_[i].current_pos = axes_[i].target_pos;
        axes_[i].is_moving = false;
      } else {
        axes_[i].current_pos += (diff > 0 ? step : -step);
      }
    }
  }
}

} // namespace amr
