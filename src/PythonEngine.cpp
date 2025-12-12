#include "PythonEngine.hpp"
#include "AppState.hpp"
#include <iostream>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;
py::object g_trace_obj;
std::thread PythonEngine::worker_thread;

// ---------------------------------------------------------
// Trace Function
// ---------------------------------------------------------
py::object trace_func(py::object frame, std::string event, py::object arg) {
  if (g_app.should_terminate) {
    throw py::value_error("Script terminated by user.");
  }

  if (event == "line") {
    int lineno = frame.attr("f_lineno").cast<int>();

    // Update State
    {
      std::lock_guard<std::mutex> lock(g_app.mtx);
      g_app.current_line = lineno;
      // Get Locals (Expensive)
      try {
        py::dict loc = frame.attr("f_locals");
        g_app.locals.clear();
        for (auto item : loc) {
          g_app.locals[py::str(item.first)] = py::str(item.second);
        }
      } catch (...) {
      }
    }

    // Check Pause
    if (g_app.is_paused) {
      std::unique_lock<std::mutex> lock(g_app.mtx);
      g_app.cv.wait(lock,
                    [] { return !g_app.is_paused || g_app.should_terminate; });
    }
  }
  return g_trace_obj;
}

// ---------------------------------------------------------
// Engine Logic
// ---------------------------------------------------------

void PythonEngine::StartWorker() {
  if (g_app.is_running) {
    {
      std::lock_guard<std::mutex> lock(g_app.mtx);
      g_app.should_terminate = true;
      g_app.is_paused = false;
      g_app.cv.notify_all();
    }
  }
  JoinWorker(); // Ensure previous is cleaned

  // Clean log if too big
  {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    if (g_app.console_log.size() > 5000)
      g_app.console_log =
          g_app.console_log.substr(g_app.console_log.size() - 4000);
    g_app.should_terminate = false;
  }

  worker_thread = std::thread(WorkerEntry);
}

void PythonEngine::JoinWorker() {
  if (worker_thread.joinable()) {
    worker_thread.join();
  }
}

void PythonEngine::WorkerEntry() {
  // This runs in a separate thread
  py::gil_scoped_acquire acquire;
  g_app.is_running = true;

  try {
    {
      std::lock_guard<std::mutex> lock(g_app.mtx);
      g_app.console_log += ">> Starting script execution...\n";
    }

    py::module_ sys = py::module_::import("sys");
    g_trace_obj = py::cpp_function(trace_func);
    sys.attr("settrace")(g_trace_obj);

    py::eval_file(g_app.script_path);

    {
      std::lock_guard<std::mutex> lock(g_app.mtx);
      g_app.console_log += ">> Script finished successfully.\n";
    }

  } catch (py::error_already_set &e) {
    std::lock_guard<std::mutex> lock(g_app.mtx);
    if (g_app.should_terminate) {
      g_app.console_log += ">> Script stopped by user.\n";
    } else {
      g_app.console_log += std::string(">> Python Error: ") + e.what() + "\n";
    }
  } catch (...) {
  }

  g_app.is_running = false;
  g_app.is_paused = false;
  g_app.should_terminate = false;
}
