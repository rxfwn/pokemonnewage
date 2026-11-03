"""Remplit seulement /Game/Pokemon/Models/DT_MorphPoints (transformation en points de lumière de l'évolution).

À utiliser quand les modèles sont déjà importés mais que l'évolution n'a pas la transformation en points
(par exemple si import_pokemon_models.py a tourné avant la compilation du C++). Quelques secondes.

Dans l'éditeur (C++ compilé) : Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> choisir Tools/Pokemon/import_morph_points.py
"""

import os
import runpy

runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "import_pokemon_models.py"),
               init_globals={"MORPH_ONLY": True})
