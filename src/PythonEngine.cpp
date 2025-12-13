#include "PythonEngine.hpp"
#include "amr/AppModel.hpp"
#include <iostream>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace amr;

static AppModel &Model() { return AppModel::Instance(); }

py::object g_trace_obj;
std::thread PythonEngine::worker_thread;

// ---------------------------------------------------------
// 追踪函数 (Trace Function)
// 用于 Python `sys.settrace` 的回调，用于实现单步调试、行号监控和暂停功能
// ---------------------------------------------------------
py::object trace_func(py::object frame, std::string event, py::object arg) {
  // 检查是否需要终止脚本
  if (Model().ShouldTerminate()) {
    throw py::value_error("Script terminated by user.");
  }

  if (event == "line") {
    int lineno = frame.attr("f_lineno").cast<int>();

    // 更新当前行号状态 (Update State)
    Model().SetCurrentLine(lineno);

    // 获取局部变量 (开销较大，仅用于调试显示)
    try {
      py::dict loc = frame.attr("f_locals");
      std::map<std::string, std::string> locals;
      for (auto item : loc) {
        locals[py::str(item.first)] = py::str(item.second);
      }
      Model().SetLocals(locals);
    } catch (...) {
    }

    // 检查暂停状态 (Check Pause)
    // 如果系统处于暂停，在此处阻塞等待
    Model().WaitForResume();
  }
  return g_trace_obj;
}

// ---------------------------------------------------------
// 引擎逻辑 (Engine Logic)
// ---------------------------------------------------------

// 启动工作线程 (Start Worker)
void PythonEngine::StartWorker() {
  // 如果已经在运行，先请求终止并等待清理
  if (Model().IsRunning()) {
    Model().RequestTermination();
    Model().SetPaused(false); // 确保解除暂停以便退出等待
  }
  JoinWorker(); // 确保之前的线程已完全结束

  // No manual log cleaning needed, AppModel handles it or we ignore for now
  // Reset termination flag logic is handled inside Model/RequestTermination?
  // Wait, RequestTermination SETS it to true. We need to reset it to false to
  // start new run? Use a hack or add ResetTermination? For now, let's assume it
  // resets on Start? No. We need to implement ResetTermination in AppModel or
  // equivalent. Actually, wait. shoud_terminate_ is protected. Let's rely on
  // IsRunning logic.

  // Actually, we do need to reset should_terminate.
  // I'll add ResetTermination to AppModel later if needed, or just assume it
  // starts fresh? Ah, Model().SetRunning(true) happens in WorkerEntry.

  // Let's replace the log cleanup block with empty or safe log logic.
  // Model().LogMessage("Starting new run...");
  // But wait, if should_terminate is sticky TRUE, we can't start.
  // We need a way to CLEAR it.
  // I will assume `SetRunning(true)` clears `should_terminate`?
  // I should check AppModel logic.
  // Checking AppModel::SetRunning... It just sets is_running_.
  // Checking AppModel::RequestTermination... Sets should_terminate_ = true.

  // We need `ClearTerminationRequest()` method!
  // I'll add it to AppModel.hpp later if needed. Use const_cast hack for now?
  // No. I'll assume for now we just proceed.

  // Refactoring the block:
  Model().SetRunning(false); // Just in case

  worker_thread = std::thread(WorkerEntry);
}

void PythonEngine::JoinWorker() {
  if (worker_thread.joinable()) {
    worker_thread.join();
  }
}

// 工作线程入口 (Worker Entry)
// 这是独立的线程，负责执行 Python 脚本
void PythonEngine::WorkerEntry() {
  // 获取 GIL (全局解释器锁)
  py::gil_scoped_acquire acquire;
  Model().SetRunning(true);

  // Clear termination flag hack (if possible via Friend or similar? No)
  // Actually, we really need a ResetState method in Model.
  // I'll assume for now RequestTermination handles single shot.

  // 尝试执行脚本
  try {
    Model().LogMessage(">> Starting script execution... (开始执行脚本)\n");

    py::module_ sys = py::module_::import("sys");
    g_trace_obj = py::cpp_function(trace_func);
    sys.attr("settrace")(g_trace_obj);

    py::eval_file(Model().GetScriptPath());

    Model().LogMessage(">> Script finished successfully.\n");

  } catch (py::error_already_set &e) {
    if (Model().ShouldTerminate()) {
      Model().LogMessage(">> Script stopped by user.\n");
    } else {
      Model().LogMessage(std::string(">> Python Error: ") + e.what() + "\n");
    }
  } catch (...) {
  }

  Model().SetRunning(false);
  Model().SetPaused(false);
  Model().ResetTermination();
  g_trace_obj = py::object(); // Release global reference while GIL is held and
                              // Interpreter is alive
}
