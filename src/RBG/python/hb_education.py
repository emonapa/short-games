from PySide6.QtWidgets import QGraphicsItem
from PySide6.QtGui import QPainter, QBrush, QPen, QColor, QFont, QFontMetrics
from PySide6.QtCore import Qt, QRectF, QPointF
import hb_solver

class EduBubble(QGraphicsItem):
    def __init__(self, text, is_dominated, is_reversible, parent=None):
        super().__init__(parent)
        self.text = text
        self.is_dominated = is_dominated
        self.is_reversible = is_reversible
        self.setZValue(50.0)

        # Dynamická šířka podle délky textu, výška fixní pro hezké kolečko
        font = QFont("Consolas", 9, QFont.Bold)
        metrics = QFontMetrics(font)
        text_width = metrics.horizontalAdvance(self.text)

        self.rect_height = 20
        self.rect_width = max(36, text_width + 16) # Minimálně kulaté

        self.offset_x = -self.rect_width / 2
        self.offset_y = -self.rect_height / 2

    def boundingRect(self):
        return QRectF(self.offset_x, self.offset_y, self.rect_width, self.rect_height)

    def paint(self, painter, option, widget):
        bg_color = QColor(220, 220, 220) if self.is_dominated else QColor(255, 255, 255)
        pen_color = QColor(200, 50, 50) if self.is_reversible else QColor(0, 0, 0)
        text_color = QColor(130, 130, 130) if self.is_dominated else (QColor(200, 50, 50) if self.is_reversible else QColor(0, 0, 0))

        painter.setBrush(QBrush(bg_color))
        painter.setPen(QPen(pen_color, 2))

        rect = self.boundingRect()
        painter.drawRoundedRect(rect, self.rect_height / 2, self.rect_height / 2) # Tvar pilulky/kolečka

        font = QFont("Consolas", 9, QFont.Bold)
        painter.setFont(font)
        painter.setPen(QPen(text_color))
        painter.drawText(rect, Qt.AlignCenter, self.text)


class HistoryManager:
    def __init__(self, scene):
        self.scene = scene
        self.undo_stack = []
        self.redo_stack = []

    def get_current_state(self):
        return {
            'player_to_move': self.scene.player_to_move,
            'vertices': [
                (
                    self.scene.vertex_pos[i].x(),
                    self.scene.vertex_pos[i].y(),
                    self.scene.is_ground[i],
                    self.scene.vertex_items[i] is not None # NOVÉ: Uložíme informaci, zda vrchol ještě žije!
                )
                for i in range(int(self.scene.g.num_vertices))
            ],
            'edges': [(e.u, e.v, e.color) for e in self.scene.edge_items]
        }

    def save_state(self):
        self.undo_stack.append(self.get_current_state())
        self.redo_stack.clear()

    def load_state(self, state):
        self.scene.clear_graph()
        self.scene.player_to_move = state['player_to_move']

        idx = 0
        for x, y, is_gnd, is_alive in state['vertices']:
            self.scene.vertex_pos[idx] = QPointF(x, y)
            self.scene.is_ground[idx] = is_gnd

            # Vykreslíme POUZE ty vrcholy, co přežily gravitaci
            if is_alive:
                self.scene._render_vertex(idx, is_ground=is_gnd)
            idx += 1

        self.scene.g.num_vertices = hb_solver.c_uint8(idx)

        for u, v, color in state['edges']:
            self.scene._add_edge(u, v, color)

        if self.scene.on_turn_changed:
            self.scene.on_turn_changed()

        self.scene.update_auras()

        window = self.scene.views()[0].window() if self.scene.views() else None
        if window and hasattr(window, 'edu_manager'):
            window.edu_manager.update_overlay()

    def undo(self):
        if not self.undo_stack: return False
        self.redo_stack.append(self.get_current_state())
        self.load_state(self.undo_stack.pop())
        return True

    def redo(self):
        if not self.redo_stack: return False
        self.undo_stack.append(self.get_current_state())
        self.load_state(self.redo_stack.pop())
        return True


