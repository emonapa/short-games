import os
import sys

from PySide6.QtCore import Qt, QTimer, QSize
from PySide6.QtGui import QPen, QBrush, QColor, QFont, QIcon
from PySide6.QtWidgets import (
    QApplication,
    QGraphicsView,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
    QStackedWidget,
    QWidgetAction,
    QFileDialog,
    QMenu,
    QMessageBox,
)

from pathlib import Path

HERE = Path(__file__).resolve().parent
HOTPOTCH_DIR = HERE / "hotpotch_utils"

if str(HOTPOTCH_DIR) not in sys.path:
    sys.path.insert(0, str(HOTPOTCH_DIR))

import hb_solver
import hb_io
import hb_education
import hb_settings
import hb_calculator
import hb_graphics
import hb_solver_worker
from hb_theme import THEME

class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Hotpotch Builder & Player")

        self._cfg = hb_settings.load_config()

        try:
            self.solver = hb_solver.HBSolver()
        except Exception as e:
            self.solver = None
            print(f"Warning: C-Solver not found, UI now running without math: {e}")

        self.solver.memory_multiplier = self._cfg.get("performance", 0.5)
        if self.solver.memory_multiplier > 0.9 or self.solver.memory_multiplier < 0.1:
            self.solver.memory_multiplier = 0.5
        self.solver.initialize()

        self.scene = hb_graphics.GraphScene(self.solver, THEME.theme)
        self.scene.on_turn_changed = self._update_player_button
        self.scene.on_build_color_changed = self._update_color_button_ui

        # education
        self.history = hb_education.HistoryManager(self.scene)
        self.edu_manager = hb_education.EducationManager(self.scene)
        self.scene.edu_manager = self.edu_manager # pro vytvareni PNG dirty trik

        self.view = QGraphicsView(self.scene)
        self.view.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view.setFocusPolicy(Qt.StrongFocus)
        self.view.setFocus()
        self.view.setStyleSheet("border: none;")

        self.setStyleSheet(THEME.theme.stylesheet())
        self.view.setBackgroundBrush(QBrush(THEME.theme.game_bg_q()))

        # Settings panel lives in a stacked widget alongside the game view
        self.settings_panel = hb_settings.SettingsPanel(self)
        self.calculator_panel = hb_calculator.CalculatorPanel(self)

        self.stack = QStackedWidget()
        self.stack.addWidget(self.view)             # index 0 - game
        self.stack.addWidget(self.settings_panel)   # index 1 - settings
        self.stack.addWidget(self.calculator_panel) # index 2 - calculator

        # --- POPISKY ---
        self.playing_lbl = QLabel("Playing: ")
        self.playing_lbl.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.playing_lbl.setStyleSheet("font-weight: bold; font-size: 16px;")

        self.building_lbl = QLabel("Building: ")
        self.building_lbl.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.building_lbl.setStyleSheet("font-weight: bold; font-size: 16px;")

        # --- TLAČÍTKA HRÁČE A EDITACE ---
        self.player_btn = QPushButton()
        self.player_btn.setFixedSize(40, 40)
        self.player_btn.setToolTip("Clicking swithces the player playing")
        self.player_btn.clicked.connect(self._manual_turn_toggle)

        self.edit_btn = QPushButton()
        self.edit_btn.setIcon(QIcon("hotpotch_utils/figs/edit-mode.svg"))
        self.edit_btn.setIconSize(QSize(33, 33))
        self.edit_btn.setFixedSize(40, 40)
        self.edit_btn.setToolTip("Edit mode: Can remove any edge")
        self.edit_btn.clicked.connect(self.toggle_edit_mode)
        self.edit_btn.setStyleSheet(THEME.theme.edit_btn_style(False))

        self._update_player_button()

        self.clear_btn = QPushButton()
        self.clear_btn.setIcon(QIcon("hotpotch_utils/figs/trash-bin.svg"))
        self.clear_btn.setIconSize(QSize(33, 33))
        self.clear_btn.setFixedSize(33, 33)
        self.clear_btn.clicked.connect(self.clear_scene)
        self.clear_btn.setStyleSheet("""
            QPushButton { background-color: transparent; border: none; }
        """)

        # --- TLAČÍTKA PRO STAVĚNÍ ---
        self.color_btn = QPushButton("")
        self.color_btn.setFixedSize(40, 40)
        self.color_btn.clicked.connect(self.toggle_build_color)
        self._update_color_button_ui(self.scene.current_color)

        self.solve_btn = QPushButton("Solve")
        self.solve_btn.clicked.connect(self.solve_current)
        self.solve_btn.setStyleSheet("""
            QPushButton { background-color: #408040; border: none; }
        """)

        self.save_btn = QPushButton("Save")
        self.save_btn.clicked.connect(self.save_game)

        self.load_btn = QPushButton("Load")
        self.load_btn.clicked.connect(self.load_game)


        self.menu_btn = QPushButton("⋮")
        self.menu_btn.setObjectName("menu_btn")
        self.menu_btn.setFixedSize(40, 40)
        self.menu_btn.setFont(QFont("", 20))
        self.menu_btn.setToolTip("Menu")
        self.menu_btn.setStyleSheet(
            "#menu_btn { background-color: #2a2a2a; color: #aaa; border: 1px solid #444;"
            " border-radius: 20px; padding: 0px; }"
            "#menu_btn:hover { background-color: #3a3a3a; color: white; }"
            "#menu_btn:pressed { background-color: #555; color: white; border-radius: 20px; }"
        )
        self.menu_btn.clicked.connect(self._show_menu)

        self.result_lbl = QLabel("Wins: ")
        self.result_lbl.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)

        # --- PLAY TLAČÍTKA ---
        icon_font = QFont()
        icon_font.setPointSize(THEME.theme.icon_btn_font_size)

        self.step_back_btn = QPushButton()
        self.step_back_btn.setIcon(QIcon("hotpotch_utils/figs/history-left.svg"))
        self.step_back_btn.setIconSize(QSize(33, 33))
        self.step_back_btn.setFixedSize(40, 40)
        self.step_back_btn.setFont(icon_font)
        self.step_back_btn.clicked.connect(self.step_backward)

        self.step_fwd_btn = QPushButton()
        self.step_fwd_btn.setIcon(QIcon("hotpotch_utils/figs/history-right.svg"))
        self.step_fwd_btn.setIconSize(QSize(33, 33))
        self.step_fwd_btn.setFixedSize(40, 40)
        self.step_fwd_btn.setFont(icon_font)
        self.step_fwd_btn.clicked.connect(self.step_forward)

        # --- HORNÍ LAYOUT ---
        top = QHBoxLayout()
        top.addWidget(self.clear_btn)
        top.addSpacing(6)
        top.addWidget(self.playing_lbl)
        top.addWidget(self.player_btn)
        top.addWidget(self.edit_btn)
        top.addSpacing(20)
        top.addWidget(self.solve_btn)
        top.addWidget(self.result_lbl, 1)

        # education
        top.addWidget(self.step_back_btn)
        top.addWidget(self.step_fwd_btn)

        top.addStretch(1)
        top.addWidget(self.building_lbl)
        top.addWidget(self.color_btn)
        top.addWidget(self.save_btn)
        top.addWidget(self.load_btn)
        top.addSpacing(6)
        top.addWidget(self.menu_btn)

        root = QVBoxLayout()
        root.addLayout(top)
        root.addWidget(self.stack, 1)

        w = QWidget()
        w.setLayout(root)
        self.setCentralWidget(w)

        self.resize(THEME.theme.win_size_x, THEME.theme.win_size_y)

        # Apply startup config
        if self._cfg.get("start_with_edit", False):
            self.toggle_edit_mode()

        # Solver pojede v jinem threadu
        self._solver_worker = None

        # Animace 3 tecek
        self._dot_timer = QTimer()
        self._dot_timer.setInterval(900)
        self._dot_timer.timeout.connect(self._tick_dots)
        self._dot_count = 0

        # predem ulozeny string posledni hry
        self._last_game_string = ""


        THEME.changed.connect(self._on_theme_changed)

    # -- Settings --------------------------------------------------------------

    def open_settings(self) -> None:
        if self.stack.currentIndex() == 1:
            self.close_settings()
            return

        self.settings_panel.reload()
        self.stack.setCurrentIndex(1)
        self.settings_btn.setStyleSheet((
            "QPushButton {{ background-color: #6c63ff; color: white; border: 1px solid #6c63ff;"
            " border-radius: 18px; font-size: {}px; padding: 0px; }}"
        ).format(THEME.theme.icon_settings_font_size))

    def close_settings(self) -> None:
        self._cfg = hb_settings.load_config()

        # update size of cache
        memory_multiplier = self._cfg.get("performance", 0.5)
        if memory_multiplier != self.solver.memory_multiplier:
            self.solver.memory_multiplier = memory_multiplier
            self.solver.free_all()
            self.solver.initialize()

        self.stack.setCurrentIndex(0)
        self.view.setFocus()
        self.settings_btn.setStyleSheet((
            "QPushButton {{ background-color: #2a2a2a; color: #aaa; border: 1px solid #444;"
            " border-radius: 18px; font-size: {}px; padding: 0px; }}"
            "QPushButton:hover {{ background-color: #3a3a3a; color: white; }}"
        ).format(THEME.theme.icon_settings_font_size))

    # -- Theme -----------------------------------------------------------------

    def _on_theme_changed(self, theme) -> None:
        self.setStyleSheet(theme.stylesheet())
        self.view.setBackgroundBrush(QBrush(theme.game_bg_q()))
        self.scene.apply_theme(theme)
        self._update_player_button()
        self._update_color_button_ui(self.scene.current_color)
        self.edit_btn.setStyleSheet(theme.edit_btn_style(self.scene.edit_mode))

    def closeEvent(self, event):
        if self.solver:
            self.solver.free_all()
            #print("C-Solver memory now freed")
        event.accept()

    def _update_player_button(self) -> None:
        if self.scene.edit_mode:
            return
        team = "blue" if self.scene.player_to_move == hb_solver.EDGE_BLUE else "red"
        self.player_btn.setStyleSheet(THEME.theme.player_btn_style(team))

    def _manual_turn_toggle(self) -> None:
        if not self.scene.edit_mode:
            self.scene.player_to_move = hb_solver.EDGE_RED if self.scene.player_to_move == hb_solver.EDGE_BLUE else hb_solver.EDGE_BLUE
            self._update_player_button()
        self.scene.update_auras()

    def solve_current(self) -> None:
        if not self.solver:
            self.result_lbl.setText("Error: Solver not loaded.")
            return

        if self._solver_worker and self._solver_worker.isRunning():
            return

        live_mask = self.scene.compute_live_mask_all_edges()

        self._dot_count = 0
        self._dot_timer.start()
        self.result_lbl.setText("Solving.")

        self._solver_worker = hb_solver_worker.SolverWorker(self.solver, self.scene.g, live_mask)
        self._solver_worker.result_ready.connect(self._on_solve_done)
        self._solver_worker.finished.connect(self._on_worker_finished)
        self._solver_worker.start()

    def _on_worker_finished(self) -> None:
        if self._solver_worker:
            self._solver_worker.deleteLater()
            self._solver_worker = None

    # wrapper around clear_graph to put into history
    def clear_scene(self) -> None:
        window = self.scene.views()[0].window() if self.scene.views() else None
        if window and hasattr(window, 'history'):
            window.history.save_state()
        self.scene.clear_graph()


    def _on_solve_done(self, result: str) -> None:
        self._dot_timer.stop()
        self.result_lbl.setText(result)
        if self._solver_worker and self._solver_worker.result_ptr:
            self._last_game_string = self.solver.get_game_value_string(
                self._solver_worker.result_ptr, 1
            )
        self._solver_worker = None
        self.edu_manager.update_overlay()

    def save_game(self) -> None:
        path, _ = QFileDialog.getSaveFileName(self, "Save Hotpotch", "", "Hotpotch (*.hbg.json);;JSON (*.json);;All Files (*)")
        if not path: return
        if not path.endswith(".json"): path += ".hbg.json"
        try:
            hb_io.save_to_file(self.scene, path)
            self.result_lbl.setText(f"Saved: {os.path.basename(path)}")
        except Exception as e:
            self.result_lbl.setText(f"Save error: {e}")

    def load_game(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Load Hotpotch", "", "Hotpotch (*.hbg.json *.json);;All Files (*)")
        if not path: return
        try:
            hb_io.load_from_file(self.scene, path)
            self._update_color_button_ui(self.scene.current_color)
            self.result_lbl.setText(f"Loaded: {os.path.basename(path)}")
            self.scene.update_auras()
        except Exception as e:
            self.result_lbl.setText(f"Load error: {e}")

    def toggle_edit_mode(self):
        self.scene.edit_mode = not self.scene.edit_mode
        self.edit_btn.setStyleSheet(THEME.theme.edit_btn_style(self.scene.edit_mode))
        if self.scene.edit_mode:
            self.player_btn.setStyleSheet(THEME.theme.player_btn_style("green"))
        else:
            self._update_player_button()
        self.scene.update_auras()

    def _update_color_button_ui(self, color: int) -> None:
        team = {hb_solver.EDGE_BLUE: "blue", hb_solver.EDGE_RED: "red", hb_solver.EDGE_GREEN: "green"}[color]
        self.color_btn.setStyleSheet(THEME.theme.player_btn_style(team))

    def toggle_build_color(self) -> None:
        colors = [hb_solver.EDGE_BLUE, hb_solver.EDGE_RED, hb_solver.EDGE_GREEN]
        current_idx = colors.index(self.scene.current_color)
        new_color = colors[(current_idx + 1) % 3]
        self.scene.set_current_color(new_color)
        self._update_color_button_ui(new_color)

    def step_backward(self):
        self.scene.hide_hint()
        self.history.undo()

    def step_forward(self):
        self.scene.hide_hint()
        if self.history.redo_stack:
            self.history.redo()
            return
        best_moves = self.scene.get_best_moves()
        if best_moves:
            self.scene._execute_cut(best_moves[0])

    def _tick_dots(self):
        self._dot_count = (self._dot_count % 3) + 1
        self.result_lbl.setText("Solving" + "." * self._dot_count)

    def _show_menu(self):
        menu = QMenu(self)
        menu.setStyleSheet(
            "QMenu { background:#2a2a2a; color:white; border:1px solid #555; }"
            "QMenu::item { padding: 8px 20px; }"
            "QMenu::item:selected { background:#444; }"
        )

        menu.addAction("Swap colors (-G)", self.negate_graph)
        menu.addAction("Clear cache", self.restart_cache)
        menu.addAction("Copy game result", self.copy_game_result)

        # edu toggle
        edu_widget = QPushButton("Analyse position")
        edu_widget.setFlat(True)
        edu_widget.setStyleSheet(f"""
            QPushButton {{ text-align: left; padding: 8px 20px; border: none; color: white;
                background: {'#2d5a2d' if self.edu_manager.active else '#2a2a2a'};
            }}
            QPushButton:hover {{ background: {'#3a6f3a' if self.edu_manager.active else '#444'};
            }}
        """)
        edu_widget.clicked.connect(lambda: (self.edu_manager.toggle(), menu.close()))

        edu_action = QWidgetAction(menu)
        edu_action.setDefaultWidget(edu_widget)
        menu.addAction(edu_action)
        # edu toggle

        # hints toggle
        hints_widget = QPushButton("Show hints")
        hints_widget.setFlat(True)
        hints_widget.setStyleSheet(f"""
            QPushButton {{ text-align: left; padding: 8px 20px; border: none; color: white;
                background: {'#2d5a2d' if self.scene.hints_active else '#2a2a2a'};
            }}
            QPushButton:hover {{ background: {'#3a6f3a' if self.scene.hints_active else '#444'};
            }}
        """)
        hints_widget.clicked.connect(lambda: (self.scene.toggle_hints(), menu.close()))

        hints_action = QWidgetAction(menu)
        hints_action.setDefaultWidget(hints_widget)
        menu.addAction(hints_action)
        # hints toggle


        # bot toggle
        bot_widget = QPushButton("Play against bot")
        bot_widget.setFlat(True)
        bot_widget.setStyleSheet(f"""
            QPushButton {{ text-align: left; padding: 8px 20px; border: none; color: white;
                background: {'#2d5a2d' if (self.scene.bot_playing_color is not None) else '#2a2a2a'};
            }}
            QPushButton:hover {{ background: {'#3a6f3a' if (self.scene.bot_playing_color is not None) else '#444'};
            }}
        """)
        bot_widget.clicked.connect(lambda: (self.scene.toggle_bot(), menu.close()))

        bot_action = QWidgetAction(menu)
        bot_action.setDefaultWidget(bot_widget)
        menu.addAction(bot_action)
        # bot toggle

        menu.addAction("Calculator", self.open_calculator)
        menu.addAction("Help", self.open_help)
        menu.addAction("Settings", self.open_settings)

        menu.exec(self.menu_btn.mapToGlobal(self.menu_btn.rect().bottomLeft()))

    def negate_graph(self):
        for i in range(self.scene.g.num_edges):
            c = self.scene.g.edges[i].color
            if c == hb_solver.EDGE_BLUE:
                self.scene.g.edges[i].color = hb_solver.EDGE_RED
            elif c == hb_solver.EDGE_RED:
                self.scene.g.edges[i].color = hb_solver.EDGE_BLUE
        t = self.scene.theme
        for e in self.scene.edge_items:
            if e.color == hb_solver.EDGE_BLUE:
                e.color = hb_solver.EDGE_RED
                e.item.edge_color = hb_solver.EDGE_RED
                pen = QPen(t.red_q(), t.edge_width)
            elif e.color == hb_solver.EDGE_RED:
                e.color = hb_solver.EDGE_BLUE
                e.item.edge_color = hb_solver.EDGE_BLUE
                pen = QPen(t.blue_q(), t.edge_width)
            else:
                continue
            pen.setCapStyle(Qt.RoundCap)
            pen.setJoinStyle(Qt.RoundJoin)
            e.item.setPen(pen)

    def open_calculator(self):
        if self.stack.currentIndex() == 2:
            self.stack.setCurrentIndex(0)
            self.view.setFocus()
            return
        self.stack.setCurrentIndex(2)

    def open_help(self):
        QMessageBox.information(
            self,
            "Help",
            """
            <h2>Hotpotch Builder & Player</h2>

            <h3>Basic controls</h3>
            <p>
            <b>Left click</b> - place or select graph elements<br>
            <b>Right click</b> - removes focus on vertex<br>
            <b>Mouse wheel</b> - changes the color of the edge you are building<br>
            <b>Edit mode</b> - allows removing any edge<br>
            <b>Playing button</b> - switches the player to move<br>
            <b>Building button</b> - switches edge color: blue, red, green
            </p>

            <h3>Top buttons</h3>
            <p>
            <b>Solve</b> - computes the current game value<br>
            <b>Save</b> - saves the current graph<br>
            <b>Load</b> - loads a graph from file<br>
            <b>Trash</b> - clears the scene<br>
            <b>Undo / Redo</b> - moves backward or forward in history, if there is no history forward, best move is played
            </p>

            <h3>Menu</h3>
            <p>
            <b>Swap colors (-G)</b> - swaps blue and red edges<br>
            <b>Clear cache</b> - clears solver cache<br>
            <b>Copy game result</b> - copies the last computed value<br>
            <b>Analyse position</b> - toggles educational analysis<br>
            <b>Show hints</b> - shows suggested moves<br>
            <b>Play against bot</b> - toggles bot mode<br>
            <b>Calculator</b> - opens game value calculator<br>
            <b>Settings</b> - opens settings
            </p>
            """
        )

    def open_settings(self):
        if self.stack.currentIndex() == 1:
            self.close_settings()
            return
        self.settings_panel.reload()
        self.stack.setCurrentIndex(1)

    def close_settings(self):
        self._cfg = hb_settings.load_config()
        self.stack.setCurrentIndex(0)
        self.view.setFocus()

    def restart_cache(self):
        self.solver.free_all()
        self.solver.initialize()

    def copy_game_result(self):
        if not self.solver:
            return
        QApplication.clipboard().setText(self._last_game_string)


def main() -> int:
    app = QApplication(sys.argv)
    hb_settings.apply_theme_from_config()  # restore saved theme before any widget is built
    win = MainWindow()
    win.show()
    return app.exec()

if __name__ == "__main__":
    raise SystemExit(main())
