# System Framework Design - Phase 9 (Detailed)

This document provides a comprehensive architectural overview of the AGV Simulation System, focusing on the synchronization between Python scripts, Logical Logic, and 3D Visualization.

## 1. System Architecture Diagram

The system follows a typical MVC (Model-View-Controller) pattern, adapted for Simulation:
- **Model**: `AppModel` (State), `AmrController` (Logic), `SimHardware` (Physics).
- **View**: `EditorView` (Script/Block UI), `SimulatorView` (3D Render), `HardwareView` (IO/Debug).
- **Controller**: `PythonEngine` (Script Execution), `HostApi` (Bridge).

```mermaid
graph TD
    subgraph "User Interface Layer (Views)"
        Editor[EditorView<br/>(Block & Source Editor)]
        SimView[SimulatorView<br/>(3D Rendering & Camera)]
        HardView[HardwareView<br/>(IO & Axis Debug)]
    end

    subgraph "Application Core (Model)"
        App[AppModel<br/>(Singleton State Mediator)]
        Sync[ModelSyncService<br/>(Parser & Config Logic)]
    end

    subgraph "Logic & Control Layer"
        PyEng[PythonEngine<br/>(Execution Thread)]
        AmrCtrl[AmrController<br/>(Safety & Motion Logic)]
        HostAPI[Host API<br/>(Python Bindings)]
    end

    subgraph "Simulation Core"
        SimCore[SimulatorCore<br/>(Physics & Odom)]
        Axes[Virtual Axes]
        Sensors[Virtual Sensors]
    end

    Editor -->|Updates| App
    SimView -->|Reads| App
    HardView -->|Reads/Writes| App
    
    App -->|Manages| AmrCtrl
    App -->|Uses| Sync
    
    PyEng -->|Calls| HostAPI
    HostAPI -->|Mutates| App
    HostAPI -->|Commands| AmrCtrl
    
    AmrCtrl -->|Drives| Axes
    SimCore -->|Updates| Sensors
    Axes -->|Feedback| AmrCtrl
```

## 2. Business Flow Diagram (User Workflow)

Describes the typical user interaction sequence for testing a new process.

```mermaid
sequenceDiagram
    participant User
    participant GUI as GUI (EditorView)
    participant Sync as ModelSyncService
    participant Sim as Visual Simulator
    participant Script as Python Engine

    User->>GUI: Select "demo_ctu.py"
    GUI->>Sync: Load Script & Metadata
    Sync->>Sync: Parse "# model: CTU"
    Sync->>Sync: Parse "# config: axes=4"
    
    Sync->>GUI: Update Block View (Blocks)
    Sync->>Sim: Switch 3D Model -> CTU
    Sync->>Sim: Configure Axis 3 (Elevator)
    
    User->>GUI: Click "Run"
    GUI->>Script: Start Execution
    
    loop Execution Cycle
        Script->>Script: host_api.axis_move(3, 100)
        Script->>Sim: Visual Axis 3 Moves
        Sim->>User: Display Animation
    end
    
    User->>GUI: Observe Result
```

## 3. Data Flow Diagram (Sync & Execution)

How data transforms from Disk -> Memory -> Visualization.

```mermaid
flowchart LR
    File[demo.py File] -->|Input| Parser[Regex Parser]
    
    subgraph "AppModel State"
        Source[Source Lines]
        Blocks[Visual Blocks]
        Config[Axis Config]
        Model[AgvType Enum]
    end
    
    Parser -->|Extracts Code| Source
    Parser -->|Extracts Metadata| Model
    Parser -->|Generates| Blocks
    
    Model -->|Triggers| ModelLoader[3D Model Loader]
    ModelLoader -->|Updates| Scene[3D Scene Graph]
    
    Blocks -->|Rendered By| BlockUI[Block Editor UI]
    
    Config -->|Initializes| AmrController
    AmrController -->|Runtime State| AxisState[Axis Positions]
    AxisState -->|Transforms| Scene
```

## 4. Signal Flow Diagram (Runtime Control)

How real-time signals (Velocity, IO, Events) propagate.

```mermaid
graph LR
    subgraph "Python Script Domain"
        Cmd[SetTwist / AxisMove]
        Wait[Sleep / WaitIO]
    end
    
    subgraph "Control Loop (100Hz)"
        Logic[AmrController::Update]
        Physics[SimulatorCore::Update]
    end
    
    subgraph "Visual Feedback"
        Render[SimulatorView::Render]
    end
    
    Cmd -->|Direct Call| Logic
    Logic -->|Target Velocity| Physics
    Physics -->|Odometry Update| Logic
    Physics -->|Transform Update| Render
    
    Wait -->|Polls| Logic
    Logic -->|IO State| Wait
```

## 5. Relationship Mapping

### 5.1 Script to Model Mapping
The `SyncService` enforces consistency. A script defines the environment it expects:

| User Selection | Script Metadata | Loaded Model | Logical Axes |
| :--- | :--- | :--- | :--- |
| **CTU Demo** | `# model: CTU` | **CTU_MODEL** | 0: Left Wheel<br>1: Right Wheel<br>3: Elevator (Linear)<br>4: Cargo Lock (Rotary) |
| **Forker Demo** | `# model: FORKER` | **FORKER_MODEL** | 0: Left Wheel<br>1: Right Wheel<br>3: Mast (Linear)<br>4: Reach (Linear) |
| **Basic** | `None` | **BASIC_MODEL** | 0: Left Wheel<br>1: Right Wheel |

### 5.2 Interface to Hardware Mapping
The Blocks (UI) equate to specific API calls, which map to Hardware Actions.

- **Block**: `MOVE_AXIS (Axis 3, Pos 100)`
  - **API**: `AmrController::AxisMove(3, 100)`
  - **Hardware**: `SimAxis[3].Target = 100` -> `SimAxis[3].Current += vel * dt`
  - **Visual**: `CTU_Chassis_Mesh` (Static) + `Elevator_Mesh` (Dynamic Z-Offset = Axis 3 Pos)
鼓
