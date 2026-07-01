import { EDGE_BLUE, EDGE_RED, EDGE_GREEN, fullLiveMask } from './hb_converter.js';

const SVG_NS = "http://www.w3.org/2000/svg";

export class GraphScene {
    constructor(svgElement, converter) {
        this.svg = svgElement;
        this.converter = converter;
        
        this.vertices = []; 
        this.edges = [];
        
        this.currentColor = EDGE_BLUE;
        this.playerToMove = EDGE_BLUE;
        this.editMode = false;
        
        this.pendingU = null;
        this.history = [];
        this.redoStack = [];

        this.groundY = 0; // Vypočítá se v resize

        // Připojení eventů
        this.svg.addEventListener('mousedown', (e) => this.onSvgMouseDown(e));
        this.svg.addEventListener('contextmenu', (e) => { e.preventDefault(); this.cancelPending(); });
        
        // Resize observer pro responzivní ground line
        new ResizeObserver(() => this.draw()).observe(this.svg);
    }

    saveState() {
        this.history.push({
            vertices: JSON.parse(JSON.stringify(this.vertices)),
            edges: JSON.parse(JSON.stringify(this.edges)),
            playerToMove: this.playerToMove
        });
        this.redoStack = [];
    }

    undo() {
        if (this.history.length === 0) return;
        this.redoStack.push({
            vertices: JSON.parse(JSON.stringify(this.vertices)),
            edges: JSON.parse(JSON.stringify(this.edges)),
            playerToMove: this.playerToMove
        });
        const state = this.history.pop();
        this.vertices = state.vertices;
        this.edges = state.edges;
        this.playerToMove = state.playerToMove;
        this.cancelPending();
        this.draw();
    }

    redo() {
        if (this.redoStack.length === 0) return;
        this.history.push({
            vertices: JSON.parse(JSON.stringify(this.vertices)),
            edges: JSON.parse(JSON.stringify(this.edges)),
            playerToMove: this.playerToMove
        });
        const state = this.redoStack.pop();
        this.vertices = state.vertices;
        this.edges = state.edges;
        this.playerToMove = state.playerToMove;
        this.cancelPending();
        this.draw();
    }

    clear() {
        this.saveState();
        this.vertices = [];
        this.edges = [];
        this.cancelPending();
        this.draw();
    }

    cancelPending() {
        this.pendingU = null;
        this.draw();
    }

    getSVGPoint(event) {
        const pt = this.svg.createSVGPoint();
        pt.x = event.clientX;
        pt.y = event.clientY;
        return pt.matrixTransform(this.svg.getScreenCTM().inverse());
    }

    onSvgMouseDown(e) {
        if (e.button !== 0) return; // Jen levé tlačítko
        if (e.target.classList.contains('edge')) return; // Kliknutí na hranu řeší hrana sama
        
        const pt = this.getSVGPoint(e);
        let y = pt.y;
        if (y >= this.groundY - 20) y = this.groundY; // Přichycení k zemi

        // Zkontrolujeme, zda jsme neklikli na existující vrchol
        let hitV = null;
        for (let i = 0; i < this.vertices.length; i++) {
            const v = this.vertices[i];
            if (!v.deleted && Math.hypot(v.x - pt.x, v.y - y) <= 20) {
                hitV = i; break;
            }
        }

        if (this.pendingU === null) {
            // Fáze 1: Výběr startovacího vrcholu
            if (hitV !== null) {
                this.pendingU = hitV;
            } else if (y === this.groundY) {
                // Můžeme tvořit vrcholy "na zelené louce" jen pokud klikneme na zem
                this.vertices.push({ x: pt.x, y: y, isGround: true, deleted: false });
                this.pendingU = this.vertices.length - 1;
            }
        } else {
            // Fáze 2: Výběr koncového vrcholu
            if (hitV === null) {
                this.vertices.push({ x: pt.x, y: y, isGround: (y === this.groundY), deleted: false });
                hitV = this.vertices.length - 1;
            }
            if (this.pendingU !== hitV) {
                this.saveState();
                this.edges.push({ u: this.pendingU, v: hitV, color: this.currentColor, deleted: false });
                this.pendingU = hitV; // Řetězení stavění
            }
        }
        this.draw();
    }

    onEdgeClick(edgeIdx) {
        const edge = this.edges[edgeIdx];
        if (edge.deleted) return;

        // Kontrola pravidel tahů
        if (!this.editMode) {
            if (this.playerToMove === EDGE_BLUE && edge.color === EDGE_RED) return;
            if (this.playerToMove === EDGE_RED && edge.color === EDGE_BLUE) return;
        }

        this.saveState();
        edge.deleted = true;

        // Aplikace gravitace (JS BFS z ground vrcholů)
        this.applyGravity();

        if (!this.editMode) {
            this.playerToMove = (this.playerToMove === EDGE_BLUE) ? EDGE_RED : EDGE_BLUE;
        }
        
        this.cancelPending();
        this.draw();
    }

