#include "amr/AppModel.hpp"
#include "amr/AmrController.hpp"
#include "amr/SimulatorCore.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>

namespace amr {

AppModel &AppModel::Instance() {
  static AppModel instance;
  return instance;
}

AppModel::AppModel() {
  last_di_state_.resize(32, false);
  AmrController::Instance().SetLogCallback(
      [this](const std::string &msg) { LogMessage(msg); });
}

void AppModel::MapInput(int pin, InputAction action, bool invert,
                        bool edge_trigger) {
  AmrController::Instance().ConfigureSafety(pin, static_cast<int>(action),
                                            invert, edge_trigger);
}

void AppModel::ResetSafetyConfig() { AmrController::Instance().ResetSafety(); }
int AppModel::GetPinForAction(InputAction action) const {
  return AmrController::Instance().GetPinForAction(action);
}

void AppModel::UpdateSafetyLogic(float dt) {
  // Always update controller logic, even if paused, to handle unpause signals
  AmrController::Instance().Update(dt);

  bool ctrl_paused = AmrController::Instance().IsPaused();
  if (is_paused_ != ctrl_paused) {
    is_paused_ = ctrl_paused;
    cv_.notify_all();
  }

  if (is_paused_) {
    static int pause_log = 0;
    if (pause_log++ % 60 == 0) // Log once per sec
      std::cout << "[DEBUG] UpdateSafetyLogic: PAUSED due to "
                << (ctrl_paused ? "Controller" : "AppModel") << std::endl;
    return;
  }

  SimulatorCore::Instance().UpdateChassis(target_twist_, dt);
  SimulatorCore::Instance().Update(dt);

  // DI 5 Return to Home
  static bool last_di5 = false;
  bool current_di5 = GetDI(5);
  if (current_di5 && !last_di5) {
    LogMessage("[Auto] DI 5 Triggered: Returning Home...");
    SetTwist(0, 0, 0);
    auto odom = SimulatorCore::Instance().GetOdometry();
    float dx = -odom.x, dy = -odom.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > 5.0f) {
      float spd = 60.0f;
      SetTwist((dx / dist) * spd, (dy / dist) * spd, 0);
    }
  }
  last_di5 = current_di5;

  /*
  // Disable Auto-Home Stop Logic - It prevents manual motion from 0,0
  if (target_twist_.linear_x != 0 || target_twist_.linear_y != 0) {
    auto odom = SimulatorCore::Instance().GetOdometry();
    if (std::abs(odom.x) < 5.0f && std::abs(odom.y) < 5.0f) {
      SetTwist(0, 0, 0);
      LogMessage("[Auto] Arrived at home.");
    }
  }
  */

  static int phys_log = 0;
  if (phys_log++ % 60 == 0) { // Log Odom/Twist every ~1s (assuming 60fps)
    auto odom = SimulatorCore::Instance().GetOdometry();
    std::cout << "[DEBUG] Phys: Odom(" << odom.x << "," << odom.y << ") Twist("
              << target_twist_.linear_x << "," << target_twist_.linear_y
              << ") Ax3:" << GetAxisPos(3) << " Ax4:" << GetAxisPos(4)
              << std::endl;
  }
}
void AppModel::FullReset() {
  LogMessage("[Sys] Full Reset Triggered.");
  SetTwist(0, 0, 0);
  ClearSafety();
  auto &mechs = GetMechanisms();
  for (auto &m : mechs) {
    AxisMove(m.axis_map, 0.0f, 20.0f);
  }
}

// Public: Locks mtx_
void AppModel::LoadScriptAsBlocks(const std::string &path) {
  // Prevent Deadlock: Reset calling back via LogMessage requires AppModel lock.
  // We must do this OUTSIDE the AppModel lock.
  ClearSafety();
  SetPaused(false);

  std::lock_guard<std::mutex> lock(mtx_);
  std::cout << "[DEBUG] LoadScriptAsBlocks Locked for " << path << std::endl;
  LoadScriptAsBlocksInternal(path);
}

