import os
import sys
import math
from dataclasses import dataclass
from typing import List, Optional

import ctypes
from ctypes import c_uint8, c_uint64, c_longlong, c_int, Structure

from PySide6.QtCore import Qt, QPointF, QTimer, QRectF
from PySide6.QtGui import QPen, QBrush, QColor, QPainterPath
from PySide6.QtWidgets import (
    QApplication, QGraphicsEllipseItem, QGraphicsScene,
    QGraphicsView, QHBoxLayout, QLabel, QMainWindow,
    QPushButton, QVBoxLayout, QWidget, QGraphicsPathItem, QGraphicsItem,
)

MAX_VERTICES = 32
MAX_EDGES    = 64
EDGE_BLUE    = 0
EDGE_RED     = 1

# ── C structures ──────────────────────────────────────────────────────────────

class CEdge(Structure):
    _fields_ = [("u", c_uint8), ("v", c_uint8), ("color", c_int)]

class CBaseGraph(Structure):
    _fields_ = [
        ("num_vertices", c_uint8),
        ("num_edges",    c_uint8),
        ("edges",        CEdge * MAX_EDGES),
    ]

class CDyadic(Structure):
    _fields_ = [("num", c_longlong), ("exp", c_int)]

# ── Library loader ────────────────────────────────────────────────────────────

def load_library() -> ctypes.CDLL:
    here     = os.path.abspath(os.path.dirname(__file__))
    lib_path = os.path.join(here, "../../../build/RB/libhb.so")
    if not os.path.exists(lib_path):
        alt = "./libhb.so"
        if os.path.exists(alt):
            lib_path = alt
        else:
            raise FileNotFoundError(f"Library not found: {lib_path}")

    lib = ctypes.CDLL(lib_path)
    lib.solver_initialize.argtypes = [ctypes.POINTER(CBaseGraph)]
    lib.solver_initialize.restype  = None
    lib.solve.argtypes = [ctypes.POINTER(CBaseGraph), c_uint64]
    lib.solve.restype  = CDyadic
    lib.cleanup_position.argtypes  = [ctypes.POINTER(CBaseGraph), c_uint64]
    lib.cleanup_position.restype   = c_uint64
    return lib

# ── Scene ─────────────────────────────────────────────────────────────────────

blue_color  = QColor(3, 105, 143)
red_color   = QColor(134, 0, 55)
bg_color    = QColor(47, 47, 47)
ground_color = QColor(77, 43, 140)
vertex_color = QColor(200, 200, 200)
vertex_ground_color = QColor(106, 50, 159)

@dataclass
class EdgeItem:
    u: int
    v: int
    color: int
    item: QGraphicsItem


