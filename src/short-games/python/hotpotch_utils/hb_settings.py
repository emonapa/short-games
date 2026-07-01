"""
hb_settings.py - Config persistence and Settings UI panel.

Public API used by main.py:
    load_config() -> dict
    save_config(cfg: dict)
    apply_theme_from_config()   -- call once at startup after QApplication exists
    CONFIG_DEFAULTS: dict
    SettingsPanel(main_window, parent=None)
"""

import os
import json
from typing import Optional

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QCheckBox, QSlider, QFrame,
)

from hb_theme import THEME, HBTheme

# ── Config ────────────────────────────────────────────────────────────────────

CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hb_config.json")

CONFIG_DEFAULTS: dict = {
    "dark_mode": True,
    "performance": 0.5,
    "start_with_edit": False,
    "dont_clear_cache_between_games": False,
}


def load_config() -> dict:
    cfg = dict(CONFIG_DEFAULTS)
    try:
        with open(CONFIG_PATH, "r") as f:
            cfg.update(json.load(f))
    except Exception:
        pass
    return cfg


def save_config(cfg: dict) -> None:
    try:
        with open(CONFIG_PATH, "w") as f:
            json.dump(cfg, f, indent=2)
    except Exception:
        pass


# ── Theme presets ─────────────────────────────────────────────────────────────

DARK_THEME = HBTheme()

LIGHT_THEME = HBTheme(
    window_bg=(235, 235, 235),

    blue=(2, 90, 160),
    red=(180, 0, 60),
    green=(0, 140, 70),

    game_bg=(210, 210, 215),
    ground=(130, 80, 200),
    ground_line=(90, 50, 140),

    settings_bg=(210, 210, 215),
    settings_card_bg=(245, 245, 248),

    vertex=(160, 160, 160),
    vertex_ground=(110, 60, 170),
    vertex_outline=(0, 0, 0),

    text="#111111",
    btn_bg="#cccccc",
    btn_bg_hover="#bbbbbb",
    btn_text="#111111",
    border_color="#999999",

    hint_on_bg="#c49a00",
    hint_on_text="white",
    hint_off_bg="#aaaaaa",
    hint_off_text="#111111",
    hint_text_color="#222222",

    edu_on_bg="#7d3baa",
    edit_on_bg="#888888",
)


def apply_theme_from_config() -> None:
    """Read dark_mode from config and push the matching theme through THEME."""
    cfg = load_config()
    THEME.set_dark(cfg.get("dark_mode", True))


# ── Internal style constants (settings panel always stays dark-ish) ───────────
# These are the panel's own chrome - they follow the theme via _on_theme_changed.

_TOGGLE_STYLE = """
    QCheckBox::indicator { width: 42px; height: 24px; border-radius: 12px; }
    QCheckBox::indicator:unchecked {
        background-color: #444; border: 2px solid #555; border-radius: 12px;
    }
    QCheckBox::indicator:checked {
        background-color: #6c63ff; border: 2px solid #6c63ff; border-radius: 12px;
    }
"""

_SLIDER_STYLE = """
    QSlider::groove:horizontal {
        height: 4px; background: #3a3a3a; border-radius: 2px;
    }
    QSlider::handle:horizontal {
        background: white; width: 16px; height: 16px;
        margin: -6px 0; border-radius: 8px;
    }
    QSlider::sub-page:horizontal {
        background: #6c63ff; border-radius: 2px;
    }
"""

_CACHE_TOOLTIP = (
    "Good when interacting with similar graphs and not wanting to calculate all over again."
)


# ── Helpers ───────────────────────────────────────────────────────────────────

def _divider(color: str = "#4a4a4a") -> QFrame:
    line = QFrame()
    line.setFrameShape(QFrame.HLine)
    line.setFixedHeight(1)
    line.setStyleSheet(f"background-color: {color};")
    return line


def _section_label(text: str, color: str = "#999") -> QLabel:
    lbl = QLabel(text)
    lbl.setStyleSheet(
        f"color: {color}; font-size: 13px; font-weight: bold; letter-spacing: 1px;"
    )
    return lbl


def _toggle(checked: bool) -> QCheckBox:
    cb = QCheckBox()
    cb.setChecked(checked)
    cb.setStyleSheet(_TOGGLE_STYLE)
    return cb


