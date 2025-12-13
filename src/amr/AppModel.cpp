#include "amr/AppModel.hpp"
#include "amr/AmrController.hpp"
#include "amr/SimulatorCore.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace amr {

// Singleton implementation
AppModel &AppModel::Instance() {
  static AppModel instance;
  return instance;
}

AppModel::AppModel() {
  // Initialize defaults if needed
  last_di_state_.resize(8, false); // Keep for minimal usage or remove?
  // Let's rely on AmrController for logic.

  // Set Logging Callback for Controller
  AmrController::Instance().SetLogCallback(
      [this](const std::string &msg) { LogMessage(msg); });
}

// --- Safety & Input ---

void AppModel::MapInput(int pin, InputAction action, bool invert,
                        bool edge_trigger) {
  // Delegate to AmrController
  AmrController::Instance().ConfigureSafety(pin, static_cast<int>(action),
                                            invert, edge_trigger);
}

void AppModel::ResetSafetyConfig() { AmrController::Instance().ResetSafety(); }

int AppModel::GetPinForAction(InputAction action) const {
  return AmrController::Instance().GetPinForAction(action);
}

void AppModel::UpdateSafetyLogic(float dt) {
  // Delegate
  AmrController::Instance().Update(dt);

  // Sync basic state back if needed (e.g. is_paused)
  bool ctrl_paused = AmrController::Instance().IsPaused();
  if (is_paused_ != ctrl_paused) {
    is_paused_ = ctrl_paused;
    cv_.notify_all();
  }
}

// --- System Control ---
void AppModel::SetRunning(bool running) { is_running_ = running; }
bool AppModel::IsRunning() const { return is_running_; }

void AppModel::SetPaused(bool paused) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    is_paused_ = paused;
  }
  AmrController::Instance().SetPaused(paused);
  cv_.notify_all();
}

bool AppModel::IsPaused() const { return is_paused_; }

void AppModel::RequestTermination() {
  should_terminate_ = true;
  cv_.notify_all(); // Wake up any sleepers
}

void AppModel::ResetTermination() { // New
  should_terminate_ = false;
}

bool AppModel::ShouldTerminate() const { return should_terminate_; }

void AppModel::WaitForResume() {
  std::unique_lock<std::mutex> lock(mtx_);
  if (is_paused_) {
    // Wait until unpaused or termination requested
    cv_.wait(lock, [this] { return !is_paused_ || should_terminate_; });
  }
}

// --- Axis Control ---
void AppModel::AxisMove(int axis, float pos, float vel) {
  AmrController::Instance().AxisMove(axis, pos, vel);
}

float AppModel::GetAxisPos(int axis) const {
  return AmrController::Instance().GetAxisPos(axis);
}

bool AppModel::IsAxisMoving(int axis) const {
  return AmrController::Instance().IsAxisMoving(axis);
}

// --- Physics ---
// --- Physics ---
void AppModel::UpdatePhysics(float dt) {
  std::lock_guard<std::mutex> lock(mtx_);

  // Safety: If paused, physics should NOT run (Motion Freeze)
  if (is_paused_)
    return;

  // Removed hardware_->Update(dt) as it is handled by AmrController::Update()
  // via UpdateSafetyLogic But wait, UpdateSafetyLogic calls
  // AmrController::Update. Is UpdateSafetyLogic called every frame? Yes in
  // main.cpp. So we don't need to call it here.

  // Delegate Visual Physics (Particles, Shake, etc.)
  // We need to unlock if SimulatorCore uses its own lock?
  // SimulatorCore has its own mutex. AppModel has mtx_.
  // If we hold mtx_ and call SimulatorCore which locks its mtx_, it's fine
  // unless SimulatorCore calls back to AppModel (Cycle).
  // SimulatorCore is a leaf. So it is fine.
  SimulatorCore::Instance().Update(dt);
}

// --- I/O ---
void AppModel::SetDO(int port, bool val) {
  AmrController::Instance().SetDO(port, val);
}

bool AppModel::GetDO(int port) const {
  return AmrController::Instance().GetDO(port);
}

bool AppModel::GetDI(int port) const {
  return AmrController::Instance().GetDI(port);
}

void AppModel::SetDI(int port, bool val) {
  AmrController::Instance().SetDI(port, val);
}

// --- Registers ---
void AppModel::SetReg(int id, float val) {
  AmrController::Instance().SetReg(id, val);
}

float AppModel::GetReg(int id) const {
  return AmrController::Instance().GetReg(id);
}

float AppModel::GetParam(const std::string &name) const {
  return AmrController::Instance().GetParam(name);
}

void AppModel::SetGlobalParams(const std::vector<GlobalParam> &params) {
  AmrController::Instance().SetGlobalParams(params);
}

// ...

// --- Visuals ---
void AppModel::PushDrawCmd(const DrawCmd &cmd) {
  SimulatorCore::Instance().PushDrawCmd(cmd);
}

