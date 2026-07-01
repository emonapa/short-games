const FORMAT_RAW = 0;
const FORMAT_FORMATED = 1;

function _optionsList(value) {
    if (value == null) return [];
    if (value instanceof GameSide || value instanceof G) return value.toList();
    if (Array.isArray(value)) return value;
    return [value];
}

class GameRuntime {
    constructor(wasmModule) {
        this.wasm = wasmModule;
        this._initialized = false;
    }

    initialize(memoryMultiplier = 0.01) {
        if (this._initialized) return;
        this.wasm._short_game_init(memoryMultiplier);
        this._initialized = true;
    }

    free() {
        if (!this._initialized) return;
        this.wasm._short_game_free();
        this._initialized = false;
    }

    game_new() { return this.wasm._game_new(); }
    game_zero() { return this.wasm._game_zero(); }
    game_star() { return this.wasm._game_star(); }
    game_up() { return this.wasm._game_up(); }
    game_down() { return this.wasm._game_down(); }
    
    make_int(n) { return this.wasm._make_int(n); }
    make_dyadic(p, q) { return this.wasm._make_dyadic(p, q); }
    make_nimber(n) { return this.wasm._make_nimber(n); }
    make_up_multiple(n, withStar = 0) { return this.wasm._make_up_multiple(n, withStar); }
    make_down_multiple(n, withStar = 0) { return this.wasm._make_down_multiple(n, withStar); }
    
    game_geq(a, b) { return Boolean(this.wasm._game_geq(a, b)); }
    game_add(a, b) { return this.wasm._game_add(a, b); }
    game_negate(game) { return this.wasm._game_negate(game); }
    game_canonicalize(game) { return this.wasm._game_canonicalize(game); }
    
    cool_with_star(game) { return this.wasm._cool_with_star(game); }
    star_projection(game) { return this.wasm._star_projection(game); }

    game_from_game(left, right) {
        return this.wasm._game_from_game(left || 0, right || 0);
    }

    game_from_game_arrays(lefts, rights) {
        return this.wasm._game_from_games(lefts, rights);
    }

    get_game_value_string(gamePtr, fmt = FORMAT_FORMATED) {
        if (!gamePtr) return "NULL";
        const strPtr = this.wasm._game_get_string(gamePtr, fmt);
        return this.wasm.UTF8ToString(strPtr);
    }

    game_from_string(text) {
        if (!text) throw new Error("Input string is None");
        const ptr = this.wasm.stringToNewUTF8(text);
        const resultPtr = this.wasm._game_from_string(ptr);
        this.wasm._free(ptr); 

        if (!resultPtr) {
            const errPtr = this.wasm._game_string_last_error();
            const errMsg = errPtr ? this.wasm.UTF8ToString(errPtr) : "Invalid game string";
            throw new Error(errMsg);
        }
        return resultPtr;
    }

    game_array_new() { return 0; }
    game_len(gamesPtr) { return this.wasm._game_len(gamesPtr); }
    game_reserve(gamesPtr, expectedCap) { return this.wasm._game_reserve(gamesPtr, expectedCap); }
    game_push(gamesPtr, valuePtr) { return this.wasm._game_push(gamesPtr, valuePtr); }
    game_append(gamesPtr, valuePtr) { return this.wasm._game_append(gamesPtr, valuePtr); }
    game_append_many(gamesPtr, otherPtr) { return this.wasm._game_append_many(gamesPtr, otherPtr); }
    game_resize(gamesPtr, newLen) { return this.wasm._game_resize(gamesPtr, newLen); }
    game_free_array(gamesPtr) { this.wasm._game_free(gamesPtr); return gamesPtr; }
    
    game_pop(gamesPtr) {
        const val = this.wasm._game_pop(gamesPtr);
        return { array: gamesPtr, value: val };
    }
    game_first(gamesPtr) { return this.wasm._game_first(gamesPtr); }
    game_last(gamesPtr) { return this.wasm._game_last(gamesPtr); }
    game_remove_unordered(gamesPtr, index) { return this.wasm._game_remove_unordered(gamesPtr, index); }
}

export class GameSide {
    constructor(game, side) {
        if (side !== "left" && side !== "right") throw new Error("side must be 'left' or 'right'");
        this.game = game;
        this.side = side;
    }

    get _rt() { return Game._rt(); }

