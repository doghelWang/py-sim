#include "amr/SafetySystem.hpp"
#include "amr/ServiceContext.hpp"
#include <iostream>

namespace amr {

SafetySystem::SafetySystem() {
    last_di_state_.resize(32, false);
}

void SafetySystem::Initialize() {
    auto& ctx = ServiceContext::Instance();
    event_bus_ = ctx.Get<EventBus>();
    // hardware_ = ctx.Get<IHardware>(); // TODO: Register Hardware Service
}

void SafetySystem::Log(const std::string& msg) {
    std::cout << "[Safety] " << msg << std::endl;
    // Ideally publish log event
    if (event_bus_) {
        // event_bus_->Publish(EventType::SCRIPT_LOG, msg);
    }
}

void SafetySystem::Update(float dt) {
    if (!hardware_) {
        // Try to lazy load hardware if not set
        hardware_ = ServiceContext::Instance().Get<IHardware>();
        if (!hardware_) return;
    }

    std::lock_guard<std::mutex> lock(mtx_);

    // Safety Logic Ported from AmrController
    for (auto &pair : input_map_) {
        int pin = pair.first;
        const auto &cfg = pair.second;

        if (pin < 0 || pin >= 32) continue;

        bool raw_val = hardware_->GetDI(pin);
        bool active = cfg.invert ? !raw_val : raw_val;

        bool was_active = last_di_state_[pin];
        bool rising_edge = active && !was_active;

        last_di_state_[pin] = active;

        switch (cfg.action) {
            case InputAction::ESTOP:
                if (active) {
                    if (!estop_active_) {
                        estop_active_ = true;
                        is_paused_ = true;
                        Log("E-STOP TRIGGERED! System Paused.");
                        if(event_bus_) event_bus_->Publish(EventType::E_STOP_TRIGGERED);
                    } else if (!is_paused_) {
                        is_paused_ = true;
                        Log("E-STOP ACTIVE. Cannot Resume.");
                    }
                } else {
                    if (estop_active_) {
                        estop_active_ = false;
                        Log("E-Stop Released. Please Resume manually.");
                        if(event_bus_) event_bus_->Publish(EventType::E_STOP_CLEARED);
                    }
                }
                break;

            case InputAction::PAUSE_TOGGLE:
                if (rising_edge) {
                    if (estop_active_) {
                        Log("Ignored Pause Toggle (E-Stop Active).");
                    } else {
                        is_paused_ = !is_paused_;
                        Log(is_paused_ ? "Pause Toggled -> PAUSED" : "Pause Toggled -> RESUMED");
                        if(event_bus_) event_bus_->Publish(EventType::PAUSE_TOGGLED, is_paused_);
                    }
                }
                break;

            case InputAction::HOME_ALL:
                if (rising_edge) {
                     if (is_paused_ || estop_active_) {
                        Log("Ignored Home All (System Paused/Stopped).");
                    } else {
                        Log("Home All Triggered.");
                        // How to trigger Home? 
                        // Option 1: SafetySystem shouldn't directly control axes.
                        // Option 2: Publish Event HOME_TRIGGERED
                        // Implementation:
                        // for (int i = 0; i < 3; ++i) hardware_->AxisMove(i, 0.0f, 5.0f);
                        // Better to publish event.
                        // For compatibility during refactor, we can access hardware directly if needed,
                        // but ideally we publish event.
                        // For now: direct hardware access to match legacy behavior
                        for (int i = 0; i < 3; ++i)
                             hardware_->AxisMove(i, 0.0f, 5.0f);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void SafetySystem::ConfigureInput(int pin, InputAction action, bool invert, bool edge_trigger) {
    std::lock_guard<std::mutex> lock(mtx_);
    InputConfig cfg;
    cfg.action = action;
    cfg.invert = invert;
    cfg.edge_trigger = edge_trigger;
    input_map_[pin] = cfg;
}

void SafetySystem::ResetConfig() {
    std::lock_guard<std::mutex> lock(mtx_);
    input_map_.clear();
    estop_active_ = false;
    Log("Config Reset.");
}

int SafetySystem::GetPinForAction(InputAction action) const {
    // No lock needed for simple iteration if map isn't changing concurrently often
    // But better safe
    // std::lock_guard<std::mutex> lock(mtx_); // Cant allow const
    for (const auto &pair : input_map_) {
        if (pair.second.action == action) return pair.first;
    }
    return -1;
}

bool SafetySystem::IsEstopActive() const {
    return estop_active_;
}

bool SafetySystem::IsPaused() const {
    return is_paused_;
}

void SafetySystem::SetPaused(bool paused) {
    std::lock_guard<std::mutex> lock(mtx_);
    is_paused_ = paused;
    if(event_bus_) event_bus_->Publish(EventType::PAUSE_TOGGLED, is_paused_);
}

void SafetySystem::ClearSafety() {
    std::lock_guard<std::mutex> lock(mtx_);
    estop_active_ = false;
    is_paused_ = false;
    Log("Safety Cleared manually.");
    if(event_bus_) {
        event_bus_->Publish(EventType::E_STOP_CLEARED);
        event_bus_->Publish(EventType::PAUSE_TOGGLED, false);
    }
}

} // namespace amr
