import initWasm from './libtoads_and_frogs.js';
import { Game, GameConvert } from '../../core/game.js';
import { TafConverter } from './taf_converter.js';

const ui = {
    grid: document.getElementById('grid'),
    len: document.getElementById('inp-len'),
    btnBuild: document.getElementById('btn-build'),
    btnCalc: document.getElementById('btn-calc'),
    res: document.getElementById('result')
};

let boardLen = 5;
let toadsMask = 0n;
let frogsMask = 0n;

function drawGrid() {
    ui.grid.innerHTML = '';
    ui.grid.style.gridTemplateColumns = `repeat(${boardLen}, var(--cell-size, 40px))`;
    
    for (let i = 0; i < boardLen; i++) {
        const cell = document.createElement('div');
        cell.className = 'cell';
        
        const isToad = (toadsMask & (1n << BigInt(i))) !== 0n;
        const isFrog = (frogsMask & (1n << BigInt(i))) !== 0n;

        if (isToad) { cell.classList.add('toad'); cell.textContent = 'T'; }
        else if (isFrog) { cell.classList.add('frog'); cell.textContent = 'F'; }
        else { cell.textContent = ''; }

        cell.onclick = () => {
            if (isToad) {
                // Cycle Toad to Frog.
                toadsMask &= ~(1n << BigInt(i));
                frogsMask |= (1n << BigInt(i));
            } else if (isFrog) {
                // Cycle Frog to empty.
                frogsMask &= ~(1n << BigInt(i));
            } else {
                // Cycle empty to Toad.
                toadsMask |= (1n << BigInt(i));
            }
            drawGrid();
        };
        ui.grid.appendChild(cell);
    }
}

initWasm().then((wasmModule) => {
    GameConvert.configureRuntime(wasmModule);
    const converter = new TafConverter();

    ui.btnBuild.onclick = () => {
        boardLen = parseInt(ui.len.value);
        if (boardLen > 10) { alert("Max 10 cells."); return; }
        toadsMask = 0n; frogsMask = 0n;
        drawGrid();
    };

    ui.btnCalc.onclick = () => {
        ui.res.textContent = "Computing...";
        const s = performance.now();
        const val = converter.convert(boardLen, toadsMask, frogsMask);
        const e = performance.now();
        ui.res.innerHTML = `Value: <b>${val.formatted}</b> <br><small>Time: ${(e-s).toFixed(1)}ms</small>`;
    };

    drawGrid();
});
