#include "amr/AppModel.hpp"
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
  last_di_state_.resize(8, false); // Assuming 8 DI pins

  // Default Hardware
  hardware_ = std::make_unique<SimHardware>();
}

// --- Safety & Input ---

void AppModel::MapInput(int pin, InputAction action, bool invert,
                        bool edge_trigger) {
  std::lock_guard<std::mutex> lock(mtx_);
  InputConfig cfg;
  cfg.action = action;
  cfg.invert = invert;
  cfg.edge_trigger = edge_trigger;
  input_map_[pin] = cfg;
}

void AppModel::ResetSafetyConfig() {
  std::lock_guard<std::mutex> lock(mtx_);
  input_map_.clear();
  estop_active_ = false;
  // Note: We do NOT unpause automatically. Safety resets don't imply
  // safe-to-move.
  console_log_ += "[Sys] Safety Config Reset.\n";
}

int AppModel::GetPinForAction(InputAction action) const {
  std::lock_guard<std::mutex> lock(mtx_);
  for (const auto &pair : input_map_) {
    if (pair.second.action == action) {
      return pair.first; // Return Pin ID
    }
  }
  return -1; // Not found
}

void AppModel::UpdateSafetyLogic(float dt) {
  std::lock_guard<std::mutex> lock(mtx_);

  // Check each mapped input
  for (auto &pair : input_map_) {
    int pin = pair.first;
    const auto &cfg = pair.second;

    if (pin < 0 || pin >= 8)
      continue;

    // Get current raw state
    // Get current raw state from Hardware
    bool raw_val = hardware_->GetDI(pin);
    // Apply inversion
    bool active = cfg.invert ? !raw_val : raw_val;

    // Edge Detection
    bool was_active = last_di_state_[pin];
    bool rising_edge = active && !was_active;
    bool falling_edge = !active && was_active;

    // Update history
    last_di_state_[pin] = active;

    // Process Actions
    switch (cfg.action) {
    case InputAction::ESTOP:
      // Level Triggered usually for E-Stop (Active = Stop)
      if (active) {
        if (!estop_active_) {
          estop_active_ = true;
          is_paused_ = true;
          console_log_ += "[Safety] E-STOP TRIGGERED! System Paused.\n";
          std::cout << "[Safety] E-STOP TRIGGERED! System Paused." << std::endl;
        } else {
          // Enforce pause if someone tried to unpause while Estop is held
          if (!is_paused_) {
            is_paused_ = true;
            console_log_ += "[Safety] E-STOP ACTIVE. Cannot Resume.\n";
            std::cout << "[Safety] E-STOP ACTIVE. Cannot Resume." << std::endl;
          }
        }
      } else {
        if (estop_active_) {
          estop_active_ = false;
          console_log_ += "[Safety] E-Stop Released. Please Resume manually.\n";
          std::cout << "[Safety] E-Stop Released. Please Resume manually."
                    << std::endl;
        }
      }
      break;

    case InputAction::PAUSE_TOGGLE:
      if (rising_edge) {
        if (estop_active_) {
          console_log_ += "[Safety] Ignored Pause Toggle (E-Stop Active).\n";
          std::cout << "[Safety] Ignored Pause Toggle (E-Stop Active)."
                    << std::endl;
        } else {
          bool new_pause = !is_paused_;
          is_paused_ = new_pause;
          cv_.notify_all();
          std::string msg = (new_pause ? "[Safety] Pause Toggled -> PAUSED\n"
                                       : "[Safety] Pause Toggled -> RESUMED\n");
          console_log_ += msg;
          std::cout << msg;
        }
      }
      break;

    case InputAction::HOME_ALL:
      if (rising_edge) {
        if (is_paused_ || estop_active_) {
          console_log_ +=
              "[Safety] Ignored Home All (System Paused/Stopped).\n";
          std::cout << "[Safety] Ignored Home All (System Paused/Stopped)."
                    << std::endl;
        } else {
          console_log_ += "[Safety] Home All Triggered.\n";
          std::cout << "[Safety] Home All Triggered." << std::endl;
          // Trigger Homing for all axes (Mock implementation)
          for (int i = 0; i < 3; ++i) {
            // Set target to 0, slow velocity
            // Set target to 0, slow velocity via Hardware
            hardware_->AxisMove(i, 0.0f, 5.0f);
          }
        }
      }
      break;

    default:
      break;
    }
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
  if (axis < 0 || axis >= 3)
    return;
  std::lock_guard<std::mutex> lock(mtx_);
  hardware_->AxisMove(axis, pos, vel);
}

float AppModel::GetAxisPos(int axis) const {
  if (axis < 0 || axis >= 3)
    return 0.0f;
  std::lock_guard<std::mutex> lock(mtx_);
  return hardware_->GetAxisPos(axis);
}

bool AppModel::IsAxisMoving(int axis) const {
  if (axis < 0 || axis >= 3)
    return false;
  std::lock_guard<std::mutex> lock(mtx_);
  return hardware_->IsAxisMoving(axis);
}

// --- Physics ---
void AppModel::UpdatePhysics(float dt) {
  std::lock_guard<std::mutex> lock(mtx_);

  // Safety: If paused, physics should NOT run (Motion Freeze)
  if (is_paused_)
    return;

  if (hardware_) {
    hardware_->Update(dt);
  }

  // Particles
  for (auto &p : particles_) {
    if (p.life > 0) {
      p.x += p.vx;
      p.y += p.vy;
      p.life -= 0.02f;
    }
  }
}

// --- I/O ---
void AppModel::SetDO(int port, bool val) {
  if (port < 0 || port >= 8)
    return;
  std::lock_guard<std::mutex> lock(mtx_);
  hardware_->SetDO(port, val);
}

bool AppModel::GetDO(int port) const {
  if (port < 0 || port >= 8)
    return false;
  std::lock_guard<std::mutex> lock(mtx_);
  return hardware_->GetDO(port);
}

bool AppModel::GetDI(int port) const {
  if (port < 0 || port >= 8)
    return false;
  std::lock_guard<std::mutex> lock(mtx_);
  return hardware_->GetDI(port);
}

void AppModel::SetDI(int port, bool val) {
  if (port < 0 || port >= 8)
    return;
  std::lock_guard<std::mutex> lock(mtx_);
  hardware_->SetDI(port, val);

  // Log it to console for debugging
  console_log_ += "[Hw] SetDI(" + std::to_string(port) +
                  ") = " + (val ? "HIGH" : "LOW") + "\n";
  std::cout << "[Hw] SetDI(" << port << ") = " << (val ? "HIGH" : "LOW")
            << std::endl;
}

// --- Registers ---
void AppModel::SetReg(int id, float val) {
  if (id < 0 || id >= 32)
    return;
  std::lock_guard<std::mutex> lock(mtx_);
  registers_[id] = val;
}

float AppModel::GetReg(int id) const {
  if (id < 0 || id >= 32)
    return 0.0f;
  std::lock_guard<std::mutex> lock(mtx_);
  return registers_[id];
}

float AppModel::GetParam(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mtx_);
  for (const auto &p : params_) {
    if (p.name == name)
      return p.value;
  }
  return 0.0f;
}

