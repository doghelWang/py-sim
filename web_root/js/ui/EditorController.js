/**
 * Editor Logic
 * Manages Files, Graph <-> Code events
 */
class EditorController {
    constructor(store, network) {
        this.store = store;
        this.network = network;
    }

    async fileListRefresh() {
        const files = await this.network.listScripts();
        // Update UI directly or via store? 
        // Let's emit a custom event or let UI handle it. 
        // For simplicity, we return it to whoever calls.
        return files;
    }

    async loadFile(name) {
        const content = await this.network.loadScript(name);
        this.store.state.selectedFile = name;
        this.store.state.sourceCode = content; // Assuming store has sourceCode

        // Auto Parse
        const graph = await this.network.parseScript(content);
        if (graph && graph.nodes) {
            this.store.setGraph(graph.nodes, graph.wires);
        } else {
            this.store.setGraph([], []);
        }
    }

    async saveFile() {
        const name = this.store.state.selectedFile || 'untitled.py';
        // TODO: Generate Code from Graph if in Graph Mode
        // For now, assume we just save the current source?
        // Wait, requirement 3: Graph -> Python.
        // We need a Generator.

        const code = this.generatePython();
        await this.network.saveScript(name, code);
    }

    generatePython() {
        // Simple Graph -> Python traversal
        const nodes = this.store.state.nodes;
        const wires = this.store.state.wires;
        if (!nodes.length) return "";

        let script = "import host\nimport time\n\nprint('Start')\n";

        // Naive linearization (Start -> Next ...)
        // Find start nodes (no input wires)
        let curr = nodes[0]; // TODO: Better traversal
        const visited = new Set();

        while (curr) {
            if (visited.has(curr.id)) break;
            visited.add(curr.id);

            const p = curr.params || {};
            const op = curr.op;

            if (op === 'move_axis_abs') script += `host.move_axis_abs(${p.axis}, ${p.pos}, ${p.vel})\n`;
            else if (op === 'set_twist') script += `host.set_twist(${p.vx}, ${p.vy}, ${p.w})\n`;
            else if (op === 'sleep_ms') script += `host.sleep_ms(${p.ms})\n`;
            else if (op === 'set_do') script += `host.set_do(${p.id}, ${p.val})\n`;
            else if (op === 'print') script += `host.print("${p.msg}")\n`;

            // Next
            const w = wires.find(x => x.from === curr.id);
            if (!w) break;
            curr = nodes.find(n => n.id === w.to);
        }

        script += "print('Done')\n";
        return script;
    }
}
