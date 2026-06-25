import os
import ctypes

import inspect # automaticke hledani knihovny
def get_default_lib_path() -> str:
    caller_file = inspect.stack()[3].filename
    caller_name = os.path.splitext(os.path.basename(caller_file))[0]

    return f"../../../build/short-games/{caller_name}/lib{caller_name}.so"

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

FORMAT_FORMATED = 1

class CGame(Structure):
    pass

CGame._fields_ = [
    ("left", POINTER(POINTER(CGame))),
    ("right", POINTER(POINTER(CGame))),
]

GamePtr = POINTER(CGame)
GamePtrArrayPtr = POINTER(GamePtr)
GamePtrArrayPtrPtr = POINTER(GamePtrArrayPtr)

def void_ptr_to(obj) -> c_void_p:
    return ctypes.cast(ctypes.byref(obj), c_void_p)

class ShortGameSolver:
    def __init__(self, lib_path: str | None = None):
        automatic_lib_path = get_default_lib_path()
        lib_name = os.path.basename(automatic_lib_path)
        local_lib_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), lib_name)

        candidates = []

        if lib_path is not None:
            candidates.append(lib_path)
        candidates.append(automatic_lib_path)
        candidates.append(local_lib_path)

        self.lib_path = None

        for candidate in candidates:
            if os.path.exists(candidate):
                self.lib_path = candidate
                break

        if self.lib_path is None:
            raise FileNotFoundError(
                "Library not found. Tried: " + ", ".join(candidates)
            )

        self.lib = ctypes.CDLL(self.lib_path)
        self.memory_multiplier = 0.5
        self._bind_common_api()

    def _bind_common_api(self) -> None:
        self.lib.short_game_init.argtypes = [c_float]
        self.lib.short_game_init.restype = None

        self.lib.short_game_free.argtypes = []
        self.lib.short_game_free.restype = None

        self.lib.solve.argtypes = [c_void_p, c_void_p]
        self.lib.solve.restype = GamePtr

        self.lib.solve_component.argtypes = [c_void_p, c_void_p]
        self.lib.solve_component.restype = GamePtr

        self.lib.num_moves.argtypes = [c_void_p]
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

        self.lib.position_cache_get.argtypes = [
            c_void_p,
            c_void_p,
            POINTER(GamePtr),
        ]
        self.lib.position_cache_get.restype = c_int

        self.lib.game_zero.argtypes = []
        self.lib.game_zero.restype = GamePtr

        self.lib.game_star.argtypes = []
        self.lib.game_star.restype = GamePtr

        self.lib.game_up.argtypes = []
        self.lib.game_up.restype = GamePtr

        self.lib.game_down.argtypes = []
        self.lib.game_down.restype = GamePtr

        self.lib.game_geq.argtypes = [GamePtr, GamePtr]
        self.lib.game_geq.restype = c_int

        self.lib.game_from_games.argtypes = [GamePtrArrayPtr, GamePtrArrayPtr]
        self.lib.game_from_games.restype = GamePtr

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

    def initialize(self) -> None:
        self.lib.short_game_init(c_float(self.memory_multiplier))

    def free_all(self) -> None:
        self.lib.short_game_free()

    def game_zero(self) -> GamePtr:
        return self.lib.game_zero()

    def game_star(self) -> GamePtr:
        return self.lib.game_star()

    def game_up(self) -> GamePtr:
        return self.lib.game_up()

    def game_down(self) -> GamePtr:
        return self.lib.game_down()

    def game_geq(self, g1: GamePtr, g2: GamePtr) -> bool:
        return bool(self.lib.game_geq(g1, g2))

    def game_array_new(self) -> GamePtrArrayPtr:
        return GamePtrArrayPtr()

    def game_len(self, games: GamePtrArrayPtr) -> int:
        return int(self.lib.game_len(byref(games)))

    def game_cap(self, games: GamePtrArrayPtr) -> int:
        return int(self.lib.game_cap(byref(games)))

    def game_free_array(self, games: GamePtrArrayPtr) -> GamePtrArrayPtr:
        self.lib.game_free(byref(games))
        return games

    def game_reserve(self, games: GamePtrArrayPtr, expected_cap: int) -> GamePtrArrayPtr:
        self.lib.game_reserve(byref(games), c_size_t(expected_cap))
        return games

    def game_push(self, games: GamePtrArrayPtr, value: GamePtr) -> GamePtrArrayPtr:
        self.lib.game_push(byref(games), value)
        return games

    def game_append(self, games: GamePtrArrayPtr, value: GamePtr) -> GamePtrArrayPtr:
        self.lib.game_append(byref(games), value)
        return games

    def game_append_many(self, games: GamePtrArrayPtr, other: GamePtrArrayPtr) -> GamePtrArrayPtr:
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

    def game_remove_unordered(self, games: GamePtrArrayPtr, index: int) -> GamePtrArrayPtr:
        self.lib.game_remove_unordered(byref(games), c_size_t(index))
        return games

    def game_array_from_list(self, values: list[GamePtr]) -> GamePtrArrayPtr:
        games = self.game_array_new()
        if values:
            games = self.game_reserve(games, len(values))
            for value in values:
                games = self.game_push(games, value)
        return games

    def game_from_game_arrays(self, lefts: GamePtrArrayPtr, rights: GamePtrArrayPtr) -> GamePtr:
        return self.lib.game_from_games(lefts, rights)

    def game_from_games(self, lefts: list, rights: list) -> GamePtr:
        left_arr = self.game_array_from_list(lefts)
        right_arr = self.game_array_from_list(rights)
        try:
            return self.game_from_game_arrays(left_arr, right_arr)
        finally:
            self.game_free_array(left_arr)
            self.game_free_array(right_arr)

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
        return self.lib.make_int(n)

    def make_dyadic(self, p: int, q: int):
        result = self.lib.make_dyadic(p, q)
        return result if result else None

    def make_nimber(self, n: int) -> GamePtr:
        return self.lib.make_nimber(n)

    def make_up_multiple(self, n: int, with_star: int) -> GamePtr:
        return self.lib.make_up_multiple(n, with_star)

    def make_down_multiple(self, n: int, with_star: int) -> GamePtr:
        return self.lib.make_down_multiple(n, with_star)

    def get_game_value_string(self, game: GamePtr, fmt: int = FORMAT_FORMATED) -> str:
        if not game:
            return "NULL"
        raw = self.lib.game_get_string(game, fmt)
        return raw.decode("utf-8")


