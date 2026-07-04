import os
import glob
import ctypes
import inspect

from ctypes import (
    Structure,
    POINTER,
    byref,
    c_char_p,
    c_float,
    c_int,
    c_size_t,
    c_uint64,
    c_void_p,
)


FORMAT_RAW = 0
FORMAT_FORMATED = 1


def _unique_existing(paths: list[str]) -> list[str]:
    seen = set()
    out = []

    for path in paths:
        if not path:
            continue

        path = os.path.abspath(path)

        if path in seen:
            continue

        seen.add(path)

        if os.path.exists(path):
            out.append(path)

    return out


def get_default_lib_candidates() -> list[str]:
    """
    Tries to find a usable shared library.

    The old version inferred the library name from the caller file. That breaks
    when high-level Game calls GameRuntime lazily from game.py, because it could
    accidentally search for libgame.so.

    This version still tries caller-based paths, but also searches common local
    and build locations.
    """

    candidates = []

    env_path = os.environ.get("SHORT_GAMES_LIB")
    if env_path:
        candidates.append(env_path)

    this_dir = os.path.dirname(os.path.abspath(__file__))
    cwd = os.getcwd()

    ignored_modules = {
        "short_games",
        "game",
        "hb_calculator",
    }

    caller_infos = []

    for frame in inspect.stack():
        filename = frame.filename

        if not filename:
            continue

        abs_file = os.path.abspath(filename)
        module_name = os.path.splitext(os.path.basename(abs_file))[0]

        if module_name in ignored_modules:
            continue

        caller_infos.append((os.path.dirname(abs_file), module_name))

    for caller_dir, module_name in caller_infos:
        candidates.append(
            os.path.join(
                caller_dir,
                "../../../build/short-games",
                module_name,
                f"lib{module_name}.so",
            )
        )

        candidates.append(os.path.join(caller_dir, f"lib{module_name}.so"))

    candidates.extend(glob.glob(os.path.join(this_dir, "lib*.so")))
    candidates.extend(glob.glob(os.path.join(cwd, "lib*.so")))

    candidates.extend(
        glob.glob(
            os.path.abspath(
                os.path.join(this_dir, "../../../build/short-games/*/lib*.so")
            )
        )
    )

    candidates.extend(
        glob.glob(
            os.path.abspath(
                os.path.join(cwd, "../../../build/short-games/*/lib*.so")
            )
        )
    )

    return _unique_existing(candidates)


def get_default_lib_path() -> str:
    candidates = get_default_lib_candidates()

    if not candidates:
        raise FileNotFoundError(
            "Library not found. Tried automatic search. "
            "Set SHORT_GAMES_LIB or call Game.configure(lib_path=...)."
        )

    return candidates[0]


def void_ptr_to(obj) -> c_void_p:
    return ctypes.cast(ctypes.byref(obj), c_void_p)


class CGame(Structure):
    pass


CGame._fields_ = [
    ("left", POINTER(POINTER(CGame))),
    ("right", POINTER(POINTER(CGame))),
]


GamePtr = POINTER(CGame)
GamePtrArrayPtr = POINTER(GamePtr)
GamePtrArrayPtrPtr = POINTER(GamePtrArrayPtr)


