# hb_theme.py
from __future__ import annotations

from dataclasses import dataclass, replace
from typing import Tuple, Any, Optional

from PySide6.QtCore import QObject, Signal
from PySide6.QtGui import QColor


RGB = Tuple[int, int, int]


def rgb_css(c: RGB) -> str:
    return "rgb({}, {}, {})".format(*c)


def qcolor(c: RGB) -> QColor:
    return QColor(c[0], c[1], c[2])


@dataclass(frozen=True)
class HBTheme:
    # -- Window ----------------------------------------------------------------
    win_size_x: int = 1200
    win_size_y: int = 900

    # -- Core palette ----------------------------------------------------------
    window_bg: RGB = (37, 37, 37)

    blue:  RGB = (3, 105, 143)
    red:   RGB = (134, 0, 55)
    green: RGB = (0, 112, 60)

    game_bg:     RGB = (47, 47, 47)
    ground:      RGB = (77, 43, 140)
    ground_line: RGB = (41, 16, 45)

    vertex:         RGB = (200, 200, 200)
    vertex_ground:  RGB = (106, 50, 159)
    vertex_outline: RGB = (0, 0, 0)

    # -- Settings panel colours ------------------------------------------------
    settings_bg:      RGB = (47, 47, 47)
    settings_card_bg: RGB = (60, 60, 60)

    # -- Sizes -----------------------------------------------------------------
    edge_width:      int = 8
    vertex_outline_w: int = 2
    ground_line_w:   int = 2

    # -- Global QSS tokens -----------------------------------------------------
    text: str = "white"

    btn_bg:       str = "#444"
    btn_bg_hover: str = "#555"
    btn_text:     str = "white"

    border_color: str = "#555"
    border_w:     int = 1
    btn_padding:  int = 6
    btn_radius:   int = 5

    hint_on_bg:    str = "#D4AF37"
    hint_on_text:  str = "black"
    hint_off_bg:   str = "#555"
    hint_off_text: str = "white"
    hint_text_color: str = "yellow"

    edu_on_bg:  str = "#555"
    edit_on_bg: str = "#555"

    icon_btn_font_size: int = 25
    icon_settings_font_size = 32

    # -- Stylesheet ------------------------------------------------------------

    def stylesheet(self) -> str:
        return """
            QMainWindow {{ background-color: {window_bg}; }}
            QLabel {{ color: {text}; }}
            QPushButton {{
                background-color: {btn_bg};
                color: {btn_text};
                border: {border_w}px solid {border_color};
                padding: {btn_padding}px;
                border-radius: {btn_radius}px;
            }}
            QPushButton:hover {{
                background-color: {btn_bg_hover};
            }}
        """.format_map({
            "window_bg":    rgb_css(self.window_bg),
            "text":         self.text,
            "btn_bg":       self.btn_bg,
            "btn_text":     self.btn_text,
            "border_color": self.border_color,
            "border_w":     self.border_w,
            "btn_padding":  self.btn_padding,
            "btn_radius":   self.btn_radius,
            "btn_bg_hover": self.btn_bg_hover,
        })

    # -- QColor helpers --------------------------------------------------------

    def blue_q(self)            -> QColor: return qcolor(self.blue)
    def red_q(self)             -> QColor: return qcolor(self.red)
    def green_q(self)           -> QColor: return qcolor(self.green)

    def game_bg_q(self)         -> QColor: return qcolor(self.game_bg)
    def ground_q(self)          -> QColor: return qcolor(self.ground)
    def ground_line_q(self)     -> QColor: return qcolor(self.ground_line)

    def vertex_q(self)          -> QColor: return qcolor(self.vertex)
    def vertex_ground_q(self)   -> QColor: return qcolor(self.vertex_ground)
    def vertex_outline_q(self)  -> QColor: return qcolor(self.vertex_outline)

    def settings_bg_css(self)       -> str: return rgb_css(self.settings_bg)
    def settings_card_bg_css(self)  -> str: return rgb_css(self.settings_card_bg)

    # -- Widget style snippets -------------------------------------------------

    def player_btn_style(self, team: str) -> str:
        bg = {"blue": self.blue_q(), "red": self.red_q()}.get(team, self.green_q()).name()
        return "background-color: {}; border-radius: 20px; border: 2px solid white;".format(bg)

    def hint_btn_style(self, active: bool) -> str:
        if active:
            return "background-color: {}; color: {}; font-weight: bold;".format(
                self.hint_on_bg, self.hint_on_text)
        return "background-color: {}; color: {};".format(self.hint_off_bg, self.hint_off_text)

    def edu_btn_style(self, active: bool) -> str:
        if active:
            return (
                "font-size: {fs}px; background-color: {bg}; color: white;"
                " border: 2px solid white; border-radius: {r}px;"
            ).format(fs=self.icon_btn_font_size, bg=self.edu_on_bg, r=self.btn_radius)
        return (
            "font-size: {fs}px; background-color: {bg}; color: {fg};"
            " border: {bw}px solid {bc}; border-radius: {r}px;"
        ).format(fs=self.icon_btn_font_size, bg=self.btn_bg, fg=self.btn_text,
                 bw=self.border_w, bc=self.border_color, r=self.btn_radius)

    def edit_btn_style(self, active: bool) -> str:
        if active:
            return (
                "font-size: {fs}px; background-color: {bg}; color: white;"
                " border: 2px solid white; border-radius: {r}px;"
            ).format(fs=self.icon_btn_font_size, bg=self.edit_on_bg, r=self.btn_radius)
        return (
            "font-size: {fs}px; background-color: {bg}; color: {fg};"
            " border: {bw}px solid {bc}; border-radius: {r}px;"
        ).format(fs=self.icon_btn_font_size, bg=self.btn_bg, fg=self.btn_text,
                 bw=self.border_w, bc=self.border_color, r=self.btn_radius)


# -- ThemeManager --------------------------------------------------------------

class ThemeManager(QObject):
    changed = Signal(object)

    def __init__(self, theme: Optional[HBTheme] = None) -> None:
        super().__init__()
        self._theme: HBTheme = theme or HBTheme()
        self._is_dark: bool = True

    @property
    def theme(self) -> HBTheme:
        return self._theme

    @property
    def is_dark(self) -> bool:
        return self._is_dark

    def set_theme(self, theme: HBTheme) -> None:
        self._theme = theme
        self.changed.emit(self._theme)

    def set_dark(self, dark: bool) -> None:
        """Switch to the dark or light preset and emit changed."""
        from hb_settings import DARK_THEME, LIGHT_THEME
        self._is_dark = dark
        self.set_theme(DARK_THEME if dark else LIGHT_THEME)

    def patch(self, **kwargs: Any) -> None:
        self.set_theme(replace(self._theme, **kwargs))


THEME = ThemeManager()
