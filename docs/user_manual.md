# User Manual: C++ Embedded Python Control System (GUI Version)

## Overview
This is a C++ GUI application (Dear ImGui) that embeds a Python interpreter. It allows you to load, run, pause, resume, and debug Python scripts with a visual interface.

## Prerequisites
- macOS or Linux/Windows with OpenGL support.
- CMake 3.14+, C++ Compiler.
- Python 3 development headers.

## Build Instructions
```bash
mkdir build && cd build
cmake ..
make
```

## Running the Application
```bash
./demo_gui
```

## Features Guide

### 1. Control Panel (Left/Top)
- **Script Path**: Enter the relative or absolute path to your Python script (default: `../scripts/demo_logic.py`).
- **Load**: Reloads the file content into the Source View.
- **Start Execution**: Launches the script in a background thread.
- **Pause/Resume**: Controls the execution flow.
- **Stop**: Terminates the script forcefully.
- **Export API Stub**: Generates a `host_api.pyi` file in the current directory for autocomplete in your IDE.

### 2. Source View (Middle)
- Displays the loaded script content.
- **Highlighting**: When the script runs, the currently executing line (or paused line) is highlighted in yellow.
- **Context**: When paused, you can clearly see the code context around the execution point.

### 3. Variable Watch (Right)
- **Select Variables**: A tree view listing all local variables in the current frame. Check the box to add them to the watch list.
- **Watched Values Table**: Shows the real-time string representation of selected variables.

### 4. Console (Bottom)
- Displays logs from both the C++ host and the Python script's `host_api.log_message()` calls.
