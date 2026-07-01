import ctypes
from ctypes import (
    Structure,
    POINTER,
    c_int,
    c_uint8,
    c_uint64,
)
from game import GameConvert, Game

DEFAULT_HB_LIB_PATH = "../../../build/short-games/hotpotch/libhotpotch.so"
MAX_VERTICES = 128
MAX_EDGES = 128
EDGE_BLUE = 0
EDGE_RED = 1
EDGE_GREEN = 2

class CUInt128(Structure):
    _fields_ = [
        ("low", c_uint64),
        ("high", c_uint64),
    ]

def int_to_uint128(value: int) -> CUInt128:
    if value < 0:
        raise ValueError("uint128 value must be non-negative")
    if value >= (1 << 128):
        raise ValueError("uint128 value is too large")
    return CUInt128(
        value & 0xFFFFFFFFFFFFFFFF,
        (value >> 64) & 0xFFFFFFFFFFFFFFFF,
    )

def uint128_to_int(value: CUInt128) -> int:
    return (int(value.high) << 64) | int(value.low)

class CEdge(Structure):
    _fields_ = [
        ("u", c_uint8),
        ("v", c_uint8),
        ("color", c_int),
    ]

class CBaseGraph(Structure):
    _fields_ = [
        ("num_vertices", c_uint8),
        ("num_edges", c_uint8),
        ("edges", CEdge * MAX_EDGES),
    ]

class CPosition(Structure):
    _fields_ = [
        ("live_mask", CUInt128),
    ]

def make_position(live_mask: int) -> CPosition:
    return CPosition(int_to_uint128(live_mask))

def full_live_mask(num_edges: int) -> int:
    if not 0 <= num_edges <= 128:
        raise ValueError("edge mask supports only edge indexes 0..127")
    return (1 << num_edges) - 1 if num_edges else 0


class HBConverter(GameConvert):
    RawGameType = CBaseGraph
    PositionType = CPosition

    def __init__(
        self,
        lib_path: str = DEFAULT_HB_LIB_PATH,
        memory_multiplier: float = 0.01,
        use_c = True,
    ):
        super().__init__(
            lib_path=lib_path,
            memory_multiplier=memory_multiplier,
            use_c = True,
        )

        rt = self._rt()
        rt.RawGameType = CBaseGraph
        rt.PositionType = CPosition
        rt.lib.cleanup_position.argtypes = [POINTER(CBaseGraph), CUInt128]
        rt.lib.cleanup_position.restype = CUInt128

    def convert(self, graph: CBaseGraph, live_mask: int) -> Game:
        position = make_position(live_mask)
        return self._as_game(self._rt().convert(graph, position))

    def convert_component(self, graph: CBaseGraph, live_mask: int) -> Game:
        position = make_position(live_mask)
        return self._as_game(self._rt().convert_component(graph, position))

    def cleanup_position(self, graph: CBaseGraph, live_mask: int) -> int:
        rt = self._rt()
        result = rt.lib.cleanup_position(
            ctypes.byref(graph),
            int_to_uint128(live_mask),
        )
        return uint128_to_int(result)
