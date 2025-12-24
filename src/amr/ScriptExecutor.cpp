#include "amr/ScriptExecutor.hpp"
#include "amr/SafetySystem.hpp" // For WaitForResume check (Pause state)
#include <iostream>
#include <filesystem>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace amr {

// Global trace object reference to keep it alive
py::object g_trace_obj;
// Global pointer to executor for static trace func to access
static ScriptExecutor* g_executor = nullptr;

// Trace function for Python (Must be static/loose function for pybind)
py::object trace_monitor(py::object frame, std::string event, py::object arg) {
    if (!g_executor) return py::none();

    // Check termination (Accessing service directly or via global pointer)
    // We can't easily throw here if we want to be clean, but throwing is how we stop Python.
    // However, ScriptExecutor::WorkerEntry handles the catch.
    
    if (event == "line") {
        int lineno = frame.attr("f_lineno").cast<int>();
        std::string filename = "unknown";
        try {
            filename = frame.attr("f_code").attr("co_filename").cast<std::string>();
        } catch(...) {}

        if (g_executor) {
            g_executor->SetCurrentLine(lineno);
            
            // Publish Trace Event
            // We pass a pair: <filename, lineno>
            if (auto bus = ServiceContext::Instance().Get<EventBus>()) {
                 bus->Publish(EventType::SCRIPT_TRACE, std::make_pair(filename, lineno));
            }

            // Pause Check
            g_executor->WaitForResume();
        }
    }
    return g_trace_obj;
}


ScriptExecutor::ScriptExecutor() {
    g_executor = this;
}

ScriptExecutor::~ScriptExecutor() {
    Shutdown();
    g_executor = nullptr;
}

void ScriptExecutor::Initialize() {
    event_bus_ = ServiceContext::Instance().Get<EventBus>();
    // Py_Initialize is handled by pybind11::scoped_interpreter usually in main, 
    // or we can assume it's initialized globally.
    // Existing code uses `pybind11::embed` which likely initializes in main or first use?
    // Let's assume the existing main.cpp or lazy init handles it.
    // Actually `py::scoped_interpreter guard{};` in main is common.
}

void ScriptExecutor::Shutdown() {
    StopScript();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void ScriptExecutor::RunScript(const std::string& path) {
    if (is_running_) {
        StopScript();
    }
    should_terminate_ = false;
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    is_running_ = true;
    if(event_bus_) {
        std::cout << "[ScriptExecutor] Publishing SCRIPT_STARTED..." << std::endl;
        event_bus_->Publish(EventType::SCRIPT_STARTED, path);
    } else {
        std::cout << "[ScriptExecutor] ERROR: No EventBus found!" << std::endl;
    }
    worker_thread_ = std::thread(&ScriptExecutor::WorkerEntry, this, path);
}

void ScriptExecutor::StopScript() {
    should_terminate_ = true;
    // We might need to unpause if it's paused so it can exit
    auto safety = ServiceContext::Instance().Get<SafetySystem>();
    if (safety && safety->IsPaused()) {
         safety->SetPaused(false);
    }
}

bool ScriptExecutor::IsRunning() const {
    return is_running_;
}

void ScriptExecutor::SetCurrentLine(int line) {
    std::lock_guard<std::mutex> lock(mtx_);
    current_line_ = line;
}

int ScriptExecutor::GetCurrentLine() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return current_line_;
}

void ScriptExecutor::SetLocals(const std::map<std::string, std::string>& locals) {
    std::lock_guard<std::mutex> lock(mtx_);
    locals_ = locals;
}

std::map<std::string, std::string> ScriptExecutor::GetLocals() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return locals_;
}

void ScriptExecutor::WaitForResume() {
    // Check SafetySystem for pause
    auto safety = ServiceContext::Instance().Get<SafetySystem>();
    if (!safety) return;

    if (should_terminate_) {
        throw py::value_error("Script Terminated");
    }

    while (safety->IsPaused()) {
        if (should_terminate_) {
             throw py::value_error("Script Terminated");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void ScriptExecutor::WorkerEntry(const std::string& path) {
    py::gil_scoped_acquire acquire;
    
    try {
        py::module_ sys = py::module_::import("sys");
        g_trace_obj = py::cpp_function(trace_monitor);
        sys.attr("settrace")(g_trace_obj);

        // Add 'scripts' folder to sys.path so 'import host' works
        // Assumes CWD is build/Release or similar
        // Try multiple paths to be safe
        sys.attr("path").attr("append")("../../scripts");     // From build/Release
        sys.attr("path").attr("append")("../scripts");        // From build
        sys.attr("path").attr("append")("./scripts");         // From root
        sys.attr("path").attr("append")("D:/code/py-sim/scripts"); // Absolute fallback

        std::cout << "[Script] Sys Path: " << py::str(sys.attr("path")).cast<std::string>() << std::endl;
        std::cout << "[Script] Executing file: " << path << " (Absolute: " << std::filesystem::absolute(path) << ")" << std::endl;

        py::eval_file(path);
        
        std::cout << "[Script] Execution Completed Successfully." << std::endl;
        if (event_bus_) event_bus_->Publish(EventType::SCRIPT_FINISHED);
    } catch (py::error_already_set& e) {
        // e.what()
        std::string err = e.what();
         if (event_bus_) event_bus_->Publish(EventType::SCRIPT_LOG, err);
         std::cout << "[Script] Error: " << err << std::endl;
    } catch (const std::exception& e) {
         std::cout << "[Script] std::exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "[Script] Unknown Exception!" << std::endl;
    }
    
    is_running_ = false;
    g_trace_obj = py::object(); // Release
}

} // namespace amr
