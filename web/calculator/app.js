import initWasm from '../core/libshortcore.js';
import { Game } from '../core/game.js';

const expression = document.getElementById('expression');
const error = document.getElementById('error');
expression.disabled = true;

try {
    const wasm = await initWasm();
    Game.configure(wasm);
    expression.disabled = false;
    expression.focus();

    expression.addEventListener('input', () => { error.textContent = ''; });
    expression.addEventListener('keydown', (event) => {
        if (event.key !== 'Enter') return;
        event.preventDefault();
        try {
            expression.value = Game.fromString(expression.value).formatted;
            error.textContent = '';
        } catch (reason) {
            error.textContent = reason instanceof Error ? reason.message : String(reason);
        }
    });
} catch (reason) {
    error.textContent = `Calculator could not be loaded: ${reason instanceof Error ? reason.message : String(reason)}`;
}
