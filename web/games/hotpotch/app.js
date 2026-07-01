import initWasm from './libhotpotch.js';
import { Game, GameConvertRuntime } from './game.js';
import { HBConverter, EDGE_BLUE, EDGE_RED, EDGE_GREEN } from './hb_converter.js';
import { GraphScene } from './graph.js';

const ui = {
    btnClear: document.getElementById('btn-clear'),
    btnPlayer: document.getElementById('btn-player'),
    btnEdit: document.getElementById('btn-edit'),
    btnUndo: document.getElementById('btn-undo'),
    btnRedo: document.getElementById('btn-redo'),
    btnConvert: document.getElementById('btn-convert'),
    btnBuildColor: document.getElementById('btn-build-color'),
    lblResult: document.getElementById('lbl-result'),
    svg: document.getElementById('game-svg')
};

initWasm().then((wasmModule) => {
    try {
        const runtime = new GameConvertRuntime(wasmModule);
        runtime.initialize();
        Game.useRuntime(runtime);
        
        const converter = new HBConverter();
        const scene = new GraphScene(ui.svg, converter);
        
        const updateColorsUI = () => {
            ui.btnPlayer.className = 'color-btn';
            if (scene.editMode) {
                ui.btnPlayer.classList.add('green', 'edit-active');
            } else {
                ui.btnPlayer.classList.add(scene.playerToMove === EDGE_BLUE ? 'blue' : 'red');
            }
            
            ui.btnBuildColor.className = 'color-btn';
            if (scene.currentColor === EDGE_BLUE) ui.btnBuildColor.classList.add('blue');
            else if (scene.currentColor === EDGE_RED) ui.btnBuildColor.classList.add('red');
            else ui.btnBuildColor.classList.add('green');
            
            ui.btnEdit.style.background = scene.editMode ? 'var(--btn-active)' : 'var(--btn-bg)';
        };

        ui.btnClear.addEventListener('click', () => {
            scene.clear();
            ui.lblResult.textContent = "Wins: ?";
        });

        ui.btnPlayer.addEventListener('click', () => {
            if (scene.editMode) return;
            scene.playerToMove = (scene.playerToMove === EDGE_BLUE) ? EDGE_RED : EDGE_BLUE;
            updateColorsUI();
        });

        ui.btnEdit.addEventListener('click', () => {
            scene.editMode = !scene.editMode;
            updateColorsUI();
        });

        ui.btnBuildColor.addEventListener('click', () => {
            if (scene.currentColor === EDGE_BLUE) scene.currentColor = EDGE_RED;
            else if (scene.currentColor === EDGE_RED) scene.currentColor = EDGE_GREEN;
            else scene.currentColor = EDGE_BLUE;
            updateColorsUI();
        });

        ui.btnUndo.addEventListener('click', () => { scene.undo(); ui.lblResult.textContent = "Wins: ?"; });
        ui.btnRedo.addEventListener('click', () => { scene.redo(); ui.lblResult.textContent = "Wins: ?"; });

        ui.btnConvert.addEventListener('click', () => {
            try {
                ui.lblResult.textContent = "Converting...";
                const start = performance.now();
                const val = scene.calculateValue();
                const end = performance.now();
                
                const zero = Game.zero();
                const gGeq0 = val.geq(zero);
                const zGeqG = zero.geq(val);
                
                let winner = "";
                if (gGeq0 && !zGeqG) winner = "Modrý/Left (G > 0)";
                else if (!gGeq0 && zGeqG) winner = "Červený/Right (G < 0)";
                else if (gGeq0 && zGeqG) winner = "Druhý hráč/Second (G = 0)";
                else winner = "První hráč/First (G || 0)";
                
                ui.lblResult.innerHTML = `Hodnota: <b style="color: #64b5f6">${val.formatted}</b> | Vítěz: ${winner} | <span style="color:#888; font-size: 12px;">(${(end-start).toFixed(1)}ms)</span>`;
            } catch (err) {
                ui.lblResult.textContent = `Error: ${err.message}`;
            }
        });

        ui.svg.addEventListener('wheel', (e) => {
            if (e.deltaY === 0) return;
            ui.btnBuildColor.click();
        });

        scene.draw();
        updateColorsUI();

    } catch (error) {
        console.error(error);
        ui.lblResult.textContent = `Chyba UI: ${error.message}`;
    }
}).catch((err) => {
    console.error(err);
    document.body.innerHTML = `<h2 style="color:red; padding: 20px;">Fatální chyba Wasm: Soubor nenalezen nebo se nepodařilo inicializovat.</h2>`;
});