void AppModel::LoadScriptAsBlocksInternal(const std::string &path) {
  // Assumes lock is held!
  std::cout << "[DEBUG] Internal Load start: " << path << std::endl;
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    LogMessageInternal("[Err] LoadScript failed: File not found at " + path);
    return;
  }

  blocks_.clear();
  source_lines_.clear();
  next_block_id_ = 0;

  std::string line;
  std::regex re_move(
      R"(host_api\.set_twist\(\s*(-?\d*\.?\d+)\s*,\s*(-?\d*\.?\d+)\s*,\s*(-?\d*\.?\d+)\s*\))");
  std::regex re_delay(R"(host_api\.sleep_ms\(\s*(\d+)\s*\))");
  std::regex re_axis(
      R"(host_api\.axis_move\(\s*(\d+)\s*,\s*(-?\d*\.?\d+)\s*,\s*(-?\d*\.?\d+)\s*\))");
  std::regex re_msg(R"(host_api\.log_message\(\s*["'](.*?)["']\s*\))");

  std::regex re_model(R"(#\s*model:\s*(\w+))");

  while (std::getline(ifs, line)) {
    source_lines_.push_back(line); // Sync source code

    // Metadata: Model Sync
    std::smatch m_meta;
    if (std::regex_search(line, m_meta, re_model)) {
      std::string type_str = m_meta[1];
      std::transform(type_str.begin(), type_str.end(), type_str.begin(),
                     ::toupper);
      if (type_str == "CTU")
        SetAgvTypeInternal(AgvType::CTU);
      else if (type_str == "FORKER")
        SetAgvTypeInternal(AgvType::FORKER);
      else
        SetAgvTypeInternal(AgvType::BASIC);
      LogMessageInternal("[Auto] Synced Model to: " + type_str);
    }
    std::smatch m;
    if (std::regex_search(line, m, re_move)) {
      VisualBlock b;
      b.id = next_block_id_++;
      b.type = BlockType::MOVE;
      b.params["vx"] = std::stof(m[1]);
      b.params["vy"] = std::stof(m[2]);
      b.params["wz"] = std::stof(m[3]);
      blocks_.push_back(b);
    } else if (std::regex_search(line, m, re_delay)) {
      VisualBlock b;
      b.id = next_block_id_++;
      b.type = BlockType::DELAY;
      b.params["ms"] = std::stof(m[1]);
      blocks_.push_back(b);
    } else if (std::regex_search(line, m, re_axis)) {
      VisualBlock b;
      b.id = next_block_id_++;
      b.type = BlockType::AXIS_MOVE;
      b.params["axis"] = std::stof(m[1]);
      b.params["pos"] = std::stof(m[2]);
      b.params["vel"] = std::stof(m[3]);
      blocks_.push_back(b);
    } else if (std::regex_search(line, m, re_msg)) {
      VisualBlock b;
      b.id = next_block_id_++;
      b.type = BlockType::MSG;
      b.message = m[1];
      blocks_.push_back(b);
    }
  }
}
void AppModel::AxisMove(int axis, float pos, float vel) {
  AmrController::Instance().AxisMove(axis, pos, vel);
}

float AppModel::GetAxisPos(int axis) const {
  return AmrController::Instance().GetAxisPos(axis);
}

bool AppModel::IsAxisMoving(int axis) const {
  return AmrController::Instance().IsAxisMoving(axis);
}

void AppModel::UpdatePhysics(float dt) {
  UpdateSafetyLogic(dt);
  // Update particles
  for (int i = (int)particles_.size() - 1; i >= 0; --i) {
    particles_[i].x += particles_[i].vx * dt;
    particles_[i].y += particles_[i].vy * dt;
    particles_[i].life -= dt;
    if (particles_[i].life <= 0)
      particles_.erase(particles_.begin() + i);
  }
}

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

