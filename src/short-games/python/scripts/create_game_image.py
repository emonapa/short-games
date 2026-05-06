import sys
import subprocess

from PySide6.QtWidgets import QApplication

from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parents[1]))

import hb_solver
import hb_graphics
import hb_io
import hb_education
from hb_theme import THEME

app = QApplication(sys.argv)

path_to_lib = "../../../../build/short-games/hackenbush/libhackenbush.so"
solver = hb_solver.HBSolver(path_to_lib)
solver.initialize()

scene = hb_graphics.GraphScene(solver, THEME.theme)

edu_manager = hb_education.EducationManager(scene)
scene.edu_manager = edu_manager
scene.edu_manager.active = True
scene.edu_manager.update_overlay()

# -----------------------------------------------------------------------

JSON_FOLDER = "./../theises/games/"
PNG_FOLDER = "./../theises/"

# upravit na spravnou slozku
hb_io.load_and_export_png(scene, JSON_FOLDER + "first_play.hbg.json", PNG_FOLDER + "first_play.png")
hb_io.load_and_export_png(scene, JSON_FOLDER + "NIM.hbg.json", PNG_FOLDER + "NIM.png")
hb_io.load_and_export_png(scene, JSON_FOLDER + "empty_game.hbg.json", PNG_FOLDER + "empty_game.png", fixedX=500, fixedY=450)

hb_io.load_and_export_png(scene, JSON_FOLDER + "down.hbg.json", PNG_FOLDER + "down.png", fixedX=500, fixedY=450)
hb_io.load_and_export_png(scene, JSON_FOLDER + "up.hbg.json", PNG_FOLDER + "up.png", fixedX=500, fixedY=450)
hb_io.load_and_export_png(scene, JSON_FOLDER + "star.hbg.json", PNG_FOLDER + "star.png", fixedX=500, fixedY=450)

hb_io.load_and_export_png(scene, JSON_FOLDER + "dominated.hbg.json", PNG_FOLDER + "dominated.png", fixedX=900, fixedY=570, include_bubbles=True)
hb_io.load_and_export_png(scene, JSON_FOLDER + "reverzible.hbg.json", PNG_FOLDER + "reverzible.png", fixedX=900, fixedY=570, include_bubbles=True)

hb_io.load_and_export_png(scene, JSON_FOLDER + "AB.hbg.json", PNG_FOLDER + "AB.png", fixedY=570)
hb_io.load_and_export_png(scene, JSON_FOLDER + "A.hbg.json", PNG_FOLDER + "A.png", fixedX=500, fixedY=570)
hb_io.load_and_export_png(scene, JSON_FOLDER + "B.hbg.json", PNG_FOLDER + "B.png", fixedX=500, fixedY=570)

# --chapter fuzzy hodnoty------------------------------------------------
hb_io.load_and_export_png(scene, JSON_FOLDER + "star_minus_one.hbg.json", PNG_FOLDER + "star_minus_one.png", fixedX=500)
hb_io.load_and_export_png(scene, JSON_FOLDER + "star_minus_half.hbg.json", PNG_FOLDER + "star_minus_half.png", fixedX=500, finalY=450)
hb_io.load_and_export_png(scene, JSON_FOLDER + "star_minus_sixteenth.hbg.json", PNG_FOLDER + "star_minus_sixteenth.png", fixedX=500, finalY=450)

hb_io.load_and_export_png(scene, JSON_FOLDER + "star_plus_one.hbg.json", PNG_FOLDER + "star_plus_one.png", fixedX=500)
hb_io.load_and_export_png(scene, JSON_FOLDER + "star_plus_half.hbg.json", PNG_FOLDER + "star_plus_half.png", fixedX=500, finalY=450)
hb_io.load_and_export_png(scene, JSON_FOLDER + "star_plus_sixteenth.hbg.json", PNG_FOLDER + "star_plus_sixteenth.png", fixedX=500, finalY=450)

hb_io.load_and_export_png(scene, JSON_FOLDER + "flower_one.hbg.json", PNG_FOLDER + "flower_one.png", fixedY=450)
hb_io.load_and_export_png(scene, JSON_FOLDER + "flower_two.hbg.json", PNG_FOLDER + "flower_two.png", fixedY=450)

hb_io.load_and_export_png(scene, JSON_FOLDER + "one.hbg.json", PNG_FOLDER + "one.png", fixedX=500, finalY=390)
hb_io.load_and_export_png(scene, JSON_FOLDER + "minus_one.hbg.json", PNG_FOLDER + "minus_one.png", fixedX=500, finalY=390)
hb_io.load_and_export_png(scene, JSON_FOLDER + "one_minus_one.hbg.json", PNG_FOLDER + "one_minus_one.png", fixedX=500, finalY=390)
# -----------------------------------------------------------------------

# --co je polovina tahu--------------------------------------------------
hb_io.load_and_export_png(scene, JSON_FOLDER + "half.hbg.json", PNG_FOLDER + "half.png", fixedX=400, fixedY=550, finalY=370)
hb_io.load_and_export_png(scene, JSON_FOLDER + "half_minus_one.hbg.json", PNG_FOLDER + "half_minus_one.png", fixedX=400, fixedY=550, finalY=370)
hb_io.load_and_export_png(scene, JSON_FOLDER + "half_half_minus_one.hbg.json", PNG_FOLDER + "half_half_minus_one.png", fixedX=400, fixedY=550, finalY=370)
hb_io.load_and_export_png(scene, JSON_FOLDER + "halfs_to_zero.hbg.json", PNG_FOLDER + "halfs_to_zero.png", fixedX=400, fixedY=550, finalY=370)
# -----------------------------------------------------------------------
