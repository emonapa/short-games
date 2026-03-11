import os
import sys
from dataclasses import dataclass
from typing import List, Optional

import ctypes
from ctypes import c_uint8, c_uint64, c_longlong, c_int, Structure

from PySide6.QtCore import Qt, QPointF
from PySide6.QtGui import QPen, QBrush
from PySide6.QtWidgets import (
    QApplication, QGraphicsEllipseItem, QGraphicsLineItem, QGraphicsScene,
    QGraphicsView, QHBoxLayout, QLabel, QMainWindow, QPushButton, QVBoxLayout, QWidget
)

# pro bazierovy krivky kdyz jde vic hran ze&do stejnych vrcholu
import math
from PySide6.QtGui import QPainterPath
from PySide6.QtWidgets import QGraphicsPathItem
from PySide6.QtWidgets import QGraphicsItem

MAX_VERTICES = 32
MAX_EDGES = 64

EDGE_BLUE = 0
EDGE_RED = 1


# -------------------- C types (1:1 s tvym C kodem) --------------------

class CEdge(Structure):
    _fields_ = [
        ("u", c_uint8),
        ("v", c_uint8),
        ("color", c_int),  # enum EdgeColor
    ]


class CBaseGraph(Structure):
    _fields_ = [
        ("num_vertices", c_uint8),
        ("num_edges", c_uint8),
        ("edges", CEdge * MAX_EDGES),
    ]


class CDyadic(Structure):
    _fields_ = [
        ("num", c_longlong),
        ("exp", c_int),
    ]


def load_hb_library() -> ctypes.CDLL:
    # default: build/libhb.so vedle Makefile
    here = os.path.abspath(os.path.dirname(__file__))
    lib_path = os.path.join(here, "../../../build/", "libhb.so")
    if not os.path.exists(lib_path):
        raise FileNotFoundError(
            f"Chybi {lib_path}. Udelej: make lib"
        )
    lib = ctypes.CDLL(lib_path)

    # void solver_initialize(const BaseGraph *g);
    lib.solver_initialize.argtypes = [ctypes.POINTER(CBaseGraph)]
    lib.solver_initialize.restype = None

    # Dyadic solver_exact_solve(const BaseGraph *g, uint64_t live_mask);
    lib.solver_exact_solve.argtypes = [ctypes.POINTER(CBaseGraph), c_uint64]
    lib.solver_exact_solve.restype = CDyadic

    return lib


# -------------------- GUI model --------------------

@dataclass
class EdgeItem:
    u: int
    v: int
    color: int
    item: QGraphicsItem