void AppModel::SetTwist(float vx, float vy, float wz) {
  std::lock_guard<std::mutex> lock(mtx_);
  target_twist_ = {vx, vy, wz};
}

Twist AppModel::GetTwist() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return target_twist_;
}

// Private internal version - assumes lock is held
void AppModel::SetAgvTypeInternal(AgvType type) {
  agv_type_ = type;

  // Reset Config logic
  mechanisms_.clear();

  if (type == AgvType::CTU) {
    Mechanism m1;
    m1.id = 3;
    m1.name = "Elevator";
    m1.axis_map = 3;
    m1.type = "LINEAR";
    mechanisms_.push_back(m1);
    Mechanism m2;
    m2.id = 4;
    m2.name = "Cargo Lock";
    m2.axis_map = 4;
    m2.type = "ROTARY";
    mechanisms_.push_back(m2);
  } else if (type == AgvType::FORKER) {
    Mechanism m1;
    m1.id = 3;
    m1.name = "Mast";
    m1.axis_map = 3;
    m1.type = "LINEAR";
    mechanisms_.push_back(m1);
    Mechanism m2;
    m2.id = 4;
    m2.name = "Reach";
    m2.axis_map = 4;
    m2.type = "LINEAR";
    mechanisms_.push_back(m2);
  }
}

void AppModel::SetAgvType(AgvType type) {
  std::lock_guard<std::mutex> lock(mtx_);
  SetAgvTypeInternal(type);
}
AgvType AppModel::GetAgvType() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return agv_type_;
}

IHardware *AppModel::GetHardware() {
  return AmrController::Instance().Hardware();
}

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

void AppModel::LogMessageInternal(const std::string &msg) {
  // Internal: Assumes lock held or not needed if caller ensures safety
  // Actually, we need to be careful. console_logs_ accesses need to be safe.
  // Since SetScriptPath holds the lock, this is safe to call from there.
  // But LogMessage (public) needs to lock.

  std::cout << "[DEBUG] Log: " << msg << std::endl;
  console_logs_.push_back(msg);
  if (console_logs_.size() > 100)
    console_logs_.erase(console_logs_.begin());
}

void AppModel::LogMessage(const std::string &msg) {
  std::lock_guard<std::mutex> lock(mtx_);
  LogMessageInternal(msg);
}

std::vector<std::string> &AppModel::GetLogs() { return console_logs_; }
void AppModel::ClearLogs() {
  std::lock_guard<std::mutex> lock(mtx_);
  console_logs_.clear();
}
void AppModel::SetBlocks(const std::vector<VisualBlock> &blocks) {
  std::lock_guard<std::mutex> lock(mtx_);
  blocks_ = blocks;
}

std::vector<VisualBlock> &AppModel::GetBlocks() { return blocks_; }

void AppModel::SetScriptPath(const std::string &path) {
  ClearSafety();
  SetPaused(false);

  std::lock_guard<std::mutex> lock(mtx_);
  std::cout << "[DEBUG] SetScriptPath Locked: " << path << std::endl;
  script_path_ = path;
  LoadScriptAsBlocksInternal(path);
  std::cout << "[DEBUG] SetScriptPath Done." << std::endl;
}

std::string AppModel::GetScriptPath() const { return script_path_; }
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

void AppModel::SetLocals(const std::map<std::string, std::string> &locals) {
  std::lock_guard<std::mutex> lock(mtx_);
  locals_ = locals;
}
std::map<std::string, std::string> AppModel::GetLocals() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return locals_;
}

bool AppModel::IsRunning() const { return is_running_; }
void AppModel::SetRunning(bool r) { is_running_ = r; }
bool AppModel::IsPaused() const { return is_paused_; }
void AppModel::SetPaused(bool p) {
  is_paused_ = p;
  AmrController::Instance().SetPaused(p);
}
void AppModel::RequestTermination() { should_terminate_ = true; }
void AppModel::ResetTermination() { should_terminate_ = false; }
bool AppModel::ShouldTerminate() const { return should_terminate_; }
void AppModel::ClearSafety() { AmrController::Instance().ClearSafety(); }

