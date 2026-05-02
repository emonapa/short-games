"""
hb_calculator.py - CGT calculator panel.

Python is responsible only for:
  - parsing the user's string into tokens
  - recursively handling the {L | R} constructor
  - calling C helpers for all primitive game values

C (singletons.c) is responsible for:
  - make_int(int n)
  - make_dyadic(int p, int q)
  - make_nimber(int n)
  - make_up_multiple(int n, int with_star)
  - make_down_multiple(int n, int with_star)
  - game_star(), game_up(), game_down(), game_zero()
  - game_add(), game_canonicalize()
  - game_get_string(Game*, enum output_format)

Required additions to HBSolver in hb_solver.py - see bottom of this file.
"""

from __future__ import annotations
from typing import List

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QLineEdit, QComboBox, QFrame, QCheckBox,
)

from hb_theme import THEME


# -- Widget helpers ------------------------------------------------------------

def _divider() -> QFrame:
    f = QFrame()
    f.setFrameShape(QFrame.HLine)
    f.setFixedHeight(2)
    f.setStyleSheet("background-color: #4a4a4a;")
    return f


def _input(placeholder: str = "") -> QLineEdit:
    le = QLineEdit()
    le.setPlaceholderText(placeholder)
    le.setStyleSheet(
        "background-color: #333; color: white; border: 1px solid #555;"
        " border-radius: 4px; padding: 4px 8px; font-size: 15px;"
    )
    return le


def _result_box() -> QLineEdit:
    le = QLineEdit()
    le.setReadOnly(True)
    le.setStyleSheet(
        "background-color: #222; color: #aef; border: 1px solid #444;"
        " border-radius: 4px; padding: 4px 8px; font-size: 15px;"
    )
    return le


def _btn(text: str) -> QPushButton:
    b = QPushButton(text)
    b.setStyleSheet(
        "QPushButton { background-color: #444; color: white; border: 1px solid #666;"
        " border-radius: 4px; padding: 5px 14px; font-size: 14px; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:pressed { background-color: #333; }"
    )
    return b


def _combo(items: list) -> QComboBox:
    cb = QComboBox()
    cb.addItems(items)
    cb.setStyleSheet(
        "QComboBox { background: #333; color: white; border: 1px solid #555;"
        " border-radius: 4px; padding: 4px 8px; font-size: 15px; }"
        "QComboBox QAbstractItemView { background: #333; color: white; }"
    )
    return cb


def _raw_toggle() -> QCheckBox:
    cb = QCheckBox("raw")
    cb.setStyleSheet("color: #888; font-size: 12px;")
    return cb


# -- Parser --------------------------------------------------------------------
#
# Handles ONLY structural parsing.  All primitive game creation is delegated
# to the solver (C functions).

class GameParser:
    def __init__(self, solver):
        self.s = solver

    def parse(self, text: str):
        t = text.strip()
        if not t:
            raise ValueError("Empty input")
        return self._parse(t)

    def _parse(self, s: str):
        s = s.strip()

        # -- {L | R} - handled in Python, children delegated to C -------------
        if s.startswith("{") and s.endswith("}"):
            inner = s[1:-1]
            pipe = self._find_pipe(inner)
            if pipe == -1:
                raise ValueError(f"Missing '|' in: {s}")
            left_str  = inner[:pipe].strip()
            right_str = inner[pipe + 1:].strip()
            lefts  = self._parse_list(left_str)
            rights = self._parse_list(right_str)
            return self.s.game_make(lefts, rights)

        # -- * (star) ----------------------------------------------------------
        if s == "*":
            return self.s.game_star()

        # -- *n (nimber) -------------------------------------------------------
        if s.startswith("*") and len(s) > 1:
            try:
                n = int(s[1:])
            except ValueError:
                raise ValueError(f"Invalid nimber: '{s}'")
            return self.s.make_nimber(n)

        # -- arrows: ↑/^ ↓/v  with optional multiplier and * suffix ---------------
        s = s.replace("^", "↑").replace("v", "↓")
        for arrow, maker in [("↑", self.s.make_up_multiple),
                              ("↓", self.s.make_down_multiple)]:
            if arrow in s:
                idx       = s.index(arrow)
                pre       = s[:idx].strip()
                suffix    = s[idx + len(arrow):].strip()
                mult      = int(pre) if pre else 1
                with_star = 1 if suffix == "*" else 0
                if suffix and suffix != "*":
                    raise ValueError(f"Unexpected suffix after arrow: '{suffix}'")
                return maker(mult, with_star)

        # -- dyadic rational p/q -----------------------------------------------
        if "/" in s:
            parts = s.split("/")
            if len(parts) == 2:
                try:
                    p = int(parts[0].strip())
                    q = int(parts[1].strip())
                except ValueError:
                    raise ValueError(f"Can't parse a fraction: '{s}'")
                result = self.s.make_dyadic(p, q)
                if result is None:
                    raise ValueError(f"The denominator must be a power of 2: '{s}'")
                return result

        # -- integer -----------------------------------------------------------
        try:
            return self.s.make_int(int(s))
        except ValueError:
            pass

        raise ValueError(f"Nelze parsovat: '{s}'")

    # -- internals -------------------------------------------------------------

    def _parse_list(self, s: str) -> list:
        s = s.strip()
        if not s:
            return []
        return [self._parse(p) for p in self._split_top(s, ",")]

    def _find_pipe(self, s: str) -> int:
        depth = 0
        for i, c in enumerate(s):
            if c == "{":   depth += 1
            elif c == "}": depth -= 1
            elif c == "|" and depth == 0:
                return i
        return -1

    def _split_top(self, s: str, sep: str) -> List[str]:
        parts, depth, cur = [], 0, []
        for c in s:
            if c == "{":   depth += 1
            elif c == "}": depth -= 1
            if c == sep and depth == 0:
                parts.append("".join(cur).strip())
                cur = []
            else:
                cur.append(c)
        parts.append("".join(cur).strip())
        return parts


