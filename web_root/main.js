import { appStore } from './js/core/Store.js';
import { network } from './js/api/Network.js';
import { HardwarePanel } from './js/ui/HardwarePanel.js';
import { LibraryPanel } from './js/ui/LibraryPanel.js';
import { SimulatorView } from './js/ui/SimulatorView.js';
import { GraphCanvas } from './js/ui/GraphCanvas.js';
import { FilePanel } from './js/ui/FilePanel.js';
import { PropertiesPanel } from './js/ui/PropertiesPanel.js';

function init() {
    // 1. Components
    new HardwarePanel();
    new LibraryPanel();
    new SimulatorView();
    const graph = new GraphCanvas();
    window.graph = graph; // Debug
    new FilePanel(graph);
    new PropertiesPanel();

    // 2. Sidebar Tab Logic
    const tabs = document.querySelectorAll('.tab-btn');
    const contents = document.querySelectorAll('.tab-content');

    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const targetId = `tab-${tab.dataset.tab}`;

            // Deactivate all
            tabs.forEach(t => t.classList.remove('active'));
            contents.forEach(c => c.classList.remove('active'));

            // Activate target
            tab.classList.add('active');
            const targetContent = document.getElementById(targetId);
            if (targetContent) targetContent.classList.add('active');
        });
    });

    // 3. Global Bindings (Navbar etc.)
    const elStatus = document.getElementById('status-dot');
    const elStatusText = document.getElementById('status-text');
    const elRun = document.getElementById('btn-run');
    const elStop = document.getElementById('btn-stop');

    // Status Logic
    appStore.subscribe((state) => {
        if (state.connected) {
            elStatus.className = "dot online";
            elStatusText.textContent = "Online";
        } else {
            elStatus.className = "dot offline";
            elStatusText.textContent = "Disconnected";
        }
    });

    // Run/Stop
    if (elRun) {
        elRun.addEventListener('click', () => {
            // Generate Code from Graph
            let code = graph.generatePython();
            console.log("Running Code:\n", code);

            // Fallback if graph empty (for testing)
            if (graph.nodes.length === 0) {
                code = "import host\nhost.print('Graph Empty. Moving Demo...')\nhost.move_axis_abs(0, 500, 50)\nhost.move_axis_abs(2, 3.14, 1.0)";
            }

            network.runCode(code);
        });
    }

    if (elStop) {
        elStop.addEventListener('click', () => {
            network.stop();
        });
    }
}

document.addEventListener('DOMContentLoaded', () => {
    init();
    network.init();
});