class GraphScene(QGraphicsScene):
    def __init__(self) -> None:
        super().__init__()

        self.setSceneRect(0, 0, 980, 640)

        self.ground_y = 560.0
        self.ground_margin = 40.0
        self.ground_radius = 9

        # klikaci radius na vrcholy
        self.hit_radius = 14.0
        # slouceni "kliknuti skoro na stejny bod" pro nove vrcholy
        self.merge_radius = 8.0

        # C graph 1:1
        self.g = CBaseGraph()
        self.g.num_vertices = 0
        self.g.num_edges = 0

        # pozice vrcholu jen pro kresleni (C je nema)
        self.vertex_pos: List[QPointF] = [QPointF(0, 0) for _ in range(MAX_VERTICES)]
        self.vertex_items: List[Optional[QGraphicsEllipseItem]] = [None for _ in range(MAX_VERTICES)]
        self.edge_items: List[EdgeItem] = []
        self.parallel_count = {}  # key = (min(u,v), max(u,v)) -> kolik hran uz mezi nima je

        self.current_color = EDGE_BLUE
        self.pending_u: Optional[int] = None
        self.pending_marker = None

        self._draw_ground()
        self._init_ground_vertex()

    def _draw_ground(self) -> None:
        x0 = self.sceneRect().left() + self.ground_margin
        x1 = self.sceneRect().right() - self.ground_margin
        self.addLine(x0, self.ground_y, x1, self.ground_y, QPen(Qt.black, self.ground_radius))

    def _init_ground_vertex(self) -> None:
        # vertex 0 = zem
        self.g.num_vertices = 1
        # doprostred
        cx = (self.sceneRect().left() + self.sceneRect().right()) * 0.5
        self.vertex_pos[0] = QPointF(cx, self.ground_y)
        self._render_vertex(0, is_ground=True)

    def set_current_color(self, color: int) -> None:
        self.current_color = color

    def clear_graph(self) -> None:
        # reset scene, znovu nakresli zem a ground vertex
        self.clear()
        self.g.num_vertices = 0
        self.g.num_edges = 0
        self.edge_items.clear()
        self.parallel_count.clear()
        self.vertex_items = [None for _ in range(MAX_VERTICES)]
        self.vertex_pos = [QPointF(0, 0) for _ in range(MAX_VERTICES)]
        self.pending_u = None
        self.pending_marker = None

        self._draw_ground()
        self._init_ground_vertex()

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.RightButton:
            self.cancel_pending()
            event.accept()
            return

        if event.button() != Qt.LeftButton:
            super().mousePressEvent(event)
            return

        pos = event.scenePos()

        # nechceme vrcholy pod zemi
        if (pos.y() - (self.ground_y + self.ground_radius * 0.8) >= 0):
            print("Jsem pod zemi :(")
            super().mousePressEvent(event)
            return

        v_hit = self._find_vertex_hit(pos)


        if self.pending_u is None:
            # start musi byt zem nebo existujici vrchol
            u = self._pick_start_vertex(pos, v_hit)
            if u is not None:
                self.pending_u = u
                self._update_pending_marker()

            event.accept()
            return

        # mame start, vyber konec
        v = self._pick_end_vertex(pos, v_hit)
        if v is not None:
            u = self.pending_u
            if u != v:
                if self._add_edge(u, v, self.current_color):
                    # po pridani hrany automaticky pokracuj z koncoveho vrcholu
                    self.pending_u = v
                else:
                    # kdyz selze pridani (limity), ukoncime pending
                    self.pending_u = None
                self._update_pending_marker()
            else:
                # kliknul na stejny vrchol, nechame pending jak je
                pass

        event.accept()

    def keyPressEvent(self, event) -> None:
        if event.key() == Qt.Key_Escape:
            self.cancel_pending()
            event.accept()
            return
        super().keyPressEvent(event)


    # --------- vertex picking rules ---------

    def _is_on_ground(self, pos: QPointF) -> bool:
        return abs(pos.y() - self.ground_y) <= 10.0

    def _pick_start_vertex(self, pos: QPointF, v_hit: Optional[int]) -> Optional[int]:
        if v_hit is not None:
            return v_hit
        if self._is_on_ground(pos):
            return 0  # zem je vzdy vertex 0
        return None

    def _pick_end_vertex(self, pos: QPointF, v_hit: Optional[int]) -> Optional[int]:
        if v_hit is not None:
            return v_hit
        # konec muze byt kdekoliv -> vytvorime novy vrchol (nebo sloucime)
        return self._get_or_create_vertex(pos)

    # --------- vertex management ---------

    def _find_vertex_hit(self, pos: QPointF) -> Optional[int]:
        r2 = self.hit_radius * self.hit_radius
        n = int(self.g.num_vertices)
        for i in range(n):
            p = self.vertex_pos[i]
            dx = p.x() - pos.x()
            dy = p.y() - pos.y()
            if dx * dx + dy * dy <= r2:
                return i
        return None

    def _find_vertex_near(self, pos: QPointF) -> Optional[int]:
        r2 = self.merge_radius * self.merge_radius
        n = int(self.g.num_vertices)
        for i in range(n):
            p = self.vertex_pos[i]
            dx = p.x() - pos.x()
            dy = p.y() - pos.y()
            if dx * dx + dy * dy <= r2:
                return i
        return None

    def _get_or_create_vertex(self, pos: QPointF) -> Optional[int]:
        near = self._find_vertex_near(pos)
        if near is not None:
            return near

        if int(self.g.num_vertices) >= MAX_VERTICES:
            return None

        idx = int(self.g.num_vertices)
        self.vertex_pos[idx] = QPointF(pos.x(), pos.y())
        self.g.num_vertices = c_uint8(idx + 1)
        self._render_vertex(idx, is_ground=False)
        return idx

    def _render_vertex(self, idx: int, is_ground: bool) -> None:
        p = self.vertex_pos[idx]
        r = self.hit_radius

        item = QGraphicsEllipseItem(p.x() - r, p.y() - r, 2 * r, 2 * r)
        item.setPen(QPen(Qt.black, 2))
        item.setBrush(QBrush(Qt.red if is_ground else Qt.white))
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
        r = self.hit_radius + 6.0

        if self.pending_marker is None:
            self.pending_marker = QGraphicsEllipseItem(p.x() - r, p.y() - r, 2*r, 2*r)
            self.pending_marker.setPen(QPen(Qt.darkGray, 2, Qt.DashLine))
            self.pending_marker.setBrush(QBrush(Qt.NoBrush))
            self.pending_marker.setZValue(3.0)
            self.addItem(self.pending_marker)
        else:
            self.pending_marker.setRect(p.x() - r, p.y() - r, 2*r, 2*r)



    # --------- edge management ---------

    def _add_edge(self, u: int, v: int, color: int) -> bool:
        if int(self.g.num_edges) >= MAX_EDGES:
            return False

        eidx = int(self.g.num_edges)
        self.g.edges[eidx].u = c_uint8(u)
        self.g.edges[eidx].v = c_uint8(v)
        self.g.edges[eidx].color = int(color)
        self.g.num_edges = c_uint8(eidx + 1)

        pu = self.vertex_pos[u]
        pv = self.vertex_pos[v]

        key = (u, v) if u < v else (v, u)
        k = self.parallel_count.get(key, 0)
        self.parallel_count[key] = k + 1

        offset_index = self._parallel_offset_index(k)
        path = self._make_edge_path(pu, pv, offset_index)

        pen = QPen(Qt.blue if color == EDGE_BLUE else Qt.red, 4)
        pen.setCapStyle(Qt.RoundCap)
        pen.setJoinStyle(Qt.RoundJoin)

        item = QGraphicsPathItem(path)
        item.setPen(pen)
        item.setBrush(Qt.NoBrush)
        item.setZValue(1.0)
        self.addItem(item)

        self.edge_items.append(EdgeItem(u=u, v=v, color=color, item=item))
        return True


    def _parallel_offset_index(self, k: int) -> int:
        # k = 0,1,2,... poradi hrany mezi stejnymi vrcholy
        # 0 -> rovna
        # 1 -> +1
        # 2 -> -1
        # 3 -> +2
        # 4 -> -2 ...
        if k == 0:
            return 0
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

        # jednotkova normalova osa (kolmo na segment)

        # nasobeni sign(offset_index) je pouze trik na to aby se prohazovaly
        # osy bazierovy krivky prakticky (bod A -> bod B -> bod A -> bod B -> ...)
        sign = 1 if offset_index > 0 else -1
        nx = (-dy / length) * sign
        ny = (dx / length) * sign

        # jak moc se to ma vypouknout:
        # stejna velikost na obe strany, roste s |offset_index|
        base = 22.0
        step = 18.0
        bulge = (base + (abs(offset_index) - 1) * step) * (1 if offset_index > 0 else -1)
        print(f"Bulge: {bulge}, offset: {offset_index}")

        # kontrolni bod doprostred a posun po normale
        mx = (pu.x() + pv.x()) * 0.5
        my = (pu.y() + pv.y()) * 0.5
        print(f"mx: {mx}, my: {my}")
        print(f"nx: {nx}, ny: {ny}")
        cx = mx + nx * bulge
        cy = my + ny * bulge
        print(f"cx: {cx}, cy: {cy}")

        path.quadTo(QPointF(cx, cy), pv)
        return path



    # --------- C solver helper ---------

    def compute_live_mask_all_edges(self) -> int:
        m = 0
        n = int(self.g.num_edges)
        for i in range(n):
            m |= (1 << i)
        return m


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Hackenbush Builder")

        self.scene = GraphScene()

        self.view = QGraphicsView(self.scene)
        self.view.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view.setFocusPolicy(Qt.StrongFocus)
        self.view.setFocus()


        self.color_btn = QPushButton("Color: Blue")
        self.color_btn.clicked.connect(self.toggle_color)

        self.clear_btn = QPushButton("Clear")
        self.clear_btn.clicked.connect(self.scene.clear_graph)

        self.solve_btn = QPushButton("Solve")
        self.solve_btn.clicked.connect(self.solve_current)

        self.result_lbl = QLabel("Value: -")
        self.result_lbl.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)

        top = QHBoxLayout()
        top.addWidget(self.clear_btn)
        top.addWidget(self.solve_btn)
        top.addWidget(self.result_lbl, 1)
        top.addStretch(1)
        top.addWidget(self.color_btn)

        root = QVBoxLayout()
        root.addLayout(top)
        root.addWidget(self.view, 1)

        w = QWidget()
        w.setLayout(root)
        self.setCentralWidget(w)

        self.resize(1024, 768)

        self.lib = None

    def toggle_color(self) -> None:
        if self.scene.current_color == EDGE_BLUE:
            self.scene.set_current_color(EDGE_RED)
            self.color_btn.setText("Color: Red")
        else:
            self.scene.set_current_color(EDGE_BLUE)
            self.color_btn.setText("Color: Blue")

    def solve_current(self) -> None:
        try:
            if self.lib is None:
                self.lib = load_hb_library()

            live_mask = self.scene.compute_live_mask_all_edges()

            # init + solve
            self.lib.solver_initialize(ctypes.byref(self.scene.g))
            val = self.lib.solver_exact_solve(ctypes.byref(self.scene.g), c_uint64(live_mask))

            # jako double
            d = float(val.num) / float(1 << val.exp) if val.exp >= 0 else float(val.num) * float(1 << (-val.exp))
            self.result_lbl.setText(f"Value: {val.num} / 2^{val.exp} = {d:.6f}")

        except Exception as e:
            self.result_lbl.setText(f"Value: error ({e})")


def main() -> int:
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
