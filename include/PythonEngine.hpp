#pragma once
#include <thread>

class PythonEngine {
public:
  static void StartWorker();
  static void JoinWorker();
  static void WorkerEntry();

private:
  static std::thread worker_thread;
};
