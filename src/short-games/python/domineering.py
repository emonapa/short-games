import sys

from ctypes import (
    Structure,
    c_uint8,
    c_uint64,
)

from game import Game, GameConvert


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
        raise ValueError("Board dimensions must be positive")

    if width > DOMINEERING_MAX_WIDTH or height > DOMINEERING_MAX_HEIGHT:
        raise ValueError(
            f"Maximum board size is {DOMINEERING_MAX_WIDTH}x{DOMINEERING_MAX_HEIGHT}"
        )

    if width * height > DOMINEERING_MAX_CELLS:
        raise ValueError("The board has too many cells for a uint64 mask")

    return CDomineeringBoard(width, height)


def make_position(occupied_mask: int = 0) -> CDomineeringPosition:
    if occupied_mask < 0 or occupied_mask >= (1 << 64):
        raise ValueError("occupied_mask must be a uint64 value")

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
        raise ValueError("Enter exactly two numbers: width height")

    return int(parts[0]), int(parts[1])


def parse_removed_cells(text: str, cell_count: int) -> list[int]:
    text = text.strip()

    if text == "":
        return []

    result = []

    for part in text.split():
        index = int(part)

        if index < 0 or index >= cell_count:
            raise ValueError(f"Cell index {index} is outside the range 0..{cell_count - 1}")

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


class DomineeringConverter(GameConvert):
    RawGameType = CDomineeringBoard
    PositionType = CDomineeringPosition

    def __init__(
        self,
        lib_path: str = DEFAULT_LIB_PATH,
        memory_multiplier: float = 0.01,
        use_c = True,
    ):
        super().__init__(
            lib_path=lib_path,
            memory_multiplier=memory_multiplier,
            use_c = True,
        )

        rt = self._rt()
        rt.RawGameType = CDomineeringBoard
        rt.PositionType = CDomineeringPosition


def print_available_moves(
    converter: DomineeringConverter,
    board: CDomineeringBoard,
    position: CDomineeringPosition,
) -> None:
    moves = converter.num_moves(board)

    left_moves = []
    right_moves = []

    for move in range(moves):
        if converter.can_left_move(board, position, move):
            left_moves.append(move)

        if converter.can_right_move(board, position, move):
            right_moves.append(move)

    print("Available moves:")
    print(f"  Left  - vertical dominoes:   {left_moves}")
    print(f"  Right - horizontal dominoes: {right_moves}")


def print_winner(game: Game) -> None:
    zero = Game.zero()

    game_geq_zero = game >= zero
    zero_geq_game = zero >= game

    print("\nWinner:")
    if game_geq_zero and zero_geq_game:
        print("Second player: G = 0")
    elif game_geq_zero and not zero_geq_game:
        print("Left (vertical dominoes): G > 0")
    elif not game_geq_zero and zero_geq_game:
        print("Right (horizontal dominoes): G < 0")
    else:
        print("First player: G || 0")


def main() -> None:
    lib_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_LIB_PATH

    while True:
        try:
            dims = input("Enter board dimensions as: width height\n> ")
            width, height = parse_dimensions(dims)
            board = make_board(width, height)
            break
        except ValueError as exc:
            print(f"Error: {exc}")
            print()

    print()
    print("Board with cell indices:")
    print_board(board.width, board.height, 0)

    cell_count = board.width * board.height

    while True:
        try:
            raw_removed = input(
                "\nEnter the indices of cells to remove, separated by spaces.\n"
                "Press Enter for an empty board.\n> "
            )
            removed_indices = parse_removed_cells(raw_removed, cell_count)
            removed_mask = make_removed_mask(removed_indices)
            break
        except ValueError as exc:
            print(f"Error: {exc}")

    position = make_position(removed_mask)

    print()
    print("Board after removing cells:")
    print_board(board.width, board.height, removed_mask)

    converter = DomineeringConverter(
        lib_path=lib_path,
        memory_multiplier=0.9,
        use_c = True,
    )

    try:
        print()
        print_available_moves(converter, board, position)

        print()
        print("Solving...")
        game = converter.convert(board, position)

        print()
        print("Result:")
        print(game.formatted)

        print_winner(game)

    finally:
        converter.free()


if __name__ == "__main__":
    main()
