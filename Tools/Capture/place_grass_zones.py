"""Zones de rencontres des hautes herbes (SCRUM-39) sur la map L_Kanto.

À lancer dans l'éditeur Unreal, map L_Kanto ouverte et C++ compilé (classe TallGrassZone) :
Outils > Exécuter un script Python... > choisir ce fichier.

Pose une zone invisible (TallGrassZone) sur chaque carré de hautes herbes du plan
(Tools/Blockout/kanto_v1_layout.py, TALL_GRASS), avec la table de DT_Encounters de la route
la plus proche et le taux de rencontre de Rouge Feu.

Ne touche ni au blockout ni au décor fait à la main : il efface puis repose seulement ses propres
zones, rangées dans le dossier « Kanto_Gameplay/Rencontres » de l'Outliner. Relançable sans risque.
Les zones posées à la main (glisser « Tall Grass Zone » depuis Placer des acteurs) ailleurs que
dans ce dossier ne sont jamais effacées.
"""

import importlib
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "Blockout"))
import kanto_v1_layout as layout  # noqa: E402

importlib.reload(layout)

MAP_NAME = "L_Kanto"
FOLDER = "Kanto_Gameplay/Rencontres"
ZONE_CLASS = "/Script/PokemonNewAge.TallGrassZone"
# Marge au-dessus et au-dessous du relief (m) : le sol retouché à la main bouge un peu
MARGIN = 2.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if MAP_NAME not in world.get_path_name():
        unreal.log_error(f"Ouvre d'abord la map {MAP_NAME}.")
        return
    zone_class = unreal.load_class(None, ZONE_CLASS)
    if zone_class is None:
        unreal.log_error("Classe TallGrassZone introuvable : compile d'abord le C++ (Unreal fermé), puis relance.")
        return
    landscapes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.Landscape)]
    if not landscapes:
        unreal.log_error("Pas de Landscape dans la map.")
        return

    origin = landscapes[0].get_actor_location()
    scale = landscapes[0].get_actor_scale3d()
    scale_xy = scale.x / 100.0 / layout.METERS_PER_PIXEL
    scale_z = scale.z / layout.LANDSCAPE_SCALE_Z

    removed = 0
    for actor in actors.get_all_level_actors():
        if str(actor.get_folder_path()) == FOLDER:
            actors.destroy_actor(actor)
            removed += 1

    placed = 0
    for index, (table, x, y, depth, width, rate) in enumerate(layout.TALL_GRASS):
        heights = [layout.height_at(x + dx * depth / 2, y + dy * width / 2) * scale_z
                   for dx in (-1, -0.5, 0, 0.5, 1) for dy in (-1, -0.5, 0, 0.5, 1)]
        low, high = min(heights) - MARGIN, max(heights) + MARGIN
        location = unreal.Vector(origin.x + x * 100 * scale_xy,
                                 origin.y + y * 100 * scale_xy,
                                 origin.z + (low + high) / 2 * 100)
        zone = actors.spawn_actor_from_class(zone_class, location, unreal.Rotator())
        zone.set_actor_label(f"Rencontres_{table}_{index}")
        zone.set_folder_path(FOLDER)
        # Toujours chargée : les rencontres marchent partout dans le monde ouvert
        zone.set_editor_property("is_spatially_loaded", False)
        zone.set_editor_property("encounter_table", table)
        zone.set_editor_property("encounter_rate", rate)
        box = zone.get_component_by_class(unreal.BoxComponent)
        box.set_box_extent(unreal.Vector(depth * 50 * scale_xy, width * 50 * scale_xy, (high - low) * 50), False)
        placed += 1

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"Hautes herbes : {placed} zones de rencontres posées ({removed} anciennes retirées).")


main()
