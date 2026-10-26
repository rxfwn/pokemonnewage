"""Blockout de Bourg Palette (SCRUM-23).

A lancer dans l'éditeur Unreal, avec la map L_PalletTown ouverte :
Outils > Exécuter un script Python... > choisir ce fichier.

Pose des formes simples (cubes, cylindres, cônes) aux bonnes dimensions pour
tester les distances et le parcours. Relançable sans risque : il efface d'abord
tout ce qu'il a posé (dossier « PalletTown_Blockout » de l'Outliner).

Repère : X = nord (vers la Route 1), Y = est, Z = haut. Cotes en mètres.
Plan détaillé : docs/BLOCKOUT_BOURG_PALETTE.md
"""

import math

import unreal

MAP_NAME = "L_PalletTown"
ROOT = "PalletTown_Blockout"
MATERIAL_DIR = "/Game/World/PalletTown/Blockout"

# Palette de docs/DIRECTION_ARTISTIQUE.md (Bourg Palette, après-midi)
COLORS = {
    "Grass": "#6DAE3E",
    "Dirt": "#A97B52",
    "Wall": "#C98E4F",
    "Roof": "#B5523A",
    "Fence": "#F4F1E8",
    "Lab": "#D9DDE3",
    "Door": "#4A3528",
    "Flowers": "#D9534F",
    "Trunk": "#6B4A2F",
    "Leaves": "#3F7A2E",
    "Water": "#3E7CB1",
}

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone")
BASE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")


def srgb_to_linear(hex_color):
    def channel(value):
        c = value / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    r, g, b = (int(hex_color[i:i + 2], 16) for i in (1, 3, 5))
    return unreal.LinearColor(channel(r), channel(g), channel(b), 1.0)


