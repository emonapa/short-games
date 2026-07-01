import initWasm from './libshortcore.js';
import { Game, GameConvert, GameSide, G, GameConvertRuntime } from './game.js';

const ui = {
    btnRun: document.getElementById('btn-run'),
    code: document.getElementById('code-editor'),
    result: document.getElementById('result')
};

initWasm().then((wasmModule) => {
    const runtime = new GameConvertRuntime(wasmModule);
    runtime.initialize();
    Game.useRuntime(runtime);

    ui.btnRun.addEventListener('click', () => {
        ui.result.innerHTML = "Počítám...";
        try {
            // Vytvoření dynamické funkce, která získá přístup k API
            const executor = new Function('Game', 'GameConvert', 'GameSide', 'G', ui.code.value);
            
            const start = performance.now();
            const val = executor(Game, GameConvert, GameSide, G);
            const end = performance.now();

            if (!(val instanceof Game)) {
                throw new Error("Tvůj kód nevrátil instanci třídy Game! (Chybí 'return')");
            }

            ui.result.innerHTML = `
                <strong>Úspěch!</strong><br><br>
                Hodnota: <b>${val.formatted}</b><br>
                Kanonický tvar: ${val.canonical.formatted}<br>
                <em>Čas běhu: ${(end - start).toFixed(2)} ms</em>
            `;
        } catch (err) {
            ui.result.innerHTML = `<span style="color:red">Chyba: ${err.message}</span>`;
            console.error(err);
        }
    });

}).catch((err) => {
    ui.result.innerHTML = `<span style="color:red">Fatální chyba Wasm: ${err.message}</span>`;
});