    get _array() {
        const offset = this.side === "left" ? 0 : 4; 
        return this._rt.wasm.getValue(this.game.ptr + offset, 'i32');
    }

    set _array(valuePtr) {
        const offset = this.side === "left" ? 0 : 4;
        this._rt.wasm.setValue(this.game.ptr + offset, valuePtr, 'i32');
    }

    toList() {
        const list = [];
        for (let i = 0; i < this.length; i++) list.push(this.get(i));
        return list;
    }

    reserve(expectedCap) {
        this._array = this._rt.game_reserve(this._array, expectedCap);
        return this;
    }

    append(value) {
        if (value instanceof GameSide) {
            this._array = this._rt.game_append_many(this._array, value._array);
            return this;
        }
        if (value instanceof G || Array.isArray(value)) {
            for (const child of value) this.append(child);
            return this;
        }
        this._array = this._rt.game_append(this._array, Game.ptrOf(value));
        return this;
    }

    push(child) {
        this._array = this._rt.game_push(this._array, Game.ptrOf(child));
        return this;
    }

    pop() {
        const res = this._rt.game_pop(this._array);
        this._array = res.array;
        return new Game(res.value);
    }

    first() { return new Game(this._rt.game_first(this._array)); }
    last() { return new Game(this._rt.game_last(this._array)); }
    
    removeUnordered(index) {
        this._array = this._rt.game_remove_unordered(this._array, index);
        return this;
    }

    clear() {
        this._array = this._rt.game_resize(this._array, 0);
        return this;
    }

    get length() { return this._rt.game_len(this._array); }
    
    get(index) {
        let n = this.length;
        if (index < 0) index += n;
        if (index < 0 || index >= n) throw new Error(`IndexError: ${index}`);
        
        const childPtr = this._rt.wasm.getValue(this._array + (index * 4), 'i32');
        return new Game(childPtr);
    }

    or(right) { return Game.new(this.toList(), _optionsList(right)); }
    ror(left) { return Game.new(_optionsList(left), this.toList()); }
}

export class G {
    constructor(...items) { this.items = items; }
    toList() { return this.items; }
    or(right) { return Game.new(this.toList(), _optionsList(right)); }
    ror(left) { return Game.new(_optionsList(left), this.toList()); }
}

export class Game {
    static _runtime = null;

    constructor(ptr) {
        if (!ptr) throw new Error("Game got NULL pointer");
        this.ptr = ptr;
        this._left = new GameSide(this, "left");
        this._right = new GameSide(this, "right");
    }

    static configure(wasmModule, memoryMultiplier = 0.01) {
        if (Game._runtime) {
            try { Game._runtime.free(); } catch (e) {}
        }
        Game._runtime = new GameRuntime(wasmModule);
        Game._runtime.initialize(memoryMultiplier);
    }

    static useRuntime(runtime) {
        Game._runtime = runtime;
    }

    static _rt() {
        if (!Game._runtime) throw new Error("Runtime not configured.");
        return Game._runtime;
    }

    static ptrOf(value) { return value instanceof Game ? value.ptr : value; }
    static wrap(ptr) { return new Game(ptr); }

    static new(left = null, right = null) {
        const rt = Game._rt();
        if (left == null && right == null) return new Game(rt.game_new());

        const leftItems = left == null ? [] : _optionsList(left);
        const rightItems = right == null ? [] : _optionsList(right);

        if (leftItems.length <= 1 && rightItems.length <= 1) {
            const leftPtr = leftItems.length ? Game.ptrOf(leftItems[0]) : 0;
            const rightPtr = rightItems.length ? Game.ptrOf(rightItems[0]) : 0;
            return new Game(rt.game_from_game(leftPtr, rightPtr));
        }

        let leftArr = rt.game_array_new();
        let rightArr = rt.game_array_new();

        try {
            for (const child of leftItems) leftArr = rt.game_push(leftArr, Game.ptrOf(child));
            for (const child of rightItems) rightArr = rt.game_push(rightArr, Game.ptrOf(child));
            return new Game(rt.game_from_game_arrays(leftArr, rightArr));
        } finally {
            rt.game_free_array(leftArr);
            rt.game_free_array(rightArr);
        }
    }

