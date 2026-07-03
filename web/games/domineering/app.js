import initWasm from './libdomineering.js';
import { Game, GameConvert } from '../../core/game.js';
import { DomineeringConverter } from './domineering_converter.js';

const ui = {
    grid: document.getElementById('grid'),
    w: document.getElementById('inp-w'),
    h: document.getElementById('inp-h'),
    btnBuild: document.getElementById('btn-build'),
    btnCalc: document.getElementById('btn-calc'),
    res: document.getElementById('result')
};

let boardW = 3;
let boardH = 3;
let mask = 0n;

function drawGrid() {
    ui.grid.innerHTML = '';
    ui.grid.style.gridTemplateColumns = `repeat(${boardW}, var(--cell-size, 40px))`;
    
    for (let r = 0; r < boardH; r++) {
        for (let c = 0; c < boardW; c++) {
            const idx = r * boardW + c;
            const cell = document.createElement('div');
            cell.className = 'cell';
            
            if (mask & (1n << BigInt(idx))) {
                cell.classList.add('removed');
            } else {
                cell.textContent = idx;
            }

            cell.onclick = () => {
                mask ^= (1n << BigInt(idx));
                drawGrid();
            };
            ui.grid.appendChild(cell);
        }
    }
}

initWasm().then((wasmModule) => {
    GameConvert.configureRuntime(wasmModule);
    const converter = new DomineeringConverter();

    ui.btnBuild.onclick = () => {
        boardW = parseInt(ui.w.value);
        boardH = parseInt(ui.h.value);
        if (boardW * boardH > 64) {
            alert("Board is too large. Maximum is 64 cells."); return;
        }
        mask = 0n;
        drawGrid();
    };

    ui.btnCalc.onclick = () => {
        ui.res.textContent = "Computing...";
        const s = performance.now();
        const val = converter.convert(boardW, boardH, mask);
        const e = performance.now();
        ui.res.innerHTML = `Value: <b>${val.formatted}</b> <br><small>Time: ${(e-s).toFixed(1)}ms</small>`;
    };

    drawGrid();
});
