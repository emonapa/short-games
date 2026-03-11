import os
import ctypes
from ctypes import c_uint8, c_uint64, c_int, Structure, POINTER
from ctypes import c_char_p,  c_void_p


# Exportované konstanty pro Python
MAX_VERTICES = 128
MAX_EDGES = 128

EDGE_BLUE = 0
EDGE_RED = 1
EDGE_GREEN = 2

# C-Struktury
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

class CGame(Structure):
    pass

class c_uint128(Structure):
    # Na běžných procesorech (x86_64, ARM) je little endian,
    # takže spodních 64 bitů musí být jako první.
    _fields_ = [
        ("low", c_uint64),
        ("high", c_uint64)
    ]

def int_to_uint128(val: int) -> c_uint128:
    low = val & 0xFFFFFFFFFFFFFFFF
    high = (val >> 64) & 0xFFFFFFFFFFFFFFFF
    return c_uint128(low, high)

def uint128_to_int(val: c_uint128) -> int:
    return (val.high << 64) | val.low

CGame._fields_ = [
    ("L_count", c_int),
    ("R_count", c_int),
    ("left", POINTER(POINTER(CGame))),
    ("right", POINTER(POINTER(CGame))),
]

class HBSolver:
    def __init__(self, lib_path="../../../build/RBG/libhb.so"):
        if not os.path.exists(lib_path):
            alt_path = "./libhb.so"
            if os.path.exists(alt_path):
                lib_path = alt_path
            else:
                raise FileNotFoundError(f"Knihovna nenalezena: {lib_path}")

        self.lib = ctypes.CDLL(lib_path)

        # Mapování C funkcí
        #self.lib.solver_initialize.argtypes = [POINTER(CBaseGraph)]
        self.lib.solver_initialize.argtypes = []
        self.lib.solver_initialize.restype = None

        self.lib.solver_solve_with_components.argtypes = [POINTER(CBaseGraph), c_uint128]
        self.lib.solver_solve_with_components.restype = POINTER(CGame)

        self.lib.game_zero.argtypes = []
        self.lib.game_zero.restype = POINTER(CGame)

        self.lib.game_geq.argtypes = [POINTER(CGame), POINTER(CGame)]
        self.lib.game_geq.restype = c_int

        self.lib.cleanup_position.argtypes = [POINTER(CBaseGraph), c_uint128]
        self.lib.cleanup_position.restype = c_uint128

        self.lib.game_get_string.argtypes = [c_void_p]
        self.lib.game_get_string.restype = c_char_p

        self.lib.solver_free.argtypes = []
        self.lib.solver_free.restype = None

    # wrappery
    def initialize(self, graph):
        if graph is None:
            self.lib.solver_initialize(None)
        else:
            self.lib.solver_initialize(ctypes.byref(graph))

    def solve_with_components(self, graph: CBaseGraph, live_mask: int):
        mask_128 = int_to_uint128(live_mask)
        return self.lib.solver_solve_with_components(ctypes.byref(graph), mask_128)

    def cleanup_position(self, graph: CBaseGraph, live_mask: int) -> int:
        mask_128 = int_to_uint128(live_mask)
        res_128 = self.lib.cleanup_position(ctypes.byref(graph), mask_128)
        # Vrácenou C strukturu c_uint128 převedeme zpět na Python int
        return uint128_to_int(res_128)

    def game_zero(self):
        return self.lib.game_zero()

    def game_geq(self, g1, g2) -> bool:
        return bool(self.lib.game_geq(g1, g2))

    def get_game_value_string(self, game_ptr) -> str:
        if not game_ptr:
            return "NULL"
        # c_char_p se v Pythonu převede na bytes, musíme ho dekódovat
        c_string_bytes = self.lib.game_get_string(game_ptr)
        return c_string_bytes.decode('utf-8')

    def free_all(self):
        self.lib.solver_free()
