# LL(1) parser and evaluator for short games

The implementation is separated into three phases:

1. `lexer.c` - lexical analysis
2. `parser.c` - recursive-descent LL(1) parser producing a simple AST
3. `semantic.c` - semantic validation and evaluation to `Game *`

`game_string.c` is the public facade that runs all three phases. Text output is
kept beside the parser in `game_format.c`, rather than in the mathematical core.

## Files

- `language_error.h/.c` - non-fatal error value with source position
- `token.h/.c` - token definitions
- `lexer.h/.c` - UTF-8 aware lexer
- `ast.h/.c` - generic AST containing only `text` and dynamic `children`
- `parser.h/.c` - LL(1) recursive-descent parser
- `semantic.h/.c` - semantic checks and evaluation
- `dyadic_value.h/.c` - exact numeric-game extraction and dyadic division
- `game_format.h/.c` - raw and symbolic `Game *` formatting
- `game_string.h/.c` - `game_from_string()` public API
- `example_ast.c` - parser-only example
- `example_evaluation.c` - example using the full game project
- `test_parser.c` - lexer, parser, evaluator and formatter regression tests

## AST representation

```c
typedef struct AstNode {
    char *text;
    struct AstNode **children;
} AstNode;
```

The children array uses the project's `darray.h` stretchy buffer.

Normalized nodes:

- leaf: the original literal, for example `"3"`, `"*2"`, `"↑*"`
- unary operator: `"+"` or `"-"` with one child
- binary operator: `"+"`, `"-"`, `"*"`, `"/"` with two children
- function: function name with its arguments as children
- game constructor: `"{}"` with `"left"` and `"right"` list children
- parentheses do not produce an AST node

For example:

```text
{0, *2 | ↑*} + 2 * (3 - 1)
```

produces:

```text
+
  {}
    left
      0
      *2
    right
      ↑*
  *
    2
    -
      3
      1
```

## Lexical forms

The supplied grammar is supported directly:

- integers: `0`, `1`, `42`
- star: `*`
- nimbers: `*0`, `*1`, `*2`, ...
- arrows: `^`, `v`, `↑`, `↓`
- arrow plus star: `^*`, `v*`, `↑*`, `↓*`
- number plus star: `2*`, interpreted as `2 + *`
- functions: ASCII identifiers followed by `(`

The existing project notation is also supported:

- arrow multiples: `2^`, `2v`, `2↑`, `2↓`
- arrow multiples plus star: `2^*`, `2v*`, `2↑*`, `2↓*`

`*` is context-sensitive at the lexical boundary:

- where a primary is expected, `*2` is a nimber and `*` is the star game
- after a completed primary, `*` is multiplication

Therefore `2*3` is tokenized as `2`, multiplication, `3`.

## Built-in functions

All currently implemented functions take exactly one argument:

- `canonical(E)` or `canonicalize(E)`
- `cool(E)`
- `projection(E)` or `star_projection(E)`

The grammar permits any number of arguments syntactically. Semantic analysis
rejects an unsupported function or incorrect arity.

## Operator semantics

- `G + H`: `game_add(G, H)`
- `G - H`: `game_add(G, game_negate(H))`
- `-G`: `game_negate(G)`
- `G * H`: core `game_multiply()` Conway product, memoized for one call
- `G / H`: only dyadic surreal numbers are accepted, and the reduced result
  must again have a power-of-two denominator

General division of arbitrary partizan games is intentionally rejected.

Exact division uses the normalized `n / 2^k` representation and the
"simplest number between options" construction already used by the dyadics
solver. The parser keeps only the error-reporting adapter; numeric-game
recognition is isolated in `dyadic_value.c`.

## Integration

Put the new files in the parser directory. Compile with include paths pointing
to the directories containing:

- `darray.h`
- `short_game.h`
- `singletons.h`
- `game_darray.h`

Example parser-only compilation:

```sh
gcc -std=c99 -Wall -Wextra -O2 \
    -Iparser -Ishared \
    parser/language_error.c \
    parser/token.c \
    parser/lexer.c \
    parser/ast.c \
    parser/parser.c \
    parser/example_ast.c \
    -o example_ast
```

For the full evaluator, add:

```text
parser/semantic.c
parser/dyadic_value.c
parser/game_format.c
parser/game_string.c
```

to the target that already links the game implementation.

## Public use

```c
Game *game = game_from_string("{0 | 1} / 2");
if (game == NULL) {
    fprintf(stderr, "%s\n", game_string_last_error());
}
```

The resulting value is `1/4`.

## Tested examples

Using the supplied game implementation, these expressions evaluated as:

```text
{|}             -> 0
1 + 1           -> 2
{0 | 1}         -> 0.5
2 * {0 | 1}     -> 1
{0 | 1} / 2     -> 0.25
*2 + *2         -> 0
2↑ + ↓          -> ↑
```
