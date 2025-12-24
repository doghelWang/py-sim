import { appStore } from '../core/Store.js';

const API_BASE = ""; // Relative path

export class NetworkService {
    constructor() {
        this.evtSource = null;
    }

    async init() {
        console.log("[Network] Initializing...");
        await this.fetchConfig();
        await this.fetchSchema();
        this.connectSSE();
    }

    async fetchConfig() {
        try {
            const res = await fetch(`${API_BASE}/api/config`);
            const data = await res.json();
            console.log("[Network] Config loaded:", data);
            appStore.setHardwareConfig(data);
        } catch (e) {
            console.error("[Network] Config fetch failed:", e);
        }
    }

    async fetchSchema() {
        try {
            const res = await fetch(`${API_BASE}/api/schema`);
            const data = await res.json();
            console.log("[Network] Schema loaded:", data);
            appStore.setLibrary(data);
        } catch (e) {
            console.error("[Network] Schema fetch failed:", e);
        }
    }

    connectSSE() {
        if (this.evtSource) this.evtSource.close();

        this.evtSource = new EventSource(`${API_BASE}/api/stream`);

        this.evtSource.onopen = () => {
            console.log("[SSE] Connected");
            appStore.setConnected(true);
        };

        this.evtSource.onerror = (err) => {
            console.warn("[SSE] Connection lost/error", err);
            appStore.setConnected(false);
            // Browser handles reconnection, but we update UI state
        };

        // Event: Telemetry
        this.evtSource.addEventListener('telemetry', (e) => {
            try {
                const data = JSON.parse(e.data);
                appStore.updateTelemetry(data);
            } catch (err) { }
        });

        // Event: Trace
        this.evtSource.addEventListener('trace', (e) => {
            const data = JSON.parse(e.data);
            appStore.updateTrace(data);
        });

        // Event: Started
        this.evtSource.addEventListener('started', (e) => {
            appStore.addLog({ level: 'INFO', msg: 'Script Started' });
        });

        // Event: Log
        this.evtSource.addEventListener('log', (e) => {
            try {
                const data = JSON.parse(e.data);
                appStore.addLog(data);
            } catch (err) { }
        });
    }

    // --- Commands ---

    async runCode(code) {
        try {
            const res = await fetch(`${API_BASE}/api/run`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ type: 'code', content: code })
            });
            return await res.json();
        } catch (e) {
            console.error("[Network] Run failed:", e);
        }
    }

    async stop() {
        try {
            await fetch(`${API_BASE}/api/stop`, { method: 'POST' });
        } catch (e) { }
    }

    async getScripts() {
        try {
            const res = await fetch(`${API_BASE}/api/scripts`);
            return await res.json();
        } catch (e) {
            console.error("[Network] List scripts failed:", e);
            return [];
        }
    }

    async readScript(name) {
        try {
            const res = await fetch(`${API_BASE}/api/scripts/${name}`);
            return await res.text();
        } catch (e) {
            console.error("[Network] Read script failed:", e);
            return "";
        }
    }
}

export const network = new NetworkService();
