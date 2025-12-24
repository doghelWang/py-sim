#include "amr/WebServer.hpp"
#include "httplib.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <set>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace amr {

// WebSocket broadcasting is deferred to Phase 2 (SSE/Long Polling preferred for now)
// We focus on robust HTTP APIs first.

WebServer::WebServer(AppModel& model) : m_model(model) {
    m_server = std::make_unique<httplib::Server>();

    // Subscribe to Events
    auto bus = ServiceContext::Instance().Get<EventBus>();
    if (bus) {
        auto handler = [this](const Event& e) { this->OnEvent(e); };
        bus->Subscribe(EventType::SCRIPT_TRACE, handler);
        bus->Subscribe(EventType::SCRIPT_LOG, handler);
        bus->Subscribe(EventType::SCRIPT_STARTED, handler);
        bus->Subscribe(EventType::SCRIPT_FINISHED, handler);
    }
}

WebServer::~WebServer() {
    Stop();
}

void WebServer::Start(int port) {
    if (m_running) return;
    
    SetupRoutes();

    m_running = true;
    m_thread = std::thread([this, port]() {
        std::cout << "Server listening on http://localhost:" << port << std::endl;
        m_server->listen("0.0.0.0", port);
    });
}

void WebServer::Stop() {
    if (m_running) {
        m_server->stop();
        if (m_thread.joinable()) m_thread.join();
        m_running = false;
    }
}

void WebServer::Broadcast(const std::string& message) {
    // Deprecated for SSE
}

void WebServer::OnEvent(const Event& e) {
    std::cout << "[WebServer] OnEvent Enqueue: " << (int)e.type << std::endl;
    std::lock_guard<std::mutex> lock(m_q_mtx);
    m_event_queue.push_back(e);
    m_q_cv.notify_all();
}

void WebServer::SseLoop(httplib::DataSink& sink) {
    // Only one client supported perfectly? Or shared queue?
    // Shared queue means one client steals events.
    // Ideally we need per-client queues or broadcast.
    // For MVP, we assume SINGLE GUI client or acceptable race.
    // Actually, we can just broadcast the event to the sink if we hold the sink?
    // No, httplib calls this function per request.
    
    // Better: We loop here.
    // Problem: If multiple clients, they compete for m_event_queue.
    // FIX: Local last_index logic? No, queue is cleared?
    // We'll just assume Single Client (Pro Node).
    
    while (m_running && sink.is_writable()) {
        std::vector<Event> local_events;
        {
            std::unique_lock<std::mutex> lock(m_q_mtx);
            // Wait for event or timeout (for telemetry 20Hz = 50ms)
            m_q_cv.wait_for(lock, std::chrono::milliseconds(50), [this]{ return !m_event_queue.empty(); });
            
            // Move events
            local_events = m_event_queue;
            m_event_queue.clear(); 
        }

        // 1. Send Events
        for(const auto& e : local_events) {
            std::string type = "unknown";
            json data;
            
            if (e.type == EventType::SCRIPT_TRACE) {
                type = "trace";
                auto pair = std::any_cast<std::pair<std::string, int>>(e.data);
                data = {{"file", pair.first}, {"line", pair.second}};
            } else if (e.type == EventType::SCRIPT_LOG) {
                type = "log";
                data = {{"msg", std::any_cast<std::string>(e.data)}}; 
            } else if (e.type == EventType::SCRIPT_STARTED) {
                type = "started";
            } else if (e.type == EventType::SCRIPT_FINISHED) {
                type = "finished";
            }

            std::stringstream ss;
            ss << "event: " << type << "\n";
            ss << "data: " << data.dump() << "\n\n";
            std::string s = ss.str();
            sink.write(s.data(), s.size());
        }

        // 2. Send Telemetry (Heartbeat) every loop
        std::string status = GetStatusJson();
        std::stringstream ss;
        ss << "event: telemetry\n";
        ss << "data: " << status << "\n\n";
        std::string s = ss.str();
        sink.write(s.data(), s.size());
        
        // Sleep slightly to throttle Telemetry if queue was empty immediately
        // (Wait_for already handled 50ms delay if empty, so we are good at ~20Hz)
    }
}