def get_material(name):
    """Crée (ou réutilise) MI_Blockout_<name>, une teinte unie de la palette."""
    asset_path = f"{MATERIAL_DIR}/MI_Blockout_{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        material = unreal.load_asset(asset_path)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            f"MI_Blockout_{name}", MATERIAL_DIR,
            unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(material, BASE_MATERIAL)
    if not unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            material, "Color", srgb_to_linear(COLORS[name])):
        unreal.log_warning(f"Couleur non appliquée sur {asset_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


MATERIALS = {}


def material(name):
    if name not in MATERIALS:
        MATERIALS[name] = get_material(name)
    return MATERIALS[name]


def spawn(mesh, label, folder, center, scale, color, rotation=None):
    """center en mètres (centre de la forme), scale en mètres (formes de base de 1 m)."""
    x, y, z = center
    actor = actors.spawn_actor_from_object(
        mesh, unreal.Vector(x * 100, y * 100, z * 100), rotation or unreal.Rotator())
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(label)
    actor.set_folder_path(f"{ROOT}/{folder}")
    actor.static_mesh_component.set_material(0, material(color))
    return actor


def box(label, folder, x, y, z_bottom, size, color):
    """Pavé posé sur z_bottom, size = (longueur X, largeur Y, hauteur) en mètres."""
    sx, sy, sz = size
    return spawn(CUBE, label, folder, (x, y, z_bottom + sz / 2), size, color)


def invisible_wall(label, x, y, size):
    wall = box(label, "Limits", x, y, 0, size, "Grass")
    wall.set_actor_hidden_in_game(True)
    return wall


def house(label, x, y, length_x, width_y, wall_height, door_side):
    """Maison à deux niveaux, toit à deux pans (faîtage nord-sud), porte côté chemin."""
    folder = f"Buildings/{label}"
    box(f"{label}_Walls", folder, x, y, 0, (length_x, width_y, wall_height), "Wall")
    # Toit : un cube tourné de 45° autour de X dont seule la moitié haute dépasse
    side = (width_y + 1.2) / math.sqrt(2)
    spawn(CUBE, f"{label}_Roof", folder, (x, y, wall_height),
          (length_x + 1.2, side, side), "Roof", unreal.Rotator(roll=45))
    door_y = y + door_side * (width_y / 2 + 0.1)
    box(f"{label}_Door", folder, x, door_y, 0, (1.2, 0.2, 2.2), "Door")


def tree(label, x, y):
    folder = "Trees"
    spawn(CYLINDER, f"{label}_Trunk", folder, (x, y, 1.5), (0.5, 0.5, 3), "Trunk")
    spawn(CONE, f"{label}_Leaves", folder, (x, y, 5.5), (3.5, 3.5, 5), "Leaves")


def clear_previous_blockout():
    for actor in actors.get_all_level_actors():
        if str(actor.get_folder_path()).startswith(ROOT):
            actors.destroy_actor(actor)
        elif isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() == "Floor":
            # sol par défaut du modèle de niveau « Basic »
            actors.destroy_actor(actor)


def build():
    # Terrain : de la plage (X = -42) à la sortie nord (X = 55)
    box("Ground", "Ground", 6.5, 0, -1, (97, 100, 1), "Grass")
    box("Sea", "Ground", -60, 0, -1.4, (36, 140, 1), "Water")

    # Chemin principal : du labo (X = -21) à la sortie vers la Route 1 (X = 55)
    box("Path_Main", "Paths", 17, 0, 0, (76, 4, 0.05), "Dirt")
    box("Path_PlayerHouse", "Paths", 24, -5.5, 0, (2, 7, 0.05), "Dirt")
    box("Path_RivalHouse", "Paths", 24, 5.5, 0, (2, 7, 0.05), "Dirt")

    # Maison du joueur (ouest) et du rival (est), portes tournées vers le chemin
    house("House_Player", 24, -14, 10, 9, 6, door_side=+1)
    house("House_Rival", 24, 14, 10, 9, 6, door_side=-1)

    # Jardins fleuris et barrières blanches le long du chemin
    for side, name in ((-1, "Player"), (+1, "Rival")):
        box(f"Flowers_{name}", "Gardens", 15, side * 7, 0, (6, 5, 0.3), "Flowers")
        box(f"Fence_{name}_South", "Gardens", 17, side * 3, 0, (11, 0.1, 1), "Fence")
        box(f"Fence_{name}_North", "Gardens", 28.75, side * 3, 0, (6.5, 0.1, 1), "Fence")

    # Laboratoire du Prof. Chen au bout du chemin, la mer derrière
    box("Lab_Walls", "Buildings/Lab", -28, 0, 0, (14, 20, 7), "Lab")
    box("Lab_Roof", "Buildings/Lab", -28, 0, 7, (15, 21, 0.5), "Lab")
    box("Lab_Door", "Buildings/Lab", -20.9, 0, 0, (0.2, 2.4, 2.6), "Door")
    spawn(CYLINDER, "Lab_Antenna", "Buildings/Lab", (-32, 7, 9.5), (0.3, 0.3, 4), "Lab")

    # Panneau de la ville à l'entrée nord
    box("Sign_Town_Post", "Props", 48, -3.5, 0, (0.15, 0.15, 1), "Trunk")
    box("Sign_Town_Board", "Props", 48, -3.5, 1, (0.15, 1.5, 0.8), "Trunk")

    # Arbres en bordure (trouée au nord pour la Route 1)
    index = 0
    for x in range(-38, 53, 7):
        for y in (-47, 47):
            index += 1
            tree(f"Tree_{index:02d}", x, y)
    for y in range(-40, 41, 7):
        if abs(y) > 6:
            index += 1
            tree(f"Tree_{index:02d}", 52, y)
    for x, y in ((34, -20), (36, 18), (8, -24), (6, 26), (-10, -18), (-12, 20)):
        index += 1
        tree(f"Tree_{index:02d}", x, y)

    # Murs invisibles : le joueur ne sort que par le nord
    invisible_wall("Wall_West", 6.5, -50, (97, 0.5, 5))
    invisible_wall("Wall_East", 6.5, 50, (97, 0.5, 5))
    invisible_wall("Wall_South", -42, 0, (0.5, 100, 5))
    invisible_wall("Wall_North_West", 55, -28, (0.5, 44, 5))
    invisible_wall("Wall_North_East", 55, 28, (0.5, 44, 5))


def place_player_start():
    starts = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    start = starts[0] if starts else actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector())
    # Devant la maison du joueur, face au labo (comme l'image d'ambiance)
    start.set_actor_location(unreal.Vector(2400, -300, 120), False, False)
    start.set_actor_rotation(unreal.Rotator(yaw=180), False)


def set_afternoon_sun():
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.DirectionalLight):
            # Après-midi : soleil à 40°, de trois quarts (docs/DIRECTION_ARTISTIQUE.md)
            actor.set_actor_rotation(unreal.Rotator(pitch=-40, yaw=135), False)


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if MAP_NAME not in world.get_path_name():
        unreal.log_error(f"Ouvre d'abord la map {MAP_NAME} : ce script ne touche qu'à elle.")
        return
    clear_previous_blockout()
    build()
    place_player_start()
    set_afternoon_sun()
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.log("Blockout de Bourg Palette posé et enregistré.")


main()