void AppModel::WaitForResume() {
  std::unique_lock<std::mutex> lock(mtx_);
  cv_.wait(lock, [this] { return !is_paused_ || should_terminate_; });
}

// --- Visuals ---
std::vector<DrawCmd> AppModel::GetDrawQueue() {
  std::lock_guard<std::mutex> lock(mtx_);
  return draw_queue_;
}
void AppModel::ClearDrawQueue() {
  std::lock_guard<std::mutex> lock(mtx_);
  draw_queue_.clear();
}
void AppModel::PushDrawCmd(const DrawCmd &cmd) {
  std::lock_guard<std::mutex> lock(mtx_);
  draw_queue_.push_back(cmd);
}
std::vector<DrawCmd> AppModel::PopDrawQueue() {
  std::lock_guard<std::mutex> lock(mtx_);
  auto copy = draw_queue_;
  draw_queue_.clear();
  return copy;
}

void AppModel::SpawnParticles(float x, float y, int count, int color) {
  std::lock_guard<std::mutex> lock(mtx_);
  for (int i = 0; i < count; ++i) {
    Particle p;
    p.x = x;
    p.y = y;
    float ang = (rand() % 360) * 0.0174f;
    float spd = 20.0f + (rand() % 50);
    p.vx = cos(ang) * spd;
    p.vy = sin(ang) * spd;
    p.life = 0.5f + (rand() % 10) * 0.1f;
    p.color = (unsigned int)color;
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
  bool val = input_sticky_[key];
  input_sticky_[key] = false;
  return val;
}
void AppModel::SetMousePos(float x, float y) {
  mouse_x_ = x;
  mouse_y_ = y;
}
std::pair<float, float> AppModel::GetMousePos() const {
  return {mouse_x_, mouse_y_};
}
void AppModel::SetMouseDown(int btn, bool val) {
  if (btn >= 0 && btn < 3)
    mouse_down_[btn] = val;
}
bool AppModel::IsMouseDown(int btn) const {
  if (btn >= 0 && btn < 3)
    return mouse_down_[btn];
  return false;
}

// --- Extras ---
void AppModel::RequestScreenshot(const std::string &filename) {
  std::lock_guard<std::mutex> lock(mtx_);
  screenshot_req_ = true;
  screenshot_file_ = filename;
}
bool AppModel::IsScreenshotRequested() { return screenshot_req_; }
std::string AppModel::GetScreenshotFile() const { return screenshot_file_; }
void AppModel::ClearScreenshotRequest() { screenshot_req_ = false; }

void AppModel::SetShakeTimer(float force) {
  SimulatorCore::Instance().SetShakeTimer(force);
}
float AppModel::GetShakeTimer() const {
  return SimulatorCore::Instance().GetShakeTimer();
}
void AppModel::ReduceShakeTimer(float dt) {} // Handled by SimulatorCore

void AppModel::SetAxisCurrentPos(int axis, float pos) {
  AmrController::Instance().SetAxisCurrentPos(axis, pos);
}

void AppModel::AddMechanism(const std::string &name) {
  Mechanism m;
  m.id = (int)mechanisms_.size();
  m.name = name;
  mechanisms_.push_back(m);
}
std::vector<Mechanism> &AppModel::GetMechanisms() { return mechanisms_; }
std::vector<GlobalParam> &AppModel::GetGlobalParams() {
  return AmrController::Instance().GetGlobalParams();
}

void AppModel::SetNextBlockId(int id) { next_block_id_ = id; }
int AppModel::GetNextBlockId() const { return next_block_id_; }
int AppModel::AllocateBlockId() { return next_block_id_++; }

} // namespace amr
