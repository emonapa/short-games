import sys

from ctypes import (
    Structure,
    c_uint8,
    c_uint64,
)

from game import Game, GameConvert


DEFAULT_LIB_PATH = "../../../build/short-games/toads_and_frogs/libtoads_and_frogs.so"

TOADS_AND_FROGS_MAX_CELLS = 64


class CToadsAndFrogsBoard(Structure):
    _fields_ = [
        ("length", c_uint8),
    ]


class CToadsAndFrogsPosition(Structure):
    _fields_ = [
        ("toads_mask", c_uint64),
        ("frogs_mask", c_uint64),
    ]


def cell_bit(index: int) -> int:
    return 1 << index


def make_board(length: int) -> CToadsAndFrogsBoard:
    if length <= 0:
        raise ValueError("Board length must be positive")

    if length > TOADS_AND_FROGS_MAX_CELLS:
        raise ValueError(f"Maximum board length is {TOADS_AND_FROGS_MAX_CELLS}")

    return CToadsAndFrogsBoard(length)


def make_position(toads_mask: int, frogs_mask: int) -> CToadsAndFrogsPosition:
    if toads_mask < 0 or frogs_mask < 0:
        raise ValueError("Masks must not be negative")

    if toads_mask >= (1 << 64) or frogs_mask >= (1 << 64):
        raise ValueError("Masks must be uint64 values")

    if (toads_mask & frogs_mask) != 0:
        raise ValueError("Toads and Frogs cannot occupy the same cell")

    return CToadsAndFrogsPosition(toads_mask, frogs_mask)


def parse_indices(text: str, length: int) -> list[int]:
    text = text.strip()

    if text == "":
        return []

    indices = []

    for part in text.split():
        index = int(part)

        if index < 0 or index >= length:
            raise ValueError(f"Index {index} is outside the range 0..{length - 1}")

        if index in indices:
            raise ValueError(f"Index {index} is specified more than once")

        indices.append(index)

    return indices


def make_mask(indices: list[int]) -> int:
    mask = 0

    for index in indices:
        mask |= cell_bit(index)

    return mask


def print_board(length: int, toads_mask: int = 0, frogs_mask: int = 0) -> None:
    index_width = max(len(str(length - 1)), 1)

    index_line = []
    cell_line = []

    for index in range(length):
        index_line.append(str(index).center(index_width))

        bit = cell_bit(index)
        if toads_mask & bit:
            cell_line.append("T".center(index_width))
        elif frogs_mask & bit:
            cell_line.append("F".center(index_width))
        else:
            cell_line.append(".".center(index_width))

    print("Index: " + " ".join(index_line))
    print("Board: " + " ".join(cell_line))


class ToadsAndFrogsConverter(GameConvert):
    RawGameType = CToadsAndFrogsBoard
    PositionType = CToadsAndFrogsPosition

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
        rt.RawGameType = CToadsAndFrogsBoard
        rt.PositionType = CToadsAndFrogsPosition


def print_available_moves(
    converter: ToadsAndFrogsConverter,
    board: CToadsAndFrogsBoard,
    position: CToadsAndFrogsPosition,
) -> None:
    moves = converter.num_moves(board)

    toad_moves = []
    frog_moves = []

    for move in range(moves):
        if converter.can_left_move(board, position, move):
            toad_moves.append(move)

        if converter.can_right_move(board, position, move):
            frog_moves.append(move)

    print("Available moves:")
    print(f"  Left  - Toads move right: {toad_moves}")
    print(f"  Right - Frogs move left:  {frog_moves}")


def print_winner(game: Game) -> None:
    zero = Game.zero()

    game_geq_zero = game >= zero
    zero_geq_game = zero >= game

    print("\nWinner:")
    if game_geq_zero and zero_geq_game:
        print("Second player: G = 0")
    elif game_geq_zero and not zero_geq_game:
        print("Left - Toads: G > 0")
    elif not game_geq_zero and zero_geq_game:
        print("Right - Frogs: G < 0")
    else:
        print("First player: G || 0")


def main() -> None:
    lib_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_LIB_PATH

    while True:
        try:
            raw_length = input("Enter the one-dimensional board length\n> ")
            board = make_board(int(raw_length))
            break
        except ValueError as exc:
            print(f"Error: {exc}")
            print()

    print()
    print("Board with indices:")
    print_board(board.length)

    while True:
        try:
            raw_toads = input(
                "\nEnter the indices for Toads, separated by spaces.\n"
                "Press Enter for no Toads.\n> "
            )
            toad_indices = parse_indices(raw_toads, board.length)
            toads_mask = make_mask(toad_indices)
            break
        except ValueError as exc:
            print(f"Error: {exc}")

    while True:
        try:
            raw_frogs = input(
                "\nEnter the indices for Frogs, separated by spaces.\n"
                "Press Enter for no Frogs.\n> "
            )
            frog_indices = parse_indices(raw_frogs, board.length)
            frogs_mask = make_mask(frog_indices)

            if toads_mask & frogs_mask:
                raise ValueError("Toads and Frogs cannot occupy the same cell")

            break
        except ValueError as exc:
            print(f"Error: {exc}")

    position = make_position(toads_mask, frogs_mask)

    print()
    print("Entered position:")
    print_board(board.length, position.toads_mask, position.frogs_mask)

    converter = ToadsAndFrogsConverter(
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