    static zero() { return new Game(Game._rt().game_zero()); }
    static one() { return new Game(Game._rt().make_int(1)); }
    static star(n = 1) { return n === 1 ? new Game(Game._rt().game_star()) : new Game(Game._rt().make_nimber(n)); }
    static nimber(n = 1) { return new Game(Game._rt().make_nimber(n)); }
    static up(n = 1) { return n === 1 ? new Game(Game._rt().game_up()) : new Game(Game._rt().make_up_multiple(n, 0)); }
    static down(n = 1) { return n === 1 ? new Game(Game._rt().game_down()) : new Game(Game._rt().make_down_multiple(n, 0)); }
    static upStar(n = 1) { return new Game(Game._rt().make_up_multiple(n, 1)); }
    static downStar(n = 1) { return new Game(Game._rt().make_down_multiple(n, 1)); }
    static integer(n) { return new Game(Game._rt().make_int(n)); }
    static dyadic(p, q = 1) {
        const ptr = Game._rt().make_dyadic(p, q);
        if (!ptr) throw new Error("Denominator must be a positive power of 2");
        return new Game(ptr);
    }
    
    static fromString(text) { return new Game(Game._rt().game_from_string(text)); }

    get left() { return this._left; }
    get right() { return this._right; }
    get raw() { return this.toString(FORMAT_RAW); }
    get formatted() { return this.toString(FORMAT_FORMATED); }
    get canonical() { return this.canonicalized(); }
    get negated() { return this.negate(); }
    
    get cooledWithStar() { return new Game(Game._rt().cool_with_star(this.ptr)); }
    get starProjection() { return new Game(Game._rt().star_projection(this.ptr)); }
    get isInfinitesimal() { return this.cooledWithStar.starProjection.eq(Game.zero()); }
    get fuzzy() { return !this.geq(Game.zero()) && !Game.zero().geq(this); }

    L(child) { this.left.append(child); return this; }
    R(child) { this.right.append(child); return this; }

    canonicalize() {
        this.ptr = Game._rt().game_canonicalize(this.ptr);
        return this;
    }

    canonicalized() { return new Game(Game._rt().game_canonicalize(this.ptr)); }
    negate() { return new Game(Game._rt().game_negate(this.ptr)); }
    add(other) { return new Game(Game._rt().game_add(this.ptr, Game.ptrOf(other))); }
    sub(other) {
        const otherNeg = Game._rt().game_negate(Game.ptrOf(other));
        return new Game(Game._rt().game_add(this.ptr, otherNeg));
    }

    geq(other) { return Game._rt().game_geq(this.ptr, Game.ptrOf(other)); }
    leq(other) { return Game._rt().game_geq(Game.ptrOf(other), this.ptr); }
    eq(other) { return this.geq(other) && this.leq(other); }
    greater(other) { return this.geq(other) && !this.leq(other); }
    less(other) { return this.leq(other) && !this.geq(other); }
    confused(other) { return !this.geq(other) && !this.leq(other); }

    toString(fmt = FORMAT_FORMATED) { return Game._rt().get_game_value_string(this.ptr, fmt); }
    or(right) { return Game.new([this], _optionsList(right)); }
    ror(left) { return Game.new(_optionsList(left), [this]); }
}

export class GameConvertRuntime extends GameRuntime {
    constructor(wasmModule) {
        super(wasmModule);
        this._convertInitialized = false;
    }

    initialize(memoryMultiplier = 0.01) {
        if (!this._convertInitialized && this.wasm._convert_init) {
            this.wasm._convert_init(memoryMultiplier);
            this._convertInitialized = true;
        }
        super.initialize(memoryMultiplier);
    }

    free() {
        if (this._convertInitialized && this.wasm._convert_free) {
            this.wasm._convert_free();
            this._convertInitialized = false;
        }
        super.free();
    }

    convert(rawGamePtr, posPtr) { return this.wasm._convert(rawGamePtr, posPtr); }
    convert_component(rawGamePtr, posPtr) { return this.wasm._convert_component(rawGamePtr, posPtr); }
    
