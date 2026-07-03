import initWasm from './libhotpotch.js';
import { Game, GameConvert } from '../../core/game.js';
import { HBConverter, EDGE_BLUE, EDGE_RED, EDGE_GREEN } from './hb_converter.js';
import { GraphScene } from './graph.js';

const ui = {
    btnClear: document.getElementById('btn-clear'),
    btnPlayer: document.getElementById('btn-player'),
    btnEdit: document.getElementById('btn-edit'),
    btnMenu: document.getElementById('btn-menu'),
    menu: document.getElementById('hotpotch-menu'),
    btnUndo: document.getElementById('btn-undo'),
    btnRedo: document.getElementById('btn-redo'),
    btnConvert: document.getElementById('btn-convert'),
    btnBest: document.getElementById('btn-best'),
    btnHints: document.getElementById('btn-hints'),
    btnAnalysis: document.getElementById('btn-analysis'),
    btnBot: document.getElementById('btn-bot'),
    btnSave: document.getElementById('btn-save'),
    btnLoad: document.getElementById('btn-load'),
    btnCopyValue: document.getElementById('btn-copy-value'),
    btnHelp: document.getElementById('btn-help'),
    fileLoad: document.getElementById('file-load'),
    btnBuildColor: document.getElementById('btn-build-color'),
    lblResult: document.getElementById('lbl-result'),
    svg: document.getElementById('game-svg')
};

