# Refactoring & Enhancement Plan

## Objective
Split the monolithic `main.cpp` into a modular architecture, expand the Host API, and verify with a complex Python script.

## 1. Modular Architecture

### A. Shared State (`src/AppState.hpp`)
*   Contains `AppState` struct (Mutex, CV, flags, Data).
*   Global instance access.

### B. Host API Module (`src/HostApi.cpp`, `src/HostApi.hpp`)
*   Defines `PYBIND11_EMBEDDED_MODULE(host_api, ...)`
*   **New APIs**:
    *   `sleep_ms(ms: int)`: Simulate heavy blocking work.
    *   `compute_prime(n: int)`: CPU intensive task.
    *   `write_file(path: str, content: str)`: File I/O.
    *   `read_file(path: str) -> str`: File I/O.
    *   `get_random_data(count: int) -> List[float]`: Generates data for potential graphing.

### C. Python Engine (`src/PythonEngine.cpp`, `src/PythonEngine.hpp`)
*   Manages `trace_func`.
*   Manages "Worker Thread" spawning and lifecycle.
*   Handles `py::scoped_interpreter`.

### D. GUI Layer (`src/GuiLayer.cpp`, `src/GuiLayer.hpp`)
*   Contains all `Draw*` functions.
*   Depends on `AppState`.
*   Contains `SetupStyle`.

### E. Main Entry (`src/main.cpp`)
*   Initializes `PythonEngine`.
*   Initializes `GLFW/ImGui`.
*   Runs Main Loop (GUI).
*   Spawns Python Thread when requested via `PythonEngine`.

## 2. CMake Update
*   Update `CMakeLists.txt` to include new source files.

## 3. Complex Test Script (`scripts/complex_test.py`)
*   **Logic**:
    *   Class `TaskProcessor`.
    *   Steps: 
        1. Read config (via API).
        2. Perform heavy calc (Primes).
        3. Write results to file.
        4. Loop with sleep.
    *   Deep call stack to test variable inspection in nested frames.

## 4. Execution Flow
1.  Main calls `GuiLayer::Render`.
2.  User clicks "Start".
3.  `GuiLayer` calls `PythonEngine::StartWorker`.
4.  `PythonLogic` imports `host_api`.
5.  `trace_func` updates `AppState`.
