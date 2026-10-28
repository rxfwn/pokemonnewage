"""Télécharge les modèles .glb des Pokémon depuis le dépôt public Pokemon-3D-api/assets.

Par défaut : les Pokémon jouables en V1 (bAvailableInV1 dans DT_Species), versions normale et
chromatique. Les fichiers sont rangés comme dans le dépôt : <sortie>/regular/1.glb, <sortie>/shiny/1.glb.

    python Tools/Pokemon/download_pokemon_glb.py <sortie>            Pokémon de la V1
    python Tools/Pokemon/download_pokemon_glb.py <sortie> --all      les 151
    python Tools/Pokemon/download_pokemon_glb.py <sortie> 25 133     numéros choisis

Ensuite : convert_pokemon_glb.py (Blender) pour les passer en FBX.
"""

import json
import os
import sys
import urllib.error
import urllib.request

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SPECIES_JSON = os.path.join(REPO_ROOT, "Data", "Pokemon", "DT_Species.json")
BASE_URL = "https://raw.githubusercontent.com/Pokemon-3D-api/assets/main/models/opt/{cat}/{n}.glb"
CATEGORIES = ("regular", "shiny")


def wanted_numbers(args):
    with open(SPECIES_JSON, encoding="utf-8") as f:
        species = json.load(f)
    if "--all" in args:
        return [s["NationalDex"] for s in species]
    numbers = [int(a) for a in args if a.isdigit()]
    return numbers or [s["NationalDex"] for s in species if s.get("bAvailableInV1")]


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    missing = []
    for n in wanted_numbers(sys.argv[2:]):
        for cat in CATEGORIES:
            os.makedirs(os.path.join(out, cat), exist_ok=True)
            dest = os.path.join(out, cat, f"{n}.glb")
            if os.path.exists(dest):
                continue
            try:
                with urllib.request.urlopen(BASE_URL.format(cat=cat, n=n)) as r, open(dest, "wb") as f:
                    f.write(r.read())
                print(f"{cat}/{n}.glb")
            except urllib.error.HTTPError as e:
                missing.append(f"{cat}/{n} ({e.code})")
    if missing:
        print("Absents du dépôt : " + ", ".join(missing))


main()
