#pragma once

#include "ServiceContext.hpp"
#include "EventBus.hpp"
#include "Hardware.hpp"
#include "Types.hpp"
#include <map>
#include <mutex>
#include <vector>

namespace amr {

class SafetySystem : public IService {
public:
    SafetySystem();
    ~SafetySystem() override = default;

    void Initialize() override;
    
    // Core Logic
    void Update(float dt);

    // Configuration
    void ConfigureInput(int pin, InputAction action, bool invert, bool edge_trigger);
    void ResetConfig();
    int GetPinForAction(InputAction action) const;

    // State Access
    bool IsEstopActive() const;
    bool IsPaused() const;
    void SetPaused(bool paused);
    void ClearSafety(); // Reset Estop and Pause

private:
    void Log(const std::string& msg);

    std::mutex mtx_;
    std::shared_ptr<EventBus> event_bus_;
    std::shared_ptr<IHardware> hardware_; // Dependency

    std::map<int, InputConfig> input_map_;
    std::vector<bool> last_di_state_;
    
    bool estop_active_ = false;
    bool is_paused_ = false;
};

} // namespace amr
