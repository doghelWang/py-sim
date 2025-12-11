# Integration Test Plan

## Test Environment
- **OS**: macOS
- **Compiler**: AppleClang 15+
- **Python**: 3.x (Embedded)
- **Framework**: C++17, Pybind11

## Test Strategy
Manual and automated verification of the command loop and Python execution state.

## Test Cases

### TC-API-01: Core API Functionality
- **Description**: Verify Python can call `host_api.log_message`.
- **Pre-condition**: App built.
- **Action**: Run app.
- **Expected**: Output contains "[HOST API] Received: ...".

### TC-CTRL-01: Pause Execution
- **Description**: Verify execution stops when `pause` command is issued.
- **Action**: 
    1. Run app.
    2. Wait 2s.
    3. Input `pause`.
- **Expected**: 
    - Output shows ">> PAUSE requested."
    - Output shows "[TRACE] Paused at ...".
    - Variable inspection logs are visible.
    - No further "Processing step" logs appear while paused.

### TC-CTRL-02: Resume Execution
- **Description**: Verify execution continues after `resume`.
- **Action**:
    1. Trigger TC-CTRL-01.
    2. Input `resume`.
- **Expected**:
    - Output shows ">> RESUME requested."
    - Output shows "[TRACE] Resuming...".
    - "Processing step" logs resume from the correct number.

### TC-SHUTDOWN-01: Clean Exit
- **Description**: Verify app exits cleanly.
- **Action**: Input `quit`.
- **Expected**: App terminates with exit code 0.
