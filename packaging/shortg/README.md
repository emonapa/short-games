# shortg

Python bindings for the short-game C runtime.

Build the package from the repository root:

```sh
make python-lib
```

Install the generated package:

```sh
python -m pip install ./shortg
```

Basic use:

```python
from shortg import Game

game = Game.from_string("{0 | 1}")
print(game.formatted)
```

The bundled native library is built for the current Linux system.
