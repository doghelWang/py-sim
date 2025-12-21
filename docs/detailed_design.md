# Detailed Module Design - Phase 9 (Revised)

This document specifies the exact implementation details for the UI Layout Optimization, Script Synchronization, and Simulation Realism upgrades.

## 1. UI Module Design (`EditorView`)

### 1.1 Class Structure Update
```cpp
namespace gui {
class EditorView : public IView {
public:
    void Render() override;

private:
    // Layout Logic
    void DrawPalette();         // Two-column layout
    void DrawWorkspace();       // Main block area
    void RenderBlock(amr::VisualBlock& b); // New block renderer
    
    // Internal State
    float m_palette_width_ratio = 0.35f; // Splitter ratio
    int m_dragging_idx = -1;
};
}
```

### 1.2 `DrawPalette` Implementation Detail
- **Grid Layout**: Use `ImGui::Columns(2, "PaletteCols", false)` to enforce a strict 2-column grid.
- **Grouping**: Buttons are grouped by functionality:
    - **Motion**: `MOVE_AXIS`, `HOME`, `AGV_VEL`
    - **Logic**: `WAIT`, `LOOP`, `IF`
    - **IO**: `DO`, `DI`, `SAFETY`
    - **FX**: `PARTICLES`, `SHAKE`

### 1.3 `RenderBlock` Interface
To solve the "generic label" issue, every block type renders a specialized UI header and body.

**Block: AXIS_MOVE**
- **Header**: "AXIS MOVE" (Orange) + [X] Button
- **Body**:
    - Row 1: `Combo("Axis", &param1)` -> Maps to "Elevator (3)", "Fork (4)"
    - Row 2: `DragFloat("Pos", &param2)`
    - Row 3: `DragFloat("Vel", &param3)`

**Block: AGV_MOVE_VEL**
- **Header**: "SET VELOCITY" (Green) + [X] Button
- **Body**:
    - Row 1: `DragFloat("VX", &param1)`
    - Row 2: `DragFloat("VY", &param2)`
    - Row 3: `DragFloat("WZ", &param3)`

## 2. Synchronization Module (`AppModel` & `ModelSyncService`)

### 2.1 `AppModel` Extension
```cpp
class AppModel {
    // New State for Sync
    AgvType current_model_ = AgvType::BASIC;
    std::map<int, std::string> axis_names_; // e.g., {3: "Elevator"}

public:
    // Metadata Parsers
    void SetAgvType(AgvType t);
    AgvType GetAgvType() const;
    
    // Axis Mapping
    void SetAxisName(int axis, const std::string& name);
    std::string GetAxisName(int axis) const;
};
```

### 2.2 Parser Logic (Pseudocode)
The `LoadScriptAsBlocksInternal` function will be enhanced to scan for metadata tags *before* parsing blocks.

```cpp
void LoadScriptAsBlocksInternal(string path) {
    auto lines = ReadFile(path);
    
    // Pass 1: Metadata Scan
    for (line : lines) {
        if (StartsWith(line, "# model: CTU")) {
            AppModel::SetAgvType(AgvType::CTU);
            AppModel::RegisterAxis(3, "Elevator");
            AppModel::RegisterAxis(4, "Cargo Lock");
        }
        else if (StartsWith(line, "# model: FORKER")) {
            AppModel::SetAgvType(AgvType::FORKER);
            AppModel::RegisterAxis(3, "Mast");
            AppModel::RegisterAxis(4, "Fork Reach");
        }
    }
    
    // Pass 2: Block Parsing (Existing Regex Logic)
    // ...
}
```

## 3. Simulation Realism (`SimulatorView`)

### 3.1 Model-Specific Rendering
The `RenderAGV` function will check `AppModel::GetAgvType()` to decide which hierarchy to draw.

**CTU Hierarchy**:
1. **Chassis (Base)**: Inherits transform from `Odom`.
2. **Axis 3 (Elevator)**: Drawn relative to Chassis.
    - `z_offset = GetAxisPos(3) * 0.01f`
3. **Axis 4 (Lock)**: Drawn relative to Elevator.
    - `rotation = GetAxisPos(4)` (Rotational action)

**Forker Hierarchy**:
1. **Chassis (Base)**: Inherits transform from `Odom`.
2. **Axis 3 (Mast)**: Drawn relative to Chassis (Vertical).
3. **Axis 4 (Reach)**: Drawn relative to Mast (Horizontal).

### 3.2 Dynamic Feedback
- **Sensors**: Create `DrawSensorState(bool active, Vec3 pos)` helper.
    - If `active`: Render Green Sphere (Emissive).
    - If `inactive`: Render Grey Wireframe Sphere.
- **IO**: When `SetDO(0)` is active, draw a small indicator led on the chassis HUD.

## 4. Integration Verification Cases

### Case 1: CTU Workflow
1. Load `ctu_demo.py`.
2. **Verify**:
    - Palette: Layout is 2-column.
    - Viewport: Model is CTU (Tall frame).
    - Blocks: "AXIS MOVE" block shows "Elevator" in dropdown.
3. Run Script.
4. **Observe**:
    - Chassis moves -> Elevator rises -> Chassis returns.
    - Sensor feedback visible during "Alignment" phase.

### Case 2: Config Mismatch Safety
1. Load a script requesting `axis_move(9)` (Invalid).
2. **Verify**:
    - System detects Axis 9 is undefined for current model.
    - Logs error: "Invalid Axis 9 for Model CTU".
    - Script pauses/halts safely.
鼓
