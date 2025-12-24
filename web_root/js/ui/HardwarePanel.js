import { appStore } from '../core/Store.js';
import { network } from '../api/Network.js';

export class HardwarePanel {
    constructor() {
        this.elMetrics = document.getElementById('hardware-metrics');
        this.elIO = document.getElementById('hardware-io');
        this.renderedConfig = false;

        // Subscribe
        appStore.subscribe(this.render.bind(this));
    }

    render(state) {
        // 1. Initial Config Render (Run once when config loads)
        if (!this.renderedConfig && state.hardware.axes.length > 0) {
            this.renderStructure(state.hardware);
            this.renderedConfig = true;
        }

        // 2. Telemetry Update (Every frame/event)
        if (this.renderedConfig) {
            this.updateValues(state.hardware, state.telemetry);
        }
    }

    renderStructure(config) {
        // Axes
        this.elMetrics.innerHTML = config.axes.map(axis => `
            <div class="metric-card" id="axis-card-${axis.id}">
                <span class="metric-label">${axis.name}</span>
                <div class="metric-value">0.00 <small>${axis.unit}</small></div>
            </div>
        `).join('');

        // IO
        if (config.io) {
            // Render DO (Outputs) as Toggles
            const doItems = config.io.filter(io => io.type === 'DO').map(io => `
                <div class="io-item" id="io-do-${io.id}">
                    <span>${io.name}</span>
                    <button class="btn btn-icon toggle-btn" data-id="${io.id}">
                        <i class="fa-solid fa-toggle-off"></i>
                    </button>
                </div>
            `).join('');

            // Render DI (Inputs) as Indicators
            const diItems = config.io.filter(io => io.type === 'DI').map(io => `
                <div class="io-item" id="io-di-${io.id}">
                    <span>${io.name}</span>
                    <i class="fa-solid fa-circle status-led"></i>
                </div>
            `).join('');

            this.elIO.innerHTML = doItems + diItems;

            // Bind Clicks
            this.elIO.querySelectorAll('.toggle-btn').forEach(btn => {
                btn.addEventListener('click', () => {
                    const id = parseInt(btn.dataset.id);
                    // Determine current state logic? 
                    // Ideally Store knows current IO state. 
                    // For now, valid Toggle requires reading Store.telemetry inside click
                    const currentVal = appStore.state.telemetry.io ? appStore.state.telemetry.io[id] : 0;
                    const newVal = !currentVal;
                    const code = `import host\nhost.set_do(${id}, ${newVal ? 'True' : 'False'})`;
                    network.runCode(code);
                });
            });
        }
    }

    updateValues(config, telemetry) {
        // Update Axes
        config.axes.forEach((axis, i) => {
            const el = document.getElementById(`axis-card-${axis.id}`);
            if (el) {
                const val = telemetry.axes[i] || 0;
                el.querySelector('.metric-value').innerHTML = `${val.toFixed(2)} <small>${axis.unit}</small>`;
            }
        });

        // Update IO
        if (telemetry.io && config.io) {
            config.io.forEach((io) => {
                if (io.type === 'DO') {
                    const el = document.getElementById(`io-do-${io.id}`);
                    const btn = el.querySelector('button');
                    const icon = btn.querySelector('i');
                    const val = telemetry.io[io.id]; // Assuming flat index mapping for now? 
                    // Wait, Config ID might be 0 for DI and 0 for DO. 
                    // Backend GetStatusJson returns single vector `io`.
                    // We need to know the offset. 
                    // backend `GetStatusJson` pushes 8 DOs?
                    // Let's assume the Telemetry IO array matches the Config IO list order?
                    // Checking WebServer.cpp: "for(int i=0; i<8; ++i) io.push_back(hw->GetDO(i));"
                    // It only returns DOs? 
                    // Config JSON has both DI and DO.
                    // IMPORTANT: Backend GetStatusJson ONLY returns 8 DOs. DIs are missing?
                    // I should verify WebServer.cpp from Step 1937.
                    // Lines 303-305: `for(int i=0; i<8; ++i) io.push_back(hw->GetDO(i));`
                    // Yes, only DOs are sent. DIs are missing from Telemetry.
                    // I will assume index matches ID for DOs.

                    if (val) {
                        icon.className = "fa-solid fa-toggle-on";
                        icon.style.color = "var(--success)";
                    } else {
                        icon.className = "fa-solid fa-toggle-off";
                        icon.style.color = "";
                    }
                }
            });
        }
    }
}
