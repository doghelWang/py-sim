#pragma once

#include "ServiceContext.hpp"
#include "EventBus.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <vector>

namespace amr {

class ScriptExecutor : public IService {
public:
    ScriptExecutor();
    ~ScriptExecutor() override;

    void Initialize() override;
    void Shutdown() override;

    void RunScript(const std::string& path);
    void StopScript();

    // State
    bool IsRunning() const;
    void SetCurrentLine(int line);
    int GetCurrentLine() const;
    void SetLocals(const std::map<std::string, std::string>& locals);
    std::map<std::string, std::string> GetLocals() const;
    
    // Helper for trace function
    void WaitForResume(); // Blocks if paused

private:
    void WorkerEntry(const std::string& path);

    std::shared_ptr<EventBus> event_bus_;
    
    std::thread worker_thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_terminate_{false};

    // Debug Info
    mutable std::mutex mtx_;
    int current_line_ = 0;
    std::map<std::string, std::string> locals_;
};

} // namespace amr
