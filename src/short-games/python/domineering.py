import sys
from ctypes import (
    Structure,
    c_uint8,
    c_uint64,
)

from short_games import RawGameSolver

DEFAULT_LIB_PATH = "../../../build/short-games/domineering/libdomineering.so"

DOMINEERING_MAX_WIDTH = 8
DOMINEERING_MAX_HEIGHT = 8
DOMINEERING_MAX_CELLS = 64

class CDomineeringBoard(Structure):
    _fields_ = [
        ("width", c_uint8),
        ("height", c_uint8),
    ]


class CDomineeringPosition(Structure):
    _fields_ = [
        ("occupied_mask", c_uint64),
    ]


def make_board(width: int, height: int) -> CDomineeringBoard:
    if width <= 0 or height <= 0:
        raise ValueError("Rozmery hraciho pole musi byt kladne")

    if width > DOMINEERING_MAX_WIDTH or height > DOMINEERING_MAX_HEIGHT:
        raise ValueError(
            f"Maximalni velikost je {DOMINEERING_MAX_WIDTH}x{DOMINEERING_MAX_HEIGHT}"
        )

    if width * height > DOMINEERING_MAX_CELLS:
        raise ValueError("Hraci pole ma moc bunek pro uint64 masku")

    return CDomineeringBoard(width, height)


def make_position(occupied_mask: int = 0) -> CDomineeringPosition:
    if occupied_mask < 0 or occupied_mask >= (1 << 64):
        raise ValueError("occupied_mask musi byt uint64")

    return CDomineeringPosition(occupied_mask)


def cell_index(row: int, col: int, width: int) -> int:
    return row * width + col


def cell_bit(index: int) -> int:
    return 1 << index


def is_removed(mask: int, index: int) -> bool:
    return (mask & cell_bit(index)) != 0


def parse_dimensions(text: str) -> tuple[int, int]:
    parts = text.strip().split()

    if len(parts) != 2:
        raise ValueError("Zadej presne dve cisla: sirka vyska")

    return int(parts[0]), int(parts[1])


def parse_removed_cells(text: str, cell_count: int) -> list[int]:
    text = text.strip()

    if text == "":
        return []

    result = []

    for part in text.split():
        index = int(part)

        if index < 0 or index >= cell_count:
            raise ValueError(f"Index bunky {index} je mimo rozsah 0..{cell_count - 1}")

        result.append(index)

    return result


def make_removed_mask(indices: list[int]) -> int:
    mask = 0

    for index in indices:
        mask |= cell_bit(index)

    return mask


def print_board(width: int, height: int, removed_mask: int = 0) -> None:
    cell_count = width * height
    index_width = max(len(str(cell_count - 1)), 1)

    cell_inner_width = index_width + 2

    canvas_height = height * 2 + 1
    canvas_width = width * (cell_inner_width + 1) + 1

    canvas = [[" " for _ in range(canvas_width)] for _ in range(canvas_height)]

    def put(row: int, col: int, ch: str) -> None:
        if 0 <= row < canvas_height and 0 <= col < canvas_width:
            canvas[row][col] = ch

    def cell_removed(row: int, col: int) -> bool:
        idx = cell_index(row, col, width)
        return is_removed(removed_mask, idx)

    for row in range(height):
        for col in range(width):
            idx = cell_index(row, col, width)

            if cell_removed(row, col):
                continue

            top = row * 2
            mid = top + 1
            bottom = top + 2

            left = col * (cell_inner_width + 1)
            right = left + cell_inner_width + 1

            for x in range(left + 1, right):
                put(top, x, "-")
                put(bottom, x, "-")

            put(mid, left, "|")
            put(mid, right, "|")

            put(top, left, "+")
            put(top, right, "+")
            put(bottom, left, "+")
            put(bottom, right, "+")

            label = str(idx).center(cell_inner_width)
            for i, ch in enumerate(label):
                put(mid, left + 1 + i, ch)

    for line in canvas:
        print("".join(line).rstrip())


class DomineeringSolver(RawGameSolver):
    RawGameType = CDomineeringBoard
    PositionType = CDomineeringPosition

    def __init__(self, lib_path: str = DEFAULT_LIB_PATH):
        super().__init__(lib_path)


def print_available_moves(
    solver: DomineeringSolver,
    board: CDomineeringBoard,
    position: CDomineeringPosition,
) -> None:
    moves = solver.num_moves(board)

    left_moves = []
    right_moves = []

    for move in range(moves):
        if solver.can_left_move(board, position, move):
            left_moves.append(move)

        if solver.can_right_move(board, position, move):
            right_moves.append(move)

    print("Dostupne tahy:")
    print(f"  Left  - vertikalni domino:   {left_moves}")
    print(f"  Right - horizontalni domino: {right_moves}")


def main() -> None:
    lib_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_LIB_PATH

    while True:
        try:
            dims = input("Zadej rozmery hraciho pole jako: sirka vyska\n> ")
            width, height = parse_dimensions(dims)
            board = make_board(width, height)
            break
        except ValueError as exc:
            print(f"Chyba: {exc}")
            print()

    print()
    print("Hraci pole s indexy bunek:")
    print_board(board.width, board.height, 0)

    cell_count = board.width * board.height

    while True:
        try:
            raw_removed = input(
                "\nZadej indexy bunek, ktere chces odstranit, oddelene mezerami.\n"
                "Pro prazdne pole jen zmackni Enter.\n> "
            )
            removed_indices = parse_removed_cells(raw_removed, cell_count)
            removed_mask = make_removed_mask(removed_indices)
            break
        except ValueError as exc:
            print(f"Chyba: {exc}")

    position = make_position(removed_mask)

    print()
    print("Hraci pole po odstraneni bunek:")
    print_board(board.width, board.height, removed_mask)

    solver = DomineeringSolver(lib_path)
    solver.memory_multiplier = 0.9
    solver.initialize()

    try:
        print()
        print_available_moves(solver, board, position)

        print()
        print("Solvuju...")
        game = solver.solve(board, position)

        print()
        print("Vysledek:")
        print(solver.get_game_value_string(game, 1))

        game_geq_zero = solver.game_geq(game, solver.game_zero())
        zero_geq_game = solver.game_geq(solver.game_zero(), game)
        print("\nVyhraje: ")
        if game_geq_zero and zero_geq_game:
            print("druhý hráč G = 0")
        if game_geq_zero and not zero_geq_game:
            print("levý hráč (vertikalní domino) G > 0")
        if not game_geq_zero and zero_geq_game:
            print("pravý hráč (horizontalní domino) G < 0")
        if not game_geq_zero and not zero_geq_game:
            print("první hráč G || 0")

    finally:
        solver.free_all()


if __name__ == "__main__":
    main()
