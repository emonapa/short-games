import { Game } from '../../core/game.js';
import { EDGE_BLUE, EDGE_RED, EDGE_GREEN, fullLiveMask } from './hb_converter.js';

const SVG_NS = "http://www.w3.org/2000/svg";

function cloneState(scene) {
    return {
        vertices: JSON.parse(JSON.stringify(scene.vertices)),
        edges: JSON.parse(JSON.stringify(scene.edges)),
        playerToMove: scene.playerToMove
    };
}

function hasMaskBit(mask, index) {
    return Boolean(mask & (1n << BigInt(index)));
}

function distPointToSegment(p, a, b) {
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const len2 = dx * dx + dy * dy;
    if (len2 === 0) return Math.hypot(p.x - a.x, p.y - a.y);
    const t = Math.max(0, Math.min(1, ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2));
    return Math.hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

function segmentsNear(a1, a2, b1, b2, threshold) {
    return Math.min(
        distPointToSegment(a1, b1, b2),
        distPointToSegment(a2, b1, b2),
        distPointToSegment(b1, a1, a2),
        distPointToSegment(b2, a1, a2)
    ) <= threshold;
}

export class GraphScene {
    constructor(svgElement, converter) {
        this.svg = svgElement;
        this.converter = converter;
        this.vertices = [];
        this.edges = [];
        this.currentColor = EDGE_BLUE;
        this.playerToMove = EDGE_BLUE;
        this.editMode = false;
        this.hintsActive = false;
        this.analysisActive = false;
        this.botPlayingColor = null;
        this.pendingU = null;
        this.history = [];
        this.redoStack = [];
        this.groundY = 0;
        this.isSlashing = false;
        this.slashPoints = [];
        this.slashPathEl = null;
        this.hoverTimer = 0;
        this.hoverLabel = null;
        this.onTurnChanged = null;

        this.svg.addEventListener('mousedown', (e) => this.onSvgMouseDown(e));
        this.svg.addEventListener('mousemove', (e) => this.onSvgMouseMove(e));
        this.svg.addEventListener('mouseup', () => this.onSvgMouseUp());
        this.svg.addEventListener('mouseleave', () => this.onSvgMouseUp());
        this.svg.addEventListener('contextmenu', (e) => { e.preventDefault(); this.cancelPending(); });
        new ResizeObserver(() => this.draw()).observe(this.svg);
    }

    saveState() { this.history.push(cloneState(this)); this.redoStack = []; }
    undo() { if (this.history.length === 0) return; this.redoStack.push(cloneState(this)); this.loadState(this.history.pop()); }
    redo() { if (this.redoStack.length === 0) return; this.history.push(cloneState(this)); this.loadState(this.redoStack.pop()); }
    loadState(state) { this.vertices = state.vertices; this.edges = state.edges; this.playerToMove = state.playerToMove; this.cancelPending(false); this.draw(); this._notifyTurn(); }
    clear() { this.saveState(); this.vertices = []; this.edges = []; this.cancelPending(false); this.draw(); }
    cancelPending(redraw = true) { this.pendingU = null; this._clearHoverHint(); if (redraw) this.draw(); }

    getSVGPoint(event) {
        const pt = this.svg.createSVGPoint();
        pt.x = event.clientX;
        pt.y = event.clientY;
        return pt.matrixTransform(this.svg.getScreenCTM().inverse());
    }

    onSvgMouseDown(e) {
        if (e.button !== 0) return;
        const pt = this.getSVGPoint(e);
        let y = pt.y;
        if (y >= this.groundY - 20) y = this.groundY;
        const pos = { x: pt.x, y };
        const hitV = this._findVertexHit(pos);
        const isGroundClick = y === this.groundY;

        if (this.pendingU === null && hitV === null && !isGroundClick) {
            this._beginSlash(pt);
            e.preventDefault();
            return;
        }

        if (this.pendingU === null) {
            if (hitV !== null) this.pendingU = hitV;
            else if (isGroundClick) {
                this.vertices.push({ x: pt.x, y, isGround: true, deleted: false });
                this.pendingU = this.vertices.length - 1;
            }
        } else {
            let endV = hitV;
            if (endV === null) {
                this.vertices.push({ x: pt.x, y, isGround: isGroundClick, deleted: false });
                endV = this.vertices.length - 1;
            }
            if (this.pendingU !== endV) {
                this.saveState();
                this.edges.push({ u: this.pendingU, v: endV, color: this.currentColor, deleted: false });
                this.pendingU = endV;
            }
        }
        this.draw();
    }

    onSvgMouseMove(e) {
        if (!this.isSlashing) return;
        const pt = this.getSVGPoint(e);
        this.slashPoints.push({ x: pt.x, y: pt.y });
        if (this.slashPoints.length > 100) this.slashPoints.shift();
        if (this.slashPathEl) this.slashPathEl.setAttribute('d', this._pointsPath(this.slashPoints));
    }

    onSvgMouseUp() {
        if (!this.isSlashing) return;
        const cutIdx = this._findSlashedEdge();
        this.isSlashing = false;
        this.slashPoints = [];
        if (this.slashPathEl) { this.slashPathEl.remove(); this.slashPathEl = null; }
        if (cutIdx !== null) this.executeCut(cutIdx);
    }

    _beginSlash(pt) {
        this.cancelPending(false);
        this.isSlashing = true;
        this.slashPoints = [{ x: pt.x, y: pt.y }];
        this.slashPathEl = document.createElementNS(SVG_NS, 'path');
        this.slashPathEl.classList.add('slash');
        this.slashPathEl.setAttribute('d', this._pointsPath(this.slashPoints));
        this.svg.appendChild(this.slashPathEl);
    }

    _pointsPath(points) { return points.length ? points.map((p, i) => `${i === 0 ? 'M' : 'L'} ${p.x} ${p.y}`).join(' ') : ''; }

    _findVertexHit(pos) {
        for (let i = 0; i < this.vertices.length; i++) {
            const v = this.vertices[i];
            if (!v.deleted && Math.hypot(v.x - pos.x, v.y - pos.y) <= 20) return i;
        }
        return null;
    }

    _isValidCut(color) {
        if (this.editMode) return true;
        if (this.playerToMove === EDGE_BLUE) return color === EDGE_BLUE || color === EDGE_GREEN;
        return color === EDGE_RED || color === EDGE_GREEN;
    }

    _findSlashedEdge() {
        if (this.slashPoints.length < 2) return null;
        for (let i = 0; i < this.edges.length; i++) {
            const edge = this.edges[i];
            if (edge.deleted || !this._isValidCut(edge.color)) continue;
            const edgePoints = this._sampleEdge(i);
            for (let s = 1; s < this.slashPoints.length; s++) {
                for (let e = 1; e < edgePoints.length; e++) {
                    if (segmentsNear(this.slashPoints[s - 1], this.slashPoints[s], edgePoints[e - 1], edgePoints[e], 8)) return i;
                }
            }
        }
        return null;
    }

    _cleanGraphWithMap() {
        const groundAlive = this.vertices.some(v => !v.deleted && v.isGround);
        const vMap = new Map();
        let next = 1;
        for (let i = 0; i < this.vertices.length; i++) {
            const v = this.vertices[i];
            if (v.deleted) continue;
            if (v.isGround) vMap.set(i, 0);
            else vMap.set(i, next++);
        }
        const edges = [];
        const edgeMap = [];
        for (let i = 0; i < this.edges.length; i++) {
            const e = this.edges[i];
            if (e.deleted || !vMap.has(e.u) || !vMap.has(e.v)) continue;
            edges.push({ u: vMap.get(e.u), v: vMap.get(e.v), color: e.color });
            edgeMap.push(i);
        }
        return { graph: { numVertices: Math.max(groundAlive ? 1 : 0, next), numEdges: edges.length, edges }, edgeMap };
    }

    getCleanGraphForConverter() { return this._cleanGraphWithMap().graph; }

    _maskAfterCut(edgeIdx) {
        const { graph, edgeMap } = this._cleanGraphWithMap();
        const cleanIdx = edgeMap.indexOf(edgeIdx);
        if (cleanIdx === -1) return null;
        const tempMask = fullLiveMask(graph.numEdges) & ~(1n << BigInt(cleanIdx));
        return { graph, edgeMap, mask: this.converter.cleanupPosition(graph, tempMask) };
    }

    executeCut(edgeIdx) {
        const edge = this.edges[edgeIdx];
        if (!edge || edge.deleted || !this._isValidCut(edge.color)) return false;
        const cut = this._maskAfterCut(edgeIdx);
        if (!cut) return false;
        this.saveState();
        for (let i = 0; i < cut.edgeMap.length; i++) {
            if (!hasMaskBit(cut.mask, i)) this.edges[cut.edgeMap[i]].deleted = true;
        }
        this._deleteLooseVertices();
        if (!this.editMode) { this.playerToMove = this.playerToMove === EDGE_BLUE ? EDGE_RED : EDGE_BLUE; this._notifyTurn(); }
        this.cancelPending(false);
        this.draw();
        if (!this.editMode && this.botPlayingColor === this.playerToMove) {
            window.setTimeout(() => { const best = this.getBestMoves(); if (best.length) this.executeCut(best[0]); }, 120);
        }
        return true;
    }

    _deleteLooseVertices() {
        const used = new Set();
        for (const e of this.edges) { if (!e.deleted) { used.add(e.u); used.add(e.v); } }
        for (let i = 0; i < this.vertices.length; i++) if (!used.has(i)) this.vertices[i].deleted = true;
    }

    calculateValue() {
        const graph = this.getCleanGraphForConverter();
        if (graph.numEdges === 0) return new Game(this.converter._rt().game_zero());
        return this.converter.convert(graph, fullLiveMask(graph.numEdges));
    }

    _validMoveIndexes() {
        const valid = [];
        for (let i = 0; i < this.edges.length; i++) {
            const e = this.edges[i];
            if (!e.deleted && this._isValidCut(e.color)) valid.push(i);
        }
        return valid;
    }

    getBestMoves() {
        const { graph, edgeMap } = this._cleanGraphWithMap();
        if (!graph.numEdges) return [];
        const validMoves = this._validMoveIndexes().filter(idx => edgeMap.includes(idx));
        if (!validMoves.length) return [];
        const isLeft = this.playerToMove === EDGE_BLUE;
        const zero = Game.zero();
        const values = new Map();
        for (const idx of validMoves) {
            const cleanIdx = edgeMap.indexOf(idx);
            const tempMask = fullLiveMask(graph.numEdges) & ~(1n << BigInt(cleanIdx));
            const maskAfter = this.converter.cleanupPosition(graph, tempMask);
            values.set(idx, this.converter.convert(graph, maskAfter));
        }
        const winning = [];
        for (const [idx, val] of values) if (isLeft ? val.geq(zero) : zero.geq(val)) winning.push(idx);
        const candidates = winning.length ? winning : [...values.keys()];
        const best = [];
        for (const idx of candidates) {
            const val = values.get(idx);
            let isWorse = false;
            const remove = [];
            for (const bIdx of best) {
                const bVal = values.get(bIdx);
                const valGeq = isLeft ? val.geq(bVal) : bVal.geq(val);
                const bestGeq = isLeft ? bVal.geq(val) : val.geq(bVal);
                if (valGeq && !bestGeq) remove.push(bIdx);
                else if (bestGeq && !valGeq) { isWorse = true; break; }
            }
            if (!isWorse) { for (const r of remove) best.splice(best.indexOf(r), 1); best.push(idx); }
        }
        return best;
    }

    analyzeMoves() {
        const { graph, edgeMap } = this._cleanGraphWithMap();
        if (!graph.numEdges) return new Map();
        const validMoves = this._validMoveIndexes().filter(idx => edgeMap.includes(idx));
        const isLeft = this.playerToMove === EDGE_BLUE;
        const root = this.converter.convert(graph, fullLiveMask(graph.numEdges));
        const moveData = new Map();
        for (const idx of validMoves) {
            const cleanIdx = edgeMap.indexOf(idx);
            const mask = this.converter.cleanupPosition(graph, fullLiveMask(graph.numEdges) & ~(1n << BigInt(cleanIdx)));
            moveData.set(idx, { game: this.converter.convert(graph, mask), mask, type: 'normal', refIdx: null });
        }
        const oppColor = isLeft ? EDGE_RED : EDGE_BLUE;
        for (const idx of validMoves) {
            const data = moveData.get(idx);
            let bestRef = null;
            let bestVal = null;
            for (let cleanJ = 0; cleanJ < graph.numEdges; cleanJ++) {
                if (!hasMaskBit(data.mask, cleanJ)) continue;
                const color = graph.edges[cleanJ].color;
                if (color !== oppColor && color !== EDGE_GREEN) continue;
                const after = this.converter.cleanupPosition(graph, data.mask & ~(1n << BigInt(cleanJ)));
                const val = this.converter.convert(graph, after);
                const reversible = isLeft ? root.geq(val) : val.geq(root);
                if (!reversible) continue;
                if (bestRef === null || (isLeft ? bestVal.geq(val) : val.geq(bestVal))) { bestRef = edgeMap[cleanJ]; bestVal = val; }
            }
            if (bestRef !== null) { data.type = 'reversible'; data.refIdx = bestRef; }
        }
        for (const idx of validMoves) {
            const data = moveData.get(idx);
            if (data.type === 'reversible') continue;
            let bestRef = null;
            let bestVal = null;
            for (const k of validMoves) {
                if (k === idx) continue;
                const gK = moveData.get(k).game;
                const gI = data.game;
                const dominated = isLeft ? (gK.geq(gI) && !gI.geq(gK)) : (gI.geq(gK) && !gK.geq(gI));
                if (!dominated) continue;
                if (bestRef === null || (isLeft ? gK.geq(bestVal) : bestVal.geq(gK))) { bestRef = k; bestVal = gK; }
            }
            if (bestRef !== null) { data.type = 'dominated'; data.refIdx = bestRef; }
        }
        return moveData;
    }

    toggleHints() { this.hintsActive = !this.hintsActive; this.draw(); return this.hintsActive; }
    toggleAnalysis() { this.analysisActive = !this.analysisActive; this.draw(); return this.analysisActive; }
    toggleBot() { this.botPlayingColor = this.botPlayingColor === null ? (this.playerToMove === EDGE_BLUE ? EDGE_RED : EDGE_BLUE) : null; return this.botPlayingColor !== null; }
    _notifyTurn() { if (this.onTurnChanged) this.onTurnChanged(); }

    draw() {
        this._clearHoverHint();
        this.svg.innerHTML = '';
        const rect = this.svg.getBoundingClientRect();
        this.groundY = rect.height * 0.9;
        const groundRect = document.createElementNS(SVG_NS, 'rect');
        groundRect.setAttribute('x', 0); groundRect.setAttribute('y', this.groundY);
        groundRect.setAttribute('width', rect.width); groundRect.setAttribute('height', rect.height - this.groundY);
        groundRect.setAttribute('fill', 'var(--color-ground)'); this.svg.appendChild(groundRect);
        const groundLine = document.createElementNS(SVG_NS, 'line');
        groundLine.setAttribute('x1', 0); groundLine.setAttribute('y1', this.groundY);
        groundLine.setAttribute('x2', rect.width); groundLine.setAttribute('y2', this.groundY);
        groundLine.setAttribute('stroke', 'var(--color-ground-line)'); groundLine.setAttribute('stroke-width', '2');
        this.svg.appendChild(groundLine);
        const bestMoves = this.hintsActive ? new Set(this.getBestMoves()) : new Set();
        const analysis = this.analysisActive ? this.analyzeMoves() : new Map();
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
            path.classList.add('edge', e.color === EDGE_BLUE ? 'blue' : e.color === EDGE_RED ? 'red' : 'green');
            if (bestMoves.has(i)) path.classList.add('best');
            if (!this.editMode && !this._isValidCut(e.color)) path.classList.add('dimmed');
            path.addEventListener('mouseenter', (evt) => this._scheduleHoverHint(i, evt));
            path.addEventListener('mouseleave', () => this._clearHoverHint());
            this.svg.appendChild(path);
        }
        for (let i = 0; i < this.vertices.length; i++) {
            const v = this.vertices[i];
            if (v.deleted) continue;
            const circle = document.createElementNS(SVG_NS, 'circle');
            circle.setAttribute('cx', v.x); circle.setAttribute('cy', v.y); circle.setAttribute('r', 12);
            circle.classList.add('vertex');
            if (v.isGround) circle.classList.add('ground');
            if (i === this.pendingU) circle.classList.add('pending');
            this.svg.appendChild(circle);
        }
        if (this.analysisActive) this._drawAnalysisBubbles(analysis);
    }

    _drawAnalysisBubbles(analysis) {
        for (let i = 0; i < this.edges.length; i++) {
            const e = this.edges[i];
            if (e.deleted) continue;
            const data = analysis.get(i);
            let text = String(i), fill = '#ffffff', stroke = '#111111', textFill = '#111111';
            if (data?.type === 'dominated') { text = `${i} D by ${data.refIdx}`; fill = '#dddddd'; textFill = '#777777'; }
            else if (data?.type === 'reversible') { text = `${i} R by ${data.refIdx}`; stroke = '#c83232'; textFill = '#c83232'; }
            const u = this.vertices[e.u];
            const v = this.vertices[e.v];
            this._bubble(text, (u.x + v.x) / 2, (u.y + v.y) / 2, fill, stroke, textFill);
        }
    }

    _bubble(text, x, y, fill, stroke, textFill) {
        const group = document.createElementNS(SVG_NS, 'g');
        const width = Math.max(36, text.length * 8 + 16);
        const height = 27;
        const rect = document.createElementNS(SVG_NS, 'rect');
        rect.setAttribute('x', x - width / 2); rect.setAttribute('y', y - height / 2);
        rect.setAttribute('width', width); rect.setAttribute('height', height); rect.setAttribute('rx', height / 2);
        rect.setAttribute('fill', fill); rect.setAttribute('stroke', stroke); rect.setAttribute('stroke-width', 2);
        const label = document.createElementNS(SVG_NS, 'text');
        label.setAttribute('x', x); label.setAttribute('y', y + 5); label.setAttribute('text-anchor', 'middle');
        label.setAttribute('font-family', 'Consolas, monospace'); label.setAttribute('font-size', '14'); label.setAttribute('font-weight', '700'); label.setAttribute('fill', textFill);
        label.textContent = text;
        group.append(rect, label); this.svg.appendChild(group);
    }

    _scheduleHoverHint(edgeIdx, evt) {
        if (!this.hintsActive || this.editMode || !this._isValidCut(this.edges[edgeIdx].color)) return;
        const pos = this.getSVGPoint(evt);
        this.hoverTimer = window.setTimeout(() => {
            try {
                const cut = this._maskAfterCut(edgeIdx);
                if (!cut) return;
                const val = this.converter.convert(cut.graph, cut.mask);
                this._showHoverHint(val.formatted, pos.x + 15, pos.y - 25);
            } catch (err) { this._showHoverHint('Error', pos.x + 15, pos.y - 25); }
        }, 400);
    }

    _showHoverHint(text, x, y) {
        this._clearHoverHint();
        const label = document.createElementNS(SVG_NS, 'text');
        label.classList.add('hint-label'); label.setAttribute('x', x); label.setAttribute('y', y); label.textContent = ` ${text} `;
        this.hoverLabel = label; this.svg.appendChild(label);
    }
    _clearHoverHint() { if (this.hoverTimer) window.clearTimeout(this.hoverTimer); this.hoverTimer = 0; if (this.hoverLabel) this.hoverLabel.remove(); this.hoverLabel = null; }

    _sampleEdge(edgeIdx) {
        const pairCounts = new Map();
        let offset = 0;
        for (let i = 0; i <= edgeIdx; i++) {
            const e = this.edges[i];
            if (e.deleted) continue;
            const key = e.u < e.v ? `${e.u}-${e.v}` : `${e.v}-${e.u}`;
            const k = pairCounts.get(key) || 0;
            pairCounts.set(key, k + 1);
            if (i === edgeIdx) offset = k;
        }
        const e = this.edges[edgeIdx];
        const u = this.vertices[e.u];
        const v = this.vertices[e.v];
        if (offset === 0) return [{ x: u.x, y: u.y }, { x: v.x, y: v.y }];
        const ctrl = this._bezierControl(u, v, offset);
        const points = [];
        for (let i = 0; i <= 16; i++) {
            const t = i / 16;
            const mt = 1 - t;
            points.push({ x: mt * mt * u.x + 2 * mt * t * ctrl.x + t * t * v.x, y: mt * mt * u.y + 2 * mt * t * ctrl.y + t * t * v.y });
        }
        return points;
    }

    _bezierControl(pu, pv, k) {
        const m = Math.floor((k + 1) / 2);
        const sign = (k % 2 === 1) ? 1 : -1;
        const offsetIndex = sign * m;
        const dx = pv.x - pu.x;
        const dy = pv.y - pu.y;
        const length = Math.hypot(dx, dy);
        if (length < 1e-6) return { x: (pu.x + pv.x) / 2, y: (pu.y + pv.y) / 2 };
        const nx = (-dy / length) * Math.sign(offsetIndex);
        const ny = (dx / length) * Math.sign(offsetIndex);
        const bulge = (32.0 + (Math.abs(offsetIndex) - 1) * 63.0) * Math.sign(offsetIndex);
        return { x: (pu.x + pv.x) * 0.5 + nx * bulge, y: (pu.y + pv.y) * 0.5 + ny * bulge };
    }

    _getBezierPath(pu, pv, k) {
        if (k === 0) return `M ${pu.x} ${pu.y} L ${pv.x} ${pv.y}`;
        const c = this._bezierControl(pu, pv, k);
        return `M ${pu.x} ${pu.y} Q ${c.x} ${c.y} ${pv.x} ${pv.y}`;
    }
}