def _setting_row(layout, label_text: str, checked: bool,
                 slot, text_color: str = "white",
                 tooltip: Optional[str] = None) -> QCheckBox:
    row = QHBoxLayout()
    lbl = QLabel(label_text)
    style = f"color: {text_color}; font-size: 16px;"
    if tooltip:
        lbl.setToolTip(tooltip)
        style += " border-bottom: 1px dotted #777;"
    lbl.setStyleSheet(style)

    cb = _toggle(checked)
    cb.stateChanged.connect(slot)

    row.addWidget(lbl)
    row.addStretch()
    row.addWidget(cb)
    layout.addLayout(row)
    return cb


# ── SettingsPanel ─────────────────────────────────────────────────────────────

class SettingsPanel(QWidget):
    """
    Full-area settings overlay shown in place of the game view.
    Lives in QStackedWidget at index 1. Call reload() before showing.
    """

    def __init__(self, main_window, parent=None):
        super().__init__(parent)
        self.mw = main_window
        self.cfg = load_config()

        # outer layout fills the full stack area and centres the card
        outer = QHBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)

        self.card = QWidget()
        self.card.setFixedWidth(520)

        outer.addStretch(1)
        outer.addWidget(self.card)
        outer.addStretch(1)

        self._card_layout = QVBoxLayout(self.card)
        self._card_layout.setContentsMargins(44, 40, 44, 40)
        self._card_layout.setSpacing(0)

        self._build_header(self._card_layout)
        self._card_layout.addSpacing(32)

        self._build_theme(self._card_layout)
        self._card_layout.addSpacing(28)
        self._dividers = []
        self._dividers.append(_divider())
        self._card_layout.addWidget(self._dividers[-1])
        self._card_layout.addSpacing(28)

        self._build_performance(self._card_layout)
        self._card_layout.addSpacing(28)
        self._dividers.append(_divider())
        self._card_layout.addWidget(self._dividers[-1])
        self._card_layout.addSpacing(28)

        self._build_startup(self._card_layout)
        self._card_layout.addSpacing(28)
        self._dividers.append(_divider())
        self._card_layout.addWidget(self._dividers[-1])
        self._card_layout.addSpacing(28)

        self._build_cache(self._card_layout)
        self._card_layout.addStretch()

        # apply current theme and subscribe to future changes
        self._apply_theme(THEME.theme)
        THEME.changed.connect(self._apply_theme)

    # ── Theme ─────────────────────────────────────────────────────────────────

    def _apply_theme(self, theme) -> None:
        bg = theme.settings_bg_css()
        card_bg = theme.settings_card_bg_css()
        text = theme.text

        self.setStyleSheet(f"background-color: {bg};")
        self.card.setStyleSheet(
            f"background-color: {card_bg}; border-radius: 14px;"
        )

        # update text colours on all labels in the card
        for lbl in self.card.findChildren(QLabel):
            name = lbl.objectName()
            if name == "section_lbl":
                lbl.setStyleSheet(
                    "color: #888; font-size: 13px; font-weight: bold; letter-spacing: 1px;"
                )
            elif name == "title_lbl":
                lbl.setStyleSheet(
                    f"color: {text}; font-size: 22px; font-weight: bold;"
                )
            elif name == "hint_lbl":
                lbl.setStyleSheet("color: #888; font-size: 13px;")
            elif name == "perf_val_lbl":
                lbl.setStyleSheet(f"color: #888; font-size: 16px; min-width: 44px;")
            else:
                lbl.setStyleSheet(f"color: {text}; font-size: 16px;")

    # ── Section builders ──────────────────────────────────────────────────────

    def _build_header(self, layout) -> None:
        row = QHBoxLayout()
        title = QLabel("Settings")
        title.setStyleSheet("color: white; font-size: 22px; font-weight: bold;")
        back_btn = QPushButton("← Back")
        back_btn.clicked.connect(self.mw.close_settings)
        row.addWidget(title)
        row.addStretch()
        row.addWidget(back_btn)
        layout.addLayout(row)

    def _build_theme(self, layout) -> None:
        sec = QLabel("THEME")
        sec.setObjectName("section_lbl")
        layout.addWidget(sec)
        layout.addSpacing(14)

        row = QHBoxLayout()
        lbl = QLabel("Dark mode")
        self.dark_toggle = _toggle(self.cfg["dark_mode"])
        self.dark_toggle.stateChanged.connect(self._on_dark_mode)
        row.addWidget(lbl)
        row.addStretch()
        row.addWidget(self.dark_toggle)
        layout.addLayout(row)

    def _build_performance(self, layout) -> None:
        sec = QLabel("PERFORMANCE")
        sec.setObjectName("section_lbl")
        layout.addWidget(sec)
        layout.addSpacing(14)

        label_row = QHBoxLayout()
        desc = QLabel("Memory usage")

        self.perf_value_lbl = QLabel(f"{self.cfg['performance'] * 100}%")
        self.perf_value_lbl.setObjectName("perf_val_lbl")
        self.perf_value_lbl.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        label_row.addWidget(desc)
        label_row.addStretch()
        label_row.addWidget(self.perf_value_lbl)
        layout.addLayout(label_row)
        layout.addSpacing(10)

        self.perf_slider = QSlider(Qt.Horizontal)
        self.perf_slider.setMinimum(1)
        self.perf_slider.setMaximum(9)
        self.perf_slider.setSingleStep(1)
        self.perf_slider.setValue(self.cfg["performance"] * 10)
        self.perf_slider.setStyleSheet(_SLIDER_STYLE)
        self.perf_slider.valueChanged.connect(self._on_perf_changed)
        layout.addWidget(self.perf_slider)

        hint = QLabel(
            "Controls how much RAM the converter may allocate for caches. "
            "Higher = faster on large positions."
        )
        hint.setObjectName("hint_lbl")
        hint.setWordWrap(True)
        layout.addSpacing(6)
        layout.addWidget(hint)

    def _build_startup(self, layout) -> None:
        sec = QLabel("STARTUP")
        sec.setObjectName("section_lbl")
        layout.addWidget(sec)
        layout.addSpacing(14)

        self.edit_check = _setting_row(
            layout, "Start in Edit mode",
            self.cfg["start_with_edit"], self._on_start_edit,
        )

    def _build_cache(self, layout) -> None:
        sec = QLabel("CACHE")
        sec.setObjectName("section_lbl")
        layout.addWidget(sec)
        layout.addSpacing(14)

        self.cache_check = _setting_row(
            layout, "Don't Clear cache in between games",
            self.cfg["dont_clear_cache_between_games"], self._on_dont_clear_cache,
            tooltip=_CACHE_TOOLTIP,
        )

    # ── Slots ─────────────────────────────────────────────────────────────────

    def _on_dark_mode(self, state: int) -> None:
        self.cfg["dark_mode"] = bool(state)
        save_config(self.cfg)
        THEME.set_dark(bool(state))

    def _on_perf_changed(self, value: int) -> None:
        pct = value * 10 # 10 - 90
        self.perf_value_lbl.setText(f"{pct}%")
        self.cfg["performance"] = pct / 100
        save_config(self.cfg)

    def _on_start_edit(self, state: int) -> None:
        self.cfg["start_with_edit"] = bool(state)
        save_config(self.cfg)

    def _on_dont_clear_cache(self, state: int) -> None:
        self.cfg["dont_clear_cache_between_games"] = bool(state)
        save_config(self.cfg)

    # ── Public ────────────────────────────────────────────────────────────────

    def reload(self) -> None:
        """Re-read config from disk and refresh all widgets without triggering slots."""
        self.cfg = load_config()

        self.dark_toggle.blockSignals(True)
        self.dark_toggle.setChecked(self.cfg["dark_mode"])
        self.dark_toggle.blockSignals(False)

        self.perf_slider.blockSignals(True)
        self.perf_slider.setValue(self.cfg["performance"] * 10)
        self.perf_slider.blockSignals(False)
        self.perf_value_lbl.setText(f"{self.cfg['performance'] * 100}%")

        self.edit_check.blockSignals(True)
        self.edit_check.setChecked(self.cfg["start_with_edit"])
        self.edit_check.blockSignals(False)

        self.cache_check.blockSignals(True)
        self.cache_check.setChecked(self.cfg["dont_clear_cache_between_games"])
        self.cache_check.blockSignals(False)