    num_moves(rawGamePtr, posPtr) { return this.wasm._num_moves(rawGamePtr, posPtr); }
    can_left_move(rawGamePtr, posPtr, move) { return Boolean(this.wasm._can_left_move(rawGamePtr, posPtr, move)); }
    can_right_move(rawGamePtr, posPtr, move) { return Boolean(this.wasm._can_right_move(rawGamePtr, posPtr, move)); }
    do_move_left(rawGamePtr, posPtr, move) { return this.wasm._do_move_left(rawGamePtr, posPtr, move); }
    do_move_right(rawGamePtr, posPtr, move) { return this.wasm._do_move_right(rawGamePtr, posPtr, move); }
    hash_raw_game_position(rawGamePtr, posPtr, move) { return this.wasm._hash_raw_game_position(rawGamePtr, posPtr, move); }
}

export class GameConvert {
    constructor({ runtime = null, useC = false } = {}) {
        this._runtime = runtime;
        this._useC = useC;
        this._positionCache = new Map();

        if (this._useC && !this._runtime) {
            this._runtime = Game._rt();
        }
    }

    _rt() {
        if (this._runtime) return this._runtime;
        if (!this._useC) throw new Error("Python/JS GameConvert has no C runtime configured");
        return Game._rt();
    }

    initialize() {
        this._positionCache.clear();
        if (this._runtime) this._runtime.initialize();
    }

    free() {
        this._positionCache.clear();
        if (this._runtime) this._runtime.free();
    }

    convert(rawGame, position) {
        if (this._useC) {
            return new Game(this._rt().convert(rawGame, position));
        }

        let total = Game.zero();
        for (const componentPosition of this.independentComponents(rawGame, position)) {
            const componentValue = this.convertComponent(rawGame, componentPosition);
            total = total.add(componentValue);
        }

        return total.canonical;
    }

    convertComponent(rawGame, position) {
        if (this._useC) {
            return new Game(this._rt().convert_component(rawGame, position));
        }

        const key = this.positionCacheKey(rawGame, position);
        if (this._positionCache.has(key)) {
            return this._positionCache.get(key);
        }

        const leftOptions = [];
        const rightOptions = [];
        const moves = this.numMoves(rawGame, position);

        for (let move = 0; move < moves; move++) {
            if (this.canLeftMove(rawGame, position, move)) {
                const childPosition = this.doMoveLeft(rawGame, position, move);
                const childGame = this.convertComponent(rawGame, childPosition);
                leftOptions.push(childGame);
            }

            if (this.canRightMove(rawGame, position, move)) {
                const childPosition = this.doMoveRight(rawGame, position, move);
                const childGame = this.convertComponent(rawGame, childPosition);
                rightOptions.push(childGame);
            }
        }

        const result = Game.new(leftOptions, rightOptions).canonical;
        this._positionCache.set(key, result);
        return result;
    }

    independentComponents(rawGame, position) { return [position]; }

    positionCacheKey(rawGame, position) {
        return `${rawGame}_${this.hashGraphState(rawGame, position)}`;
    }

    hashGraphState(rawGame, position) {
        let totalHash = 0;
        const moves = this.numMoves(rawGame, position);
        for (let move = 0; move < moves; move++) {
            if (this.canLeftMove(rawGame, position, move) || this.canRightMove(rawGame, position, move)) {
                totalHash ^= this.hashRawGamePosition(rawGame, position, move);
            }
        }
        return totalHash;
    }

    numMoves(rawGame, position = null) {
        if (this._useC) return this._rt().num_moves(rawGame, position);
        throw new Error("numMoves must be implemented for JS backend");
    }
    canLeftMove(rawGame, position, move) {
        if (this._useC) return this._rt().can_left_move(rawGame, position, move);
        throw new Error("canLeftMove must be implemented for JS backend");
    }
    canRightMove(rawGame, position, move) {
        if (this._useC) return this._rt().can_right_move(rawGame, position, move);
        throw new Error("canRightMove must be implemented for JS backend");
    }
    doMoveLeft(rawGame, position, move) {
        if (this._useC) return this._rt().do_move_left(rawGame, position, move);
        throw new Error("doMoveLeft must be implemented for JS backend");
    }
    doMoveRight(rawGame, position, move) {
        if (this._useC) return this._rt().do_move_right(rawGame, position, move);
        throw new Error("doMoveRight must be implemented for JS backend");
    }
    hashRawGamePosition(rawGame, position, move) {
        if (this._useC) return this._rt().hash_raw_game_position(rawGame, position, move);
        throw new Error("hashRawGamePosition must be implemented for JS backend");
    }
}
