#pragma once

#include "amr/Types.hpp"
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace amr {

class AmrController {
public:
  static AmrController &Instance();

  // Safety & Config
  // InputConfig is in Types.hpp

  void Update(float dt); // Main Loop for Logic

  void ConfigureSafety(int pin, int action, bool invert, bool edge);
  void ResetSafety();
  int GetPinForAction(InputAction action) const;

  // Motion API
  void AxisMove(int axis, float pos, float vel);
  float GetAxisPos(int axis) const;
  bool IsAxisMoving(int axis) const;
  void SetAxisCurrentPos(int axis, float pos);

  // IO API
  void SetDO(int port, bool val);
  bool GetDO(int port) const;
  bool GetDI(int port) const;
  void SetDI(int port, bool val); // For simulation injection

  // Registers & Params
  void SetReg(int id, float val);
  float GetReg(int id) const;
  std::vector<GlobalParam> &GetGlobalParams();
  float GetParam(const std::string &name) const;
  void SetGlobalParams(const std::vector<GlobalParam> &params);

  // State
  void SetPaused(bool paused);
  bool IsPaused() const;

  // Callbacks
  void SetLogCallback(std::function<void(const std::string &)> cb) {
    log_cb_ = cb;
  }

private:
  AmrController();
  ~AmrController();

  void Log(const std::string &msg);

  // Internal State
  mutable std::mutex mtx_;
  std::function<void(const std::string &)> log_cb_;

  std::unique_ptr<class IHardware> hardware_;

  // Safety
  std::map<int, InputConfig> input_map_;
  std::vector<bool> last_di_state_;
  bool estop_active_ = false;
  bool is_paused_ = false;

  // Data
  std::map<int, float> registers_; // 0-31
  std::vector<GlobalParam> params_;
};

} // namespace amr
