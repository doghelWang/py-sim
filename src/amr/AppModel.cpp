#include "amr/AppModel.hpp"
#include "amr/AmrController.hpp"
#include "amr/ScriptExecutor.hpp"
#include "amr/PhysicsContext.hpp"
#include <fstream>
#include <iostream>

namespace amr {

AppModel &AppModel::Instance() {
  static AppModel instance;
  return instance;
}

AppModel::AppModel() {
  auto& ctx = ServiceContext::Instance();
  if (!ctx.Get<EventBus>()) ctx.Register(std::make_shared<EventBus>());
  if (!ctx.Get<SafetySystem>()) ctx.Register(std::make_shared<SafetySystem>());
  if (!ctx.Get<PhysicsContext>()) ctx.Register(std::make_shared<PhysicsContext>());
  if (!ctx.Get<ScriptExecutor>()) ctx.Register(std::make_shared<ScriptExecutor>());
  // Register Hardware
  if (!ctx.Get<IHardware>()) ctx.Register(std::make_shared<SimHardware>());
}

// --- Safety & Input ---
void AppModel::UpdateSafetyLogic(float dt) {
  auto safety = ServiceContext::Instance().Get<SafetySystem>();
  if (safety) safety->Update(dt);
  AmrController::Instance().Update(dt);
}

void AppModel::UpdatePhysics(float dt) {
    UpdateSafetyLogic(dt);

    auto safety = ServiceContext::Instance().Get<SafetySystem>();
    if (safety && safety->IsPaused()) return;

    auto phys = ServiceContext::Instance().Get<PhysicsContext>();
    if (phys) {
        // 1. Physics Engine Step (Particles etc)
        const float TIME_STEP = 0.01f;
        accumulator_ += dt;
        if (accumulator_ > 0.1f) accumulator_ = 0.1f;
        while (accumulator_ >= TIME_STEP) {
             phys->UpdateChassis(target_twist_, TIME_STEP);
             accumulator_ -= TIME_STEP;
        }

        // 2. Interpolation Logic (PTP Motion)
        auto odom = phys->GetOdometry();
        bool dirty = false;

        // Helper to update axis
        // Note: Odometry usually uses double for precision.
        auto update_axis = [&](int axis, double& current_val) {
            if (axis_targets_.find(axis) != axis_targets_.end()) {
                float target = axis_targets_[axis];
                float vel = axis_velocities_[axis];
                if (vel < 0.001f) vel = 0.1f; // Safety min vel

                float diff = target - (float)current_val;
                if (std::abs(diff) > 0.001f) {
                    float step = vel * dt;
                    if (std::abs(diff) <= step) {
                        current_val = target;
                    } else {
                        current_val += (diff > 0 ? 1 : -1) * step;
                    }
                    dirty = true;
                    // Also update hardware for telemetry immediately
                    SetAxisCurrentPos(axis, (float)current_val);
                }
            }
        };

        // Axis 0 (X), 1 (Y), 2 (Theta)
        update_axis(0, odom.x);
        update_axis(1, odom.y); // Note: Y is Y
        update_axis(2, odom.theta);

        if (dirty) {
            phys->SetOdometry(odom);
        }
        
        // Also sync hardware (redundant but safe)
        auto* hw = GetHardware();
        if (hw) {
            hw->SetAxisData(0, (float)odom.x);
            hw->SetAxisData(1, (float)odom.y);
            hw->SetAxisData(2, (float)odom.theta);
            
            // Handle Lift (Axis 3) - Logic without Physics Odom
            // Check if Axis 3 has target
             if (axis_targets_.find(3) != axis_targets_.end()) {
                 // We don't have 'current' in Odom. Need to fetch from HW or store locally?
                 // Let's fetch from HW.
                 float current = hw->GetAxisPos(3);
                 float target = axis_targets_[3];
                 float vel = axis_velocities_[3];
                 
                 float diff = target - current;
                 if (std::abs(diff) > 0.001f) {
                     float step = vel * dt;
                     if (std::abs(diff) <= step) current = target;
                     else current += (diff > 0 ? 1 : -1) * step;
                     hw->SetAxisData(3, current);
                 }
             }
        }
    }
}

// --- System Control ---
void AppModel::SetRunning(bool running) {
    auto exec = ServiceContext::Instance().Get<ScriptExecutor>();
    if (exec) {
        if(running && !exec->IsRunning()) {
            // Can't run without script path? 
            // LoadScriptFromContent handles the run.
        } else if (!running) {
             exec->StopScript();
        }
    }
}

bool AppModel::IsRunning() const {
    auto exec = ServiceContext::Instance().Get<ScriptExecutor>();
    return exec ? exec->IsRunning() : false;
}

// --- Hardware Access ---
IHardware *AppModel::GetHardware() {
    return ServiceContext::Instance().Get<IHardware>().get();
}

// --- Scripting ---
void AppModel::LoadScriptFromContent(const std::string &content) {
    std::cerr << "[AppModel] LoadScriptFromContent called. Size: " << content.size() << std::endl;
    auto exec = ServiceContext::Instance().Get<ScriptExecutor>();
    if (exec) {
        std::cerr << "[AppModel] Found Executor. Running..." << std::endl;
        std::string temp_path = "temp_script.py";
        std::ofstream out(temp_path);
        out << content;
        out.close();
        exec->RunScript(temp_path);
    } else {
        std::cerr << "[AppModel] ERROR: No ScriptExecutor!" << std::endl;
    }
}

void AppModel::WaitForResume() {
    auto exec = ServiceContext::Instance().Get<ScriptExecutor>();
    if (exec) exec->WaitForResume();
}

void AppModel::AxisMove(int axis, float pos, float vel) {
    // Set Target and Velocity. UpdatePhysics will handle the move.
    std::cout << "[AppModel] AxisMove Request: Axis=" << axis << " Pos=" << pos << " Vel=" << vel << std::endl;
    
    // Ensure map entries exist
    axis_targets_[axis] = pos;
    axis_velocities_[axis] = vel;
    
    // Safety: If velocity is 0, instant set? No, keep it 0. User error?
    // If vel 0, it won't move. HostApi checks vel > 0.
} 
float AppModel::GetAxisPos(int) const { return 0; }
bool AppModel::IsAxisMoving(int) const { return false; }
void AppModel::SetDO(int, bool) {}
bool AppModel::GetDO(int) const { return false; }
bool AppModel::GetDI(int) const { return false; }
void AppModel::SetDI(int, bool) {}
void AppModel::SetTwist(float vx, float vy, float wz) { target_twist_ = {vx, vy, wz}; }
Twist AppModel::GetTwist() const { return target_twist_; }
void AppModel::SetAgvType(AgvType type) { agv_type_ = type; }
AgvType AppModel::GetAgvType() const { return agv_type_; }
void AppModel::SetReg(int, float) {}
float AppModel::GetReg(int) const { return 0; }
float AppModel::GetParam(const std::string &) const { return 0; }
void AppModel::SetGlobalParams(const std::vector<GlobalParam> &) {}
void AppModel::PushDrawCmd(const DrawCmd &) {}
std::vector<DrawCmd> AppModel::SwapDrawQueue() { return {}; }
void AppModel::SpawnParticles(float, float, int, int) {}
std::vector<Particle>& AppModel::GetParticles() { static std::vector<Particle> p; return p; }
void AppModel::SetInputSticky(const std::string &, bool) {}
bool AppModel::GetInputSticky(const std::string &) { return false; }
void AppModel::SetMousePos(float, float) {}
std::pair<float, float> AppModel::GetMousePos() const { return {0,0}; }
void AppModel::SetMouseDown(int, bool) {}
bool AppModel::IsMouseDown(int) const { return false; }
void AppModel::LogMessage(const std::string &msg) { LogMessageInternal(msg); }
std::vector<std::string> AppModel::SwapLogs() { return {}; }
void AppModel::ClearDrawQueue() {}
void AppModel::RequestScreenshot(const std::string &) {}
bool AppModel::IsScreenshotRequested() { return false; }
std::string AppModel::GetScreenshotFile() const { return ""; }
void AppModel::ClearScreenshotRequest() {}
void AppModel::SetShakeTimer(float) {}
float AppModel::GetShakeTimer() const { return 0; }
void AppModel::ReduceShakeTimer(float) {}
void AppModel::SetAxisCurrentPos(int axis, float pos) {
    auto hw = ServiceContext::Instance().Get<IHardware>();
    if (hw) {
        hw->SetAxisData(axis, pos);
    }
}
// --- Legacy Stubs (Restored) ---
void AppModel::MapInput(int, InputAction, bool, bool) {}
void AppModel::ResetSafetyConfig() {}
int AppModel::GetPinForAction(InputAction) const { return -1; }
void AppModel::SetPaused(bool) {}
bool AppModel::IsPaused() const { return false; }
void AppModel::RequestTermination() {}
void AppModel::ResetTermination() {}
bool AppModel::ShouldTerminate() const { return false; }
void AppModel::ClearSafety() {}
void AppModel::FullReset() {}
void AppModel::LoadScriptAsBlocks(const std::string &) {}

void AppModel::AddMechanism(const std::string &) {}
std::vector<Mechanism> &AppModel::GetMechanisms() { static std::vector<Mechanism> m; return m; }
std::vector<VisualBlock> &AppModel::GetBlocks() { static std::vector<VisualBlock> b; return b; }
void AppModel::SetBlocks(const std::vector<VisualBlock> &) {}
std::vector<GlobalParam> &AppModel::GetGlobalParams() { static std::vector<GlobalParam> p; return p; }
void AppModel::SetNextBlockId(int) {}
int AppModel::GetNextBlockId() const { return 0; }
int AppModel::AllocateBlockId() { return 0; }
void AppModel::SetScriptPath(const std::string &) {}
std::string AppModel::GetScriptPath() const { return ""; }
void AppModel::SetSourceLines(const std::vector<std::string> &) {}
std::vector<std::string> AppModel::GetSourceLines() const { return {}; }
void AppModel::SetLocals(const std::map<std::string, std::string> &) {}
std::map<std::string, std::string> AppModel::GetLocals() const { return {}; }
int AppModel::GetCurrentLine() const { return 0; }
void AppModel::SetCurrentLine(int) {}

void AppModel::LogMessageInternal(const std::string &msg) {
  std::cout << "[LOG] " << msg << std::endl;
}

} // namespace amr