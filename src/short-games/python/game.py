from __future__ import annotations

from typing import Iterable, Iterator, Literal

FORMAT_RAW = 0
FORMAT_FORMATED = 1

def _options_list(value) -> list:
    if value is None:
        return []

    if isinstance(value, GameSide):
        return value.to_list()

    if isinstance(value, G):
        return value.to_list()

    if isinstance(value, Iterable) and not isinstance(value, (Game, str, bytes)):
        return list(value)

    return [value]

class GameSide:
    """
    One side of a Game: left or right.

    This wraps the C dynamic array Game**.
    """

    def __init__(self, game: "Game", side: Literal["left", "right"]):
        if side not in ("left", "right"):
            raise ValueError("side must be 'left' or 'right'")

        self.game = game
        self.side = side

    @property
    def _rt(self):
        return Game._rt()

    @property
    def _array(self):
        if self.side == "left":
            return self.game.ptr.contents.left

        return self.game.ptr.contents.right

    @_array.setter
    def _array(self, value) -> None:
        if self.side == "left":
            self.game.ptr.contents.left = value
        else:
            self.game.ptr.contents.right = value

    def to_list(self) -> list["Game"]:
        return list(self)

    def reserve(self, expected_cap: int) -> "GameSide":
        arr = self._array
        arr = self._rt.game_reserve(arr, expected_cap)
        self._array = arr
        return self

    def append(self, value) -> "GameSide":
        if isinstance(value, GameSide):
            arr = self._array
            arr = self._rt.game_append_many(arr, value._array)
            self._array = arr
            return self

        if isinstance(value, G):
            for child in value:
                self.append(child)
            return self

        if isinstance(value, Iterable) and not isinstance(value, (Game, str, bytes)):
            for child in value:
                self.append(child)
            return self

        arr = self._array
        arr = self._rt.game_append(arr, Game.ptr_of(value))
        self._array = arr
        return self

    def push(self, child) -> "GameSide":
        arr = self._array
        arr = self._rt.game_push(arr, Game.ptr_of(child))
        self._array = arr
        return self

    def pop(self) -> "Game":
        arr, value = self._rt.game_pop(self._array)
        self._array = arr
        return Game(value)

    def first(self) -> "Game":
        return Game(self._rt.game_first(self._array))

    def last(self) -> "Game":
        return Game(self._rt.game_last(self._array))

    def remove_unordered(self, index: int) -> "GameSide":
        arr = self._array
        arr = self._rt.game_remove_unordered(arr, index)
        self._array = arr
        return self

    def clear(self) -> "GameSide":
        arr = self._array
        arr = self._rt.game_resize(arr, 0)
        self._array = arr
        return self

    def __len__(self) -> int:
        return self._rt.game_len(self._array)

    def __bool__(self) -> bool:
        return len(self) != 0

    def __getitem__(self, index: int) -> "Game":
        n = len(self)

        if index < 0:
            index += n

        if index < 0 or index >= n:
            raise IndexError(index)

        return Game(self._array[index])

    def __iter__(self) -> Iterator["Game"]:
        for i in range(len(self)):
            yield self[i]

    def __or__(self, right) -> "Game":
        return Game.new(self.to_list(), _options_list(right))

    def __ror__(self, left) -> "Game":
        return Game.new(_options_list(left), self.to_list())



class G:
    def __init__(self, *items):
        self.items = list(items)

    def to_list(self) -> list:
        return self.items

    def __or__(self, right) -> "Game":
        return Game.new(self.to_list(), _options_list(right))

    def __ror__(self, left) -> "Game":
        return Game.new(_options_list(left), self.to_list())

    def __iter__(self):
        return iter(self.items)

    def __len__(self) -> int:
        return len(self.items)

    def __getitem__(self, index: int):
        return self.items[index]

