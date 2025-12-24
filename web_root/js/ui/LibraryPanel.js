import { appStore } from '../core/Store.js';

export class LibraryPanel {
    constructor() {
        this.elLib = document.getElementById('block-library');
        appStore.subscribe(this.render.bind(this));

        // Render Categories? For now just a flat list or simple grouping
    }

    render(state) {
        if (state.library.length === 0) return;

        // Check if already rendered to avoid redraw drag issues
        // Simple hash check or length check
        if (this.elLib.children.length > 0) return;

        this.elLib.innerHTML = state.library.map(block => `
            <div class="block-item" draggable="true" data-op="${block.op}">
                <i class="fa-solid ${block.icon || 'fa-cube'}"></i>
                <span>${block.name}</span>
            </div>
        `).join('');

        // Bind Drag
        this.elLib.querySelectorAll('.block-item').forEach(el => {
            el.addEventListener('dragstart', (e) => {
                const op = el.dataset.op;
                e.dataTransfer.setData('application/json', JSON.stringify({
                    type: 'block',
                    op: op
                }));
            });
        });
    }
}
