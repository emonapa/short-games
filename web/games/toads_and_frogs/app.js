import initWasm from './libtoads_and_frogs.js';
import { Game, GameConvertRuntime } from '../../core/game.js';
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
    ui.grid.style.gridTemplateColumns = `repeat(${boardLen}, 40px)`;
    
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
                // Přepni Toad na Frog
                toadsMask &= ~(1n << BigInt(i));
                frogsMask |= (1n << BigInt(i));
            } else if (isFrog) {
                // Přepni Frog na prázdno
                frogsMask &= ~(1n << BigInt(i));
            } else {
                // Přepni prázdno na Toad
                toadsMask |= (1n << BigInt(i));
            }
            drawGrid();
        };
        ui.grid.appendChild(cell);
    }
}

initWasm().then((wasmModule) => {
    const runtime = new GameConvertRuntime(wasmModule);
    runtime.initialize();
    Game.useRuntime(runtime);
    const converter = new TafConverter();

    ui.btnBuild.onclick = () => {
        boardLen = parseInt(ui.len.value);
        if (boardLen > 64) { alert("Max 64!"); return; }
        toadsMask = 0n; frogsMask = 0n;
        drawGrid();
    };

    ui.btnCalc.onclick = () => {
        ui.res.textContent = "Počítám...";
        const s = performance.now();
        const val = converter.convert(boardLen, toadsMask, frogsMask);
        const e = performance.now();
        ui.res.innerHTML = `Hodnota: <b>${val.formatted}</b> <br> Kanonicky: ${val.canonical.formatted} <br><small>Čas: ${(e-s).toFixed(1)}ms</small>`;
    };

    drawGrid();
});
