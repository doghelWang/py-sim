# Deploying on Windows (Clean Slate)

This guide assumes you have a Windows 10/11 machine with NO existing development tools.

## 1. Install Prerequisites

### A. Visual Studio Community (Compiler)
1. Download [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/).
2. Run the installer.
3. In the "Workloads" tab, check **Desktop development with C++**.
4. In the "Installation Details" (right side), ensure **C++ CMake tools for Windows** is checked.
5. Click **Install**.

### B. Python
1. Download the latest Python 3 installer (Windows x86-64 executable) from [python.org](https://www.python.org/downloads/).
2. Run the installer.
3. **CRITICAL**: Check the box **"Add Python to PATH"** at the bottom of the first screen.
4. Click **Install Now**.

### C. Git (Optional but recommended)
1. Download [Git for Windows](https://git-scm.com/download/win).
2. Install with default options.

## 2. Get the Source Code
Open PowerShell or Command Prompt:

```powershell
git clone https://github.com/your-repo/py_embed_gui.git
cd py_embed_gui
```
*(Or download the ZIP from GitHub and extract it).*

## 3. Build with Visual Studio
1. Open **Visual Studio**.
2. Select **"Open a local folder"**.
3. Choose the `py_embed_gui` folder.
4. Visual Studio will automatically detect the `CMakeLists.txt` and start configuring.
   - Watch the Output window at the bottom. It will download dependencies (`pybind11`, `glfw`, `imgui`) automatically.
5. Once configuration finishes (shows "CMake generation finished"), click the **Play** button (Green Arrow) in the top toolbar. 
   - Ensure the target is set to `demo_gui.exe`.

## 4. Troubleshooting
- **CMake not found?** Ensure you installed "C++ CMake tools" in Step 1.
- **Python not found?** Re-run Python installer and ensure "Add to PATH" is checked. You might need to restart Visual Studio.
- **Missing DLLs?** If running the `.exe` outside VS, you might need to copy `python3.dll` next to the executable.

## 5. Running
- The compiled exe will be in `out/build/x64-Debug/demo_gui.exe` (or similar path depending on VS settings).
- Ensure your `scripts/` folder is accessible relative to the executable, or use the absolute path in the GUI.
