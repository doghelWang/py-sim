# Project Walkthrough: Modular Architecture & Extended API

## Overview
This version creates a robust, modular C++ application with embedded Python.
- **Refactored**: Split `main.cpp` into `HostApi`, `PythonEngine`, `GuiLayer`.
- **Extended API**: Added File I/O, Prime Calculation (CPU load), and random data generation.
- **Complex Test**: New `complex_test.py` validates deep recursion, file operations, and long-running tasks.

## Key Features
1.  **Modular Code**: Easier to maintain. `g_app` defined in `main.cpp`, shared via `AppState.hpp`.
2.  **Heavy Tasks**: `host_api.compute_prime(n)` simulates CPU work to test UI responsiveness (which remains smooth due to threading!).
3.  **File I/O**: Python script can write logs to the host filesystem.

## How to Run

### 1. Build
```bash
cd build
cmake ..
make
```

### 2. Run
```bash
./demo_gui
```

### 3. Test Complex Workflow
1.  In the GUI "Details" or "Path" box, ensure `../scripts/complex_test.py` is selected.
2.  Click **Run Script**.
3.  Observe:
    *   **Log**: "Starting Batch...", "Computed 1000th prime..."
    *   **Variables**: See `processor`, `status`, `iteration` updating.
    *   **Responsive UI**: The GUI doesn't freeze even during "Heavy Computation" because Python runs in a worker thread.
    *   **Files**: Check `process_log.txt` created in the execution directory.

## Architecture
- `src/main.cpp`: Entry, Global State, Main Loop.
- `src/AppState.hpp`: Shared Data.
- `src/GuiLayer.cpp`: ImGui Rendering.
- `src/PythonEngine.cpp`: Python execution & Trace.
- `src/HostApi.cpp`: `host_api` module definition.