class RawGameSolver(ShortGameSolver):
    RawGameType = None
    PositionType = None

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

    def solve(self, raw_game, position) -> GamePtr:
        return self.lib.solve(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
        )

    def solve_component(self, raw_game, position) -> GamePtr:
        return self.lib.solve_component(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
        )

    def num_moves(self, raw_game) -> int:
        return int(self.lib.num_moves(self.raw_game_ptr(raw_game)))

    def can_left_move(self, raw_game, position, move: int) -> bool:
        return bool(
            self.lib.can_left_move(
                self.raw_game_ptr(raw_game),
                self.position_ptr(position),
                move,
            )
        )

    def can_right_move(self, raw_game, position, move: int) -> bool:
        return bool(
            self.lib.can_right_move(
                self.raw_game_ptr(raw_game),
                self.position_ptr(position),
                move,
            )
        )

    def do_move_left_ptr(self, raw_game, position, move: int) -> c_void_p:
        return self.lib.do_move_left(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
            move,
        )

    def do_move_right_ptr(self, raw_game, position, move: int) -> c_void_p:
        return self.lib.do_move_right(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
            move,
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
                move,
            )
        )

    def position_cache_lookup(self, raw_game, position):
        out = GamePtr()

        found = self.lib.position_cache_get(
            self.raw_game_ptr(raw_game),
            self.position_ptr(position),
            ctypes.byref(out),
        )

        return out if found else None
