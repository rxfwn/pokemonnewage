"""Allège en mémoire vidéo les textures des modèles Pokémon déjà importés (message « Video memory has been exhausted »).

Textures sans canal alpha (compression deux fois plus légère, même netteté), groupe « Character »,
streaming actif. Rien n'est réimporté ; Unreal recompresse les textures (une à deux minutes).

Dans l'éditeur : Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> choisir Tools/Pokemon/optimize_pokemon_textures.py
"""

import os
import runpy

runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "import_pokemon_models.py"),
               init_globals={"MODE": "textures"})
