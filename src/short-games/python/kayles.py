from __future__ import annotations

from dataclasses import dataclass

from game import Game, GameConvert


@dataclass(frozen=True)
class KaylesGame:
    length: int


@dataclass(frozen=True)
class KaylesPosition:
    # bit 1 znamena, ze kuzelka jeste stoji
    mask: int


class KaylesConvert(GameConvert):
    def __init__(self):
        super().__init__(use_c=False)

    def num_moves(self, raw_game: KaylesGame, position: KaylesPosition) -> int:
        # move 0..n-1       = srazit jednu kuzelku i
        # move n..2n-2      = srazit dvojici i, i+1
        n = raw_game.length
        return 2 * n - 1

    def _single_index(self, raw_game: KaylesGame, move: int) -> int:
        return move

    def _pair_index(self, raw_game: KaylesGame, move: int) -> int:
        return move - raw_game.length

    def _is_standing(self, position: KaylesPosition, index: int) -> bool:
        return (position.mask & (1 << index)) != 0

    def _can_move(self, raw_game: KaylesGame, position: KaylesPosition, move: int) -> bool:
        n = raw_game.length

        if 0 <= move < n:
            i = self._single_index(raw_game, move)
            return self._is_standing(position, i)

        if n <= move < 2 * n - 1:
            i = self._pair_index(raw_game, move)
            return (
                self._is_standing(position, i)
                and self._is_standing(position, i + 1)
            )

        return False

    def can_left_move(
        self,
        raw_game: KaylesGame,
        position: KaylesPosition,
        move: int,
    ) -> bool:
        return self._can_move(raw_game, position, move)

    def can_right_move(
        self,
        raw_game: KaylesGame,
        position: KaylesPosition,
        move: int,
    ) -> bool:
        return self._can_move(raw_game, position, move)

    def _do_move(
        self,
        raw_game: KaylesGame,
        position: KaylesPosition,
        move: int,
    ) -> KaylesPosition:
        if not self._can_move(raw_game, position, move):
            raise ValueError("Invalid move")

        n = raw_game.length
        mask = position.mask

        if 0 <= move < n:
            i = self._single_index(raw_game, move)
            mask &= ~(1 << i)
            return KaylesPosition(mask)

        i = self._pair_index(raw_game, move)
        mask &= ~(1 << i)
        mask &= ~(1 << (i + 1))
        return KaylesPosition(mask)

    def do_move_left(
        self,
        raw_game: KaylesGame,
        position: KaylesPosition,
        move: int,
    ) -> KaylesPosition:
        return self._do_move(raw_game, position, move)

    def do_move_right(
        self,
        raw_game: KaylesGame,
        position: KaylesPosition,
        move: int,
    ) -> KaylesPosition:
        return self._do_move(raw_game, position, move)

    def hash_raw_game_position(
        self,
        raw_game: KaylesGame,
        position: KaylesPosition,
        move: int,
    ) -> int:
        return hash((raw_game.length, position.mask, move))

    def position_cache_key(self, raw_game: KaylesGame, position: KaylesPosition):
        return (raw_game.length, position.mask)


def winner_text(game: Game) -> str:
    zero = Game.zero()

    game_geq_zero = game >= zero
    zero_geq_game = zero >= game

    if game_geq_zero and zero_geq_game:
        return "Vyhraje druhy hrac: G = 0"

    if game_geq_zero and not zero_geq_game:
        return "Vyhraje Left: G > 0"

    if not game_geq_zero and zero_geq_game:
        return "Vyhraje Right: G < 0"

    return "Vyhraje prvni hrac: G || 0"


def main() -> None:
    n = int(input("Pocet kuzelek: "))

    if n < 0:
        raise ValueError("Pocet kuzelek musi byt nezaporny")

    raw_game = KaylesGame(n)
    position = KaylesPosition((1 << n) - 1)

    solver = KaylesConvert()
    game = solver.convert(raw_game, position)

    print("Game:", game.formatted)
    print(winner_text(game))


if __name__ == "__main__":
    main()