class EducationManager:
    def __init__(self, scene):
        self.scene = scene
        self.active = False
        self.bubbles = []

    def toggle(self):
        self.active = not self.active
        self.update_overlay()
        return self.active

    def clear_bubbles(self):
        for b in self.bubbles:
            try:
                if b.scene() == self.scene:
                    self.scene.removeItem(b)
            except RuntimeError:
                pass
        self.bubbles.clear()

    def update_overlay(self):
        self.clear_bubbles()
        if not self.active or self.scene.edit_mode or self.scene.g.num_edges == 0:
            return

        solver = self.scene.solver
        g = self.scene.g
        live_mask = self.scene.compute_live_mask_all_edges()
        is_left = (self.scene.player_to_move == hb_solver.EDGE_BLUE)

        solver.initialize(None)
        root_game = solver.solve_with_components(g, live_mask)

        valid_moves = []
        for i, e in enumerate(self.scene.edge_items):
            if self.scene._is_valid_cut(e.color):
                valid_moves.append(i)

        move_data = {}
        for idx in valid_moves:
            temp_mask = live_mask & ~(1 << idx)
            mask_after_i = solver.cleanup_position(g, temp_mask)

            g_i = solver.solve_with_components(g, mask_after_i)
            move_data[idx] = { 'g_i': g_i, 'mask': mask_after_i, 'type': 'normal', 'ref_idx': None }

        # 1. REVERZIBILNÍ TAHY (Hledáme tu absolutně NEJHORŠÍ odpověď protihráče)
        opp_color = hb_solver.EDGE_RED if is_left else hb_solver.EDGE_BLUE
        for idx in valid_moves:
            g_i = move_data[idx]['g_i']
            mask_after_i = move_data[idx]['mask']

            best_rev_j = None
            best_rev_val = None

            for j in range(g.num_edges):
                if not (mask_after_i & (1 << j)): continue
                c = g.edges[j].color
                if c == opp_color or c == hb_solver.EDGE_GREEN:
                    temp_mask_j = mask_after_i & ~(1 << j)
                    mask_after_j = solver.cleanup_position(g, temp_mask_j)

                    g_ij = solver.solve_with_components(g, mask_after_j)
                    if not g_ij: continue

                    is_rev = False
                    # Správná CGT logika reverzibility:
                    # Modrého reverzuje Červený, pokud najde tah g_ij <= root_game
                    # Červeného reverzuje Modrý, pokud najde tah g_ij >= root_game
                    if is_left and solver.game_geq(root_game, g_ij):
                        is_rev = True
                    elif not is_left and solver.game_geq(g_ij, root_game):
                        is_rev = True

                    if is_rev:
                        if best_rev_j is None:
                            best_rev_j = j
                            best_rev_val = g_ij
                        else:
                            # Chceme pro nás to nejhorší (Tedy pro reverzujícího soupeře to nejlepší)
                            if is_left:
                                # Modrý hledá minimum g_ij (nejlepší pro Červeného)
                                if solver.game_geq(best_rev_val, g_ij):
                                    best_rev_j = j
                                    best_rev_val = g_ij
                            else:
                                # Červený hledá maximum g_ij (nejlepší pro Modrého)
                                if solver.game_geq(g_ij, best_rev_val):
                                    best_rev_j = j
                                    best_rev_val = g_ij

            if best_rev_j is not None:
                move_data[idx]['type'] = 'reversible'
                move_data[idx]['ref_idx'] = best_rev_j

        # 2. DOMINOVANÉ TAHY (Hledáme ten absolutně NEJLEPŠÍ tah, co ho dominuje)
        for idx in valid_moves:
            if move_data[idx]['type'] == 'reversible': continue
            g_i = move_data[idx]['g_i']

            best_dom_k = None
            best_dom_val = None

            for k in valid_moves:
                if k == idx: continue
                g_k = move_data[k]['g_i']

                is_dom = False
                if is_left:
                    if solver.game_geq(g_k, g_i) and not solver.game_geq(g_i, g_k):
                        is_dom = True
                else:
                    if solver.game_geq(g_i, g_k) and not solver.game_geq(g_k, g_i):
                        is_dom = True

                if is_dom:
                    if best_dom_k is None:
                        best_dom_k = k
                        best_dom_val = g_k
                    else:
                        # Hledáme náš absolutně nejlepší tah, který tuto variantu dominuje
                        if is_left:
                            # Modrý hledá maximum g_k
                            if solver.game_geq(g_k, best_dom_val):
                                best_dom_k = k
                                best_dom_val = g_k
                        else:
                            # Červený hledá minimum g_k
                            if solver.game_geq(best_dom_val, g_k):
                                best_dom_k = k
                                best_dom_val = g_k

            if best_dom_k is not None:
                move_data[idx]['type'] = 'dominated'
                move_data[idx]['ref_idx'] = best_dom_k

        # 3. Vykreslení aktivního hráče
        for idx in valid_moves:
            data = move_data[idx]
            text = f"{idx}"
            if data['type'] == 'dominated':
                text = f"{idx} D by {data['ref_idx']}"
            elif data['type'] == 'reversible':
                text = f"{idx} R by {data['ref_idx']}"

            bubble = EduBubble(text, data['type'] == 'dominated', data['type'] == 'reversible')

            e_item = self.scene.edge_items[idx]
            center_x = (self.scene.vertex_pos[e_item.u].x() + self.scene.vertex_pos[e_item.v].x()) / 2
            center_y = (self.scene.vertex_pos[e_item.u].y() + self.scene.vertex_pos[e_item.v].y()) / 2
            bubble.setPos(center_x, center_y)

            self.scene.addItem(bubble)
            self.bubbles.append(bubble)

        # 4. Obyčejné indexy pro hrany protihráče
        for idx in range(self.scene.g.num_edges):
            if idx not in valid_moves:
                bubble = EduBubble(f"{idx}", False, False)
                e_item = self.scene.edge_items[idx]
                center_x = (self.scene.vertex_pos[e_item.u].x() + self.scene.vertex_pos[e_item.v].x()) / 2
                center_y = (self.scene.vertex_pos[e_item.u].y() + self.scene.vertex_pos[e_item.v].y()) / 2
                bubble.setPos(center_x, center_y)

                self.scene.addItem(bubble)
                self.bubbles.append(bubble)
