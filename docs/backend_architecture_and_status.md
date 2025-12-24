# Backend Architecture & Status Archive
**Date**: 2025-12-23
**Version**: 2.1 (Headless / API-Driven)

## 1. Architectural Overview
The Backend has been successfully refactored from a Monolithic GUI Application (`ImGui` + `GLFW`) to a **Headless Service-Oriented Architecture**.

### 1.1 Core Components
-   **`WebServer` (`src/amr/WebServer.cpp`)**:
    -   **Role**: The primary interface for external control.
    -   **Stack**: `cpp-httplib` (Multi-threaded).
    -   **Function**: Serves static frontend files and exposes REST/SSE APIs.
-   **`EventBus` (`include/amr/EventBus.hpp`)**:
    -   **Role**: Decouples internal logic from the WebServer.
    -   **Mechanism**: Publish/Subscribe pattern using `std::any` for payload flexibility.
    -   **Key Events**: `LOG`, `TRACE`, `ScriptStarted`, `ScriptFinished`.
-   **`AppModel` & `ServiceContext`**:
    -   **Role**: Global Dependency Injection container.
    -   **Services**: `SimHardware`, `ScriptExecutor`, `PhysicsContext`.

### 1.2 Data Flow
1.  **Command**: User (Frontend) -> `POST /api/run` -> `WebServer`.
2.  **Execution**: `WebServer` -> `AppModel::LoadScript` -> `ScriptExecutor`.
3.  **Feedback**: `ScriptExecutor` -> `EventBus::Publish(TRACE)` -> `WebServer::OnEvent`.
4.  **Broadcast**: `WebServer` -> SSE Event Queue -> Frontend (`/api/stream`).

---

## 2. API Interface Status (Verified)
All verification passed using `tests/verify_backend.py`.

### 2.1 REST Endpoints
| Method | Endpoint | Status | Description |
| :--- | :--- | :--- | :--- |
| **GET** | `/api/config` | ✅ Ready | Returns Robot Capability (Axes: X,Y,Theta,Lift; Sensors: Lidar). |
| **GET** | `/api/schema` | ✅ Ready | Returns Block Library Definitions (Move, Sleep, IO, Print). |
| **POST** | `/api/run` | ✅ Ready | Accepts Python Script Content (Text or JSON) and starts execution. |
| **POST** | `/api/stop` | ✅ Ready | Aborts current script. |
| **POST** | `/api/parse` | ✅ Ready | Invokes Python AST to convert Script -> Graph JSON (for Import). |

### 2.2 Real-Time Stream (SSE)
**Endpoint**: `/api/stream`
**Format**: `text/event-stream`

-   **Event: `telemetry`** (60Hz)
    ```json
    { "axes": [0.0, 0.5, 0.0, 0.0], "io": [0, 1, 0, ...] }
    ```
-   **Event: `trace`** (On Step)
    ```json
    { "file": "script.py", "line": 12 }
    ```
-   **Event: `log`** (On Print)
    ```json
    { "msg": "Hello World" }
    ```

---

## 3. Implementation Notes

### 3.1 Static File Serving
-   **Issue**: Relative paths (`./web_root`) failed when running from different CWD (e.g., VS Code vs Build Dir).
-   **Resolution**: Hardcoded Absolute Path buffer (`D:/code/py-sim/web_root`) in `WebServer.cpp` for development reliability.
-   **Future**: Should be configurable via Command Line Argument.

### 3.2 Thread Safety
-   **Event Queue**: Implemented `std::mutex` protected queue in `WebServer` to safely bridge the `EventBus` (app thread) to `httplib` (worker threads).

### 3.3 Python Integration
-   **Embedding**: Uses `pybind11` scoped interpreter.
-   **Safety**: Explicitly guarded to prevent interpreter crashes on restart.

---

## 4. Pending Backend Work
-   [ ] **Argument Parsing**: Add `-w <web_root>` flag to remove absolute path hardcoding.
-   [ ] **Multi-Client Optimization**: Currently optimized for single control client (One SSE Stream consumer is ideal).
-   [ ] **Security**: No Authentication implemented (Design choice for Localhost Sim).
