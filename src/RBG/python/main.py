# propojit game_negate
# nahazet veci z kalkulacky do C

import os
import sys
from dataclasses import dataclass
from typing import List, Optional
import math

from PySide6.QtCore import Qt, QPointF, QTimer, QRectF, QSize
from PySide6.QtGui import QPen, QBrush, QColor, QPainterPath, QFont, QIcon
from PySide6.QtWidgets import (
    QApplication, QGraphicsEllipseItem, QGraphicsScene,
    QGraphicsView, QHBoxLayout, QLabel, QMainWindow, QPushButton, QVBoxLayout, QWidget, QGraphicsPathItem, QGraphicsItem,
    QStackedWidget,
)
from PySide6.QtWidgets import QFileDialog, QGraphicsTextItem, QGraphicsDropShadowEffect, QMenu

import hb_solver
import hb_io
import hb_education
import hb_settings
import hb_calculator
from hb_theme import THEME


class InteractiveEdgeItem(QGraphicsPathItem):
    def __init__(self, scene, edge_idx, color, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.custom_scene = scene
        self.edge_idx = edge_idx
        self.edge_color = color
        self.setAcceptHoverEvents(True)

    def hoverEnterEvent(self, event):
        if self.custom_scene.hints_active:
            self.custom_scene.show_hint_for_edge(self.edge_idx, self.edge_color, event.scenePos())
        super().hoverEnterEvent(event)

    def hoverLeaveEvent(self, event):
        self.custom_scene.hide_hint()
        super().hoverLeaveEvent(event)

class GraphScene(QGraphicsScene):
    def __init__(self, solver: Optional[hb_solver.HBSolver], theme=None) -> None:
        super().__init__()
        self.solver = solver
        self.theme = theme or THEME.theme
        self.setSceneRect(0, 0, theme.win_size_x, theme.win_size_y)

        self.ground_y = theme.win_size_y * 0.9
        self.hit_radius = 12.0
        self.merge_radius = 22.0

        self.g = hb_solver.CBaseGraph()
        self.g.num_vertices = 0
        self.g.num_edges = 0

        self.vertex_pos: List[QPointF] = [QPointF(0, 0) for _ in range(hb_solver.MAX_VERTICES)]
        self.vertex_items: List[Optional[QGraphicsEllipseItem]] = [None for _ in range(hb_solver.MAX_VERTICES)]
        self.is_ground: List[bool] = [False for _ in range(hb_solver.MAX_VERTICES)]

        self.edge_items: List['EdgeItem'] = []
        self.parallel_count = {}

        self.current_color = hb_solver.EDGE_BLUE
        self.pending_u: Optional[int] = None
        self.pending_marker = None

        self.player_to_move = hb_solver.EDGE_BLUE
        self.on_turn_changed = None

        self.is_slashing = False
        self.slash_points = []
        self.slash_path = None
        self.slash_item = None
        self.active_slashes = []

        self.fade_timer = QTimer()
        self.fade_timer.timeout.connect(self._process_fades)
        self.fade_timer.start(25)

        self.hints_active = False

        self.floating_hint = QGraphicsTextItem()
        self.floating_hint.setFont(QFont("Consolas", 18, QFont.Bold))
        self.floating_hint.setDefaultTextColor(QColor(self.theme.hint_text_color))
        self.floating_hint.setZValue(100.0)
        self.floating_hint.hide()
        self.addItem(self.floating_hint)

        self.hint_timer = QTimer()
        self.hint_timer.setSingleShot(True)
        self.hint_timer.timeout.connect(self._process_delayed_hint)
        self.pending_hint_data = None

        self.edit_mode = False
        self.on_build_color_changed = None
        self.trash_bin = []

        self._draw_ground()

    def _draw_ground(self) -> None:
        ground_rect = QRectF(-10000, self.ground_y, 20000, 2000)
        self.addRect(ground_rect, QPen(Qt.NoPen), QBrush(self.theme.ground_q()))
        self.addLine(-10000, self.ground_y, 10000, self.ground_y,
                     QPen(self.theme.ground_line_q(), self.theme.ground_line_w))

    def apply_theme(self, theme) -> None:
        self.theme = theme
        for e in self.edge_items:
            pen_color = {
                hb_solver.EDGE_BLUE:  theme.blue_q(),
                hb_solver.EDGE_RED:   theme.red_q(),
                hb_solver.EDGE_GREEN: theme.green_q(),
            }[e.color]
            pen = QPen(pen_color, theme.edge_width)
            pen.setCapStyle(Qt.RoundCap)
            pen.setJoinStyle(Qt.RoundJoin)
            e.item.setPen(pen)
        for i in range(int(self.g.num_vertices)):
            item = self.vertex_items[i]
            if item:
                item.setPen(QPen(theme.vertex_outline_q(), theme.vertex_outline_w))
                item.setBrush(QBrush(theme.vertex_ground_q() if self.is_ground[i] else theme.vertex_q()))
        self.setBackgroundBrush(QBrush(theme.game_bg_q()))
        self.floating_hint.setDefaultTextColor(QColor(theme.hint_text_color))

    def set_current_color(self, color: int) -> None:
        self.current_color = color

    def clear_graph(self) -> None:
        self.hide_hint()

        window = self.views()[0].window() if self.views() else None
        if window and hasattr(window, 'edu_manager'):
            window.edu_manager.bubbles.clear()

        # Clear solver cache based on config
        cfg = hb_settings.load_config()
        if not cfg.get("dont_clear_cache_between_games", False) and self.solver:
            self.solver.free_all()
            self.solver.initialize()

        self.clear()
        self.g.num_vertices = 0
        self.g.num_edges = 0
        self.edge_items.clear()
        self.parallel_count.clear()
        self.vertex_items = [None for _ in range(hb_solver.MAX_VERTICES)]
        self.vertex_pos = [QPointF(0, 0) for _ in range(hb_solver.MAX_VERTICES)]
        self.is_ground = [False for _ in range(hb_solver.MAX_VERTICES)]
        self.pending_u = None
        self.pending_marker = None
        self.active_slashes.clear()

        self._draw_ground()

        self.floating_hint = QGraphicsTextItem()
        self.floating_hint.setFont(QFont("Consolas", 18, QFont.Bold))
        self.floating_hint.setDefaultTextColor(QColor(self.theme.hint_text_color))
        self.floating_hint.setZValue(100.0)
        self.floating_hint.hide()
        self.addItem(self.floating_hint)

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.RightButton:
            self.cancel_pending()
            event.accept()
            return

        if event.button() != Qt.LeftButton:
            super().mousePressEvent(event)
            return

        pos = event.scenePos()

        if pos.y() >= self.ground_y - 20:
            pos.setY(self.ground_y)

        v_hit = self._find_vertex_hit(pos)
        is_on_ground_line = (pos.y() == self.ground_y)

        if self.pending_u is None and v_hit is None and not is_on_ground_line:
            self.is_slashing = True
            self.slash_points = [pos]
            self.slash_path = QPainterPath()
            self.slash_path.moveTo(pos)
            self.slash_item = QGraphicsPathItem(self.slash_path)

            pen = QPen(QColor(190, 190, 190, 120), 5, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)
            self.slash_item.setPen(pen)
            self.slash_item.setZValue(10.0)
            self.addItem(self.slash_item)
            event.accept()
            return

        if self.pending_u is None:
            u = self._pick_start_vertex(pos, v_hit)
            if u is not None:
                self.pending_u = u
                self._update_pending_marker()
            event.accept()
            return

        v = self._pick_end_vertex(pos, v_hit)
        if v is not None:
            u = self.pending_u
            if u != v:
                if self._add_edge(u, v, self.current_color):
                    self.pending_u = v
                else:
                    self.pending_u = None
                self._update_pending_marker()
        event.accept()

    def mouseMoveEvent(self, event) -> None:
        if self.is_slashing and self.slash_item:
            self.slash_points.append(event.scenePos())
            if len(self.slash_points) > 100:
                self.slash_points.pop(0)

            if len(self.slash_points) > 1:
                path = QPainterPath()
                path.moveTo(self.slash_points[0])
                for p in self.slash_points[1:]:
                    path.lineTo(p)
                self.slash_item.setPath(path)
            event.accept()
            return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        if self.is_slashing and self.slash_item:
            self.is_slashing = False

            cut_edge_idx = None
            for i, e_obj in enumerate(self.edge_items):
                if self.slash_item.collidesWithItem(e_obj.item):
                    if self._is_valid_cut(e_obj.color):
                        cut_edge_idx = i
                        break

            if cut_edge_idx is not None:
                self._execute_cut(cut_edge_idx)

            self.active_slashes.append(self.slash_item)
            self.slash_item = None
            self.slash_points = []
            event.accept()
            return

        super().mouseReleaseEvent(event)

    def wheelEvent(self, event) -> None:
        delta = event.delta()
        if delta == 0:
            return

        direction = 1 if delta > 0 else -1
        colors = [hb_solver.EDGE_BLUE, hb_solver.EDGE_RED, hb_solver.EDGE_GREEN]
        current_idx = colors.index(self.current_color)
        new_idx = (current_idx + direction) % 3
        self.set_current_color(colors[new_idx])

        if self.on_build_color_changed:
            self.on_build_color_changed(self.current_color)

        event.accept()

    def _is_valid_cut(self, color: int) -> bool:
        if self.edit_mode:
            return True
        if self.player_to_move == hb_solver.EDGE_BLUE and color in (hb_solver.EDGE_BLUE, hb_solver.EDGE_GREEN):
            return True
        if self.player_to_move == hb_solver.EDGE_RED and color in (hb_solver.EDGE_RED, hb_solver.EDGE_GREEN):
            return True
        return False

    def _execute_cut(self, cut_idx: int) -> None:
        if not self.solver:
            print("C-Solver not found, can't execute cut/cleanup")
            return

        self.hide_hint()

        window = self.views()[0].window() if self.views() else None
        if window and hasattr(window, 'history'):
            window.history.save_state()

        temp_mask = ((1 << self.g.num_edges) - 1) & ~(1 << cut_idx)
        new_mask = self.solver.cleanup_position(self.g, temp_mask)

        to_remove = []
        for i in range(self.g.num_edges):
            if not (new_mask & (1 << i)):
                to_remove.append(i)

        to_remove.sort(reverse=True)
        items_to_delete_later = []
        for idx in to_remove:
            e_obj = self.edge_items[idx]
            item = e_obj.item

            item.hide()
            item.setEnabled(False)
            item.setAcceptHoverEvents(False)
            items_to_delete_later.append(item)

            self.edge_items.pop(idx)

            for j in range(idx, self.g.num_edges - 1):
                self.g.edges[j] = self.g.edges[j+1]
            self.g.num_edges -= 1

        QTimer.singleShot(0, lambda: self._safe_remove_items(items_to_delete_later))

        for j in range(len(self.edge_items)):
            self.edge_items[j].item.edge_idx = j

        used = set()
        for e in self.edge_items:
            used.add(e.u)
            used.add(e.v)

        for i in range(hb_solver.MAX_VERTICES):
            if i not in used and self.vertex_items[i] is not None:
                self.removeItem(self.vertex_items[i])
                self.vertex_items[i] = None
                self.is_ground[i] = False

        if not self.edit_mode:
            if self.player_to_move == hb_solver.EDGE_BLUE:
                self.player_to_move = hb_solver.EDGE_RED
            else:
                self.player_to_move = hb_solver.EDGE_BLUE

        if self.on_turn_changed:
            self.on_turn_changed()

        self.update_auras()
        if window and hasattr(window, 'edu_manager'):
            window.edu_manager.update_overlay()

    def _safe_remove_items(self, items_to_trash):
        for item in items_to_trash:
            if item.scene() == self:
                self.removeItem(item)
        self.trash_bin.extend(items_to_trash)
        if len(self.trash_bin) > 50:
            self.trash_bin = self.trash_bin[-50:]

    def _process_fades(self):
        for item in list(self.active_slashes):
            op = item.opacity() - 0.15
            if op <= 0:
                self.removeItem(item)
                self.active_slashes.remove(item)
            else:
                item.setOpacity(op)

    def _pick_start_vertex(self, pos: QPointF, v_hit: Optional[int]) -> Optional[int]:
        if v_hit is not None: return v_hit
        if pos.y() == self.ground_y: return self._get_or_create_vertex(pos)
        return None

    def _pick_end_vertex(self, pos: QPointF, v_hit: Optional[int]) -> Optional[int]:
        if v_hit is not None: return v_hit
        return self._get_or_create_vertex(pos)

    def _find_vertex_hit(self, pos: QPointF) -> Optional[int]:
        r2 = self.hit_radius * self.hit_radius
        for i in range(int(self.g.num_vertices)):
            if self.vertex_items[i] is None: continue
            p = self.vertex_pos[i]
            if (p.x() - pos.x())**2 + (p.y() - pos.y())**2 <= r2:
                return i
        return None

    def _get_or_create_vertex(self, pos: QPointF) -> Optional[int]:
        r2 = self.merge_radius * self.merge_radius
        for i in range(int(self.g.num_vertices)):
            if self.vertex_items[i] is None: continue
            p = self.vertex_pos[i]
            if (p.x() - pos.x())**2 + (p.y() - pos.y())**2 <= r2:
                return i

        if int(self.g.num_vertices) >= hb_solver.MAX_VERTICES:
            return None

        idx = int(self.g.num_vertices)
        self.vertex_pos[idx] = QPointF(pos.x(), pos.y())

        is_gnd = (pos.y() == self.ground_y)
        self.is_ground[idx] = is_gnd

        self.g.num_vertices = hb_solver.c_uint8(idx + 1)
        self._render_vertex(idx, is_ground=is_gnd)
        return idx

    def _render_vertex(self, idx: int, is_ground: bool) -> None:
        p = self.vertex_pos[idx]
        r = self.hit_radius
        item = QGraphicsEllipseItem(p.x() - r, p.y() - r, 2 * r, 2 * r)

        item.setPen(QPen(self.theme.vertex_outline_q(), self.theme.vertex_outline_w))
        item.setBrush(QBrush(self.theme.vertex_ground_q() if is_ground else self.theme.vertex_q()))
        item.setZValue(2.0)
        self.addItem(item)
        self.vertex_items[idx] = item

    def cancel_pending(self) -> None:
        self.pending_u = None
        self._update_pending_marker()

    def _update_pending_marker(self) -> None:
        if self.pending_u is None:
            if self.pending_marker is not None:
                self.removeItem(self.pending_marker)
                self.pending_marker = None
            return

        p = self.vertex_pos[self.pending_u]
        r = self.hit_radius + 4.0

        if self.pending_marker is None:
            self.pending_marker = QGraphicsEllipseItem(p.x() - r, p.y() - r, 2*r, 2*r)
            self.pending_marker.setPen(QPen(Qt.white, 2, Qt.DashLine))
            self.pending_marker.setZValue(3.0)
            self.addItem(self.pending_marker)
        else:
            self.pending_marker.setRect(p.x() - r, p.y() - r, 2*r, 2*r)

    def _add_edge(self, u: int, v: int, color: int) -> bool:
        if int(self.g.num_edges) >= hb_solver.MAX_EDGES:
            return False

        eidx = int(self.g.num_edges)

        c_u = 0 if self.is_ground[u] else u
        c_v = 0 if self.is_ground[v] else v

        self.g.edges[eidx].u = hb_solver.c_uint8(c_u)
        self.g.edges[eidx].v = hb_solver.c_uint8(c_v)
        self.g.edges[eidx].color = color
        self.g.num_edges = hb_solver.c_uint8(eidx + 1)

        pu = self.vertex_pos[u]
        pv = self.vertex_pos[v]

        key = (u, v) if u < v else (v, u)
        k = self.parallel_count.get(key, 0)
        self.parallel_count[key] = k + 1

        offset_index = self._parallel_offset_index(k)
        path = self._make_edge_path(pu, pv, offset_index)

        pen_color = {
            hb_solver.EDGE_BLUE:  self.theme.blue_q(),
            hb_solver.EDGE_RED:   self.theme.red_q(),
            hb_solver.EDGE_GREEN: self.theme.green_q(),
        }[color]

        pen = QPen(pen_color, self.theme.edge_width)
        pen.setCapStyle(Qt.RoundCap)
        pen.setJoinStyle(Qt.RoundJoin)

        item = InteractiveEdgeItem(self, eidx, color)
        item.setPath(path)
        item.setPen(pen)
        item.setZValue(1.0)
        self.addItem(item)

        @dataclass
        class EdgeItem:
            u: int
            v: int
            color: int
            item: QGraphicsItem

        self.edge_items.append(EdgeItem(u=u, v=v, color=color, item=item))
        return True

    def _parallel_offset_index(self, k: int) -> int:
        if k == 0: return 0
        m = (k + 1) // 2
        sign = 1 if (k % 2) == 1 else -1
        return sign * m

    def _make_edge_path(self, pu: QPointF, pv: QPointF, offset_index: int) -> QPainterPath:
        path = QPainterPath()
        path.moveTo(pu)

        if offset_index == 0:
            path.lineTo(pv)
            return path

        dx = pv.x() - pu.x()
        dy = pv.y() - pu.y()
        length = math.hypot(dx, dy)
        if length < 1e-6:
            path.lineTo(pv)
            return path

        sign = 1 if offset_index > 0 else -1
        nx = (-dy / length) * sign
        ny = (dx / length) * sign

        bulge = (32.0 + (abs(offset_index) - 1) * 63.0) * (1 if offset_index > 0 else -1)
        mx, my = (pu.x() + pv.x()) * 0.5, (pu.y() + pv.y()) * 0.5
        cx, cy = mx + nx * bulge, my + ny * bulge

        path.quadTo(QPointF(cx, cy), pv)
        return path

    def compute_live_mask_all_edges(self) -> int:
        return (1 << int(self.g.num_edges)) - 1

    def show_hint_for_edge(self, edge_idx, color, pos):
        if not self.hints_active or self.edit_mode or not self.solver or not self._is_valid_cut(color):
            return
        self.pending_hint_data = (edge_idx, color, pos)
        self.hint_timer.start(400)

    def hide_hint(self):
        self.hint_timer.stop()
        self.floating_hint.hide()

    def _process_delayed_hint(self):
        if not self.pending_hint_data:
            return

        edge_idx, color, pos = self.pending_hint_data

        if edge_idx >= self.g.num_edges:
            print(f"[CHYBA] edge_idx = {edge_idx}, num_edges = {self.g.num_edges}")
            return

        temp_mask = ((1 << int(self.g.num_edges)) - 1) & ~(1 << edge_idx)
        new_mask = self.solver.cleanup_position(self.g, temp_mask)
        try:
            game_ptr = self.solver.solve(self.g, new_mask)
            formated = 1
            val_str = self.solver.get_game_value_string(game_ptr, formated)
        except Exception as e:
            print(f"[ERROR]: {e}")
            val_str = "Error"

        self.floating_hint.setPlainText(f" {val_str} ")
        self.floating_hint.setPos(pos.x() + 15, pos.y() - 25)
        self.floating_hint.show()

    def toggle_hints(self):
        self.hints_active = not self.hints_active
        if not self.hints_active:
            self.hide_hint()
        self.update_auras()
        return self.hints_active

    def get_best_moves(self):
        if not self.solver or self.g.num_edges == 0:
            return []

        valid_moves = []
        for i, e_obj in enumerate(self.edge_items):
            if self._is_valid_cut(e_obj.color):
                valid_moves.append(i)

        if not valid_moves:
            return []

        move_values = {}
        for idx in valid_moves:
            temp_mask = ((1 << int(self.g.num_edges)) - 1) & ~(1 << idx)
            new_mask = self.solver.cleanup_position(self.g, temp_mask)
            game_ptr = self.solver.solve(self.g, new_mask)
            move_values[idx] = game_ptr

        is_left = (self.player_to_move == hb_solver.EDGE_BLUE)
        zero_ptr = self.solver.solve(self.g, 0)

        winning = []
        for idx, val in move_values.items():
            if is_left:
                favorable = self.solver.game_geq(val, zero_ptr)
            else:
                favorable = self.solver.game_geq(zero_ptr, val)
            if favorable:
                winning.append(idx)

        candidates = winning if winning else list(move_values.keys())

        best_moves = []
        for idx in candidates:
            val = move_values[idx]
            is_worse = False
            to_remove = []

            for b_idx in best_moves:
                b_val = move_values[b_idx]

                if is_left:
                    val_geq = self.solver.game_geq(val, b_val)
                    b_geq = self.solver.game_geq(b_val, val)
                else:
                    val_geq = self.solver.game_geq(b_val, val)
                    b_geq = self.solver.game_geq(val, b_val)

                if val_geq and not b_geq:
                    to_remove.append(b_idx)
                elif b_geq and not val_geq:
                    is_worse = True
                    break

            if not is_worse:
                for r in to_remove:
                    best_moves.remove(r)
                best_moves.append(idx)

        return best_moves

    def update_auras(self):
        for e in self.edge_items:
            e.item.setGraphicsEffect(None)

        if not self.hints_active or self.edit_mode:
            return

        best_moves = self.get_best_moves()
        for idx in best_moves:
            effect = QGraphicsDropShadowEffect()
            effect.setBlurRadius(20)
            effect.setColor(QColor(255, 255, 255, 255))
            effect.setOffset(0, 0)
            self.edge_items[idx].item.setGraphicsEffect(effect)

        window = self.views()[0].window()
        if hasattr(window, 'edu_manager'):
            window.edu_manager.update_overlay()


from PySide6.QtCore import Qt, QPointF, QTimer, QRectF, QSize, QThread, Signal as QtSignal

class SolverWorker(QThread):
    finished = QtSignal(str)   # výsledný string

    def __init__(self, solver, g, live_mask):
        super().__init__()
        self.solver = solver
        self.g = g
        self.live_mask = live_mask

    def run(self):
        try:
            val = self.solver.solve(self.g, self.live_mask)
            if not val:
                self.finished.emit("Value: Null pointer returned")
                return
            zero_game = self.solver.game_zero()
            g_geq_0 = self.solver.game_geq(val, zero_game)
            zero_geq_g = self.solver.game_geq(zero_game, val)
            if g_geq_0 and not zero_geq_g:
                res_str = " Modrý/Left (G > 0)"
            elif not g_geq_0 and zero_geq_g:
                res_str = " Červený/Right (G < 0)"
            elif g_geq_0 and zero_geq_g:
                res_str = " 2. hráč/Second (G = 0)"
            else:
                res_str = " 1. hráč/First (G || 0)"

            self.result_ptr = val
            self.finished.emit(f"Wins: {res_str}")
        except Exception as e:
            self.finished.emit(f"Error ({e})")


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Hackenbush Builder & Player")

        # Load config once; individual settings read from hb_settings.load_config() where needed
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

        self.scene = GraphScene(self.solver, THEME.theme)
        self.scene.on_turn_changed = self._update_player_button
        self.scene.on_build_color_changed = self._update_color_button_ui


        # education
        self.history = hb_education.HistoryManager(self.scene)
        self.edu_manager = hb_education.EducationManager(self.scene)

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
        self.player_btn.setToolTip("Kliknutím přepneš hráče")
        self.player_btn.clicked.connect(self._manual_turn_toggle)

        self.edit_btn = QPushButton()
        self.edit_btn.setIcon(QIcon("figs/edit-mode.svg"))
        self.edit_btn.setIconSize(QSize(33, 33))
        self.edit_btn.setFixedSize(40, 40)
        self.edit_btn.setToolTip("Edit mode: Can remove any edge")
        self.edit_btn.clicked.connect(self.toggle_edit_mode)
        self.edit_btn.setStyleSheet(THEME.theme.edit_btn_style(False))

        self._update_player_button()

        # --- TLAČÍTKA PRO STAVĚNÍ ---
        self.color_btn = QPushButton("")
        self.color_btn.setFixedSize(40, 40)
        self.color_btn.clicked.connect(self.toggle_build_color)
        self._update_color_button_ui(self.scene.current_color)

        self.clear_btn = QPushButton("Clear")
        self.clear_btn.clicked.connect(self.scene.clear_graph)

        self.solve_btn = QPushButton("Solve")
        self.solve_btn.clicked.connect(self.solve_current)

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



        self.result_lbl = QLabel("Vyhrává: ")
        self.result_lbl.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)

        # --- HINTY a EDUCATION ---
        self.hint_btn = QPushButton("Hints: OFF")
        self.hint_btn.setStyleSheet(THEME.theme.hint_btn_style(False))
        self.hint_btn.clicked.connect(self.toggle_hint_mode)

        self.edu_btn = QPushButton()
        self.edu_btn.setIcon(QIcon("figs/edu-mode.svg"))
        self.edu_btn.setIconSize(QSize(28, 28))
        self.edu_btn.setFixedSize(40, 40)
        self.edu_btn.setToolTip("Education mode: Analysing CGT")
        self.edu_btn.clicked.connect(self.toggle_edu_mode)
        self.edu_btn.setStyleSheet(THEME.theme.edu_btn_style(False))

        # --- PLAY TLAČÍTKA ---
        icon_font = QFont()
        icon_font.setPointSize(THEME.theme.icon_btn_font_size)

        self.step_back_btn = QPushButton()
        self.step_back_btn.setIcon(QIcon("figs/history-left.svg"))
        self.step_back_btn.setIconSize(QSize(33, 33))
        self.step_back_btn.setFixedSize(40, 40)
        self.step_back_btn.setFont(icon_font)
        self.step_back_btn.clicked.connect(self.step_backward)

        self.step_fwd_btn = QPushButton()
        self.step_fwd_btn.setIcon(QIcon("figs/history-right.svg"))
        self.step_fwd_btn.setIconSize(QSize(33, 33))
        self.step_fwd_btn.setFixedSize(40, 40)
        self.step_fwd_btn.setFont(icon_font)
        self.step_fwd_btn.clicked.connect(self.step_forward)

        # --- HORNÍ LAYOUT ---
        top = QHBoxLayout()
        top.addWidget(self.playing_lbl)
        top.addWidget(self.player_btn)
        top.addWidget(self.edit_btn)
        top.addSpacing(15)
        top.addWidget(self.hint_btn)
        top.addWidget(self.clear_btn)
        top.addWidget(self.solve_btn)
        top.addWidget(self.result_lbl, 1)

        # education
        top.addWidget(self.step_back_btn)
        top.addWidget(self.step_fwd_btn)
        top.addWidget(self.edu_btn)

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

    # ── Settings ──────────────────────────────────────────────────────────────

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
            self.free_all()
            self.solver.initialize()

        self.stack.setCurrentIndex(0)
        self.view.setFocus()
        self.settings_btn.setStyleSheet((
            "QPushButton {{ background-color: #2a2a2a; color: #aaa; border: 1px solid #444;"
            " border-radius: 18px; font-size: {}px; padding: 0px; }}"
            "QPushButton:hover {{ background-color: #3a3a3a; color: white; }}"
        ).format(THEME.theme.icon_settings_font_size))

    # ── Theme ─────────────────────────────────────────────────────────────────

    def _on_theme_changed(self, theme) -> None:
        self.setStyleSheet(theme.stylesheet())
        self.view.setBackgroundBrush(QBrush(theme.game_bg_q()))
        self.scene.apply_theme(theme)
        self._update_player_button()
        self._update_color_button_ui(self.scene.current_color)
        self.hint_btn.setStyleSheet(theme.hint_btn_style(self.scene.hints_active))
        self.edu_btn.setStyleSheet(theme.edu_btn_style(self.edu_manager.active))
        self.edit_btn.setStyleSheet(theme.edit_btn_style(self.scene.edit_mode))

    def closeEvent(self, event):
        if self.solver:
            self.solver.free_all()
            print("C-Solver memory now freed")
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

        # Pokud už běží, ignoruj
        if self._solver_worker and self._solver_worker.isRunning():
            return

        live_mask = self.scene.compute_live_mask_all_edges()

        self._dot_count = 0
        self._dot_timer.start()
        self.result_lbl.setText("Solving.")

        self._solver_worker = SolverWorker(self.solver, self.scene.g, live_mask)
        self._solver_worker.finished.connect(self._on_solve_done)
        self._solver_worker.start()

    def _on_solve_done(self, result: str) -> None:
        self._dot_timer.stop()
        self.result_lbl.setText(result)
        if self._solver_worker and self._solver_worker.result_ptr:
            self._last_game_string = self.solver.get_game_value_string(
                self._solver_worker.result_ptr, 1
            )
        self._solver_worker = None

    def save_game(self) -> None:
        path, _ = QFileDialog.getSaveFileName(self, "Save Hackenbush", "", "Hackenbush (*.hbg.json);;JSON (*.json);;All Files (*)")
        if not path: return
        if not path.endswith(".json"): path += ".hbg.json"
        try:
            hb_io.save_to_file(self.scene, path)
            self.result_lbl.setText(f"Saved: {os.path.basename(path)}")
        except Exception as e:
            self.result_lbl.setText(f"Save error: {e}")

    def load_game(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Load Hackenbush", "", "Hackenbush (*.hbg.json *.json);;All Files (*)")
        if not path: return
        try:
            hb_io.load_from_file(self.scene, path)
            self._update_color_button_ui(self.scene.current_color)
            self.result_lbl.setText(f"Loaded: {os.path.basename(path)}")
            self.scene.update_auras()
        except Exception as e:
            self.result_lbl.setText(f"Load error: {e}")

    def toggle_hint_mode(self):
        is_active = self.scene.toggle_hints()
        self.hint_btn.setText("Hints: ON" if is_active else "Hints: OFF")
        self.hint_btn.setStyleSheet(THEME.theme.hint_btn_style(is_active))

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

    def toggle_edu_mode(self):
        is_active = self.edu_manager.toggle()
        self.edu_btn.setStyleSheet(THEME.theme.edu_btn_style(is_active))

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
        menu.addAction("Swap colors (−G)", self.negate_graph)
        menu.addAction("Clear cache", self.restart_cache)
        menu.addAction("Copy game result", self.copy_game_result)
        menu.addAction("Calculator", self.open_calculator)
        menu.addAction("Settings", self.open_settings)
        menu.exec(self.menu_btn.mapToGlobal(self.menu_btn.rect().bottomLeft()))

    def negate_graph(self):
        from PySide6.QtCore import Qt
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
