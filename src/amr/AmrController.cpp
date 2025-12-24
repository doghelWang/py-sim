#include "amr/AmrController.hpp"
#include "amr/Hardware.hpp"
#include <iostream>
#include "amr/ServiceContext.hpp"
#include "amr/SafetySystem.hpp"

namespace amr {

AmrController &AmrController::Instance() {
  static AmrController instance;
  return instance;
}

AmrController::AmrController() {
  // Default Hardware
  hardware_ = std::make_shared<SimHardware>();
  ServiceContext::Instance().Register(hardware_);

  // Resize DI state
  last_di_state_.resize(32, false);
}

AmrController::~AmrController() {}

void AmrController::Log(const std::string &msg) {
  if (log_cb_) {
    log_cb_(msg);
  } else {
    std::cout << "[Controller] " << msg << std::endl;
  }
}

void AmrController::Update(float dt) {
  std::lock_guard<std::mutex> lock(mtx_);

  bool paused = false;
  auto safety = ServiceContext::Instance().Get<SafetySystem>();
  if (safety && safety->IsPaused()) paused = true;

  // Update Hardware
  if (hardware_ && !paused) { 
    hardware_->Update(dt);
  }
  
  // Legacy Safety Logic Removed (Moved to SafetySystem)
}

// Safety Configuration
void AmrController::ConfigureSafety(int pin, int action, bool invert,
                                    bool edge) {
  std::lock_guard<std::mutex> lock(mtx_);
  InputConfig cfg;
  cfg.action = static_cast<InputAction>(action);
  cfg.invert = invert;
  cfg.edge_trigger = edge;
  input_map_[pin] = cfg;
}

void AmrController::ResetSafety() {
  std::lock_guard<std::mutex> lock(mtx_);
  input_map_.clear();
  estop_active_ = false;
  Log("[Sys] Safety Config Reset.\n");
}

void AmrController::ClearSafety() {
  std::lock_guard<std::mutex> lock(mtx_);
  estop_active_ = false;
  is_paused_ = false;
  Log("[Sys] Safety Cleared manually.\n");
}

int AmrController::GetPinForAction(InputAction action) const {
  // std::lock_guard<std::mutex> lock(mtx_); // Method is const, mtx_ is
  // mutable? Use const_cast or make mtx_ mutable. Usually mtx_ in modern C++ is
  // mutable. Assuming mtx_ is mutable or I use const_cast. Simplest: just
  // iterate.
  for (const auto &pair : input_map_) {
    if (pair.second.action == action)
      return pair.first;
  }
  return -1;
}

// Motion
void AmrController::AxisMove(int axis, float pos, float vel) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (hardware_)
    hardware_->AxisMove(axis, pos, vel);
}

float AmrController::GetAxisPos(int axis) const {
  // std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
  if (hardware_)
    return hardware_->GetAxisPos(axis);
  return 0.0f;
}

bool AmrController::IsAxisMoving(int axis) const {
  if (hardware_)
    return hardware_->IsAxisMoving(axis);
  return false;
}

void AmrController::SetAxisCurrentPos(int axis, float pos) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (hardware_)
    hardware_->SetAxisData(axis, pos);
}

// IO
void AmrController::SetDO(int port, bool val) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (hardware_)
    hardware_->SetDO(port, val);
}

bool AmrController::GetDO(int port) const {
  if (hardware_)
    return hardware_->GetDO(port);
  return false;
}

bool AmrController::GetDI(int port) const {
  if (hardware_)
    return hardware_->GetDI(port);
  return false;
}

void AmrController::SetDI(int port, bool val) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (hardware_)
    hardware_->SetDI(port, val);
  // Log("[Hw] SetDI(" + std::to_string(port) + ") = " + (val ? "HIGH" : "LOW")
  // + "\n");
}

// Registers
void AmrController::SetReg(int id, float val) {
  std::lock_guard<std::mutex> lock(mtx_);
  registers_[id] = val;
}

float AmrController::GetReg(int id) const {
  // lock?
  auto it = registers_.find(id);
  if (it != registers_.end())
    return it->second;
  return 0.0f;
}

void AmrController::SetGlobalParams(const std::vector<GlobalParam> &params) {
  std::lock_guard<std::mutex> lock(mtx_);
  params_ = params;
}

float AmrController::GetParam(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mtx_);
  for (const auto &p : params_) {
    if (p.name == name)
      return p.value;
  }
  return 0.0f;
}

std::vector<GlobalParam> &AmrController::GetGlobalParams() {
  std::lock_guard<std::mutex> lock(mtx_);
  return params_;
}

void AmrController::SetPaused(bool paused) {
  std::lock_guard<std::mutex> lock(mtx_);
  is_paused_ = paused;
}

bool AmrController::IsPaused() const { return is_paused_; }

} // namespace amr
