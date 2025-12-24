import { appStore } from '../core/Store.js';

export class GraphCanvas {
    constructor() {
        this.container = document.getElementById('graph-canvas');
        this.nodes = [];
        this.edges = [];
        this.width = this.container.clientWidth;
        this.height = this.container.clientHeight;

        // SVG Layer for connections
        this.svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        this.svg.style.position = 'absolute';
        this.svg.style.top = '0';
        this.svg.style.left = '0';
        this.svg.style.width = '100%';
        this.svg.style.height = '100%';
        this.svg.style.pointerEvents = 'none';
        this.svg.style.zIndex = '0'; // Bottom
        this.container.appendChild(this.svg);

        this.container.addEventListener('dragover', e => e.preventDefault());
        this.container.addEventListener('drop', this.onDrop.bind(this));

        this.nextNodeId = 1;
        this.logDebug("GraphCanvas Initialized");
    }

    logDebug(msg) {
        const con = document.getElementById('console-output');
        if (con) {
            const line = document.createElement('div');
            line.style.color = 'cyan';
            line.innerText = "[Debug] " + msg;
            con.appendChild(line);
            con.scrollTop = con.scrollHeight;
        }
        console.log("[GraphCanvas] " + msg);
    }

    onDrop(e) {
        e.preventDefault();
        const raw = e.dataTransfer.getData('application/json');
        if (!raw) return;

        const data = JSON.parse(raw);
        if (data.type === 'block') {
            const rect = this.container.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const y = e.clientY - rect.top;
            this.createNode(data.op, x, y);
        }
    }

    createNode(op, x, y, initialParams = {}) {
        const schema = appStore.state.library.find(b => b.op === op);
        const nodeName = schema ? schema.name : op;
        const nodeArgs = schema ? (schema.args || []) : [];

        const node = {
            id: this.nextNodeId++,
            op: op,
            name: nodeName,
            x: x,
            y: y,
            args: nodeArgs,
            values: initialParams || {}
        };

        // Defaults
        node.args.forEach(arg => {
            if (node.values[arg] === undefined) node.values[arg] = 0;
        });

        this.nodes.push(node);
        this.renderNode(node);
        this.updateConnections();
    }

    renderNode(node) {
        // Ensure standard styles exist via class, but enforce position here
        const el = document.createElement('div');
        el.className = 'graph-node';
        if (appStore.state.editor.selectedNode && appStore.state.editor.selectedNode.id === node.id) {
            el.classList.add('selected');
        }

        el.style.position = 'absolute';
        el.style.left = node.x + 'px';
        el.style.top = node.y + 'px';
        el.style.zIndex = '10'; // Above SVG
        // Force basic visibility in case CSS fails
        el.style.minWidth = '150px';
        el.style.backgroundColor = '#1e1e1e';
        el.style.border = '1px solid #444';
        el.style.color = '#fff';

        el.dataset.id = node.id;

        el.innerHTML = `
            <div class="node-header" style="padding: 5px; background: #333; cursor: grab;">
                <i class="fa-solid fa-cube"></i> ${node.name}
            </div>
            <div class="node-body-minimal" style="padding: 10px;">
                ${Object.keys(node.values).length > 0 ? '<i class="fa-solid fa-sliders"></i> Params' : ''}
            </div>
        `;

        el.addEventListener('mousedown', (e) => {
            if (e.button !== 0) return;
            if (appStore.state.editor.selectedNode !== node) {
                appStore.state.editor.selectedNode = node;
                this.container.querySelectorAll('.graph-node').forEach(n => n.classList.remove('selected'));
                el.classList.add('selected');
                el.style.borderColor = '#007bff'; // Visual feedback
                appStore.notify();
            }
        });

        this.makeDraggable(el, node);
        this.container.appendChild(el);
    }

    loadGraph(data) {
        this.logDebug("Loading Graph Data...");

        // SAFE CLEAR: Remove only nodes, keep SVG container
        const oldNodes = this.container.querySelectorAll('.graph-node');
        oldNodes.forEach(n => n.remove());

        this.nodes = [];
        this.edges = [];
        this.svg.innerHTML = ''; // Clear wires

        this.nextNodeId = 1;

        if (!data) {
            this.logDebug("No data provided");
            return;
        }

        // Support {nodes: [], wires: []} OR [nodes]
        const nodesList = Array.isArray(data) ? data : (data.nodes || []);
        const wiresList = Array.isArray(data) ? [] : (data.wires || []);

        this.logDebug(`Found ${nodesList.length} nodes, ${wiresList.length} wires`);

        nodesList.forEach(n => {
            if (n.id >= this.nextNodeId) this.nextNodeId = n.id + 1;
            this.createNode(n.op, n.x, n.y, n.params || n.values);
        });

        this.edges = wiresList;
        this.updateConnections();

        this.logDebug("Graph Load Complete");
    }

    updateConnections() {
        this.svg.innerHTML = '';
        const getNode = (id) => this.nodes.find(n => n.id === id);

        if (!this.edges) return;

        this.edges.forEach(edge => {
            const n1 = getNode(edge.from);
            const n2 = getNode(edge.to);
            if (n1 && n2) {
                const p1 = { x: n1.x + 90, y: n1.y + 40 };
                const p2 = { x: n2.x + 90, y: n2.y };

                const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
                const d = `M ${p1.x} ${p1.y} C ${p1.x} ${p1.y + 50}, ${p2.x} ${p2.y - 50}, ${p2.x} ${p2.y}`;

                path.setAttribute('d', d);
                path.setAttribute('stroke', '#666');
                path.setAttribute('stroke-width', '2');
                path.setAttribute('fill', 'none');
                this.svg.appendChild(path);
            }
        });
    }

    makeDraggable(el, node) {
        let isDragging = false;
        let startX, startY;

        const header = el.querySelector('.node-header');
        header.addEventListener('mousedown', (e) => {
            if (e.button !== 0) return;
            isDragging = true;
            startX = e.clientX;
            startY = e.clientY;
            const nodeStartX = node.x;
            const nodeStartY = node.y;

            el.style.opacity = '0.8';

            const onMove = (mv) => {
                if (!isDragging) return;
                const dx = mv.clientX - startX;
                const dy = mv.clientY - startY;
                node.x = nodeStartX + dx;
                node.y = nodeStartY + dy;
                el.style.left = node.x + 'px';
                el.style.top = node.y + 'px';
                this.updateConnections();
            };

            const onUp = () => {
                isDragging = false;
                el.style.opacity = '1.0';
                window.removeEventListener('mousemove', onMove);
                window.removeEventListener('mouseup', onUp);
            };

            window.addEventListener('mousemove', onMove);
            window.addEventListener('mouseup', onUp);
            e.stopPropagation();
        });
    }

    generatePython() {
        const sorted = [...this.nodes].sort((a, b) => a.y - b.y);
        let code = "import host\nimport time\n\n";

        sorted.forEach(node => {
            const args = node.args.map(arg => {
                const val = node.values[arg];
                return isNaN(val) ? `'${val}'` : val;
            }).join(', ');

            if (node.op === 'sleep_ms') {
                code += `host.sleep_ms(int(${node.values['ms']}))\n`;
            } else if (node.op === 'print') {
                code += `host.log('${node.values['msg']}')\n`;
            } else {
                code += `host.${node.op}(${args})\n`;
            }
        });
        return code;
    }
}
