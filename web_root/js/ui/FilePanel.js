import { network } from '../api/Network.js';

export class FilePanel {
    constructor(graphCanvas) {
        this.graph = graphCanvas;
        this.elList = document.getElementById('file-list');
        this.btnRefresh = document.getElementById('btn-refresh-files');

        if (this.btnRefresh) {
            this.btnRefresh.addEventListener('click', () => this.refresh());
        }

        // Init
        this.refresh();
    }

    async refresh() {
        try {
            const files = await network.getScripts();
            this.render(files);
        } catch (e) {
            console.error("Failed to list files", e);
        }
    }

    render(files) {
        if (!this.elList) return;
        this.elList.innerHTML = files.map(f => `
            <li class="file-item" data-name="${f}">
                <i class="fa-solid fa-file-code"></i>
                <span>${f}</span>
                <button class="btn-load" title="Load"><i class="fa-solid fa-folder-open"></i></button>
            </li>
        `).join('');

        this.elList.querySelectorAll('.file-item').forEach(item => {
            item.addEventListener('click', async () => {
                const name = item.dataset.name;
                await this.loadScript(name);
            });
        });

        // Prevent double trigger on button
        this.elList.querySelectorAll('.btn-load').forEach(btn => {
            btn.addEventListener('click', (e) => e.stopPropagation()); // Let bubble to li? 
            // Actually, if we bind to LI, clicking button bubbles to LI.
            // We can just remove button listener or keep it for semantics but let it bubble.
            // Best to just rely on LI click.
        });
    }

    async loadScript(name) {
        console.log("Loading script:", name);
        document.getElementById('current-file-name').textContent = name;

        // 1. Get Content
        const content = await network.readScript(name);

        // 2. Parse to Graph
        // We need a parse API wrapper in Network.js usually, but let's do direct fetch for now or add it.
        // Assuming network has no parse method yet.
        const res = await fetch('/api/parse', {
            method: 'POST',
            body: content
        });

        if (res.ok) {
            const text = await res.text();
            console.log("Parse Response Raw:", text);
            try {
                const graphData = JSON.parse(text);
                this.graph.loadGraph(graphData);
            } catch (e) {
                console.error("JSON Parse Error:", e);
                alert("Failed to parse script JSON: " + text.substring(0, 50));
            }
        } else {
            console.error("Failed to parse script");
            alert("Could not parse script to graph. Is it valid Python?");
        }
    }
}
