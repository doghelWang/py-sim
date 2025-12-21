# System Analysis Report - Phase 9

## 1. Issue Diagnosis

### 1.1 Block View Visibility (Empty/Generic Labels)
**Symptom**: Blocks show generic labels like "BLOCK: 18", "BLOCK: 19" instead of meaningful titles.
**Cause**: `EditorView.cpp` has a hardcoded `if/else if` chain for rendering blocks. The new `BlockType` values introduced for Python-to-Block synchronization (`MOVE`, `DELAY`, `AXIS_MOVE`, `MSG`) were not added to this UI rendering logic, causing them to fall into the default `else` block.
**Resolution**: Implement specific rendering UI for these types in `EditorView.cpp`.

### 1.2 Simulation "Static" Issue (No Movement)
**Symptom**: Python scripts execute (logs appear), but the AGV and mechanisms do not move.
**Cause A (Velocity)**: `AppModel::UpdateSafetyLogic` contains an "Auto-Home" check that returns `target_twist_` to zero if the AGV is within 5 units of the origin. Since scripts start from (0,0), any movement command is instantly suppressed.
**Cause B (Mechanisms)**: Demo scripts like `ctu_demo.py` access `axis(3)`. If the simulation is currently in `BASIC` or `FORKER` mode, axis 3 may not be mapped or initialized as an active mechanism, leading to silent failures or no visual feedback.
**Resolution**: Refine the Auto-Home logic and implement automatic model switching on script load.

### 1.3 Layout & UX Friction
**Symptom**: Interface feels cluttered; buttons are truncated; block width is excessive.
**Cause**: Default ImGui column ratios and single-column palette layout. Blocks are rendered in a fixed-height child window that doesn't accommodate multi-line parameters well.
**Resolution**: Redesign the layout with resizable splitters, two-column palette buttons, and multi-line block displays.

---

## 2. Structural Mismatches

### 2.1 Model vs. Configuration
The current system allows the 3D model (Visual) and the Axis Configuration (Logical) to diverge. 
- **Requirement**: Changing a demo script (e.g., to CTU) must simultaneously update:
    1. The 3D chassis model.
    2. The mechanism definitions (Axis 3-10).
    3. The Global Parameter defaults.

### 2.2 Script vs. Interface
The bidirectional synchronization is currently a "snapshot" on load.
- **Requirement**: The parser should handle not just simple calls but also indicate which model is "suggested" by the script (via metadata or specific API calls).

---

## 3. Recommended Actions
1. **Refactor `EditorView`**: Optimize layout and add missing block renderers.
2. **Implement `ModelSync`**: Link script metadata to the simulation environment.
3. **Fix `UpdateSafetyLogic`**: Remove the aggressive auto-stop at origin.
4. **Enhanced Feedback**: Ensure DI/DO triggers have visual indicators in the 3D view (e.g., glowing sensors).

