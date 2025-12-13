#pragma once
#include <vector>

namespace amr {

class IHardware {
public:
  virtual ~IHardware() = default;

  // --- Digital I/O ---
  virtual void SetDO(int pin, bool value) = 0;
  virtual bool GetDO(int pin) const = 0;
  virtual bool GetDI(int pin) const = 0;

  // Simulation/Testing Hook: Inject Input
  // In real hardware, this wouldn't exist or would match physical override
  virtual void SetDI(int pin, bool value) = 0;

  // --- Axis Control ---
  struct AxisState {
    float current_pos;
    float target_pos;
    float max_vel;
    bool is_moving;
  };

  virtual void AxisMove(int axis, float pos, float vel) = 0;
  virtual void AxisStop(int axis) = 0; // Stop implies Velocity=0
  virtual void SetAxisData(int axis,
                           float pos) = 0; // Forcing position (Homing/Reset)
  virtual bool IsAxisMoving(int axis) const = 0;
  virtual float GetAxisPos(int axis) const = 0;

  // --- Loop ---
  // dt: Delta time in seconds
  virtual void Update(float dt) = 0;
};

// Default Simulation Hardware
class SimHardware : public IHardware {
public:
  SimHardware();
  virtual ~SimHardware() = default;

  void SetDO(int pin, bool value) override;
  bool GetDO(int pin) const override;
  bool GetDI(int pin) const override;
  void SetDI(int pin, bool value) override;

  void AxisMove(int axis, float pos, float vel) override;
  void AxisStop(int axis) override;
  void SetAxisData(int axis, float pos) override;
  bool IsAxisMoving(int axis) const override;
  float GetAxisPos(int axis) const override;

  void Update(float dt) override;

private:
  struct Axis {
    float current_pos = 0.0f;
    float target_pos = 0.0f;
    float max_vel = 0.0f;
    bool is_moving = false;
  };

  std::vector<Axis> axes_;
  std::vector<bool> di_; // Inputs
  std::vector<bool> do_; // Outputs
};

} // namespace amr