initWasm().then((wasmModule) => {
    try {
        GameConvert.configureRuntime(wasmModule);

        const converter = new HBConverter();
        const scene = new GraphScene(ui.svg, converter);
        let lastComputedValue = null;

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

            ui.btnEdit.classList.toggle('active-toggle', scene.editMode);
            ui.btnHints.classList.toggle('active-toggle', scene.hintsActive);
            ui.btnAnalysis.classList.toggle('active-toggle', scene.analysisActive);
            ui.btnBot.classList.toggle('active-toggle', scene.botPlayingColor !== null);
        };

        scene.onTurnChanged = updateColorsUI;

        const closeMenu = () => { ui.menu.hidden = true; };

        const saveGame = () => {
            // Save only the logical graph state, not transient SVG elements.
            const payload = {
                version: 1,
                vertices: scene.vertices,
                edges: scene.edges,
                currentColor: scene.currentColor,
                playerToMove: scene.playerToMove,
                editMode: scene.editMode
            };
            const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
            const link = document.createElement('a');
            link.href = URL.createObjectURL(blob);
            link.download = 'hotpotch-game.hbg.json';
            document.body.appendChild(link);
            link.click();
            link.remove();
            URL.revokeObjectURL(link.href);
        };

        const loadGame = (data) => {
            if (!Array.isArray(data.vertices) || !Array.isArray(data.edges)) {
                throw new Error('Invalid Hotpotch save file.');
            }
            scene.saveState();
            scene.vertices = data.vertices;
            scene.edges = data.edges;
            scene.currentColor = data.currentColor ?? EDGE_BLUE;
            scene.playerToMove = data.playerToMove ?? EDGE_BLUE;
            scene.editMode = Boolean(data.editMode);
            scene.cancelPending(false);
            scene.draw();
            updateColorsUI();
            ui.lblResult.textContent = 'Loaded.';
        };

        const copyComputedValue = async () => {
            if (!lastComputedValue) {
                ui.lblResult.textContent = 'No computed value to copy.';
                return;
            }

            try {
                await navigator.clipboard.writeText(lastComputedValue);
            } catch {
                const textarea = document.createElement('textarea');
                textarea.value = lastComputedValue;
                textarea.style.position = 'fixed';
                textarea.style.opacity = '0';
                document.body.appendChild(textarea);
                textarea.select();
                document.execCommand('copy');
                textarea.remove();
            }
            ui.lblResult.textContent = `Copied value: ${lastComputedValue}`;
        };

        const showHelp = () => {
            const backdrop = document.createElement('div');
            backdrop.className = 'help-backdrop';
            backdrop.innerHTML = `
                <section class="help-dialog" role="dialog" aria-modal="true" aria-label="Hotpotch help">
                    <h2>Hotpotch Help</h2>
                    <ul>
                        <li><b>Left click on the ground</b>: create or select a ground vertex.</li>
                        <li><b>Left click a vertex, then another point</b>: build an edge. New edges keep chaining from the last endpoint.</li>
                        <li><b>Hold and drag left mouse button through an edge</b>: cut that edge in Fruit Ninja style.</li>
                        <li><b>Right click</b>: cancel the selected build vertex.</li>
                        <li><b>Mouse wheel</b>: switch the build color between blue, red, and green.</li>
                        <li><b>Playing color button</b>: switch the player to move.</li>
                        <li><b>Convert</b>: compute the current game value and winner class.</li>
                        <li><b>Undo / Redo</b>: move through the edit and play history.</li>
                        <li><b>Menu</b>: best moves, play best move, analysis, bot, edit mode, copy value, save, load, and this help.</li>
                        <li><b>Escape</b>: close this help or the menu.</li>
                    </ul>
                    <div class="help-actions"><button type="button" id="btn-help-close">Close</button></div>
                </section>`;
            const close = () => backdrop.remove();
            backdrop.addEventListener('click', (event) => { if (event.target === backdrop) close(); });
            backdrop.querySelector('#btn-help-close').addEventListener('click', close);
            document.body.appendChild(backdrop);
        };

        ui.btnMenu.addEventListener('click', (event) => {
            event.stopPropagation();
            ui.menu.hidden = !ui.menu.hidden;
        });
        ui.menu.addEventListener('click', (event) => event.stopPropagation());
        document.addEventListener('click', closeMenu);
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Escape') {
                closeMenu();
                document.querySelector('.help-backdrop')?.remove();
            }
        });

        ui.btnClear.addEventListener('click', () => {
            scene.clear();
            ui.lblResult.textContent = 'Wins: ?';
        });

        ui.btnPlayer.addEventListener('click', () => {
            if (scene.editMode) return;
            scene.playerToMove = scene.playerToMove === EDGE_BLUE ? EDGE_RED : EDGE_BLUE;
            updateColorsUI();
        });

        ui.btnEdit.addEventListener('click', () => {
            scene.editMode = !scene.editMode;
            scene.draw();
            updateColorsUI();
        });

        ui.btnBuildColor.addEventListener('click', () => {
            if (scene.currentColor === EDGE_BLUE) scene.currentColor = EDGE_RED;
            else if (scene.currentColor === EDGE_RED) scene.currentColor = EDGE_GREEN;
            else scene.currentColor = EDGE_BLUE;
            updateColorsUI();
        });

        ui.btnUndo.addEventListener('click', () => { scene.undo(); ui.lblResult.textContent = 'Wins: ?'; });
        ui.btnRedo.addEventListener('click', () => { scene.redo(); ui.lblResult.textContent = 'Wins: ?'; });

        ui.btnBest.addEventListener('click', () => {
            const best = scene.getBestMoves();
            if (best.length) {
                scene.executeCut(best[0]);
                ui.lblResult.textContent = 'Wins: ?';
            }
            closeMenu();
        });

        ui.btnHints.addEventListener('click', () => {
            scene.toggleHints();
            updateColorsUI();
        });

        ui.btnAnalysis.addEventListener('click', () => {
            scene.toggleAnalysis();
            updateColorsUI();
        });

        ui.btnBot.addEventListener('click', () => {
            scene.toggleBot();
            updateColorsUI();
        });

        ui.btnSave.addEventListener('click', () => {
            saveGame();
            closeMenu();
        });

        ui.btnLoad.addEventListener('click', () => {
            ui.fileLoad.click();
            closeMenu();
        });

        ui.fileLoad.addEventListener('change', async () => {
            const file = ui.fileLoad.files?.[0];
            if (!file) return;
            try {
                loadGame(JSON.parse(await file.text()));
            } catch (err) {
                ui.lblResult.textContent = `Load error: ${err.message}`;
            } finally {
                ui.fileLoad.value = '';
            }
        });

        ui.btnCopyValue.addEventListener('click', () => {
            copyComputedValue();
            closeMenu();
        });

        ui.btnHelp.addEventListener('click', () => {
            showHelp();
            closeMenu();
        });

        ui.btnConvert.addEventListener('click', () => {
            try {
                ui.lblResult.textContent = 'Converting...';
                const start = performance.now();
                const val = scene.calculateValue();
                const end = performance.now();

                const zero = Game.zero();
                const gGeq0 = val.geq(zero);
                const zGeqG = zero.geq(val);

                let winner = '';
                if (gGeq0 && !zGeqG) winner = 'Blue/Left (G > 0)';
                else if (!gGeq0 && zGeqG) winner = 'Red/Right (G < 0)';
                else if (gGeq0 && zGeqG) winner = 'Second player (G = 0)';
                else winner = 'First player (G || 0)';

                lastComputedValue = val.formatted;
                ui.lblResult.innerHTML = `Value: <b style="color: #64b5f6">${val.formatted}</b> | Winner: ${winner} | <span style="color:#888; font-size: 12px;">(${(end-start).toFixed(1)}ms)</span>`;
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
        ui.lblResult.textContent = `UI error: ${error.message}`;
    }
}).catch((err) => {
    console.error(err);
    document.body.innerHTML = '<h2 style="color:red; padding: 20px;">Fatal Wasm error: the file was not found or initialization failed.</h2>';
});
