#pragma once

#include "ServiceContext.hpp"
#include <functional>
#include <vector>
#include <map>
#include <mutex>
#include <any>

namespace amr {

enum class EventType {
    E_STOP_TRIGGERED,
    E_STOP_CLEARED,
    PAUSE_TOGGLED,
    SCRIPT_STARTED,
    SCRIPT_FINISHED,
    SCRIPT_LOG,
    SCRIPT_TRACE,
    PHYSICS_UPDATE
};

struct Event {
    EventType type;
    std::any data;
};

using EventHandler = std::function<void(const Event&)>;

class EventBus : public IService {
public:
    void Publish(EventType type, std::any data = {}) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (subscribers_.find(type) != subscribers_.end()) {
            for (auto& handler : subscribers_[type]) {
                handler({type, data});
            }
        }
    }

    void Subscribe(EventType type, EventHandler handler) {
        std::lock_guard<std::mutex> lock(mtx_);
        subscribers_[type].push_back(handler);
    }

private:
    std::mutex mtx_;
    std::map<EventType, std::vector<EventHandler>> subscribers_;
};

} // namespace amr
