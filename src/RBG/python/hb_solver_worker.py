from PySide6.QtCore import Qt, QPointF, QTimer, QRectF, QSize, QThread, Signal as QtSignal

import hb_solver

class SolverWorker(QThread):
    result_ready = QtSignal(str)

    def __init__(self, solver, g, live_mask):
        super().__init__()
        self.solver = solver
        self.g = g
        self.live_mask = live_mask
        self.result_ptr = None

    def run(self):
        try:
            val = self.solver.solve(self.g, self.live_mask)
            if not val:
                self.result_ready.emit("Value: Null pointer returned")
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
            self.result_ready.emit(f"Wins: {res_str}")
        except Exception as e:
            self.result_ready.emit(f"Error ({e})")