class GraphScene(QGraphicsScene):
    def __init__(self) -> None:
        super().__init__()
        self.setSceneRect(0, 0, 1200, 900)

        self.ground_y    = 810.0
        self.hit_radius  = 12.0
        self.merge_radius = 22.0

        self.g = CBaseGraph()
        self.g.num_vertices = 0
        self.g.num_edges    = 0

        self.vertex_pos:   List[QPointF]                    = [QPointF(0, 0) for _ in range(MAX_VERTICES)]
        self.vertex_items: List[Optional[QGraphicsEllipseItem]] = [None] * MAX_VERTICES
        self.is_ground:    List[bool]                       = [False] * MAX_VERTICES
        self.edge_items:   List[EdgeItem]                   = []
        self.parallel_count = {}

        self.current_color = EDGE_BLUE
        self.pending_u: Optional[int] = None
        self.pending_marker = None

        self.player_to_move = EDGE_BLUE
        self.on_turn_changed = None

        self.is_slashing  = False
        self.slash_points = []
        self.slash_item   = None
        self.active_slashes = []

        self.fade_timer = QTimer()
        self.fade_timer.timeout.connect(self._process_fades)
        self.fade_timer.start(25)

        self.edit_mode = False
        self.on_build_color_changed = None
        self.trash_bin = []

        self._draw_ground()

    # ── Drawing ───────────────────────────────────────────────────────────────

    def _draw_ground(self) -> None:
        self.addRect(QRectF(-10000, self.ground_y, 20000, 2000),
                     QPen(Qt.NoPen), QBrush(ground_color))
        self.addLine(-10000, self.ground_y, 10000, self.ground_y,
                     QPen(QColor(41, 16, 45), 2))

    def _render_vertex(self, idx: int, is_ground: bool) -> None:
        p = self.vertex_pos[idx]
        r = self.hit_radius
        item = QGraphicsEllipseItem(p.x() - r, p.y() - r, 2*r, 2*r)
        item.setPen(QPen(QColor(255, 255, 255), 2))
        item.setBrush(QBrush(vertex_ground_color if is_ground else vertex_color))
        item.setZValue(2.0)
        self.addItem(item)
        self.vertex_items[idx] = item

    # ── Mouse ─────────────────────────────────────────────────────────────────

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.RightButton:
            self.cancel_pending(); event.accept(); return
        if event.button() != Qt.LeftButton:
            super().mousePressEvent(event); return

        pos = event.scenePos()
        if pos.y() >= self.ground_y - 20:
            pos.setY(self.ground_y)

        v_hit = self._find_vertex_hit(pos)
        on_ground = (pos.y() == self.ground_y)

        # start slash
        if self.pending_u is None and v_hit is None and not on_ground:
            self.is_slashing = True
            self.slash_points = [pos]
            self.slash_item = QGraphicsPathItem()
            self.slash_item.setPen(QPen(QColor(190, 190, 190, 120), 5,
                                        Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
            self.slash_item.setZValue(10.0)
            self.addItem(self.slash_item)
            event.accept(); return

        if self.pending_u is None:
            u = self._pick_start(pos, v_hit)
            if u is not None:
                self.pending_u = u
                self._update_pending_marker()
            event.accept(); return

        v = self._pick_end(pos, v_hit)
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
            event.accept(); return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        if self.is_slashing and self.slash_item:
            self.is_slashing = False
            cut = None
            for i, e in enumerate(self.edge_items):
                if self.slash_item.collidesWithItem(e.item):
                    if self._is_valid_cut(e.color):
                        cut = i; break
            if cut is not None:
                self._execute_cut(cut)
            self.active_slashes.append(self.slash_item)
            self.slash_item = None
            self.slash_points = []
            event.accept(); return
        super().mouseReleaseEvent(event)

    def wheelEvent(self, event) -> None:
        direction = 1 if event.delta() > 0 else -1
        colors = [EDGE_BLUE, EDGE_RED]
        new = colors[(colors.index(self.current_color) + direction) % 2]
        self.current_color = new
        if self.on_build_color_changed:
            self.on_build_color_changed(new)
        event.accept()

    # ── Game logic ────────────────────────────────────────────────────────────

    def _is_valid_cut(self, color: int) -> bool:
        if self.edit_mode:
            return True
        return color == self.player_to_move

    def _execute_cut(self, cut_idx: int, lib=None) -> None:
        self.hide_cut_pending()

        temp_mask = ((1 << self.g.num_edges) - 1) & ~(1 << cut_idx)
        if lib:
            new_mask = int(lib.cleanup_position(ctypes.byref(self.g), c_uint64(temp_mask)))
        else:
            new_mask = temp_mask

        to_remove = [i for i in range(self.g.num_edges) if not (new_mask & (1 << i))]
        to_remove.sort(reverse=True)

        items_later = []
        for idx in to_remove:
            item = self.edge_items[idx].item
            item.hide(); item.setEnabled(False)
            items_later.append(item)
            self.edge_items.pop(idx)
            for j in range(idx, self.g.num_edges - 1):
                self.g.edges[j] = self.g.edges[j+1]
            self.g.num_edges -= 1

        QTimer.singleShot(0, lambda: self._safe_remove(items_later))

        for j, e in enumerate(self.edge_items):
            e.item.edge_idx = j

        used = {e.u for e in self.edge_items} | {e.v for e in self.edge_items}
        for i in range(MAX_VERTICES):
            if i not in used and self.vertex_items[i]:
                self.removeItem(self.vertex_items[i])
                self.vertex_items[i] = None
                self.is_ground[i] = False

        if not self.edit_mode:
            self.player_to_move = EDGE_RED if self.player_to_move == EDGE_BLUE else EDGE_BLUE

        if self.on_turn_changed:
            self.on_turn_changed()

    def hide_cut_pending(self):
        pass  # placeholder for hint system if added later

    def _safe_remove(self, items):
        for item in items:
            if item.scene() == self:
                self.removeItem(item)
        self.trash_bin.extend(items)
        if len(self.trash_bin) > 50:
            self.trash_bin = self.trash_bin[-50:]

    def _process_fades(self):
        for item in list(self.active_slashes):
            op = item.opacity() - 0.15
            if op <= 0:
                self.removeItem(item); self.active_slashes.remove(item)
            else:
                item.setOpacity(op)

    # ── Vertex helpers ────────────────────────────────────────────────────────

    def _pick_start(self, pos, v_hit):
        if v_hit is not None: return v_hit
        if pos.y() == self.ground_y: return self._get_or_create_vertex(pos)
        return None

    def _pick_end(self, pos, v_hit):
        if v_hit is not None: return v_hit
        return self._get_or_create_vertex(pos)

    def _find_vertex_hit(self, pos):
        r2 = self.hit_radius ** 2
        for i in range(int(self.g.num_vertices)):
            if self.vertex_items[i] is None: continue
            p = self.vertex_pos[i]
            if (p.x()-pos.x())**2 + (p.y()-pos.y())**2 <= r2:
                return i
        return None

    def _get_or_create_vertex(self, pos):
        r2 = self.merge_radius ** 2
        for i in range(int(self.g.num_vertices)):
            if self.vertex_items[i] is None: continue
            p = self.vertex_pos[i]
            if (p.x()-pos.x())**2 + (p.y()-pos.y())**2 <= r2:
                return i
        if int(self.g.num_vertices) >= MAX_VERTICES:
            return None
        idx = int(self.g.num_vertices)
        self.vertex_pos[idx] = QPointF(pos.x(), pos.y())
        is_gnd = (pos.y() == self.ground_y)
        self.is_ground[idx] = is_gnd
        self.g.num_vertices = c_uint8(idx + 1)
        self._render_vertex(idx, is_gnd)
        return idx

    def cancel_pending(self):
        self.pending_u = None
        self._update_pending_marker()

    def _update_pending_marker(self):
        if self.pending_u is None:
            if self.pending_marker:
                self.removeItem(self.pending_marker)
                self.pending_marker = None
            return
        p = self.vertex_pos[self.pending_u]
        r = self.hit_radius + 4.0
        if self.pending_marker is None:
            self.pending_marker = QGraphicsEllipseItem(p.x()-r, p.y()-r, 2*r, 2*r)
            self.pending_marker.setPen(QPen(Qt.white, 2, Qt.DashLine))
            self.pending_marker.setZValue(3.0)
            self.addItem(self.pending_marker)
        else:
            self.pending_marker.setRect(p.x()-r, p.y()-r, 2*r, 2*r)

    # ── Edge helpers ──────────────────────────────────────────────────────────

    def _add_edge(self, u, v, color) -> bool:
        if int(self.g.num_edges) >= MAX_EDGES:
            return False
        eidx = int(self.g.num_edges)
        c_u = 0 if self.is_ground[u] else u
        c_v = 0 if self.is_ground[v] else v
        self.g.edges[eidx].u     = c_uint8(c_u)
        self.g.edges[eidx].v     = c_uint8(c_v)
        self.g.edges[eidx].color = color
        self.g.num_edges = c_uint8(eidx + 1)

        pu, pv = self.vertex_pos[u], self.vertex_pos[v]
        key = (min(u,v), max(u,v))
        k = self.parallel_count.get(key, 0)
        self.parallel_count[key] = k + 1

        path = self._make_edge_path(pu, pv, self._offset_index(k))
        pen  = QPen(blue_color if color == EDGE_BLUE else red_color, 8)
        pen.setCapStyle(Qt.RoundCap); pen.setJoinStyle(Qt.RoundJoin)

        item = QGraphicsPathItem(path)
        item.setPen(pen); item.setZValue(1.0)
        self.addItem(item)
        self.edge_items.append(EdgeItem(u=u, v=v, color=color, item=item))
        return True

    def _offset_index(self, k):
        if k == 0: return 0
        m = (k+1) // 2
        return m if k % 2 == 1 else -m

    def _make_edge_path(self, pu, pv, offset_index):
        path = QPainterPath()
        path.moveTo(pu)
        if offset_index == 0:
            path.lineTo(pv); return path
        dx, dy = pv.x()-pu.x(), pv.y()-pu.y()
        length = math.hypot(dx, dy)
        if length < 1e-6:
            path.lineTo(pv); return path
        sign = 1 if offset_index > 0 else -1
        nx, ny = (-dy/length)*sign, (dx/length)*sign
        bulge = (32.0 + (abs(offset_index)-1)*63.0) * sign
        cx = (pu.x()+pv.x())*0.5 + nx*bulge
        cy = (pu.y()+pv.y())*0.5 + ny*bulge
        path.quadTo(QPointF(cx, cy), pv)
        return path

    def compute_live_mask(self) -> int:
        return (1 << int(self.g.num_edges)) - 1

    def clear_graph(self):
        self.clear()
        self.g.num_vertices = 0
        self.g.num_edges    = 0
        self.edge_items.clear()
        self.parallel_count.clear()
        self.vertex_items = [None] * MAX_VERTICES
        self.vertex_pos   = [QPointF(0,0)] * MAX_VERTICES
        self.is_ground    = [False] * MAX_VERTICES
        self.pending_u    = None
        self.pending_marker = None
        self.active_slashes.clear()
        self._draw_ground()

    def set_current_color(self, color):
        self.current_color = color


# ── Main window ───────────────────────────────────────────────────────────────

class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Blue-Red Hackenbush")

        try:
            self.lib = load_library()
        except Exception as e:
            self.lib = None
            print(f"Warning: library not loaded: {e}")

        self.scene = GraphScene()
        self.scene.on_turn_changed       = self._update_player_btn
        self.scene.on_build_color_changed = self._update_build_btn

        self.view = QGraphicsView(self.scene)
        self.view.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view.setFocusPolicy(Qt.StrongFocus)
        self.view.setFocus()
        self.view.setStyleSheet("border: none;")
        self.view.setBackgroundBrush(QBrush(bg_color))

        self.setStyleSheet("""
            QMainWindow { background-color: rgb(31,31,31); }
            QLabel      { color: white; }
            QPushButton { background-color: #333; color: white;
                          border: 1px solid #555; padding: 6px; border-radius: 4px; }
            QPushButton:hover { background-color: #444; }
        """)

        # ── top bar ──────────────────────────────────────────────────────────
        self.playing_lbl = QLabel("Playing:")
        self.playing_lbl.setStyleSheet("font-weight: bold; font-size: 14px;")

        self.player_btn = QPushButton()
        self.player_btn.setFixedSize(40, 40)
        self.player_btn.setToolTip("Click to switch player")
        self.player_btn.clicked.connect(self._toggle_player)

        self.edit_btn = QPushButton("✏")
        self.edit_btn.setFixedSize(40, 40)
        self.edit_btn.setToolTip("Edit mode: remove any edge")
        self.edit_btn.clicked.connect(self._toggle_edit)

        self.building_lbl = QLabel("Building:")
        self.building_lbl.setStyleSheet("font-weight: bold; font-size: 14px;")

        self.build_btn = QPushButton()
        self.build_btn.setFixedSize(40, 40)
        self.build_btn.clicked.connect(self._cycle_build_color)

        self.clear_btn = QPushButton("Clear")
        self.clear_btn.clicked.connect(self.scene.clear_graph)

        self.solve_btn = QPushButton("Solve")
        self.solve_btn.clicked.connect(self._solve)
        self.solve_btn.setStyleSheet(
            "QPushButton { background-color: #408040; color: white; border: none;"
            " padding: 6px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #509050; }"
        )

        self.result_lbl = QLabel("Value: —")
        self.result_lbl.setStyleSheet("font-size: 14px;")

        top = QHBoxLayout()
        top.addWidget(self.playing_lbl)
        top.addWidget(self.player_btn)
        top.addWidget(self.edit_btn)
        top.addSpacing(20)
        top.addWidget(self.solve_btn)
        top.addWidget(self.result_lbl, 1)
        top.addStretch(1)
        top.addWidget(self.building_lbl)
        top.addWidget(self.build_btn)
        top.addSpacing(10)
        top.addWidget(self.clear_btn)

        root = QVBoxLayout()
        root.addLayout(top)
        root.addWidget(self.view, 1)

        w = QWidget()
        w.setLayout(root)
        self.setCentralWidget(w)
        self.resize(1200, 900)

        self._update_player_btn()
        self._update_build_btn(self.scene.current_color)

    # ── Slots ─────────────────────────────────────────────────────────────────

    def _update_player_btn(self):
        if self.scene.edit_mode:
            self.player_btn.setStyleSheet(
                f"background-color: {QColor(0,112,60).name()};"
                " border-radius: 20px; border: 2px solid white;")
            return
        color = blue_color if self.scene.player_to_move == EDGE_BLUE else red_color
        self.player_btn.setStyleSheet(
            f"background-color: {color.name()}; border-radius: 20px; border: 2px solid white;")

    def _toggle_player(self):
        if not self.scene.edit_mode:
            self.scene.player_to_move = (
                EDGE_RED if self.scene.player_to_move == EDGE_BLUE else EDGE_BLUE)
            self._update_player_btn()

    def _toggle_edit(self):
        self.scene.edit_mode = not self.scene.edit_mode
        if self.scene.edit_mode:
            self.edit_btn.setStyleSheet(
                "background-color: #555; color: white; border: 2px solid white; border-radius: 5px;")
            self.player_btn.setStyleSheet(
                f"background-color: {QColor(0,112,60).name()};"
                " border-radius: 20px; border: 2px solid white;")
        else:
            self.edit_btn.setStyleSheet(
                "background-color: #333; color: white; border: 1px solid #555; border-radius: 5px;")
            self._update_player_btn()

    def _update_build_btn(self, color):
        c = blue_color if color == EDGE_BLUE else red_color
        self.build_btn.setStyleSheet(
            f"background-color: {c.name()}; border-radius: 20px; border: 2px solid white;")

    def _cycle_build_color(self):
        new = EDGE_RED if self.scene.current_color == EDGE_BLUE else EDGE_BLUE
        self.scene.set_current_color(new)
        self._update_build_btn(new)

    def _solve(self):
        if not self.lib:
            self.result_lbl.setText("Value: solver not loaded")
            return
        try:
            live = self.scene.compute_live_mask()
            self.lib.solver_initialize(ctypes.byref(self.scene.g))
            val: CDyadic = self.lib.solve(ctypes.byref(self.scene.g), c_uint64(live))
            if val.exp >= 0:
                d = float(val.num) / float(1 << val.exp)
            else:
                d = float(val.num) * float(1 << (-val.exp))
            if val.exp == 0:
                self.result_lbl.setText(f"Value: {val.num}")
            else:
                self.result_lbl.setText(f"Value: {val.num} / 2^{val.exp}  =  {d:.4f}")
        except Exception as e:
            self.result_lbl.setText(f"Value: error ({e})")

    # Pass lib to execute_cut so cleanup_position works
    def _cut_with_lib(self, idx):
        self.scene._execute_cut(idx, lib=self.lib)


def main() -> int:
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec()

if __name__ == "__main__":
    raise SystemExit(main())