class Game:
    """
    High-level Python wrapper around C Game*.

    Instance state:
        self.ptr

    The C runtime is hidden as a class-level internal runtime. The user does
    not have to call Game.bind(...).
    """

    _runtime = None

    def __init__(self, ptr):
        if not ptr:
            raise ValueError("Game got NULL pointer")

        self.ptr = ptr
        self._left = GameSide(self, "left")
        self._right = GameSide(self, "right")

    @classmethod
    def configure(
        cls,
        lib_path: str | None = None,
        memory_multiplier: float = 0.01,
    ) -> None:
        from game_runtime import GameRuntime

        if cls._runtime is not None:
            try:
                cls._runtime.free_all()
            except Exception:
                pass

        cls._runtime = GameRuntime(
            lib_path=lib_path,
            memory_multiplier=memory_multiplier,
        )

    @classmethod
    def use_runtime(cls, runtime) -> None:
        """
        Optional advanced method.

        Useful when you already created a GameRuntime or GameConvert and want
        high-level Game to use that same loaded library.
        """

        cls._runtime = runtime

    @classmethod
    def _rt(cls):
        if cls._runtime is None:
            cls.configure()

        return cls._runtime

    @staticmethod
    def ptr_of(value):
        if isinstance(value, Game):
            return value.ptr

        return value

    @classmethod
    def wrap(cls, ptr) -> "Game":
        return cls(ptr)

    @classmethod
    def new(cls, left=None, right=None) -> "Game":
        rt = cls._rt()

        if left is None and right is None:
            return cls(rt.game_new())

        left_items = [] if left is None else _options_list(left)
        right_items = [] if right is None else _options_list(right)

        if len(left_items) <= 1 and len(right_items) <= 1:
            left_ptr = cls.ptr_of(left_items[0]) if left_items else None
            right_ptr = cls.ptr_of(right_items[0]) if right_items else None
            return cls(rt.game_from_game(left_ptr, right_ptr))

        left_arr = rt.game_array_new()
        right_arr = rt.game_array_new()

        try:
            for child in left_items:
                left_arr = rt.game_push(left_arr, cls.ptr_of(child))

            for child in right_items:
                right_arr = rt.game_push(right_arr, cls.ptr_of(child))

            return cls(rt.game_from_game_arrays(left_arr, right_arr))
        finally:
            rt.game_free_array(left_arr)
            rt.game_free_array(right_arr)

    @classmethod
    def zero(cls) -> "Game":
        return cls(cls._rt().game_zero())

    @classmethod
    def one(cls) -> "Game":
        return cls(cls._rt().game_one())

    @classmethod
    def star(cls, n: int = 1) -> "Game":
        if n == 1:
            return cls(cls._rt().game_star())
        return cls(cls._rt().make_nimber(n))

    @classmethod
    def nimber(cls, n: int = 1) -> "Game":
        return cls(cls._rt().make_nimber(n))

    @classmethod
    def up(cls, n: int = 1) -> "Game":
        if n == 1:
            return cls(cls._rt().game_up())
        return cls(cls._rt().make_up_multiple(n, False))

    @classmethod
    def down(cls, n: int = 1) -> "Game":
        if n == 1:
            return cls(cls._rt().game_down())
        return cls(cls._rt().make_down_multiple(n, False))

    @classmethod
    def up_star(cls, n: int = 1) -> "Game":
        return cls(cls._rt().make_up_multiple(n, True))

    @classmethod
    def down_star(cls, n: int = 1) -> "Game":
        return cls(cls._rt().make_down_multiple(n, True))

    @classmethod
    def integer(cls, n: int) -> "Game":
        return cls(cls._rt().make_int(n))

    @classmethod
    def dyadic(cls, p: int, q: int = 1) -> "Game":
        ptr = cls._rt().make_dyadic(p, q)

        if not ptr:
            raise ValueError("Denominator must be a positive power of 2")

        return cls(ptr)

    @property
    def left(self) -> GameSide:
        return self._left

    @property
    def right(self) -> GameSide:
        return self._right

    @property
    def raw(self) -> str:
        return self.to_string(FORMAT_RAW)

    @property
    def formatted(self) -> str:
        return self.to_string(FORMAT_FORMATED)

    @property
    def canonical(self) -> "Game":
        return self.canonicalized()

    @property
    def negated(self) -> "Game":
        return -self

    @property
    def cooled_with_star(self) -> "Game":
        return Game(Game._rt().cool_with_star(self.ptr))

    @property
    def star_projection(self) -> "Game":
        return Game(Game._rt().star_projection(self.ptr))

    @property
    def is_infinitesimal(self) -> bool:
        return self.cooled_with_star.star_projection == Game.zero()

    @property
    def fuzzy(self) -> bool:
        return not self.geq(Game.zero()) and not Game.zero().geq(self)

    def L(self, child) -> "Game":
        self.left.append(child)
        return self

    def R(self, child) -> "Game":
        self.right.append(child)
        return self

    def canonicalize(self) -> "Game":
        self.ptr = Game._rt().game_canonicalize(self.ptr)
        return self

    def canonicalized(self) -> "Game":
        return Game(Game._rt().game_canonicalize(self.ptr))

    def negate(self) -> "Game":
        return Game(Game._rt().game_negate(self.ptr))

    def add(self, other) -> "Game":
        return Game(Game._rt().game_add(self.ptr, Game.ptr_of(other)))

    def sub(self, other) -> "Game":
        other_neg = Game._rt().game_negate(Game.ptr_of(other))
        return Game(Game._rt().game_add(self.ptr, other_neg))

    def geq(self, other) -> bool:
        return Game._rt().game_geq(self.ptr, Game.ptr_of(other))

    def leq(self, other) -> bool:
        return Game._rt().game_geq(Game.ptr_of(other), self.ptr)

    def eq(self, other) -> bool:
        return self.geq(other) and self.leq(other)

    def greater(self, other) -> bool:
        return self.geq(other) and not self.leq(other)

    def less(self, other) -> bool:
        return self.leq(other) and not self.geq(other)

    def confused(self, other) -> bool:
        return not self.geq(other) and not self.leq(other)

    def to_string(self, fmt: int = FORMAT_FORMATED) -> str:
        return Game._rt().get_game_value_string(self.ptr, fmt)

    def __add__(self, other) -> "Game":
        return self.add(other)

    def __sub__(self, other) -> "Game":
        return self.sub(other)

    def __neg__(self) -> "Game":
        return self.negate()

    def __ge__(self, other) -> bool:
        return self.geq(other)

    def __le__(self, other) -> bool:
        return self.leq(other)

    def __gt__(self, other) -> bool:
        return self.greater(other)

    def __lt__(self, other) -> bool:
        return self.less(other)

    def __eq__(self, other) -> bool:
        if not isinstance(other, Game):
            return False

        return self.eq(other)

    def __str__(self) -> str:
        return self.formatted

    def __repr__(self) -> str:
        return f"Game({self.formatted})"

    def __or__(self, right) -> "Game":
        return Game.new([self], _options_list(right))

    def __ror__(self, left) -> "Game":
        return Game.new(_options_list(left), [self])