void AppModel::SetGlobalParams(const std::vector<GlobalParam> &params) {
  std::lock_guard<std::mutex> lock(mtx_);
  params_ = params;
}

// --- Visuals ---
void AppModel::PushDrawCmd(const DrawCmd &cmd) {
  std::lock_guard<std::mutex> lock(mtx_);
  draw_queue_.push_back(cmd);
}

std::vector<DrawCmd> AppModel::GetDrawQueue() {
  std::lock_guard<std::mutex> lock(mtx_);
  return draw_queue_; // Return a copy
}

void AppModel::ClearDrawQueue() {
  std::lock_guard<std::mutex> lock(mtx_);
  draw_queue_.clear();
}

void AppModel::LogMessage(const std::string &msg) {
  std::lock_guard<std::mutex> lock(mtx_);
  console_log_ += msg + "\n";
  std::cout << msg << std::endl; // Mirror to terminal
}

// --- Particles ---
void AppModel::SpawnParticles(float x, float y, int count, int color) {
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

std::vector<Particle> &AppModel::GetParticles() { return particles_; }

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

std::vector<GlobalParam> &AppModel::GetGlobalParams() { return params_; }

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

void AppModel::SetShakeTimer(float force) { shake_timer_ = force; }

float AppModel::GetShakeTimer() const { return shake_timer_; }

void AppModel::ReduceShakeTimer(float dt) {
  if (shake_timer_ > 0) {
    shake_timer_ -= dt;
    if (shake_timer_ < 0)
      shake_timer_ = 0;
  }
}

// Motion Extras
void AppModel::SetAxisCurrentPos(int axis, float pos) {
  if (axis >= 0 && axis < 3) {
    std::lock_guard<std::mutex> lock(mtx_);
    hardware_->SetAxisData(axis, pos);
  }
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
