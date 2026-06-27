from game import Game, G


g = Game.up_star(2) | G(Game.zero(), Game.star())
g.left.append(Game.star())
g.canonicalize()

K = Game.new(Game.zero(), g)
g += K
if g >= Game.zero():
    print(f"1: {g - Game.star()}")
else:
    print(f"2: {(g + Game.up()).negated}")
print(g)
