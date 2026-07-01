import { Game, GameConvert } from '../../core/game.js';

export class DomineeringConverter extends GameConvert {
    constructor() { super({ useC: true }); }

    _allocBoard(w, h) {
        const wasm = this._rt().wasm;
        const ptr = wasm._malloc(2); // width(uint8), height(uint8)
        wasm.setValue(ptr, w, 'i8');
        wasm.setValue(ptr + 1, h, 'i8');
        return ptr;
    }

    _allocPosition(maskBigInt) {
        const wasm = this._rt().wasm;
        const ptr = wasm._malloc(8); // occupied_mask(uint64)
        const low = Number(maskBigInt & 0xFFFFFFFFn);
        const high = Number((maskBigInt >> 32n) & 0xFFFFFFFFn);
        wasm.setValue(ptr, low, 'i32');
        wasm.setValue(ptr + 4, high, 'i32');
        return ptr;
    }

    convert(w, h, maskBigInt) {
        const bPtr = this._allocBoard(w, h);
        const pPtr = this._allocPosition(maskBigInt);
        try {
            return new Game(this._rt().convert(bPtr, pPtr));
        } finally {
            this._rt().wasm._free(bPtr);
            this._rt().wasm._free(pPtr);
        }
    }
}