class GameRuntime:
    """
    Low-level wrapper around general short-game C API.

    This is not tied to a concrete raw game. It only wraps functions that
    operate on Game* and Game** dynamic arrays.
    """

    def __init__(
        self,
        lib_path: str | None = None,
        memory_multiplier: float = 0.01,
    ):
        if lib_path is None:
            self.lib_path = get_default_lib_path()
        else:
            self.lib_path = os.path.abspath(lib_path)

        if not os.path.exists(self.lib_path):
            raise FileNotFoundError(f"Library not found: {self.lib_path}")

        self.lib = ctypes.CDLL(self.lib_path)
        self.memory_multiplier = self._sanitize_memory_multiplier(memory_multiplier)
        self._initialized = False

        self._bind_game_api()
        GameRuntime.initialize(self)

    def _bind_game_api(self) -> None:
        self.lib.short_game_init.argtypes = [c_float]
        self.lib.short_game_init.restype = None

        self.lib.short_game_free.argtypes = []
        self.lib.short_game_free.restype = None

        self.lib.game_new.argtypes = []
        self.lib.game_new.restype = GamePtr

        self.lib.game_from_game.argtypes = [GamePtr, GamePtr]
        self.lib.game_from_game.restype = GamePtr

        self.lib.game_from_games.argtypes = [GamePtrArrayPtr, GamePtrArrayPtr]
        self.lib.game_from_games.restype = GamePtr

        self.lib.game_zero.argtypes = []
        self.lib.game_zero.restype = GamePtr

        self.lib.game_star.argtypes = []
        self.lib.game_star.restype = GamePtr

        self.lib.game_up.argtypes = []
        self.lib.game_up.restype = GamePtr

        self.lib.game_down.argtypes = []
        self.lib.game_down.restype = GamePtr

        if hasattr(self.lib, "game_one"):
            self.lib.game_one.argtypes = []
            self.lib.game_one.restype = GamePtr

        self.lib.game_geq.argtypes = [GamePtr, GamePtr]
        self.lib.game_geq.restype = c_int

        self.lib.game_canonicalize.argtypes = [GamePtr]
        self.lib.game_canonicalize.restype = GamePtr

        self.lib.game_add.argtypes = [GamePtr, GamePtr]
        self.lib.game_add.restype = GamePtr

        self.lib.game_negate.argtypes = [GamePtr]
        self.lib.game_negate.restype = GamePtr

        self.lib.cool_with_star.argtypes = [GamePtr]
        self.lib.cool_with_star.restype = GamePtr

        self.lib.star_projection.argtypes = [GamePtr]
        self.lib.star_projection.restype = GamePtr

        self.lib.make_int.argtypes = [c_int]
        self.lib.make_int.restype = GamePtr

        self.lib.make_dyadic.argtypes = [c_int, c_int]
        self.lib.make_dyadic.restype = GamePtr

        self.lib.make_nimber.argtypes = [c_int]
        self.lib.make_nimber.restype = GamePtr

        self.lib.make_up_multiple.argtypes = [c_int, c_int]
        self.lib.make_up_multiple.restype = GamePtr

        self.lib.make_down_multiple.argtypes = [c_int, c_int]
        self.lib.make_down_multiple.restype = GamePtr

        self.lib.game_get_string.argtypes = [c_void_p, c_int]
        self.lib.game_get_string.restype = c_char_p

        self.lib.game_from_string.argtypes = [c_char_p]
        self.lib.game_from_string.restype = GamePtr

        self.lib.game_string_last_error.argtypes = []
        self.lib.game_string_last_error.restype = c_char_p

        self.lib.game_len.argtypes = [GamePtrArrayPtrPtr]
        self.lib.game_len.restype = c_size_t

        self.lib.game_cap.argtypes = [GamePtrArrayPtrPtr]
        self.lib.game_cap.restype = c_size_t

        self.lib.game_free.argtypes = [GamePtrArrayPtrPtr]
        self.lib.game_free.restype = None

        self.lib.game_reserve.argtypes = [GamePtrArrayPtrPtr, c_size_t]
        self.lib.game_reserve.restype = None

        self.lib.game_push.argtypes = [GamePtrArrayPtrPtr, GamePtr]
        self.lib.game_push.restype = None

        self.lib.game_append.argtypes = [GamePtrArrayPtrPtr, GamePtr]
        self.lib.game_append.restype = None

        self.lib.game_append_many.argtypes = [GamePtrArrayPtrPtr, GamePtrArrayPtr]
        self.lib.game_append_many.restype = None

        self.lib.game_resize.argtypes = [GamePtrArrayPtrPtr, c_size_t]
        self.lib.game_resize.restype = None

        self.lib.game_pop.argtypes = [GamePtrArrayPtrPtr]
        self.lib.game_pop.restype = GamePtr

        self.lib.game_first.argtypes = [GamePtrArrayPtrPtr]
        self.lib.game_first.restype = GamePtr

        self.lib.game_last.argtypes = [GamePtrArrayPtrPtr]
        self.lib.game_last.restype = GamePtr

        self.lib.game_remove_unordered.argtypes = [GamePtrArrayPtrPtr, c_size_t]
        self.lib.game_remove_unordered.restype = None

    @staticmethod
    def _sanitize_memory_multiplier(memory_multiplier: float) -> float:
        try:
            memory_multiplier = float(memory_multiplier)
        except (TypeError, ValueError):
            return 0.5

        if memory_multiplier > 0.9 or memory_multiplier < 0.1:
            return 0.5

        return memory_multiplier

    def initialize(self) -> None:
        if self._initialized:
            return

        self.lib.short_game_init(c_float(self.memory_multiplier))
        self._initialized = True

    def free(self) -> None:
        if not self._initialized:
            return

        self.lib.short_game_free()
        self._initialized = False

    def game_new(self) -> GamePtr:
        return self.lib.game_new()

    def game_zero(self) -> GamePtr:
        return self.lib.game_zero()

    def game_star(self) -> GamePtr:
        return self.lib.game_star()

    def game_one(self) -> GamePtr:
        if hasattr(self.lib, "game_one"):
            return self.lib.game_one()

        return self.make_int(1)

    def game_up(self) -> GamePtr:
        return self.lib.game_up()

    def game_down(self) -> GamePtr:
        return self.lib.game_down()

    def game_geq(self, a: GamePtr, b: GamePtr) -> bool:
        return bool(self.lib.game_geq(a, b))

    def game_canonicalize(self, game: GamePtr) -> GamePtr:
        return self.lib.game_canonicalize(game)

    def game_add(self, a: GamePtr, b: GamePtr) -> GamePtr:
        return self.lib.game_add(a, b)

    def game_negate(self, game: GamePtr) -> GamePtr:
        return self.lib.game_negate(game)

    def cool_with_star(self, game: GamePtr) -> GamePtr:
        return self.lib.cool_with_star(game)

    def star_projection(self, game: GamePtr) -> GamePtr:
        return self.lib.star_projection(game)

    def make_int(self, n: int) -> GamePtr:
        return self.lib.make_int(c_int(n))

    def make_dyadic(self, p: int, q: int) -> GamePtr | None:
        result = self.lib.make_dyadic(c_int(p), c_int(q))
        return result if result else None

    def make_nimber(self, n: int) -> GamePtr:
        return self.lib.make_nimber(c_int(n))

    def make_up_multiple(self, n: int, with_star: bool | int = False) -> GamePtr:
        return self.lib.make_up_multiple(c_int(n), c_int(int(with_star)))

    def make_down_multiple(self, n: int, with_star: bool | int = False) -> GamePtr:
        return self.lib.make_down_multiple(c_int(n), c_int(int(with_star)))

    def get_game_value_string(self, game: GamePtr, fmt: int = FORMAT_FORMATED) -> str:
        if not game:
            return "NULL"

        raw = self.lib.game_get_string(game, c_int(fmt))
        return raw.decode("utf-8")

    def game_from_string(self, text: str) -> GamePtr:
        if text is None:
            raise ValueError("Input string is None")

        ptr = self.lib.game_from_string(text.encode("utf-8"))

        if not ptr:
            raw_error = self.lib.game_string_last_error()
            message = raw_error.decode("utf-8") if raw_error else "Invalid game string"
            raise ValueError(message)

        return ptr

    def game_from_game(self, left: GamePtr | None, right: GamePtr | None) -> GamePtr:
        return self.lib.game_from_game(left, right)

    def game_from_game_arrays(
        self,
        lefts: GamePtrArrayPtr,
        rights: GamePtrArrayPtr,
    ) -> GamePtr:
        return self.lib.game_from_games(lefts, rights)

    def game_from_games(self, lefts: list[GamePtr], rights: list[GamePtr]) -> GamePtr:
        left_arr = self.game_array_from_list(lefts)
        right_arr = self.game_array_from_list(rights)

        try:
            return self.game_from_game_arrays(left_arr, right_arr)
        finally:
            self.game_free_array(left_arr)
            self.game_free_array(right_arr)

    def game_array_new(self) -> GamePtrArrayPtr:
        return GamePtrArrayPtr()

    def game_len(self, games: GamePtrArrayPtr) -> int:
        return int(self.lib.game_len(byref(games)))

    def game_cap(self, games: GamePtrArrayPtr) -> int:
        return int(self.lib.game_cap(byref(games)))

    def game_free_array(self, games: GamePtrArrayPtr) -> GamePtrArrayPtr:
        self.lib.game_free(byref(games))
        return games

    def game_reserve(
        self,
        games: GamePtrArrayPtr,
        expected_cap: int,
    ) -> GamePtrArrayPtr:
        self.lib.game_reserve(byref(games), c_size_t(expected_cap))
        return games

    def game_push(self, games: GamePtrArrayPtr, value: GamePtr) -> GamePtrArrayPtr:
        self.lib.game_push(byref(games), value)
        return games

    def game_append(self, games: GamePtrArrayPtr, value: GamePtr) -> GamePtrArrayPtr:
        self.lib.game_append(byref(games), value)
        return games

    def game_append_many(
        self,
        games: GamePtrArrayPtr,
        other: GamePtrArrayPtr,
    ) -> GamePtrArrayPtr:
        self.lib.game_append_many(byref(games), other)
        return games

    def game_resize(self, games: GamePtrArrayPtr, new_len: int) -> GamePtrArrayPtr:
        self.lib.game_resize(byref(games), c_size_t(new_len))
        return games

    def game_pop(self, games: GamePtrArrayPtr) -> tuple[GamePtrArrayPtr, GamePtr]:
        value = self.lib.game_pop(byref(games))
        return games, value

    def game_first(self, games: GamePtrArrayPtr) -> GamePtr:
        return self.lib.game_first(byref(games))

    def game_last(self, games: GamePtrArrayPtr) -> GamePtr:
        return self.lib.game_last(byref(games))

    def game_remove_unordered(
        self,
        games: GamePtrArrayPtr,
        index: int,
    ) -> GamePtrArrayPtr:
        self.lib.game_remove_unordered(byref(games), c_size_t(index))
        return games

    def game_array_from_list(self, values: list[GamePtr]) -> GamePtrArrayPtr:
        games = self.game_array_new()

        if values:
            games = self.game_reserve(games, len(values))

            for value in values:
                games = self.game_push(games, value)

        return games


