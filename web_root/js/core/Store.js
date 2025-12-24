/**
 * Central Reactive Store
 * Implements a simple Observer pattern for state management.
 */
class Store {
    constructor() {
        this.state = {
            connected: false,
            // Hardware Config (Static)
            hardware: {
                robot_type: "Unknown",
                axes: [],
                io: [],
                sensors: []
            },
            // Block Library (Static)
            library: [],
            // Real-time Telemetry
            telemetry: {
                axes: [],
                io: [],
                running: false
            },
            // Execution State
            editor: {
                graph: null, // Will be instance of GraphCanvas
                library: [], // Cached copy
                selectedNode: null // Currently selected node for Properties
            },
            logs: []
        };

        this.listeners = new Set();
    }

    // Subscribe to state changes
    subscribe(callback) {
        this.listeners.add(callback);
        return () => this.listeners.delete(callback);
    }

    // Notify all subscribers
    notify() {
        for (const listener of this.listeners) {
            listener(this.state);
        }
    }

    // Actions
    setConnected(isConnected) {
        this.state.connected = isConnected;
        this.notify();
    }

    setHardwareConfig(config) {
        this.state.hardware = config;
        this.notify();
    }

    setLibrary(schema) {
        this.state.library = schema;
        this.notify();
    }

    updateTelemetry(data) {
        // Differential update logic could go here, but replacement is fine for MVP
        this.state.telemetry = data;
        this.notify();
    }

    updateTrace(traceData) {
        this.state.editor.traceLine = traceData.line;
        this.notify();
    }

    addLog(logData) {
        this.state.logs.push(logData);
        if (this.state.logs.length > 200) this.state.logs.shift(); // Keep last 200
        // Notify specific 'log' event? Or general. 
        // For simple UI, general notify is barely ok, but dedicated log listener is better.
        // We'll stick to full notify for now (MVP).
        this.notify();
    }

    clearLogs() {
        this.state.logs = [];
        this.notify();
    }
}

export const appStore = new Store();
