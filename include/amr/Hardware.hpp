#include "amr/VehicleTypes.hpp"
#include <string>
#include <vector>

namespace amr {

// ---------------------------------------------------------
// Sub-Interfaces (Composite Hardware)
// ---------------------------------------------------------

// Mobility Interface
class IChassis {
public:
  virtual ~IChassis() = default;
  virtual void SetVelocity(const Twist &cmd) = 0;
  virtual Odometry GetOdometry() const = 0;
  virtual void ResetOdometry() = 0;
};

// Actuation Interface (Enhanced Axis)
class IAxis {
public:
  virtual ~IAxis() = default;

  // Configuration
  virtual void SetMode(AxisMode mode) = 0;
  virtual void SetLimits(float min_pos, float max_pos, float max_vel) = 0;

  // Control
  virtual void SetTarget(float val) = 0; // Pos/Vel/Torque depending on Mode
  virtual void Stop() = 0;

  // Feedback
  virtual AxisStatusExtended GetStatus() const = 0;
};

// Perception Interface
class ILidar {
public:
  virtual ~ILidar() = default;
  virtual LidarScan GetScanData() const = 0;
  virtual void SetEnabled(bool enabled) = 0;
};

// Interaction Interface
class ISystemIO {
public:
  virtual ~ISystemIO() = default;
  virtual void SetLightStrip(int id, const RGB &color) = 0;
  virtual void PlayAudioClip(const std::string &name) = 0;
};

// ---------------------------------------------------------
// Main Hardware Abstraction (Aggregator)
// ---------------------------------------------------------

class IHardware {
public:
  virtual ~IHardware() = default;

  // --- Legacy Digital I/O (Maintained for PLC compatibility) ---
  virtual void SetDO(int pin, bool value) = 0;
  virtual bool GetDO(int pin) const = 0;
  virtual bool GetDI(int pin) const = 0;
  virtual void SetDI(int pin, bool value) = 0; // Sim Injection

  // --- Legacy Axis Control (To be deprecated or mapped to IAxis) ---
  virtual void AxisMove(int axis, float pos, float vel) = 0;
  virtual void AxisStop(int axis) = 0;
  virtual void SetAxisData(int axis, float pos) = 0;
  virtual bool IsAxisMoving(int axis) const = 0;
  virtual float GetAxisPos(int axis) const = 0;

  // --- New Composite Subsystems ---
  // Returns pointer to subsystem or nullptr if not available
  virtual IChassis *GetChassis() = 0;
  virtual ILidar *GetLidar() = 0;
  virtual ISystemIO *GetSystemIO() = 0;
  // Get generic axis by index (0-based)
  virtual IAxis *GetAxis(int index) = 0;

  // --- Loop ---
  // dt: Delta time in seconds
  virtual void Update(float dt) = 0;
};

// Default Simulation Hardware
class SimHardware : public IHardware {
public:
  SimHardware();
  virtual ~SimHardware();

  // Legacy Impl
  void SetDO(int pin, bool value) override;
  bool GetDO(int pin) const override;
  bool GetDI(int pin) const override;
  void SetDI(int pin, bool value) override;

  void AxisMove(int axis, float pos, float vel) override;
  void AxisStop(int axis) override;
  void SetAxisData(int axis, float pos) override;
  bool IsAxisMoving(int axis) const override;
  float GetAxisPos(int axis) const override;

  // New Composite Impl
  IChassis *GetChassis() override;
  ILidar *GetLidar() override;
  ISystemIO *GetSystemIO() override;
  IAxis *GetAxis(int index) override;

  void Update(float dt) override;

private:
  // Internal definition of Sub-Modules for SimHardware
  class SimChassis;
  class SimAxis;
  class SimLidar;
  class SimSystemIO;

  SimChassis *chassis_ = nullptr;
  SimLidar *lidar_ = nullptr;
  SimSystemIO *sys_io_ = nullptr;
  std::vector<SimAxis *> extended_axes_;

  // Legacy Storage (Simulates PLC IO Card)
  std::vector<bool> di_;
  std::vector<bool> do_;
};

} // namespace amr
