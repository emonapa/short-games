import sys
import subprocess

from PySide6.QtWidgets import QApplication

import hb_solver
import hb_graphics
import hb_io
import hb_education
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

GAMES_JSON_FOLDER = "./../theises/games/"
GAMES_PNG_FOLDER = "./../theises/"

# upravit na spravnou slozku
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "first_play.hbg.json", GAMES_PNG_FOLDER + "first_play.png")
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "NIM.hbg.json", GAMES_PNG_FOLDER + "NIM.png")
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "empty_game.hbg.json", GAMES_PNG_FOLDER + "empty_game.png", fixedX=500, fixedY=450)

hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "down.hbg.json", GAMES_PNG_FOLDER + "down.png", fixedX=500, fixedY=450)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "up.hbg.json", GAMES_PNG_FOLDER + "up.png", fixedX=500, fixedY=450)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "star.hbg.json", GAMES_PNG_FOLDER + "star.png", fixedX=500, fixedY=450)

hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "dominated.hbg.json", GAMES_PNG_FOLDER + "dominated.png", fixedX=900, fixedY=570, include_bubbles=True)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "reverzible.hbg.json", GAMES_PNG_FOLDER + "reverzible.png", fixedX=900, fixedY=570, include_bubbles=True)

hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "AB.hbg.json", GAMES_PNG_FOLDER + "AB.png", fixedY=570)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "A.hbg.json", GAMES_PNG_FOLDER + "A.png", fixedX=500, fixedY=570)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "B.hbg.json", GAMES_PNG_FOLDER + "B.png", fixedX=500, fixedY=570)

# chapter fuzzy hodnoty
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "star_minus_one.hbg.json", GAMES_PNG_FOLDER + "star_minus_one.png", fixedX=500)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "star_minus_half.hbg.json", GAMES_PNG_FOLDER + "star_minus_half.png", fixedX=500, finalY=450)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "star_minus_sixteenth.hbg.json", GAMES_PNG_FOLDER + "star_minus_sixteenth.png", fixedX=500, finalY=450)

hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "star_plus_one.hbg.json", GAMES_PNG_FOLDER + "star_plus_one.png", fixedX=500)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "star_plus_half.hbg.json", GAMES_PNG_FOLDER + "star_plus_half.png", fixedX=500, finalY=450)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "star_plus_sixteenth.hbg.json", GAMES_PNG_FOLDER + "star_plus_sixteenth.png", fixedX=500, finalY=450)

hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "flower_one.hbg.json", GAMES_PNG_FOLDER + "flower_one.png", fixedY=450)
hb_io.load_and_export_png(scene, GAMES_JSON_FOLDER + "flower_two.hbg.json", GAMES_PNG_FOLDER + "flower_two.png", fixedY=450)

# -----------------------------------------------------------------------
