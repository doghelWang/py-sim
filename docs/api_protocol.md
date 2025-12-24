# Protocol Design Document
Version: 2.0
Date: 2025-12-23

## 1. Overview
The communication between the C++ Backend (Simulator/Controller) and the Web Frontend (HMI/IDE) is split into two channels:
1. **HTTP/REST API**: For transactional operations (File Mgmt, config, Command/Control).
2. **WebSocket (WS)**: For high-frequency real-time data (Telemetry, Logs, Execution Hooks).

---

## 2. HTTP REST API
Port: `8080`
Base URL: `http://localhost:8080/api`

### 2.1 System & Config
- **GET /api/status**  
  *Legacy Polling (Deprecated by WS, kept for sanity checks)*.
- **GET /api/config**  
  Returns hardware definition (Axes, IOs, Sensors).
  ```json
  {
    "axes": [ {"id":0, "name":"X-Axis", "limit":1000} ],
    "io": [ {"id":0, "name":"E-Stop", "type":"DI"} ]
  }
  ```
- **GET /api/schema**  
  Returns Python API introspection for Graph Block generation.
  ```json
  [ {"op":"move", "args":["axis", "pos"], "doc":"Move Axis"} ]
  ```

### 2.2 Script Management
- **GET /api/scripts**  
  List all available `.py` files in `scripts/` directory.
  ```json
  { "files": ["test.py", "mission.py"] }
  ```
- **GET /api/scripts/:filename**  
  Read content of a script.
- **POST /api/scripts/:filename**  
  Save content to a file.
  Body: Raw Python Text.
- **POST /api/parse**  
  Convert Python Code -> Node Graph JSON.

### 2.3 Execution Control
- **POST /api/run**  
  Run a specific file or code snippet.
  ```json
  { "msg": "started" }
  ```
- **POST /api/stop**  
  Stop current execution.
- **POST /api/pause**
- **POST /api/resume**

---

## 3. WebSocket Protocol
Endpoint: `ws://localhost:8080/ws`

### 3.1 Packet Structure
All WS messages are JSON:
```json
{
  "type": "TELEMETRY | LOG | EXEC_TRACE | ERROR",
  "ts": 123456789,
  "payload": { ... }
}
```

### 3.2 Message Types (Server -> Client)

#### TELEMETRY (10Hz - 60Hz)
High-speed machine state for Visualization.
```json
{
  "type": "TELEMETRY",
  "payload": {
    "axes": [10.5, 20.0, 0.0, 100.0],
    "io": [0, 1, 0, 0...],
    "robots": [ {"x":10, "y":20, "theta":1.57} ] // 3D Pos
  }
}
```

#### EXEC_TRACE (Event Driven)
Real-time feedback from Python Interpreter.
```json
{
  "type": "EXEC_TRACE",
  "payload": {
    "file": "mission.py",
    "line": 42,
    "func": "main",
    "locals": { "i": "5" } // Optional debug info
  }
}
```

#### LOG
Console output redirection.
```json
{
  "type": "LOG",
  "payload": {
    "level": "INFO",
    "msg": "[Axis] Move Complete"
  }
}
```

---

## 4. Frontend-to-Backend (WS)
The Frontend can also send WS messages for low-latency manual control (Jogging).

- **JOG_START** `{"type":"JOG", "payload":{"axis":0, "vel":10}}`
- **JOG_STOP**  `{"type":"JOG_STOP", "payload":{"axis":0}}`
