import { Game, GameConvert } from '../../core/game.js';

export const MAX_VERTICES = 128;
export const MAX_EDGES = 128;
export const EDGE_BLUE = 0;
export const EDGE_RED = 1;
export const EDGE_GREEN = 2;

function intToUInt128(valueBigInt) {
    if (valueBigInt < 0n) throw new Error("uint128 value must be non-negative");
    const low = valueBigInt & 0xFFFFFFFFFFFFFFFFn;
    const high = (valueBigInt >> 64n) & 0xFFFFFFFFFFFFFFFFn;
    return { low, high };
}

function uint128FromPtr(wasm, ptr) {
    const low1 = BigInt(wasm.getValue(ptr, 'i32') >>> 0);
    const low2 = BigInt(wasm.getValue(ptr + 4, 'i32') >>> 0);
    const high1 = BigInt(wasm.getValue(ptr + 8, 'i32') >>> 0);
    const high2 = BigInt(wasm.getValue(ptr + 12, 'i32') >>> 0);
    return low1 | (low2 << 32n) | (high1 << 64n) | (high2 << 96n);
}

export function fullLiveMask(numEdges) {
    if (numEdges < 0 || numEdges > 128) {
        throw new Error("edge mask supports only edge indexes 0..127");
    }
    return numEdges ? (1n << BigInt(numEdges)) - 1n : 0n;
}

export class HBConverter extends GameConvert {
    constructor() {
        super({ useC: true });
    }

    _allocGraph(graphObj) {
        const wasm = this._rt().wasm;
        const ptr = wasm._malloc(1028); // 4-byte header + 128 edge records.

        wasm.setValue(ptr, graphObj.numVertices, 'i8');
        wasm.setValue(ptr + 1, graphObj.numEdges, 'i8');

        for (let i = 0; i < graphObj.numEdges; i++) {
            const edge = graphObj.edges[i];
            const edgePtr = ptr + 4 + (i * 8);
            
            wasm.setValue(edgePtr, edge.u, 'i8');
            wasm.setValue(edgePtr + 1, edge.v, 'i8');
            wasm.setValue(edgePtr + 4, edge.color, 'i32');
        }

        return ptr;
    }

    _allocPosition(liveMaskBigInt) {
        const wasm = this._rt().wasm;
        const ptr = wasm._malloc(16);
        const { low, high } = intToUInt128(liveMaskBigInt);
        
        // Store uint128 as four 32-bit words because wasm memory is byte-addressed.
        const low1 = Number(low & 0xFFFFFFFFn);
        const low2 = Number((low >> 32n) & 0xFFFFFFFFn);
        const high1 = Number(high & 0xFFFFFFFFn);
        const high2 = Number((high >> 32n) & 0xFFFFFFFFn);

        wasm.setValue(ptr, low1, 'i32');
        wasm.setValue(ptr + 4, low2, 'i32');
        wasm.setValue(ptr + 8, high1, 'i32');
        wasm.setValue(ptr + 12, high2, 'i32');
        
        return ptr;
    }

    convert(graphObj, liveMaskBigInt) {
        const graphPtr = this._allocGraph(graphObj);
        const posPtr = this._allocPosition(liveMaskBigInt);
        
        try {
            const resultPtr = this._rt().convert(graphPtr, posPtr);
            return new Game(resultPtr);
        } finally {
            this._rt().wasm._free(graphPtr);
            this._rt().wasm._free(posPtr);
        }
    }

    convertComponent(graphObj, liveMaskBigInt) {
        const graphPtr = this._allocGraph(graphObj);
        const posPtr = this._allocPosition(liveMaskBigInt);
        
        try {
            const resultPtr = this._rt().convert_component(graphPtr, posPtr);
            return new Game(resultPtr);
        } finally {
            this._rt().wasm._free(graphPtr);
            this._rt().wasm._free(posPtr);
        }
    }

    cleanupPosition(graphObj, liveMaskBigInt) {
        const wasm = this._rt().wasm;
        if (!wasm._cleanup_position) {
            throw new Error("Hotpotch wasm does not export cleanup_position");
        }

        const graphPtr = this._allocGraph(graphObj);
        const resultPtr = wasm._malloc(16);
        const { low, high } = intToUInt128(liveMaskBigInt);

        try {
            wasm._cleanup_position(resultPtr, graphPtr, low, high);
            return uint128FromPtr(wasm, resultPtr);
        } finally {
            wasm._free(resultPtr);
            wasm._free(graphPtr);
        }
    }
}
