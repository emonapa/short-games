from game import Game


def show(label: str, g: Game) -> None:
    c = g.canonical
    print(f"{label}")
    print(f"  canonical fmt:  {c.formatted}")
    print(f"  canonical raw:  {c.raw}")



def up_table(max_n: int = 5, max_star: int = 4) -> None:
    print("=== UP TABLE ===")

    for n in range(1, max_n + 1):
        base = Game.up(n).canonical

        print()
        show(f"{n}↑", base)

        for k in range(1, max_star + 1):
            g = (Game.up(n) + Game.star(k)).canonical
            label = f"{n}↑ + *{k}" if k != 1 else f"{n}↑ + *"

            show(label, g)


def down_table(max_n: int = 5, max_star: int = 4) -> None:
    print()
    print("=== DOWN TABLE ===")

    for n in range(1, max_n + 1):
        base = Game.down(n).canonical

        print()
        show(f"{n}↓", base)

        for k in range(1, max_star + 1):
            g = (Game.down(n) + Game.star(k)).canonical
            label = f"{n}↓ + *{k}" if k != 1 else f"{n}↓ + *"

            show(label, g)


def cross_check() -> None:
    print()
    print("=== SPECIFIC CHECKS ===")

    a = Game.up(2).canonical
    b = (Game.up(2) + Game.star()).canonical

    show("2↑", a)
    show("2↑ + *", b)

    a = Game.down(2).canonical
    b = (Game.down(2) + Game.star()).canonical

    show("2↓", a)
    show("2↓ + *", b)


if __name__ == "__main__":
    up_table(max_n=5, max_star=4)
    down_table(max_n=5, max_star=4)
    cross_check()