std::vector<DrawCmd> AppModel::GetDrawQueue() {
  return SimulatorCore::Instance().GetDrawQueue();
}

void AppModel::ClearDrawQueue() { SimulatorCore::Instance().ClearDrawQueue(); }

void AppModel::LogMessage(const std::string &msg) {
  std::lock_guard<std::mutex> lock(mtx_);
  console_log_ += msg + "\n";
  std::cout << msg << std::endl; // Mirror to terminal
}

// --- Particles ---
void AppModel::SpawnParticles(float x, float y, int count, int color) {
  SimulatorCore::Instance().SpawnParticles(x, y, count, color);
}

std::vector<Particle> &AppModel::GetParticles() {
  return SimulatorCore::Instance().GetParticles();
}

// --- Input State ---
void AppModel::SetInputSticky(const std::string &key, bool val) {
  std::lock_guard<std::mutex> lock(mtx_);
  input_sticky_[key] = val;
}

bool AppModel::GetInputSticky(const std::string &key) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (input_sticky_.count(key)) {
    bool v = input_sticky_[key];
    input_sticky_[key] = false; // Auto-clear for "down" semantics
    return v;
  }
  return false;
}

void AppModel::SetMousePos(float x, float y) {
  std::lock_guard<std::mutex> lock(mtx_);
  mouse_x_ = x;
  mouse_y_ = y;
}

std::pair<float, float> AppModel::GetMousePos() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return {mouse_x_, mouse_y_};
}

void AppModel::SetMouseDown(int btn, bool val) {
  if (btn < 0 || btn > 2)
    return;
  std::lock_guard<std::mutex> lock(mtx_);
  mouse_down_[btn] = val;
}

bool AppModel::IsMouseDown(int btn) const {
  if (btn < 0 || btn > 2)
    return false;
  std::lock_guard<std::mutex> lock(mtx_);
  return mouse_down_[btn];
}

std::string AppModel::GetLog() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return console_log_;
}

// --- Accessors ---
std::vector<Mechanism> &AppModel::GetMechanisms() {
  // Note: Not thread safe if modified while being read elsewhere.
  // Usually these are config-time only.
  return mechanisms_;
}

std::vector<VisualBlock> &AppModel::GetBlocks() { return blocks_; }

std::vector<GlobalParam> &AppModel::GetGlobalParams() {
  return AmrController::Instance().GetGlobalParams();
}

// --- Script Helpers ---
void AppModel::SetNextBlockId(int id) {
  std::lock_guard<std::mutex> lock(mtx_);
  next_block_id_ = id;
}

int AppModel::GetNextBlockId() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return next_block_id_;
}

int AppModel::AllocateBlockId() {
  std::lock_guard<std::mutex> lock(mtx_);
  return next_block_id_++;
}

// --- Script Data ---
void AppModel::SetScriptPath(const std::string &path) {
  // Only lock to set, no mutex needed if strictly single writer config time?
  // Better be safe.
  std::lock_guard<std::mutex> lock(mtx_);
  script_path_ = path;
}

std::string AppModel::GetScriptPath() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return script_path_;
}

void AppModel::SetSourceLines(const std::vector<std::string> &lines) {
  std::lock_guard<std::mutex> lock(mtx_);
  source_lines_ = lines;
}

std::vector<std::string> AppModel::GetSourceLines() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return source_lines_;
}

void AppModel::SetCurrentLine(int line) {
  std::lock_guard<std::mutex> lock(mtx_);
  current_line_ = line;
}

int AppModel::GetCurrentLine() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return current_line_;
}

// Extras
void AppModel::RequestScreenshot(const std::string &filename) {
  std::lock_guard<std::mutex> lock(mtx_);
  screenshot_req_ = true;
  screenshot_file_ = filename;
}

bool AppModel::IsScreenshotRequested() { return screenshot_req_; }

std::string AppModel::GetScreenshotFile() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return screenshot_file_;
}

void AppModel::ClearScreenshotRequest() { screenshot_req_ = false; }

void AppModel::SetShakeTimer(float force) {
  SimulatorCore::Instance().SetShakeTimer(force);
}

float AppModel::GetShakeTimer() const {
  return SimulatorCore::Instance().GetShakeTimer();
}

void AppModel::ReduceShakeTimer(float dt) {
  // handled by SimulatorCore::Update()
}

// --- Additional Helpers ---
void AppModel::SetAxisCurrentPos(int axis, float pos) {
  AmrController::Instance().SetAxisCurrentPos(axis, pos);
}

void AppModel::SetLocals(const std::map<std::string, std::string> &locals) {
  std::lock_guard<std::mutex> lock(mtx_);
  locals_ = locals;
}

std::map<std::string, std::string> AppModel::GetLocals() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return locals_;
}

} // namespace amr
