# Python Embedding C++ GUI (PyEmbedGUI)

A cross-platform C++ application that embeds a Python interpreter, allowing you to run, pause, resume, and inspect Python scripts with a professional GUI.

## Features
- **Cross-Platform**: Runs on Windows, Linux, and macOS.
- **Visual Debugging**: 
    - Real-time line highlighting.
    - Variable inspection table.
- **Control**: Play, Pause, Resume, Stop execution.
- **Embedded API**: Scripts can use `host_api` to access file I/O, heavy computation, and system logging from C++.
- **Zero-Dependency Build**: CMake automatically fetches `glfw`, `imgui`, and `pybind11`.

## Quick Start (macOS/Linux)
```bash
mkdir build && cd build
cmake ..
make
./demo_gui
```

## Quick Start (Windows)
See [DEPLOY_WINDOWS.md](DEPLOY_WINDOWS.md) for a step-by-step guide setting up a clean Windows environment.

## Complex Testing
Load `scripts/complex_test.py` to see recursion, file I/O, and CPU-intensive tasks in action (proving the UI stays responsive).

## Architecture
- **GUI**: Dear ImGui (Docking/Tables)
- **Binding**: Pybind11 (Embedding)
- **Windowing**: GLFW
- **Threading**: Python runs in a dedicated worker thread; C++ manages the GIL and Event Loop.
