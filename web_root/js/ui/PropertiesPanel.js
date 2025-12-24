import { appStore } from '../core/Store.js';

export class PropertiesPanel {
    constructor() {
        this.container = document.querySelector('.panel-content.properties-content'); // Needs to be added to HTML

        // Subscribe to Store updates
        appStore.subscribe((state) => {
            this.render(state.editor.selectedNode);
        });

        this.currentNodeId = null;
    }

    render(node) {
        if (!this.container) {
            // Lazy binding if container not ready
            this.container = document.querySelector('.panel-content.properties-content');
            if (!this.container) return;
        }

        if (!node) {
            this.container.innerHTML = '<div class="empty-state">Select a block to edit properties</div>';
            this.currentNodeId = null;
            return;
        }

        // Avoid re-rendering inputs if same node (preserve focus)
        // But if values changed externally, we might need to sync.
        // For now, full re-render on selection change
        if (this.currentNodeId === node.id) {
            // Just update values if needed?
            // Actually, full re-render is safer for dynamic args, but loses focus if typing.
            // We'll trust that store updates don't trigger while typing (local state first).
            // But wait, Store update causes render.
            // We need to differentiate "Source of Truth" vs "Input Focus".
            return;
        }

        this.currentNodeId = node.id;
        this.container.innerHTML = `
            <div class="prop-group">
                <h3>${node.name}</h3>
                <div class="prop-id">ID: ${node.id}</div>
            </div>
        `;

        // Render Inputs based on Args
        node.args.forEach(arg => {
            const group = document.createElement('div');
            group.className = 'prop-group';

            const label = document.createElement('label');
            label.textContent = arg;

            const input = document.createElement('input');
            input.type = 'text';
            input.value = node.values[arg] !== undefined ? node.values[arg] : '';

            input.addEventListener('input', (e) => {
                // Update Local Node State directly (Reference)
                // In a perfect Redux, we dispatch action.
                // Here we mutate for speed, then trigger UI update if needed.
                node.values[arg] = e.target.value;

                // TODO: Dispatch specific action if other components need to know immediately 
                // e.g. appStore.dispatch({type: 'UPDATE_NODE', payload: node});
            });

            group.appendChild(label);
            group.appendChild(input);
            this.container.appendChild(group);
        });
    }
}
