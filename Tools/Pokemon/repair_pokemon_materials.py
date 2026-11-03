"""Rebranche les matériaux des modèles Pokémon déjà importés (Pokémon gris à damier, textures pourtant présentes).

Chaque emplacement de matériau des SK_/SM_ est relié à /Game/Pokemon/Models/<Espèce>/Materials/<nom de l'emplacement>.
Quelques secondes, rien n'est réimporté.

Dans l'éditeur : Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> choisir Tools/Pokemon/repair_pokemon_materials.py
"""

import os
import runpy

runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "import_pokemon_models.py"),
               init_globals={"MODE": "materials"})