class GameConvert:
    """
    High-level wrapper over GameConvertRuntime.

    It hides GamePtr from the user.

    It supports two modes:

        1. C backend:
            conv = GameConvert()
            g = conv.solve(raw_game, position)

        2. Pure Python backend:
            conv = GameConvert.from_python(solve=my_solve)
            g = conv.solve(raw_game, position)

    Python callbacks may return either:
        - Game
        - raw GamePtr
    """

    _default_runtime = None

    def __init__(
        self,
        runtime=None,
        *,
        lib_path: str | None = None,
        memory_multiplier: float = 0.01,
        use_c: bool = True,
        solve: Callable | None = None,
        solve_component: Callable | None = None,
        num_moves: Callable | None = None,
        can_left_move: Callable | None = None,
        can_right_move: Callable | None = None,
        do_move_left: Callable | None = None,
        do_move_right: Callable | None = None,
        hash_raw_game_position: Callable | None = None,
    ):
        self._solve_fn = solve
        self._solve_component_fn = solve_component
        self._num_moves_fn = num_moves
        self._can_left_move_fn = can_left_move
        self._can_right_move_fn = can_right_move
        self._do_move_left_fn = do_move_left
        self._do_move_right_fn = do_move_right
        self._hash_raw_game_position_fn = hash_raw_game_position

        self._use_c = use_c
        self._runtime = runtime

        if self._use_c and self._runtime is None:
            self._runtime = self._make_runtime(
                lib_path=lib_path,
                memory_multiplier=memory_multiplier,
            )

        if self._runtime is not None:
            Game.use_runtime(self._runtime)

    @classmethod
    def configure_runtime(
        cls,
        lib_path: str | None = None,
        memory_multiplier: float = 0.01,
    ) -> None:
        if cls._default_runtime is not None:
            try:
                cls._default_runtime.free()
            except Exception:
                pass

        cls._default_runtime = cls._make_runtime(
            lib_path=lib_path,
            memory_multiplier=memory_multiplier,
        )

        Game.use_runtime(cls._default_runtime)

    @classmethod
    def use_runtime(cls, runtime) -> None:
        cls._default_runtime = runtime
        Game.use_runtime(runtime)

    @classmethod
    def _make_runtime(
        cls,
        lib_path: str | None = None,
        memory_multiplier: float = 0.01,
    ):
        from game_runtime import GameConvertRuntime

        return GameConvertRuntime(
            lib_path=lib_path,
            memory_multiplier=memory_multiplier,
        )

    @classmethod
    def runtime(cls):
        if cls._default_runtime is None:
            cls.configure_runtime()

        return cls._default_runtime

    @classmethod
    def from_python(
        cls,
        *,
        solve: Callable | None = None,
        solve_component: Callable | None = None,
        num_moves: Callable | None = None,
        can_left_move: Callable | None = None,
        can_right_move: Callable | None = None,
        do_move_left: Callable | None = None,
        do_move_right: Callable | None = None,
        hash_raw_game_position: Callable | None = None,
    ) -> "GameConvert":
        return cls(
            use_c=False,
            solve=solve,
            solve_component=solve_component,
            num_moves=num_moves,
            can_left_move=can_left_move,
            can_right_move=can_right_move,
            do_move_left=do_move_left,
            do_move_right=do_move_right,
            hash_raw_game_position=hash_raw_game_position,
        )

    @staticmethod
    def _as_game(value) -> Game:
        if isinstance(value, Game):
            return value

        return Game.wrap(value)

    def _rt(self):
        if self._runtime is not None:
            return self._runtime

        if not self._use_c:
            raise RuntimeError("This GameConvert has no C runtime.")

        self._runtime = self.runtime()
        Game.use_runtime(self._runtime)
        return self._runtime

    def initialize(self) -> None:
        if self._runtime is not None:
            self._runtime.initialize()

    def free(self) -> None:
        if self._runtime is not None:
            self._runtime.free()

    def solve(self, raw_game, position) -> Game:
        if self._solve_fn is not None:
            return self._as_game(self._solve_fn(raw_game, position))

        if not self._use_c:
            raise NotImplementedError("solve is not implemented")

        return self._as_game(self._rt().solve(raw_game, position))

    def solve_component(self, raw_game, position) -> Game:
        if self._solve_component_fn is not None:
            return self._as_game(self._solve_component_fn(raw_game, position))

        if not self._use_c:
            raise NotImplementedError("solve_component is not implemented")

        return self._as_game(self._rt().solve_component(raw_game, position))

    def num_moves(self, raw_game) -> int:
        if self._num_moves_fn is not None:
            return int(self._num_moves_fn(raw_game))

        if not self._use_c:
            raise NotImplementedError("num_moves is not implemented")

        return int(self._rt().num_moves(raw_game))

    def can_left_move(self, raw_game, position, move: int) -> bool:
        if self._can_left_move_fn is not None:
            return bool(self._can_left_move_fn(raw_game, position, move))

        if not self._use_c:
            raise NotImplementedError("can_left_move is not implemented")

        return bool(self._rt().can_left_move(raw_game, position, move))

    def can_right_move(self, raw_game, position, move: int) -> bool:
        if self._can_right_move_fn is not None:
            return bool(self._can_right_move_fn(raw_game, position, move))

        if not self._use_c:
            raise NotImplementedError("can_right_move is not implemented")

        return bool(self._rt().can_right_move(raw_game, position, move))

    def do_move_left(self, raw_game, position, move: int):
        if self._do_move_left_fn is not None:
            return self._do_move_left_fn(raw_game, position, move)

        if not self._use_c:
            raise NotImplementedError("do_move_left is not implemented")

        return self._rt().do_move_left(raw_game, position, move)

    def do_move_right(self, raw_game, position, move: int):
        if self._do_move_right_fn is not None:
            return self._do_move_right_fn(raw_game, position, move)

        if not self._use_c:
            raise NotImplementedError("do_move_right is not implemented")

        return self._rt().do_move_right(raw_game, position, move)

    def hash_raw_game_position(self, raw_game, position, move: int) -> int:
        if self._hash_raw_game_position_fn is not None:
            return int(self._hash_raw_game_position_fn(raw_game, position, move))

        if not self._use_c:
            raise NotImplementedError("hash_raw_game_position is not implemented")

        return int(self._rt().hash_raw_game_position(raw_game, position, move))
