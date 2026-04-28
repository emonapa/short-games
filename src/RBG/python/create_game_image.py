import sys
import subprocess

from PySide6.QtWidgets import QApplication

import hb_solver
import hb_graphics
import hb_io
import sys
from PySide6.QtWidgets import QApplication

import hb_solver
import hb_graphics
import hb_education
import hb_io
from hb_theme import THEME

app = QApplication(sys.argv)

solver = hb_solver.HBSolver()
solver.initialize()

scene = hb_graphics.GraphScene(solver, THEME.theme)

edu_manager = hb_education.EducationManager(scene)
scene.edu_manager = edu_manager
scene.edu_manager.active = True
scene.edu_manager.update_overlay()

# -----------------------------------------------------------------------

hb_io.load_and_export_png(scene, "./theises/games/first_play.hbg.json", "./theises/first_play.png")
hb_io.load_and_export_png(scene, "./theises/games/NIM.hbg.json", "./theises/NIM.png")
hb_io.load_and_export_png(scene, "./theises/games/empty_game.hbg.json", "./theises/empty_game.png", fixedX=500, fixedY=450)

hb_io.load_and_export_png(scene, "./theises/games/down.hbg.json", "./theises/down.png", fixedX=500, fixedY=450)
hb_io.load_and_export_png(scene, "./theises/games/up.hbg.json", "./theises/up.png", fixedX=500, fixedY=450)
hb_io.load_and_export_png(scene, "./theises/games/star.hbg.json", "./theises/star.png", fixedX=500, fixedY=450)

hb_io.load_and_export_png(scene, "./theises/games/dominated.hbg.json", "./theises/dominated.png", fixedX=900, fixedY=570, include_bubbles=True)
hb_io.load_and_export_png(scene, "./theises/games/reverzible.hbg.json", "./theises/reverzible.png", fixedX=900, fixedY=570, include_bubbles=True)

hb_io.load_and_export_png(scene, "./theises/games/AB.hbg.json", "./theises/AB.png", fixedY=570)
hb_io.load_and_export_png(scene, "./theises/games/A.hbg.json", "./theises/A.png", fixedX=500, fixedY=570)
hb_io.load_and_export_png(scene, "./theises/games/B.hbg.json", "./theises/B.png", fixedX=500, fixedY=570)

# chapter fuzzy hodnoty
hb_io.load_and_export_png(scene, "./theises/games/star_minus_one.hbg.json", "./theises/star_minus_one.png", fixedX=500)
hb_io.load_and_export_png(scene, "./theises/games/star_minus_half.hbg.json", "./theises/star_minus_half.png", fixedX=500, finalY=450)
hb_io.load_and_export_png(scene, "./theises/games/star_minus_sixteenth.hbg.json", "./theises/star_minus_sixteenth.png", fixedX=500, finalY=450)

hb_io.load_and_export_png(scene, "./theises/games/star_plus_one.hbg.json", "./theises/star_plus_one.png", fixedX=500)
hb_io.load_and_export_png(scene, "./theises/games/star_plus_half.hbg.json", "./theises/star_plus_half.png", fixedX=500, finalY=450)
hb_io.load_and_export_png(scene, "./theises/games/star_plus_sixteenth.hbg.json", "./theises/star_plus_sixteenth.png", fixedX=500, finalY=450)
# ----------------------

hb_io.load_and_export_png(scene, "./theises/games/flower_one.hbg.json", "./theises/flower_one.png", fixedY=450)
hb_io.load_and_export_png(scene, "./theises/games/flower_two.hbg.json", "./theises/flower_two.png", fixedY=450)