    applyGravity() {
        const activeEdges = this.edges.filter(e => !e.deleted);
        const activeVertices = this.vertices.map((v, i) => ({ ...v, id: i })).filter(v => !v.deleted);
        
        const adj = new Map();
        activeVertices.forEach(v => adj.set(v.id, []));
        
        activeEdges.forEach((e, i) => {
            adj.get(e.u).push({ to: e.v, edgeObj: e });
            adj.get(e.v).push({ to: e.u, edgeObj: e });
        });

        const visitedVertices = new Set();
        const queue = activeVertices.filter(v => v.isGround).map(v => v.id);
        queue.forEach(id => visitedVertices.add(id));

        while (queue.length > 0) {
            const curr = queue.shift();
            for (const neighbor of adj.get(curr)) {
                if (!visitedVertices.has(neighbor.to)) {
                    visitedVertices.add(neighbor.to);
                    queue.push(neighbor.to);
                }
            }
        }

        // Smažeme vrcholy, které nejsou dosažitelné
        for (let i = 0; i < this.vertices.length; i++) {
            if (!this.vertices[i].deleted && !visitedVertices.has(i)) {
                this.vertices[i].deleted = true;
            }
        }

        // Smažeme hrany, které vedou do smazaných vrcholů
        for (let i = 0; i < this.edges.length; i++) {
            if (!this.edges[i].deleted) {
                const uDel = this.vertices[this.edges[i].u].deleted;
                const vDel = this.vertices[this.edges[i].v].deleted;
                if (uDel || vDel) {
                    this.edges[i].deleted = true;
                }
            }
        }
    }

    // Pro C-Converter potřebujeme setřást smazané entity a vrátit čistý graf
    getCleanGraphForConverter() {
        const vMap = new Map(); // old_id -> new_id
        let newVCount = 0;
        
        for (let i = 0; i < this.vertices.length; i++) {
            if (!this.vertices[i].deleted) {
                vMap.set(i, newVCount++);
            }
        }

        const cleanEdges = [];
        for (const e of this.edges) {
            if (!e.deleted) {
                cleanEdges.push({ u: vMap.get(e.u), v: vMap.get(e.v), color: e.color });
            }
        }

        return {
            numVertices: newVCount,
            numEdges: cleanEdges.length,
            edges: cleanEdges
        };
    }

    calculateValue() {
        const cGraph = this.getCleanGraphForConverter();
        if (cGraph.numEdges === 0) return this.converter._rt().game_zero();
        const mask = fullLiveMask(cGraph.numEdges);
        return this.converter.convert(cGraph, mask);
    }

    draw() {
        this.svg.innerHTML = '';
        const rect = this.svg.getBoundingClientRect();
        this.groundY = rect.height * 0.9;

        // Země
        const groundRect = document.createElementNS(SVG_NS, 'rect');
        groundRect.setAttribute('x', 0); groundRect.setAttribute('y', this.groundY);
        groundRect.setAttribute('width', rect.width); groundRect.setAttribute('height', rect.height - this.groundY);
        groundRect.setAttribute('fill', 'var(--color-ground)');
        
        const groundLine = document.createElementNS(SVG_NS, 'line');
        groundLine.setAttribute('x1', 0); groundLine.setAttribute('y1', this.groundY);
        groundLine.setAttribute('x2', rect.width); groundLine.setAttribute('y2', this.groundY);
        groundLine.setAttribute('stroke', 'var(--color-ground-line)');
        groundLine.setAttribute('stroke-width', '2');

        this.svg.appendChild(groundRect);
        this.svg.appendChild(groundLine);

        // Kreslení hran (s ohledem na paralelní posuny)
        const pairCounts = new Map();
        
        for (let i = 0; i < this.edges.length; i++) {
            const e = this.edges[i];
            if (e.deleted) continue;

            const u = this.vertices[e.u];
            const v = this.vertices[e.v];
            
            const key = e.u < e.v ? `${e.u}-${e.v}` : `${e.v}-${e.u}`;
            const k = pairCounts.get(key) || 0;
            pairCounts.set(key, k + 1);

            const path = document.createElementNS(SVG_NS, 'path');
            path.setAttribute('d', this._getBezierPath(u, v, k));
            path.classList.add('edge');
            if (e.color === EDGE_BLUE) path.classList.add('blue');
            else if (e.color === EDGE_RED) path.classList.add('red');
            else path.classList.add('green');

            path.addEventListener('mousedown', (evt) => {
                evt.stopPropagation();
                this.onEdgeClick(i);
            });

            this.svg.appendChild(path);
        }

        // Kreslení vrcholů
        for (let i = 0; i < this.vertices.length; i++) {
            const v = this.vertices[i];
            if (v.deleted) continue;

            const circle = document.createElementNS(SVG_NS, 'circle');
            circle.setAttribute('cx', v.x);
            circle.setAttribute('cy', v.y);
            circle.setAttribute('r', 12);
            circle.classList.add('vertex');
            if (v.isGround) circle.classList.add('ground');
            if (i === this.pendingU) circle.classList.add('pending');

            this.svg.appendChild(circle);
        }
    }

    _getBezierPath(pu, pv, k) {
        if (k === 0) return `M ${pu.x} ${pu.y} L ${pv.x} ${pv.y}`;
        
        const m = Math.floor((k + 1) / 2);
        const sign = (k % 2 === 1) ? 1 : -1;
        const offsetIndex = sign * m;

        const dx = pv.x - pu.x;
        const dy = pv.y - pu.y;
        const length = Math.hypot(dx, dy);
        
        if (length < 1e-6) return `M ${pu.x} ${pu.y} L ${pv.x} ${pv.y}`;

        const nx = (-dy / length) * Math.sign(offsetIndex);
        const ny = (dx / length) * Math.sign(offsetIndex);

        const bulge = (32.0 + (Math.abs(offsetIndex) - 1) * 63.0) * Math.sign(offsetIndex);
        const cx = (pu.x + pv.x) * 0.5 + nx * bulge;
        const cy = (pu.y + pv.y) * 0.5 + ny * bulge;

        return `M ${pu.x} ${pu.y} Q ${cx} ${cy} ${pv.x} ${pv.y}`;
    }
}
