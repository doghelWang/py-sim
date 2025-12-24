# Frontend Detailed Design (v2.0)

## 1. Overview
The Frontend is a Single-Page Application (SPA) that acts as the control center for the AMR (Autonomous Mobile Robot) Simulator. It communicates with the C++ Backend via HTTP REST and Server-Sent Events (SSE).

**Core Philosophy**:
-   **Data-Driven**: The UI (Hardware Panel, Block Library, 3D Scene) is dynamically generated from the Backend's `/api/config` and `/api/schema`.
-   **Reactive**: State changes (Telemetry, Logs) drive UI updates immediately via a central Store.
-   **Premium Aesthetics**: Modern dark theme, glassmorphism, and smooth animations.

## 2. Architecture: Store-Service-Component (SSC)

### 2.1 Store (`js/core/Store.js`)
The "Single Source of Truth". It holds the application state and provides methods to mutate it.
**State Structure**:
```javascript
{
    connected: false,
    hardware: { robot_type: "", axes: [], sensors: [] }, // From /api/config
    library: [], // From /api/schema
    telemetry: { 
        axes: [0,0,0,0], 
        io: [0,0], 
        running: false 
    },
    logs: [],
    editor: {
        currentFile: "scratch.py",
        nodes: [],   // Graph Model
        traceLine: -1 // Current Execution Line
    }
}
```
**Optimizations**: telemetry updates are high-frequency (60Hz). The Store might throttle UI notification to 30Hz or use `requestAnimationFrame` for smooth rendering without blocking the main thread.

### 2.2 Network Service (`js/api/Network.js`)
Handles all communication.
-   **HTTP**: `fetch` wrappers for `/api/config`, `/api/schema`, `/api/run`, etc.
-   **SSE**: Manages `EventSource` connection to `/api/stream`.
    -   Parses `event: telemetry` -> Updates `Store.telemetry`.
    -   Parses `event: trace` -> Updates `Store.editor.traceLine`.
    -   Parses `event: log` -> Appends to `Store.logs`.
    -   Auto-reconnect logic.

### 2.3 Visual Components
1.  **Sidebar (Hardware & Files)** (`js/ui/Sidebar.js`)
    -   **Hardware Tab**: lists Axes (with Live Position/Velocity) and I/O (Clickable Switches).
    -   **Files Tab**: List scripts from `/api/scripts`. Drag-and-drop to open.
2.  **3D Simulator View** (`js/ui/SimulatorView.js`)
    -   **Engine**: Three.js.
    -   **Dynamic Loader**:
        -   Reads `Store.hardware.dimensions` -> Creates Chassis Mesh.
        -   Reads `Store.hardware.sensors` -> Adds Lidar/Camera Meshes at `mount` coordinates.
    -   **Update Loop**: Syncs Mesh positions with `Store.telemetry.axes` (using Odometry/Kinematics).
3.  **Graph Editor** (`js/ui/Editor.js`)
    -   **Canvas**: HTML5 Canvas or SVG (using logic similar to LiteGraph or a custom simple engine).
    -   **Block Generation**: Iterates `Store.library`. Each API block is a draggable node.
    -   **Trace Highlighting**: When `Store.editor.traceLine` changes, the corresponding Node glows.
4.  **Control Bar** (`js/ui/Navbar.js`)
    -   Run/Stop/Pause Buttons (Call API).
    -   Status Indicator (Online/Offline).

---

## 3. Implementation Steps

### Phase 1: Foundation
1.  **Skeleton**: `index.html` with Layout (Grid: Sidebar, Editor, Sim, Console).
2.  **State**: Implement `Store.js` and `Network.js` (basic connection).
3.  **Styles**: `style.css` (Dark Theme, CSS Grid/Flex).

### Phase 2: Dynamic UI
1.  **Hardware Panel**: Fetch `/api/config` and render list. Bind live Telemetry.
2.  **Graph Editor Logic**: Fetch `/api/schema` and render Palette. Implement Node creation (Drag & Drop).

### Phase 3: Visualization & Control
1.  **3D Scene**: Implement Three.js scene. Sync Robot Movement.
2.  **Execution Interaction**: 'Run' button -> POST JSON -> Listen for Trace events -> Highlight Node.

---

## 4. Key Protocol Handling (v2.1)

### 4.1 Running a Graph
When user clicks "Run":
1.  **Compile**: `EditorController` traverses the Node Graph -> Generates Python Code.
2.  **Send**: `Network.runCode(code_string)`.
    -   Payload: `{ "type": "code", "content": "..." }`.
3.  **Feedback**:
    -   Server replies `{ "status": "started" }`.
    -   SSE stream receives `started` event.
    -   SSE stream receives `trace` events `{ "line": 10 }`.
    -   Editor highlights Node corresponding to line 10.

### 4.2 Real-time Telemetry
The SSE stream sends:
`{"axes":[10, 20, 3.14], "io":...}`
The Frontend must map these array indices to the `Store.hardware.axes[i].name` to display meaningful labels (e.g., "X-Axis: 10mm").

---

## 5. Verification Plan
-   **Check 1**: UI loads and sidebar populates from Backend Config.
-   **Check 2**: "Run" button sends correct POST request.
-   **Check 3**: Graphs generate valid Python syntax.
-   **Check 4**: Telemetry updates axis numbers on screen.
