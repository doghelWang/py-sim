#pragma once

#include "amr/Types.hpp"
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace amr {

class AppModel {
public:
  // 定义输入与安全动作枚举
  enum class InputAction {
    NONE = 0,
    ESTOP,        // 急停 (Hold to Stop): High=Stop, Low=Resume (if recovered)
    PAUSE_TOGGLE, // 暂停/恢复切换 (Edge Trigger)
    HOME_ALL,     // 全部回零 (Edge Trigger)
  };

  struct InputConfig {
    InputAction action = InputAction::NONE;
    bool invert = false;       // True: Low Active, False: High Active
    bool edge_trigger = false; // True: Action on Edge, False: Action on Level
  };

  // 单例访问器 (用于主要访问点)
  static AppModel &Instance();
  // --- 安全与输入框架 (Safety & Input) ---
  void UpdateSafetyLogic(float dt); // 在主循环调用
  void MapInput(int pin, InputAction action, bool invert = false,
                bool edge_trigger = false);
  void ResetSafetyConfig(); // Clear all input mappings

  // 禁止拷贝
  AppModel(const AppModel &) = delete;
  AppModel &operator=(const AppModel &) = delete;

  // --- 系统控制 (System Control) ---
  void SetRunning(bool running); // 设置运行状态
  bool IsRunning() const;

  void SetPaused(bool paused); // 设置暂停状态
  bool IsPaused() const;

  void RequestTermination(); // 请求终止脚本
  void ResetTermination();   // 重置终止标志 (新运行前)
  bool ShouldTerminate() const;

  void WaitForResume(); // 如果处于暂停状态，阻塞当前线程，直到恢复或终止

  // --- 运动控制 (Motion Control) ---
  void SetAxisTarget(int axis, float pos, float vel); // 设置轴目标位置
  float GetAxisPos(int axis) const;                   // 获取当前轴位置
  bool IsAxisMoving(int axis) const;                  // 判断轴是否在运动

  // 物理更新逻辑 (在主循环中调用)
  void UpdatePhysics(float dt);

  // --- I/O 控制 (I/O Control) ---
  void SetDO(int port, bool val); // 设置数字输出
  bool GetDO(int port) const;
  bool GetDI(int port) const;     // 获取数字输入
  void SetDI(int port, bool val); // 设置数字输入 (用于仿真/GUI模拟)

  // --- 寄存器与参数 (Register & Params) ---
  void SetReg(int id, float val); // 设置内部寄存器 (R0-R31)
  float GetReg(int id) const;
  float GetParam(const std::string &name) const; // 获取全局配置参数
  void SetGlobalParams(const std::vector<GlobalParam> &params);

  // --- 视觉绘制 (Visuals) ---
  std::vector<DrawCmd> GetDrawQueue();  // 获取绘制队列副本
  void ClearDrawQueue();                // 清空队列
  void PushDrawCmd(const DrawCmd &cmd); // 添加绘制指令
  std::vector<DrawCmd> PopDrawQueue();  // 获取并清空 (原子操作)

  // 粒子系统 (Particles)
  void SpawnParticles(float x, float y, int count,
                      int color);        // 简化的粒子生成API
  std::vector<Particle> &GetParticles(); // Mutable access for Physics

  // --- Input State ---
  void SetInputSticky(const std::string &key, bool val);
  bool GetInputSticky(const std::string &key); // Reads and Clears
  void SetMousePos(float x, float y);
  std::pair<float, float> GetMousePos() const;
  void SetMouseDown(int btn, bool val);
  bool IsMouseDown(int btn) const;

  // --- logging ---
  void LogMessage(const std::string &msg);
  std::string GetLog() const;

  // --- Extras (Screenshot, Shake) ---
  void RequestScreenshot(const std::string &filename);
  bool IsScreenshotRequested(); // Returns true and clears flag? Or checks?
  // Let's say checks. But we need a way to consume valid filename?
  // Let's make: std::string ConsumeScreenshotRequest(); // returns empty if
  // none
  std::string GetScreenshotFile() const;
  void ClearScreenshotRequest();

  void SetShakeTimer(float force);
  float GetShakeTimer() const;
  void ReduceShakeTimer(float dt);

  // --- Motion Extras ---
  void SetAxisCurrentPos(int axis, float pos);

  // --- 数据成员访问 (Data Member Access) ---
  // 用于GUI编辑器直接操作数据
  std::vector<Mechanism> &GetMechanisms(); // 获取机构列表 (由配置编辑器使用)
  std::vector<VisualBlock> &GetBlocks();   // 获取可视化块列表
  std::vector<GlobalParam> &GetGlobalParams(); // 获取全局参数

  // --- 脚本辅助 (Script Helpers) ---
  void SetNextBlockId(int id);
  int GetNextBlockId() const;
  int AllocateBlockId(); // 分配下一个块ID并递增

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

  // Safety & Input State
  std::map<int, InputConfig> input_map_;
  std::vector<bool> last_di_state_;
  bool estop_active_ = false;

  // State
  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_paused_{false};
  std::atomic<bool> should_terminate_{false};

  // Hardware
  Axis axes_[3];
  bool di_[8] = {false};
  bool do_[8] = {false};
  float registers_[32] = {0};

  // Data
  std::vector<Mechanism> mechanisms_;
  std::vector<GlobalParam> params_;
  std::vector<VisualBlock> blocks_;

  // Visuals
  std::vector<DrawCmd> draw_queue_;
  std::vector<Particle> particles_;

  std::string console_log_;

  // Extras
  std::atomic<bool> screenshot_req_{false};
  std::string screenshot_file_;
  float shake_timer_ = 0.0f;

  // Input
  std::map<std::string, bool> input_sticky_;
  float mouse_x_ = 0, mouse_y_ = 0;
  bool mouse_down_[3] = {false};

  // Script Data
  std::string script_path_ = "../scripts/snake_game.py";
  std::vector<std::string> source_lines_;
  std::map<std::string, std::string> locals_;
  int current_line_ = 0;
  int next_block_id_ = 0;
};

} // namespace amr
