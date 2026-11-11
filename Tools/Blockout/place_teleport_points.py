"""Points de téléportation de test : un TeleportPoint dans chaque ville de Kanto.

Outil de TEST uniquement : les commandes Teleport / Teleports ne marchent pas dans le jeu
final (build Shipping), et les points sont invisibles en jeu.

À lancer dans l'éditeur Unreal, map L_Kanto ouverte, projet compilé (la classe TeleportPoint
vient du C++) : Outils > Exécuter un script Python... (Tools > Execute Python Script...).

Relançable : un point qui existe déjà (même Point Name) est remis à sa place au lieu d'être
dupliqué. Les points posés à la main avec un autre nom ne sont pas touchés. Ils vont dans le
dossier « Tests/Teleport » de l'Outliner, que le script de blockout n'efface pas.

Les positions viennent de kanto_v1_layout.py (TELEPORT_POINTS). Pour déplacer un point,
le plus simple est de le bouger à la main dans l'éditeur et de ne plus relancer ce script.
"""

import importlib
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kanto_v1_layout as layout  # noqa: E402

importlib.reload(layout)

MAP_NAME = "L_Kanto"
FOLDER = "Tests/Teleport"
ABOVE_GROUND = 0.5  # mètres au-dessus du sol (la commande Teleport rajoute 1 m)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def ground_z(world, landscape, x_cm, y_cm, fallback_m):
    """Hauteur du sol réel (avec les retouches au sculpt) ; sinon celle de la heightmap."""
    start = unreal.Vector(x_cm, y_cm, landscape.get_actor_location().z + 200000)
    end = unreal.Vector(x_cm, y_cm, landscape.get_actor_location().z - 200000)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True)
    if hit:
        location = hit.to_tuple()[4]  # Location
        return location.z
    return landscape.get_actor_location().z + fallback_m * 100


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if MAP_NAME not in world.get_path_name():
        unreal.log_error(f"Ouvre d'abord la map {MAP_NAME}.")
        return
    if not hasattr(unreal, "TeleportPoint"):
        unreal.log_error("Classe TeleportPoint introuvable : recompile le projet (PR #28) puis relance.")
        return
    landscapes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.Landscape)]
    if not landscapes:
        unreal.log_error("Pas de Landscape dans la map.")
        return
    landscape = landscapes[0]
    origin = landscape.get_actor_location()
    scale = landscape.get_actor_scale3d()
    scale_xy = scale.x / 100.0 / layout.METERS_PER_PIXEL
    scale_z = scale.z / layout.LANDSCAPE_SCALE_Z

    existing = {}
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.TeleportPoint):
            existing[str(actor.get_editor_property("point_name"))] = actor

    created = moved = 0
    for name, town, x, y in layout.TELEPORT_POINTS:
        x_cm = origin.x + x * 100 * scale_xy
        y_cm = origin.y + y * 100 * scale_xy
        z_cm = ground_z(world, landscape, x_cm, y_cm, layout.height_at(x, y) * scale_z) + ABOVE_GROUND * 100
        location = unreal.Vector(x_cm, y_cm, z_cm)
        rotation = unreal.Rotator(yaw=0)  # regarde vers le nord (X)
        point = existing.get(name)
        if point:
            point.set_actor_location(location, False, False)
            point.set_actor_rotation(rotation, False)
            moved += 1
        else:
            point = actors.spawn_actor_from_class(unreal.TeleportPoint, location, rotation)
            point.set_editor_property("point_name", name)
            created += 1
        point.set_actor_label(f"TP_{name}")
        point.set_folder_path(FOLDER)
        point.set_editor_property("is_spatially_loaded", False)
        point.rerun_construction_scripts()  # met à jour le nom affiché au-dessus du point

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"Points de téléportation : {created} créés, {moved} remis en place. "
               "En jeu : console (²) > Teleports.")


main()
