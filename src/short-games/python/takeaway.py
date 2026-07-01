from __future__ import annotations

from dataclasses import dataclass

from game import Game, GameConvert


@dataclass(frozen=True)
class TakeAwayGame:
    left_takes: tuple[int, ...] = (1, 2)
    right_takes: tuple[int, ...] = (1, 3)


@dataclass(frozen=True)
class TakeAwayPosition:
    stones: int


class TakeAwayConvert(GameConvert):
    def __init__(self):
        super().__init__(use_c=False)

    def num_moves(self, raw_game: TakeAwayGame, position: TakeAwayPosition) -> int:
        return 3

    def _take_for_move(self, move: int) -> int:
        if move < 0 or move >= 3:
            raise ValueError("Invalid move")

        return move + 1

    def can_left_move(
        self,
        raw_game: TakeAwayGame,
        position: TakeAwayPosition,
        move: int,
    ) -> bool:
        take = self._take_for_move(move)
        return take in raw_game.left_takes and position.stones >= take

    def can_right_move(
        self,
        raw_game: TakeAwayGame,
        position: TakeAwayPosition,
        move: int,
    ) -> bool:
        take = self._take_for_move(move)
        return take in raw_game.right_takes and position.stones >= take

    def do_move_left(
        self,
        raw_game: TakeAwayGame,
        position: TakeAwayPosition,
        move: int,
    ) -> TakeAwayPosition:
        if not self.can_left_move(raw_game, position, move):
            raise ValueError("Invalid left move")

        take = self._take_for_move(move)
        return TakeAwayPosition(position.stones - take)

    def do_move_right(
        self,
        raw_game: TakeAwayGame,
        position: TakeAwayPosition,
        move: int,
    ) -> TakeAwayPosition:
        if not self.can_right_move(raw_game, position, move):
            raise ValueError("Invalid right move")

        take = self._take_for_move(move)
        return TakeAwayPosition(position.stones - take)

    def hash_raw_game_position(
        self,
        raw_game: TakeAwayGame,
        position: TakeAwayPosition,
        move: int,
    ) -> int:
        return hash((position.stones, move))


def print_winner(game: Game) -> None:
    zero = Game.zero()

    game_geq_zero = game >= zero
    zero_geq_game = zero >= game

    if game_geq_zero and zero_geq_game:
        print("Vyhraje druhy hrac: G = 0")
    elif game_geq_zero and not zero_geq_game:
        print("Vyhraje Left: G > 0")
    elif not game_geq_zero and zero_geq_game:
        print("Vyhraje Right: G < 0")
    else:
        print("Vyhraje prvni hrac: G || 0")


def main() -> None:
    stones = int(input("Pocet kamenu: "))

    raw_game = TakeAwayGame()
    position = TakeAwayPosition(stones)

    solver = TakeAwayConvert()
    game = solver.convert(raw_game, position)

    print("Game:", game.formatted)
    print_winner(game)


if __name__ == "__main__":
    main()