void WebServer::SetupRoutes() {
    // 1. Static Files
    // Force Absolute Path for debugging/reliability
    std::string web_root = "D:/code/py-sim/web_root";
    if (!fs::exists(web_root)) {
        std::cerr << "ERROR: web_root not found at: " << web_root << std::endl;
    }
    bool ret = m_server->set_mount_point("/", web_root);
    if (!ret) {
        std::cerr << "WARNING: Could not mount web_root (Absolute) !" << std::endl;
        // Fallback
         ret = m_server->set_mount_point("/", "./web_root");
    }
    if (!ret) {
         std::cerr << "WARNING: Could not mount web_root (All variants) !" << std::endl;
    }

    // --- SSE Stream (NEW) ---
    m_server->Get("/api/stream", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content_provider("text/event-stream", [&](size_t offset, httplib::DataSink &sink) {
            this->SseLoop(sink);
            return true; // Keep open? With set_content_provider, usually we loop until done.
            // Actually, in cpp-httplib, if we return true, it might call us again?
            // "The content provider is called repeatedly until it returns false."
            // But if we loop INSIDE SseLoop, we block the thread.
            // This is valid for chunked encoding as long as we write chunks.
        });
    });

    // --- File Management ---
    // List Scripts
    m_server->Get("/api/scripts", [](const httplib::Request&, httplib::Response& res) {
        json j = json::array();
        std::string scripts_dir = "D:/code/py-sim/scripts/";
        if (fs::exists(scripts_dir)) {
            for (const auto& entry : fs::directory_iterator(scripts_dir)) {
                if (entry.path().extension() == ".py") {
                    j.push_back(entry.path().filename().string());
                }
            }
        }
        res.set_content(j.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // Read Script
    m_server->Get(R"(/api/scripts/(.*))", [](const httplib::Request& req, httplib::Response& res) {
        std::string name = req.matches[1];
        std::string scripts_dir = "D:/code/py-sim/scripts/";
        std::string path = scripts_dir + name;
        std::ifstream in(path);
        if (in.is_open()) {
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            res.set_content(content, "text/plain");
        } else {
            res.status = 404;
        }
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // Save Script
    m_server->Post(R"(/api/scripts/(.*))", [](const httplib::Request& req, httplib::Response& res) {
        std::string name = req.matches[1];
        std::string scripts_dir = "D:/code/py-sim/scripts/";
        std::string path = scripts_dir + name;
        std::ofstream out(path);
        if (out.is_open()) {
            out << req.body;
            res.set_content("{\"status\":\"saved\"}", "application/json");
        } else {
            res.status = 500;
        }
        res.set_header("Access-Control-Allow-Origin", "*");
    });


    // --- Status & IO ---

    // API: Config (Hardware Definition)
    m_server->Get("/api/config", [this](const httplib::Request&, httplib::Response& res) {
         json j;
         // Robot Physical Config
         j["robot_type"] = "AMR_v1";
         j["dimensions"] = {{"width", 0.6}, {"length", 0.9}, {"height", 0.3}};
         
         j["axes"] = {
             {{"id",0}, {"name","X-Axis (Locomotion)"}, {"type","linear"}, {"unit","mm"}, {"min",-5000}, {"max",5000}},
             {{"id",1}, {"name","Y-Axis (Locomotion)"}, {"type","linear"}, {"unit","mm"}, {"min",-5000}, {"max",5000}},
             {{"id",2}, {"name","Theta"}, {"type","rotary"}, {"unit","rad"}, {"min",-3.14}, {"max",3.14}},
             {{"id",3}, {"name","Lift"}, {"type","linear"}, {"unit","mm"}, {"min",0}, {"max",1000}}
         };
         
         j["sensors"] = {
             {
                 {"id", "lidar_front"}, 
                 {"type", "lidar"}, 
                 {"mount", {{"parent","base_link"}, {"x",0.4}, {"y",0.0}, {"z",0.2}, {"roll",0}, {"pitch",0}, {"yaw",0}}}
             }
         };

         j["io"] = {
             {{"id",0}, {"name","Estop"}, {"type","DI"}},
             {{"id",0}, {"name","Contactor"}, {"type","DO"}},
             {{"id",1}, {"name","Brake"}, {"type","DO"}}
         };
         res.set_content(j.dump(), "application/json");
         res.set_header("Access-Control-Allow-Origin", "*");
    });

    // API: Schema (Block Library)
    m_server->Get("/api/schema", [this](const httplib::Request&, httplib::Response& res) {
        json j = json::array({
            {{"op","move_axis_abs"}, {"name","Move Abs"}, {"cat","Motion"}, {"icon","fa-arrow-right"}, {"args",{"axis","pos","vel"}}},
            {{"op","set_twist"}, {"name","Chassis Drive"}, {"cat","Motion"}, {"icon","fa-car"}, {"args",{"vx","vy","w"}}},
            {{"op","sleep_ms"}, {"name","Wait"}, {"cat","Logic"}, {"icon","fa-clock"}, {"args",{"ms"}}},
            {{"op","set_do"}, {"name","Set Output"}, {"cat","I/O"}, {"icon","fa-toggle-on"}, {"args",{"id","val"}}},
            {{"op","print"}, {"name","Log Message"}, {"cat","Logic"}, {"icon","fa-terminal"}, {"args",{"msg"}}}
        });
        res.set_content(j.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // API: Parse (Python -> Graph)
    m_server->Post("/api/parse", [this](const httplib::Request& req, httplib::Response& res) {
        std::string scripts_dir = "D:/code/py-sim/scripts/";
        {
            std::ofstream out(scripts_dir + "temp_parse.py");
            out << req.body;
        }
        // python d:/code/py-sim/scripts/ast_to_json.py d:/code/py-sim/scripts/temp_parse.py > d:/code/py-sim/scripts/parsed.json
        std::string cmd = "python " + scripts_dir + "ast_to_json.py " + scripts_dir + "temp_parse.py > " + scripts_dir + "parsed.json";
        system(cmd.c_str());
        
        std::ifstream in(scripts_dir + "parsed.json");
        if(in.is_open()) {
            std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            res.set_content(s, "application/json");
        } else {
            std::cerr << "Failed to open parsed.json" << std::endl;
            res.set_content("{\"nodes\":[]}", "application/json");
        }
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // API: Control - Run
    m_server->Post("/api/run", [this](const httplib::Request& req, httplib::Response& res) {
        std::string code;
        try {
            // Check for JSON
            if (req.body.find("{") == 0) {
                 auto j = json::parse(req.body);
                 if (j.contains("content")) code = j["content"];
                 else if (j.contains("type") && j["type"] == "code") code = j["content"];
                 else code = req.body; // Fallback
            } else {
                code = req.body;
            }
        } catch (...) {
            code = req.body;
        }
        
        m_model.LoadScriptFromContent(code); 
        
        json response;
        response["status"] = "started";
        response["timestamp"] = std::time(nullptr);
        res.set_content(response.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });
    
    m_server->Post("/api/stop", [this](const httplib::Request&, httplib::Response& res) {
        m_model.SetRunning(false);
        res.set_content("{\"status\":\"stopped\"}", "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });
}

std::string WebServer::GetStatusJson() {
    auto* hw = m_model.GetHardware();
    if(!hw) return "{}";

    json j;
    // Axis 0-3
    std::vector<float> axes;
    for(int i=0; i<4; ++i) axes.push_back(hw->GetAxisPos(i));
    j["axes"] = axes;
    
    // IO
    std::vector<int> io;
    for(int i=0; i<8; ++i) io.push_back(hw->GetDO(i));
    j["io"] = io;

    j["running"] = m_model.IsRunning();

    return j.dump();
}

}
