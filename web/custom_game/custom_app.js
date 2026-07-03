import initWasm from '../core/libshortcore.js';
import { Game, GameConvert, GameSide, G } from '../core/game.js';
import { CodeJar } from './codejar.min.js';

const ui = {
    btnRun: document.getElementById('btn-run'),
    code: document.getElementById('code-editor'),
    result: document.getElementById('result')
};

const jar = CodeJar(ui.code, (editor) => {
    Prism.highlightElement(editor);
}, { tab: "    " });

initWasm().then((wasmModule) => {
    Game.configure(wasmModule);

    ui.btnRun.addEventListener('click', () => {
        ui.result.innerHTML = "Running...";
        try {
            // Execute user code with access to the public game API only.
            const executor = new Function('Game', 'GameConvert', 'GameSide', 'G', jar.toString());
            
            const start = performance.now();
            const val = executor(Game, GameConvert, GameSide, G);
            const end = performance.now();

            if (!(val instanceof Game)) {
                throw new Error("Your code did not return a Game instance. Did you forget return?");
            }

            ui.result.innerHTML = `
                <strong>Success!</strong><br><br>
                Value: <b>${val.formatted}</b><br>
                <em>Runtime: ${(end - start).toFixed(2)} ms</em>
            `;
        } catch (err) {
            ui.result.innerHTML = `<span style="color:red">Error: ${err.message}</span>`;
            console.error(err);
        }
    });

}).catch((err) => {
    ui.result.innerHTML = `<span style="color:red">Fatal Wasm error: ${err.message}</span>`;
});