# -- CalculatorPanel -----------------------------------------------------------

class CalculatorPanel(QWidget):
    """Calculator panel shown at stack index 2."""

    def __init__(self, main_window, parent=None):
        super().__init__(parent)
        self.mw = main_window

        outer = QHBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)

        self.card = QWidget()
        self.card.setFixedWidth(620)

        outer.addStretch(1)
        outer.addWidget(self.card)
        outer.addStretch(1)

        layout = QVBoxLayout(self.card)
        layout.setContentsMargins(44, 40, 44, 40)
        layout.setSpacing(0)

        # -- Header ---------------------------------------------------------
        header_row = QHBoxLayout()
        title = QLabel("Calculator")
        title.setStyleSheet("color: white; font-size: 22px; font-weight: bold;")
        back_btn = QPushButton("← Back")
        back_btn.setFixedWidth(80)
        back_btn.clicked.connect(main_window.open_calculator)
        header_row.addWidget(title)
        header_row.addStretch()
        header_row.addWidget(back_btn)
        layout.addLayout(header_row)

        syntax = QLabel(
            "Syntax:  0  1  -2  1/2  -3/4  *  *3  ↑  ↓  2↑ 2^  3↓ 3v  ↑*  ↓*  {0,* | 1}"
        )
        syntax.setStyleSheet("color: #777; font-size: 12px;")
        layout.addWidget(syntax)
        layout.addSpacing(28)

        # -- Unary ----------------------------------------------------------
        sec1 = QLabel("UNARY OPERATION")
        sec1.setStyleSheet(
            "color: #999; font-size: 13px; font-weight: bold; letter-spacing: 1px;"
        )
        layout.addWidget(sec1)
        layout.addSpacing(14)

        # Game A -> Game B
        row1 = QHBoxLayout()
        self.unary_input = _input("Game G")
        self.unary_op    = _combo(["−G  (negace)", "Canonization", "Reduced canonical form"])
        calc1_btn        = _btn("=")
        row1.addWidget(self.unary_input, 3)
        row1.addWidget(self.unary_op, 2)
        row1.addWidget(calc1_btn)
        layout.addLayout(row1)
        layout.addSpacing(8)

        # unary result
        res1_row = QHBoxLayout()
        res1_lbl = QLabel("Result:")
        res1_lbl.setStyleSheet("color: #aaa; font-size: 14px;")
        self.unary_result = _result_box()
        self.unary_raw    = _raw_toggle()
        res1_row.addWidget(res1_lbl)
        res1_row.addWidget(self.unary_result, 1)
        res1_row.addWidget(self.unary_raw)
        layout.addLayout(res1_row)

        calc1_btn.clicked.connect(self._calc_unary)

        layout.addSpacing(28)
        layout.addWidget(_divider())
        layout.addSpacing(28)

        # -- Binary ---------------------------------------------------------
        sec2 = QLabel("BINARY OPERATION")
        sec2.setStyleSheet(
            "color: #999; font-size: 13px; font-weight: bold; letter-spacing: 1px;"
        )
        layout.addWidget(sec2)
        layout.addSpacing(14)

        # (Game, Game B) -> Game C
        row2 = QHBoxLayout()
        self.bin_left  = _input("Game A")
        self.bin_op    = _combo(["+", "-", "≥", "≤", ">", "<", "=", "||"])
        self.bin_right = _input("Game B")
        calc2_btn      = _btn("=")
        row2.addWidget(self.bin_left, 3)
        row2.addWidget(self.bin_op, 1)
        row2.addWidget(self.bin_right, 3)
        row2.addWidget(calc2_btn)
        layout.addLayout(row2)
        layout.addSpacing(8)

        # binary result
        res2_row = QHBoxLayout()
        res2_lbl = QLabel("Result:")
        res2_lbl.setStyleSheet("color: #aaa; font-size: 14px;")
        self.bin_result = _result_box()
        self.bin_raw    = _raw_toggle()
        res2_row.addWidget(res2_lbl)
        res2_row.addWidget(self.bin_result, 1)
        res2_row.addWidget(self.bin_raw)
        layout.addLayout(res2_row)

        # note
        layout.addSpacing(18)
        note_row = QHBoxLayout()
        layout.addWidget(_divider())
        layout.addSpacing(18)
        note_lbl = QLabel("Note: raw output formats only defined number, which is 0 = { | }.")
        note_lbl.setStyleSheet("color: #aaa; font-size: 14px;")
        note_row.addWidget(note_lbl)
        layout.addLayout(note_row)

        calc2_btn.clicked.connect(self._calc_binary)

        layout.addStretch()

        self._apply_theme(THEME.theme)
        THEME.changed.connect(self._apply_theme)

    # -- Theme -----------------------------------------------------------------

    def _apply_theme(self, theme) -> None:
        self.setStyleSheet(f"background-color: {theme.settings_bg_css()};")
        self.card.setStyleSheet(
            f"background-color: {theme.settings_card_bg_css()}; border-radius: 14px;"
        )

    # -- Internals -------------------------------------------------------------

    def _solver(self):
        s = self.mw.solver
        if s is None:
            raise RuntimeError("Solver is not loaded")
        return s

    def _fmt(self, raw_cb: QCheckBox) -> int:
        return 0 if raw_cb.isChecked() else 1

    def _str(self, ptr, raw_cb: QCheckBox) -> str:
        return self._solver().get_game_value_string(ptr, self._fmt(raw_cb))

    # -- Calculation -----------------------------------------------------------

    def _calc_unary(self):
        try:
            s = self._solver()
            g = GameParser(s).parse(self.unary_input.text())
            if self.unary_op.currentIndex() == 0: # negace
                result = s.game_negate(g)
            elif self.unary_op.currentIndex() == 1: # kanonizace
                result = s.game_canonicalize(g)
            elif self.unary_op.currentIndex() == 2: # reduced canonical form
                result_cooling = s.cool_with_star(g)
                result = s.star_projection(result_cooling)
            self.unary_result.setText(self._str(result, self.unary_raw))
        except Exception as e:
            self.unary_result.setText(f"Error: {e}")

    def _calc_binary(self):
        try:
            s  = self._solver()
            p  = GameParser(s)
            a  = p.parse(self.bin_left.text())
            b  = p.parse(self.bin_right.text())
            op = self.bin_op.currentText()

            if op == "+":
                a_canon = s.game_canonicalize(a)
                b_canon = s.game_canonicalize(b)
                result = s.game_canonicalize(s.game_add(a_canon, b_canon))
                self.bin_result.setText(self._str(result, self.bin_raw))

            elif op == "-":
                a_canon = s.game_canonicalize(a)
                b_canon = s.game_canonicalize(b)
                b_canon_negative = s.game_negate(b_canon)
                result = s.game_canonicalize(s.game_add(a_canon, b_canon_negative))
                self.bin_result.setText(self._str(result, self.bin_raw))

            else:
                geq_ab = bool(s.game_geq(a, b))
                geq_ba = bool(s.game_geq(b, a))
                ans = {
                    "≥":  geq_ab,
                    "≤":  geq_ba,
                    ">":  geq_ab and not geq_ba,
                    "<":  geq_ba and not geq_ab,
                    "=":  geq_ab and geq_ba,
                    "||": not geq_ab and not geq_ba,
                }[op]
                self.bin_result.setText(" True" if ans else " False")
        except Exception as e:
            self.bin_result.setText(f"Error: {e}")
