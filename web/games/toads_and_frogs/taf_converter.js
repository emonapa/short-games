import { Game, GameConvert } from '../../core/game.js';

export class TafConverter extends GameConvert {
    constructor() { super({ useC: true }); }

    _allocBoard(length) {
        const wasm = this._rt().wasm;
        const ptr = wasm._malloc(1); // length(uint8)
        wasm.setValue(ptr, length, 'i8');
        return ptr;
    }

    _allocPosition(toadsMask, frogsMask) {
        const wasm = this._rt().wasm;
        const ptr = wasm._malloc(16); // toads_mask(uint64), frogs_mask(uint64)
        
        wasm.setValue(ptr, Number(toadsMask & 0xFFFFFFFFn), 'i32');
        wasm.setValue(ptr + 4, Number((toadsMask >> 32n) & 0xFFFFFFFFn), 'i32');
        
        wasm.setValue(ptr + 8, Number(frogsMask & 0xFFFFFFFFn), 'i32');
        wasm.setValue(ptr + 12, Number((frogsMask >> 32n) & 0xFFFFFFFFn), 'i32');
        return ptr;
    }

    convert(len, toadsMask, frogsMask) {
        const bPtr = this._allocBoard(len);
        const pPtr = this._allocPosition(toadsMask, frogsMask);
        try {
            return new Game(this._rt().convert(bPtr, pPtr));
        } finally {
            this._rt().wasm._free(bPtr);
            this._rt().wasm._free(pPtr);
        }
    }
}
