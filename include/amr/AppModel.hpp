#pragma once

#include "amr/Hardware.hpp"
#include "amr/Types.hpp"

// Service Headers
#include "amr/ServiceContext.hpp"
#include "amr/SafetySystem.hpp"
#include "amr/PhysicsContext.hpp"
#include "amr/ScriptExecutor.hpp"
#include "amr/ScriptExecutor.hpp"
#include "amr/EventBus.hpp"
#include "amr/DoubleBuffer.hpp"
#include "amr/DoubleBuffer.hpp"

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace amr {

class AppModel {
public:
  static AppModel &Instance();

  // --- Safety & Input ---
  void UpdateSafetyLogic(float dt);
  void MapInput(int pin, InputAction action, bool invert = false, bool edge_trigger = false);
  void ResetSafetyConfig();
  int GetPinForAction(InputAction action) const;

  AppModel(const AppModel &) = delete;
  AppModel &operator=(const AppModel &) = delete;

  // --- System Control ---
  void SetRunning(bool running); 
  bool IsRunning() const; // Maps to ScriptExecutor::IsRunning

  void SetPaused(bool paused);
  bool IsPaused() const;

  void RequestTermination();
  void ResetTermination();
  bool ShouldTerminate() const; // Deprecated/Mapped to Executor
  void ClearSafety();
  void FullReset();
  void LoadScriptAsBlocks(const std::string &path);
  void LoadScriptFromContent(const std::string &content);

  void WaitForResume(); 

  // --- Motion Control ---
  void AxisMove(int axis, float pos, float vel);
  float GetAxisPos(int axis) const;
  bool IsAxisMoving(int axis) const;

  void UpdatePhysics(float dt);

  // --- I/O Control ---
  void SetDO(int port, bool val);
  bool GetDO(int port) const;
  bool GetDI(int port) const;
  void SetDI(int port, bool val);

  // --- Chassis ---
  void SetTwist(float vx, float vy, float wz);
  Twist GetTwist() const;
  void SetAgvType(AgvType type);
  AgvType GetAgvType() const;

  // --- Hardware ---
  IHardware *GetHardware(); // Legacy

  // --- Registers/Params ---
  void SetReg(int id, float val);
  float GetReg(int id) const;
  float GetParam(const std::string &name) const;
  void SetGlobalParams(const std::vector<GlobalParam> &params);

  // --- Visuals ---
  void PushDrawCmd(const DrawCmd &cmd);
  std::vector<DrawCmd> SwapDrawQueue();

  void SpawnParticles(float x, float y, int count, int color);
  std::vector<Particle>& GetParticles(); 

  // --- Input State (UI) ---
  void SetInputSticky(const std::string &key, bool val);
  bool GetInputSticky(const std::string &key);
  void SetMousePos(float x, float y);
  std::pair<float, float> GetMousePos() const;
  void SetMouseDown(int btn, bool val);
  bool IsMouseDown(int btn) const;

  // --- Logging ---
  void LogMessage(const std::string &msg);
  std::vector<std::string> SwapLogs();

  // --- Extras ---
  void ClearDrawQueue(); // Added

  // --- Extras ---
  void RequestScreenshot(const std::string &filename);
  bool IsScreenshotRequested();
  std::string GetScreenshotFile() const;
  void ClearScreenshotRequest();

  void SetShakeTimer(float force);
  float GetShakeTimer() const;
  void ReduceShakeTimer(float dt);

  void SetAxisCurrentPos(int axis, float pos);

  // --- Data Access ---
  void AddMechanism(const std::string &name);
  std::vector<Mechanism> &GetMechanisms();
  std::vector<VisualBlock> &GetBlocks();
  void SetBlocks(const std::vector<VisualBlock> &blocks);
  std::vector<GlobalParam> &GetGlobalParams();

  // --- Script Helpers ---
  void SetNextBlockId(int id);
  int GetNextBlockId() const;
  int AllocateBlockId();

  void SetScriptPath(const std::string &path);
  std::string GetScriptPath() const;
  void SetSourceLines(const std::vector<std::string> &lines);
  std::vector<std::string> GetSourceLines() const;
  void SetLocals(const std::map<std::string, std::string> &locals);
  std::map<std::string, std::string> GetLocals() const;
  int GetCurrentLine() const;
  void SetCurrentLine(int line);

private:
  AppModel();
  ~AppModel() = default;

  mutable std::mutex mtx_;
  std::condition_variable cv_;

  // Service Accessor Helpers
  // std::shared_ptr<SafetySystem> Safety() const; // Implementation detail

  // Legacy/UI Data (Still kept in AppModel as Facade for UI)
  std::vector<Mechanism> mechanisms_;
  std::vector<VisualBlock> blocks_;
  
  // Double Buffers
  DoubleBuffer<DrawCmd> draw_queue_; 
  DoubleBuffer<std::string> console_logs_;
  
  // Script Metadata (Cached for UI)
  // Actually ScriptExecutor has locals/current line. 
  // But AppModel needs to expose them. 
  // We can fetch from Service. 
  // Keep source_lines here as it's static data loaded from file? 
  // ScriptExecutor doesn't necessarily store source lines (it runs file).
  // AppModel acts as "IDE State" too.
  std::vector<std::string> source_lines_;
  std::string script_path_ = "../scripts/snake_game.py";

  int next_block_id_ = 0;

  // Chassis State
  Twist target_twist_ = {0, 0, 0};
  AgvType agv_type_ = AgvType::BASIC;
  float accumulator_ = 0.0f;
  
  // IO
  std::atomic<bool> screenshot_req_{false};
  std::string screenshot_file_;
  
  // Input UI
  std::map<std::string, bool> input_sticky_;
  float mouse_x_ = 0, mouse_y_ = 0;
  bool mouse_down_[3] = {false};

  // Interpolation State
  std::map<int, float> axis_targets_;
  std::map<int, float> axis_velocities_;
  // We use PhysicsContext as 'Current' source of truth for X/Y/Theta
  // For other axes (Lift), we need storage if SimHardware doesn't persist properly
  // or just use SimHardware->GetAxisData for current.

  void LoadScriptAsBlocksInternal(const std::string &path);
  void SetAgvTypeInternal(AgvType type);
  void LogMessageInternal(const std::string &msg);
};

} // namespace amr