class GameConvertRuntime(GameRuntime):
    """
    Runtime for a concrete raw game implementation.

    This extends GameRuntime with raw-game operations:
    convert, convert_component, num_moves, can_left_move, ...
    """

    RawGameType = None
    PositionType = None

    def __init__(
        self,
        lib_path: str | None = None,
        memory_multiplier: float = 0.01,
    ):
        super().__init__(
            lib_path=lib_path,
            memory_multiplier=memory_multiplier,
        )

        self._convert_initialized = False

        self._bind_raw_game_api()
        self.initialize()

    def _bind_raw_game_api(self) -> None:
        self.lib.convert.argtypes = [c_void_p, c_void_p]
        self.lib.convert.restype = GamePtr

        self.lib.convert_component.argtypes = [c_void_p, c_void_p]
        self.lib.convert_component.restype = GamePtr

        self.lib.convert_init.argtypes = [c_float]
        self.lib.convert_init.restype = None

        self.lib.convert_free.argtypes = []
        self.lib.convert_free.restype = None

        self.lib.num_moves.argtypes = [c_void_p, c_void_p]
        self.lib.num_moves.restype = c_int

        self.lib.can_left_move.argtypes = [c_void_p, c_void_p, c_int]
        self.lib.can_left_move.restype = c_int

        self.lib.can_right_move.argtypes = [c_void_p, c_void_p, c_int]
        self.lib.can_right_move.restype = c_int

        self.lib.do_move_left.argtypes = [c_void_p, c_void_p, c_int]
        self.lib.do_move_left.restype = c_void_p

        self.lib.do_move_right.argtypes = [c_void_p, c_void_p, c_int]
        self.lib.do_move_right.restype = c_void_p

        self.lib.hash_raw_game_position.argtypes = [c_void_p, c_void_p, c_int]
        self.lib.hash_raw_game_position.restype = c_uint64

        self.lib.get_independent_components.argtypes = [
            c_void_p,
            c_void_p,
            POINTER(POINTER(c_void_p)),
        ]
        self.lib.get_independent_components.restype = c_int

        self.lib.position_cache_get.argtypes = [
            c_void_p,
            c_void_p,
            POINTER(GamePtr),
        ]
        self.lib.position_cache_get.restype = c_int

    def raw_game_ptr(self, raw_game) -> c_void_p:
        return void_ptr_to(raw_game)

    def position_ptr(self, position) -> c_void_p:
        return void_ptr_to(position)

    def position_from_ptr(self, ptr):
        if not ptr:
            return None

        if self.PositionType is None:
            raise TypeError("PositionType is not set")

        return ctypes.cast(ptr, POINTER(self.PositionType)).contents

    def convert(self, raw_game, position) -> GamePtr:
        return self.lib.convert(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
        )

    def convert_component(self, raw_game, position) -> GamePtr:
        return self.lib.convert_component(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
        )

    def initialize(self) -> None:
        if not self._convert_initialized:
            self.lib.convert_init(c_float(self.memory_multiplier))
            self._convert_initialized = True

        GameRuntime.initialize(self)

    def free(self) -> None:
        if self._convert_initialized:
            self.lib.convert_free()
            self._convert_initialized = False

        GameRuntime.free(self)

    def num_moves(self, raw_game, position = None) -> int:
        return int(self.lib.num_moves(self.raw_game_ptr(raw_game),
                                      self.position_ptr(position)))

    def can_left_move(self, raw_game, position, move: int) -> bool:
        return bool(
            self.lib.can_left_move(
                self.raw_game_ptr(raw_game),
                self.position_ptr(position),
                c_int(move),
            )
        )

    def can_right_move(self, raw_game, position, move: int) -> bool:
        return bool(
            self.lib.can_right_move(
                self.raw_game_ptr(raw_game),
                self.position_ptr(position),
                c_int(move),
            )
        )

    def do_move_left_ptr(self, raw_game, position, move: int) -> c_void_p:
        return self.lib.do_move_left(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
            c_int(move),
        )

    def do_move_right_ptr(self, raw_game, position, move: int) -> c_void_p:
        return self.lib.do_move_right(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
            c_int(move),
        )

    def do_move_left(self, raw_game, position, move: int):
        ptr = self.do_move_left_ptr(raw_game, position, move)

        if not ptr:
            raise ValueError("Invalid left move")

        return self.position_from_ptr(ptr)

    def do_move_right(self, raw_game, position, move: int):
        ptr = self.do_move_right_ptr(raw_game, position, move)

        if not ptr:
            raise ValueError("Invalid right move")

        return self.position_from_ptr(ptr)

    def hash_raw_game_position(self, raw_game, position, move: int) -> int:
        return int(
            self.lib.hash_raw_game_position(
                self.raw_game_ptr(raw_game),
                self.position_ptr(position),
                c_int(move),
            )
        )

    def get_independent_components(self, raw_game, position):
        out = POINTER(c_void_p)()

        count = self.lib.get_independent_components(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
            byref(out),
        )

        if count == 0:
            return [position]
        return [out[i] for i in range(count)]

    def position_cache_lookup(self, raw_game, position) -> GamePtr | None:
        out = GamePtr()

        found = self.lib.position_cache_get(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
            byref(out),
        )

        return out if found else None
