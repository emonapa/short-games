from game import Game, Ga

try:
    K = Game.from_string("{**|0}")
except ValueError as e:
    print(e)
    exit(1)

K += Game.one()
print(K)
