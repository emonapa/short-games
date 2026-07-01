from __future__ import annotations

from typing import Callable, Iterable, Iterator, Literal

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

    @classmethod
    def from_string(cls, text: str) -> "Game":
        return cls(cls._rt().game_from_string(text))

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


# Sada jmen metod, ktere lze prepisovat v podtride.
_OVERRIDABLE_METHODS = frozenset({
    "convert",
    "convert_component",
    "num_moves",
    "can_left_move",
    "can_right_move",
    "do_move_left",
    "do_move_right",
    "hash_raw_game_position",
})


class GameConvert:
    """
    High-level converter from raw game positions to Game.

    Rezimy:

    1. C backend:
        conv = GameConvert(use_c=True)
        g = conv.convert(raw_game, position)

    2. Python backend:
        class MyConvert(GameConvert):
            def __init__(self):
                super().__init__(use_c=False)

            def num_moves(...): ...
            def can_left_move(...): ...
            def can_right_move(...): ...
            def do_move_left(...): ...
            def do_move_right(...): ...
            def hash_raw_game_position(...): ...

        conv = MyConvert()
        g = conv.convert(raw_game, position)

    convert() a convert_component() se neprepisuji.
    Pokud use_c=False, pouzije se Python implementace techto dvou metod.
    Pokud use_c=True, pouzije se C implementace.
    """

    _default_runtime = None

    def __init__(
        self,
        runtime=None,
        *,
        lib_path: str | None = None,
        memory_multiplier: float = 0.01,
        use_c: bool = False,
    ):
        self._runtime = runtime
        self._use_c = use_c
        self._position_cache = {}

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

    @staticmethod
    def _as_game(value) -> Game:
        if isinstance(value, Game):
            return value

        return Game.wrap(value)

    def _rt(self):
        if self._runtime is not None:
            return self._runtime

        if not self._use_c:
            raise RuntimeError("Python GameConvert has no C runtime")

        self._runtime = self.runtime()
        Game.use_runtime(self._runtime)
        return self._runtime

    def initialize(self) -> None:
        self._position_cache.clear()

        if self._runtime is not None:
            self._runtime.initialize()

    def free(self) -> None:
        self._position_cache.clear()

        if self._runtime is not None:
            self._runtime.free()

    # ------------------------------------------------------------------
    # convert API
    # ------------------------------------------------------------------

    def convert(self, raw_game, position) -> Game:
        if self._use_c:
            return self._as_game(self._rt().convert(raw_game, position))

        total = Game.zero()

        for component_position in self.independent_components(raw_game, position):
            component_value = self.convert_component(raw_game, component_position)
            total = total + component_value

        return total.canonical

    def convert_component(self, raw_game, position) -> Game:
        if self._use_c:
            return self._as_game(self._rt().convert_component(raw_game, position))

        key = self.position_cache_key(raw_game, position)

        if key in self._position_cache:
            return self._position_cache[key]

        left_options = []
        right_options = []

        for move in range(self.num_moves(raw_game, position)):
            if self.can_left_move(raw_game, position, move):
                child_position = self.do_move_left(raw_game, position, move)
                child_game = self.convert_component(raw_game, child_position)
                left_options.append(child_game)

            if self.can_right_move(raw_game, position, move):
                child_position = self.do_move_right(raw_game, position, move)
                child_game = self.convert_component(raw_game, child_position)
                right_options.append(child_game)

        result = Game.new(left_options, right_options).canonical
        self._position_cache[key] = result

        return result

    # ------------------------------------------------------------------
    # Python cache helpers
    # ------------------------------------------------------------------

    def independent_components(self, raw_game, position):
        """
        Python analogie C get_independent_components.

        Defaultne hra nema zadny rozklad na komponenty.
        Pokud tvoje hra komponenty ma, prepis tuto metodu.
        """
        return [position]

    def position_cache_key(self, raw_game, position):
        """
        Python analogie C position_cache_get/insert.

        C verze hashuje pozici pres vsechny legalni tahy.
        Tady delame podobnou vec, plus pridame id(raw_game), aby se nemichaly
        pozice ruznych raw her ve stejnem solveru.
        """
        return (id(raw_game), self.hash_graph_state(raw_game, position))

    def hash_graph_state(self, raw_game, position) -> int:
        total_hash = 0

        for move in range(self.num_moves(raw_game, position)):
            if (
                self.can_left_move(raw_game, position, move)
                or self.can_right_move(raw_game, position, move)
            ):
                total_hash ^= self.hash_raw_game_position(raw_game, position, move)

        return total_hash

    # ------------------------------------------------------------------
    # Primitive raw-game API
    #
    # Pokud use_c=True, tyto metody volaji C runtime.
    # Pokud use_c=False, musi je prepsat podtrida.
    # ------------------------------------------------------------------

    def num_moves(self, raw_game, position = None) -> int:
        if self._use_c:
            return int(self._rt().num_moves(raw_game, position))

        raise NotImplementedError("num_moves must be implemented for Python backend")

    def can_left_move(self, raw_game, position, move: int) -> bool:
        if self._use_c:
            return bool(self._rt().can_left_move(raw_game, position, move))

        raise NotImplementedError("can_left_move must be implemented for Python backend")

    def can_right_move(self, raw_game, position, move: int) -> bool:
        if self._use_c:
            return bool(self._rt().can_right_move(raw_game, position, move))

        raise NotImplementedError("can_right_move must be implemented for Python backend")

    def do_move_left(self, raw_game, position, move: int):
        if self._use_c:
            return self._rt().do_move_left(raw_game, position, move)

        raise NotImplementedError("do_move_left must be implemented for Python backend")

    def do_move_right(self, raw_game, position, move: int):
        if self._use_c:
            return self._rt().do_move_right(raw_game, position, move)

        raise NotImplementedError("do_move_right must be implemented for Python backend")

    def hash_raw_game_position(self, raw_game, position, move: int) -> int:
        if self._use_c:
            return int(self._rt().hash_raw_game_position(raw_game, position, move))

        raise NotImplementedError(
            "hash_raw_game_position must be implemented for Python backend"
        )
