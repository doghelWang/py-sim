# Backend Interface Specification (v2.1)

## 1. Overview
This document defines the interface contract between the C++ Backend (Simulator/Controller) and any Frontend client.
The backend acts as a **Headless Simulation Service** providing:
1.  **Hardware Abstraction Layer (HAL)**: Control of simulated Axes and I/O.
2.  **Script Execution Engine**: Running Python scripts with trace capability.
3.  **Real-Time Telemetry**: Broadcasting state via Server-Sent Events (SSE).

## 2. Communication Channels
-   **Control Plane (HTTP REST)**: Port `8080`
    -   **Protocol**: HTTP/1.1
    -   **Format**: JSON (UTF-8)
    -   **Usage**: Configuration, File Management, Command/Control.
-   **Data Plane (Real-Time)**: Port `8080`, Endpoint `/api/stream`
    -   **Protocol**: Server-Sent Events (SSE) / EventSource
    -   **Usage**: High-frequency Telemetry (60Hz), Execution Trace, Logs.
    -   **Connection**: Client initiates `GET`, Server keeps socket open and pushes `data:` payloads.

---

## 3. API Discovery (Graph Programming Support)
The Frontend needs to know what "Blocks" are available based on the Backend's version.

### 3.1 Get Block Schema
**Endpoint**: `GET /api/schema`
**Description**: Returns a list of available API functions that can be represented as blocks.

**Request**:
```http
GET /api/schema HTTP/1.1
Host: localhost:8080
```

**Response (JSON)**:
```json
{
  "version": "1.0",
  "blocks": [
    {
      "opcode": "move_axis_abs",
      "name": "Move Absolute",
      "category": "Motion",
      "description": "Moves an axis to a specific position.",
      "icon": "fa-arrow-right",
      "parameters": [
        { "name": "axis", "type": "select", "source": "config.axes", "label": "Axis ID" },
        { "name": "pos", "type": "float", "unit": "mm", "default": 0.0, "label": "Position" },
        { "name": "vel", "type": "float", "unit": "mm/s", "default": 10.0, "label": "Velocity" }
      ]
    },
    {
      "opcode": "set_do",
      "name": "Set Digital Output",
      "category": "I/O",
      "parameters": [
        { "name": "channel", "type": "select", "source": "config.io.do", "label": "Channel" },
        { "name": "value", "type": "bool", "label": "State" }
      ]
    }
  ]
}
```

### 3.2 Get Hardware Configuration
**Endpoint**: `GET /api/config`
**Description**: Describes the physical makeup of the robot, including spatial relationships (mounting) for visualization.

**Request**:
```http
GET /api/config HTTP/1.1
```

**Response (JSON)**:
```json
{
  "robot_type": "AMR_v1",
  "dimensions": { "width": 0.6, "length": 0.9, "height": 0.3 },
  "axes": [
    { "id": 0, "name": "X-Axis (Locomotion)", "type": "linear", "min": -5000, "max": 5000, "unit": "mm" },
    { "id": 1, "name": "Y-Axis (Locomotion)", "type": "linear", "min": -5000, "max": 5000, "unit": "mm" },
    { "id": 2, "name": "Theta", "type": "rotary", "min": -3.14, "max": 3.14, "unit": "rad" }
  ],
  "sensors": [
    { 
      "id": "lidar_front", 
      "type": "lidar", 
      "mount": { "parent": "base_link", "x": 0.4, "y": 0.0, "z": 0.2, "roll":0, "pitch":0, "yaw":0 } 
    }
  ],
  "io": {
    "di": [ { "id": 0, "name": "E-Stop" }, { "id": 1, "name": "Bumper" } ],
    "do": [ { "id": 0, "name": "Light Tower Red" }, { "id": 1, "name": "Brake Release" } ]
  }
}
```

---

## 4. Script Management Protocol

### 4.1 Script Transfer (Save)
**Endpoint**: `POST /api/scripts/:filename`
**Description**: Uploads a raw Python script to the server.

**Request**:
```http
POST /api/scripts/mission.py HTTP/1.1
Content-Type: text/plain

import host
host.move_axis_abs(0, 100, 10)
```

**Response**: 
```json
{ "status": "saved", "size": 45 }
```

### 4.2 Script Execution (Run)
**Endpoint**: `POST /api/run`
**Description**: Triggers execution of a script.

**Request**:
```http
POST /api/run HTTP/1.1
Content-Type: application/json

{
  "type": "code",
  "content": "import host; host.print('hello')"
}
```

**Response**: 
```json
{ "status": "started", "timestamp": 1670001234 }
```

### 4.3 Script Control
**Endpoints**:
1. `POST /api/stop`
2. `POST /api/pause`
3. `POST /api/resume`

**Request**:
```http
POST /api/stop HTTP/1.1
```

**Response**:
```json
{ "status": "stopped", "state": "IDLE" }
```

---

## 5. Real-Time Feedback Protocol (SSE)
**Endpoint**: `/api/stream`
**Method**: `GET`
**Connection**: Long-lived HTTP connection.

### 5.1 Connection Establishment
**Client**:
```js
const evtSource = new EventSource("http://localhost:8080/api/stream");
evtSource.onmessage = (e) => { ... };
```

**Server Stream Format**:
The server pushes blocks separated by `\n\n`. Each block has an `event` type and `data` payload.

### 5.2 Telemetry Packet (Heartbeat)
Frequency: 60Hz.

```text
event: telemetry
data: {
  "ts": 1678900000123,
  "system": { "state": "RUNNING", "cpu_load": 0.12 },
  "axes": [ 100.2, 50.5, 0.0, 10.0 ],
  "io": { "di": 0, "do": 3 }
}

```

### 5.3 Execution Trace Packet
Sent when interpreter steps to a new line.

```text
event: trace
data: {
  "file": "mission.py",
  "line": 14,
  "scope": "main"
}

```

### 5.4 Log Packet
Console output.

```text
event: log
data: { "level": "INFO", "msg": "Axis 0 Reached Target" }

```
