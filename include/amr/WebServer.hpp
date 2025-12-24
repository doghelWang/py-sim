#pragma once

#include "amr/AppModel.hpp"
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include "amr/EventBus.hpp"

// Forward declaration to avoid full header inclusion if possible
namespace httplib { 
    class Server; 
    class DataSink; 
}

namespace amr {

class WebServer {
public:
    WebServer(AppModel& model);
    ~WebServer();

    void Start(int port);
    void Stop();

    // Helper to serialize state
    std::string GetStatusJson();

    // WebSocket Broadcasting
    void Broadcast(const std::string& message);

    // Event Handling
    void OnEvent(const Event& e);

private:
    void SetupRoutes();
    
    // SSE Helper
    void SseLoop(httplib::DataSink& sink);

    AppModel& m_model;
    std::unique_ptr<httplib::Server> m_server;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    
    // Event Queue for SSE
    std::mutex m_q_mtx;
    std::condition_variable m_q_cv;
    std::vector<Event> m_event_queue; // Using vector as queue
};

}
