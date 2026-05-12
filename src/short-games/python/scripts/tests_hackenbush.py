import importlib.util
import math
import sys
import time
from pathlib import Path

from PySide6.QtWidgets import QApplication

# Uprav podle umístění souboru test_hackenbush.py
ROOT = Path(__file__).resolve().parents[1]
sys.path.append(str(ROOT))

import hb_io
import hb_solver
import hb_graphics
from hb_theme import THEME


PATH_TO_LIB = "../../../../build/short-games/hackenbush/libhackenbush.so"
JSON_FOLDER = "./tests_hackenbush/"

DYADICS_PATH = Path(__file__).resolve().parent / "../../../dyadics/python/hackenbush.py"


app = QApplication.instance()
if app is None:
    app = QApplication(sys.argv)


def import_dyadics_module():
    path = DYADICS_PATH.resolve()
    spec = importlib.util.spec_from_file_location("dyadics_hackenbush", path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot import dyadics hackenbush from {path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules["dyadics_hackenbush"] = module
    spec.loader.exec_module(module)
    return module


def is_expected_float(expected: str) -> bool:
    try:
        float(expected)
        return True
    except ValueError:
        return False


def format_seconds(seconds: float) -> str:
    return f"{seconds:.2f}s"


def solve_short_games_file(scene: hb_graphics.GraphScene, path: str) -> str:
    hb_io.load_from_file(scene, path)

    live_mask = scene.compute_live_mask_all_edges()
    game_ptr = scene.solver.solve(scene.g, live_mask)

    formatted = 1
    return scene.solver.get_game_value_string(game_ptr, formatted)


def solve_dyadics_file(dyadics_module, scene, path: str) -> str:
    hb_io.load_from_file(scene, path)

    live = scene.compute_live_mask()
    scene._dyadics_lib.solver_initialize(dyadics_module.ctypes.byref(scene.g))
    val = scene._dyadics_lib.solve(
        dyadics_module.ctypes.byref(scene.g),
        dyadics_module.c_uint64(live),
    )

    if val.exp >= 0:
        value = float(val.num) / float(1 << val.exp)
    else:
        value = float(val.num) * float(1 << (-val.exp))

    return f"{value:g}"


def run_short_games_test(scene: hb_graphics.GraphScene, path: str, expected: str) -> tuple[str, float]:
    start = time.perf_counter()
    try:
        scene.clear_graph()
        result = solve_short_games_file(scene, path)
        elapsed = time.perf_counter() - start

        if result != expected:
            return f"FAIL expected {expected!r}, got {result!r}", elapsed

        return "OK", elapsed
    except Exception as e:
        elapsed = time.perf_counter() - start
        return f"ERROR {e}", elapsed


def run_dyadics_test(dyadics_module, scene, path: str, expected: str) -> tuple[str, float | None]:
    if not is_expected_float(expected):
        return "SKIP non-float expected", None

    start = time.perf_counter()
    try:
        scene.clear_graph()
        result = solve_dyadics_file(dyadics_module, scene, path)
        elapsed = time.perf_counter() - start

        expected_value = float(expected)
        result_value = float(result)

        if not math.isclose(result_value, expected_value, rel_tol=0.0, abs_tol=1e-12):
            return f"FAIL expected {expected!r}, got {result!r}", elapsed

        return "OK", elapsed
    except Exception as e:
        elapsed = time.perf_counter() - start
        return f"ERROR {e}", elapsed


def fmt_status(label: str, status: str, elapsed: float | None, width: int) -> str:
    if elapsed is None:
        text = f"{label} - {status}"
    else:
        text = f"{label} - {status} {format_seconds(elapsed)}"
    return f"{text:<{width}}"


def main() -> None:
    short_solver = hb_solver.HBSolver(PATH_TO_LIB)
    short_solver.initialize()

    dyadics_module = import_dyadics_module()
    dyadics_lib = dyadics_module.load_library()

    short_scene = hb_graphics.GraphScene(short_solver, THEME.theme)
    dyadics_scene = dyadics_module.GraphScene()
    dyadics_scene._dyadics_lib = dyadics_lib

    tests = [
        (JSON_FOLDER + "down.hbg.json", "↓"),
        (JSON_FOLDER + "empty_game.hbg.json", "0"),
        (JSON_FOLDER + "half.hbg.json", "0.5"),
        (JSON_FOLDER + "minus_three_fourths.hbg.json", "-0.75"),
        (JSON_FOLDER + "one_fourth.hbg.json", "0.25"),
        (JSON_FOLDER + "star.hbg.json", "*"),
        (JSON_FOLDER + "star_minus_half.hbg.json", "-0.5 + *"),
        (JSON_FOLDER + "star_plus_sixteenth.hbg.json", "0.0625 + *"),
        (JSON_FOLDER + "up.hbg.json", "↑"),
        (JSON_FOLDER + "up_plus_star.hbg.json", "↑ + *"),
        (JSON_FOLDER + "hard_diamond.hbg.json", "-0.375"),
    ]

    name_width = max(len(Path(path).name) for path, _ in tests) + 2
    short_width = 38

    try:
        for path, expected in tests:
            name = Path(path).name

            short_status, short_time = run_short_games_test(short_scene, path, expected)
            dyadics_status, dyadics_time = run_dyadics_test(dyadics_module, dyadics_scene, path, expected)

            short_part = fmt_status("short games", short_status, short_time, short_width)
            dyadics_part = fmt_status("dyadics", dyadics_status, dyadics_time, 0)

            print(f"[{name:<{name_width}}] {short_part} {dyadics_part}")

    finally:
        short_solver.free_all()


if __name__ == "__main__":
    main()
