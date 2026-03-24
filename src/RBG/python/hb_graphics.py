from dataclasses import dataclass
from typing import List, Optional
import math

from PySide6.QtCore import Qt, QPointF, QTimer, QRectF
from PySide6.QtGui import QPen, QBrush, QColor, QPainterPath, QFont
from PySide6.QtWidgets import (
    QGraphicsEllipseItem,
    QGraphicsScene,
    QGraphicsPathItem,
    QGraphicsItem,
    QGraphicsTextItem,
    QGraphicsDropShadowEffect,
)

import hb_solver
import hb_settings
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
