"""Blockout du monde ouvert de la V1, de Bourg Palette à Argenta (SCRUM-62).

À lancer dans l'éditeur Unreal, avec la map L_Kanto ouverte et le Landscape déjà importé
(voir docs/MONDE_OUVERT_V1.md) : Outils > Exécuter un script Python... > choisir ce fichier.

Pose sur le relief : les chemins, les bâtiments à leur vraie taille, les postes de passage,
les hautes herbes, les arbres, la mer, les panneaux de chaque lieu, le point de départ,
la lumière réaliste (ciel, soleil, nuages, brume) et un matériau simple pour le sol.
Relançable sans risque : il efface d'abord tout ce qu'il a posé (dossier « Kanto_Blockout »).

Le plan vient de kanto_v1_layout.py (même dossier) : on modifie le plan là, pas ici.
"""

import importlib
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kanto_v1_layout as layout  # noqa: E402

importlib.reload(layout)

MAP_NAME = "L_Kanto"
ROOT = "Kanto_Blockout"
MATERIAL_DIR = "/Game/World/Kanto/Blockout"

# Palette de docs/DIRECTION_ARTISTIQUE.md
COLORS = {
    "Dirt": "#A97B52",
    "Wall": "#C98E4F",
    "Roof": "#B5523A",
    "Lab": "#D9DDE3",
    "Center": "#D9534F",
    "Mart": "#3E7CB1",
    "Gym": "#8C8C8C",
    "Gate": "#B9A88F",
    "Door": "#4A3528",
    "TallGrass": "#4E8F2A",
    "Trunk": "#6B4A2F",
    "Leaves": "#2F5E24",
    "Water": "#2F6E9E",
}

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone")
BASE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
mel = unreal.MaterialEditingLibrary


def srgb_to_linear(hex_color):
    def channel(value):
        c = value / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    r, g, b = (int(hex_color[i:i + 2], 16) for i in (1, 3, 5))
    return unreal.LinearColor(channel(r), channel(g), channel(b), 1.0)


MATERIALS = {}


def material(name):
    """MI_Blockout_<name> : une teinte unie de la palette (créée une seule fois)."""
    if name in MATERIALS:
        return MATERIALS[name]
    asset_path = f"{MATERIAL_DIR}/MI_Blockout_{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        instance = unreal.load_asset(asset_path)
    else:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            f"MI_Blockout_{name}", MATERIAL_DIR,
            unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(instance, BASE_MATERIAL)
    mel.set_material_instance_vector_parameter_value(instance, "Color", srgb_to_linear(COLORS[name]))
    unreal.EditorAssetLibrary.save_loaded_asset(instance)
    MATERIALS[name] = instance
    return instance


# --- Repère : mètres du plan -> centimètres d'Unreal ---------------------------------

class Ground:
    """Lit la position du Landscape pour que le plan tombe pile sur le relief importé."""

    def __init__(self, landscape):
        origin = landscape.get_actor_location()
        scale = landscape.get_actor_scale3d()
        self.origin = origin
        self.scale_xy = scale.x / 100.0 / layout.METERS_PER_PIXEL
        self.scale_z = scale.z / 100.0
        if abs(self.scale_xy - 1) > 0.01 or abs(self.scale_z - 1) > 0.01:
            unreal.log_warning(
                f"Échelle du Landscape inattendue ({scale.x}, {scale.y}, {scale.z}) : "
                f"il faut {layout.METERS_PER_PIXEL * 100:.0f}, {layout.METERS_PER_PIXEL * 100:.0f}, 100.")

    def point(self, x, y, z_offset=0.0):
        """Point au sol (x, y en mètres) + décalage vertical en mètres, en centimètres."""
        z = layout.height_at(x, y) * self.scale_z + z_offset
        return unreal.Vector(self.origin.x + x * 100 * self.scale_xy,
                             self.origin.y + y * 100 * self.scale_xy,
                             self.origin.z + z * 100)

    def lowest(self, x, y, size_x, size_y):
        """Hauteur la plus basse sous une emprise, pour qu'un bâtiment ne flotte pas."""
        samples = [layout.height_at(x + dx * size_x / 2, y + dy * size_y / 2)
                   for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
        return min(samples) * self.scale_z


GROUND = None


def spawn(mesh, label, folder, location, scale, color, rotation=None, collision=True):
    actor = actors.spawn_actor_from_object(mesh, location, rotation or unreal.Rotator())
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(label)
    actor.set_folder_path(f"{ROOT}/{folder}")
    # Toujours chargé : le blockout reste visible et le script peut tout effacer au relancement.
    actor.set_editor_property("is_spatially_loaded", False)
    component = actor.static_mesh_component
    component.set_material(0, material(color))
    if not collision:
        component.set_collision_profile_name("NoCollision")
    return actor


def box_on_ground(label, folder, x, y, size, color, sink=0.5, yaw=0.0, collision=True):
    """Pavé posé au sol : size = (profondeur x, largeur y, hauteur) en mètres.
    Il s'enfonce de « sink » mètres sous le point le plus bas pour ne jamais flotter."""
    sx, sy, sz = size
    base = GROUND.lowest(x, y, sx, sy) - sink
    location = GROUND.point(x, y)
    location.z = GROUND.origin.z + (base + (sz + sink) / 2) * 100
    return spawn(CUBE, label, folder, location, (sx, sy, sz + sink), color,
                 unreal.Rotator(yaw=yaw), collision)


# --- Éléments du plan -------------------------------------------------------------------

def build_paths():
    for name, points in layout.PATHS + [("Chemin Forêt de Jade", layout.FOREST_PATH)]:
        for index, ((x0, y0), (x1, y1)) in enumerate(zip(points, points[1:])):
            length = math.hypot(x1 - x0, y1 - y0)
            pieces = max(1, int(length // 6))
            yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
            for piece in range(pieces):
                t = (piece + 0.5) / pieces
                x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
                width = 3.0 if "Forêt" in name else 4.0
                box_on_ground(f"{name}_{index}_{piece}", "Chemins", x, y,
                              (length / pieces + 1.0, width, 0.6), "Dirt", sink=0.55, yaw=yaw)


def build_buildings():
    for zone, name, x, y, depth, width, height, color in layout.BUILDINGS:
        folder = f"Bâtiments/{zone}"
        box_on_ground(f"{name}", folder, x, y, (depth, width, height), color)
        roof = GROUND.point(x, y)
        roof.z = GROUND.origin.z + (GROUND.lowest(x, y, depth, width) + height) * 100
        if color == "Wall":
            # Maison : toit à deux pans, un cube tourné de 45° dont seule la moitié haute dépasse.
            side = (width + 1.2) / math.sqrt(2)
            spawn(CUBE, f"{name}_Toit", folder, roof, (depth - 0.04, side, side), "Roof",
                  unreal.Rotator(roll=45))
        else:
            # Grand bâtiment : toit plat qui déborde un peu.
            roof.z += 25
            spawn(CUBE, f"{name}_Toit", folder, roof, (depth + 1.0, width + 1.0, 0.5), "Roof")
        # Porte au sud, comme dans le jeu d'origine.
        door_x = x - depth / 2 - 0.1
        box_on_ground(f"{name}_Porte", folder, door_x, y, (0.3, 1.6, 2.4), "Door")


def build_gates():
    for name, x, y, axis in layout.GATES:
        folder = f"Postes/{name}"
        # Deux murs et un toit : on traverse le poste par un couloir de 4 m.
        if axis == "x":
            for side in (-1, 1):
                box_on_ground(f"{name}_Mur{side:+d}", folder, x, y + side * 6, (12, 8, 6), "Gate")
        else:
            for side in (-1, 1):
                box_on_ground(f"{name}_Mur{side:+d}", folder, x + side * 6, y, (8, 12, 6), "Gate")
        roof = GROUND.point(x, y)
        roof.z = GROUND.origin.z + (GROUND.lowest(x, y, 12, 12) + 6.2) * 100
        spawn(CUBE, f"{name}_Toit", folder, roof, (14, 14, 0.5), "Roof")
        if "fermé" in name:
            box_on_ground(f"{name}_Fermeture", folder, x, y, (12, 4, 6), "Gate")


def build_tall_grass():
    for index, (zone, x, y, depth, width) in enumerate(layout.TALL_GRASS):
        # Herbe haute de 70 cm, traversable.
        box_on_ground(f"Herbes_{zone}_{index}", "Hautes herbes", x, y, (depth, width, 0.7),
                      "TallGrass", sink=0.4, collision=False)


def near_path(x, y, margin):
    for _, points in layout.PATHS + [("forêt", layout.FOREST_PATH)]:
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            dx, dy = x1 - x0, y1 - y0
            t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy)))
            if math.hypot(x - (x0 + dx * t), y - (y0 + dy * t)) < margin:
                return True
    return False


def near_building(x, y, margin):
    for _, _, bx, by, depth, width, _, _ in layout.BUILDINGS:
        if abs(x - bx) < depth / 2 + margin and abs(y - by) < width / 2 + margin:
            return True
    for _, gx, gy, _ in layout.GATES:
        if abs(x - gx) < 8 + margin and abs(y - gy) < 8 + margin:
            return True
    return False


def tree(index, x, y, rng):
    height = rng.uniform(12, 20)
    width = height * rng.uniform(0.32, 0.42)
    base = GROUND.point(x, y)
    trunk = unreal.Vector(base.x, base.y, base.z + 150)
    spawn(CYLINDER, f"Arbre_{index:04d}_Tronc", "Arbres", trunk, (0.5, 0.5, 4.0), "Trunk")
    leaves = unreal.Vector(base.x, base.y, base.z + (2.5 + height / 2) * 100)
    spawn(CONE, f"Arbre_{index:04d}_Feuillage", "Arbres", leaves, (width, width, height), "Leaves")


def v1_bounds(margin):
    """Rectangle (x0, x1, y0, y1) en mètres qui contient tous les lieux de la V1."""
    us = [u + s * ru for _, u, _, ru, _ in layout.AREAS for s in (-1, 1)]
    vs = [v + s * rv for _, _, v, _, rv in layout.AREAS for s in (-1, 1)]
    for _, _, points in layout.ROUTES:
        us += [u for u, _ in points]
        vs += [v for _, v in points]
    xs = [layout.from_map(u, v)[0] for u in (min(us), max(us)) for v in (min(vs), max(vs))]
    ys = [layout.from_map(u, v)[1] for u in (min(us), max(us)) for v in (min(vs), max(vs))]
    return min(xs) - margin, max(xs) + margin, min(ys) - margin, max(ys) + margin


def build_trees():
    rng = random.Random(151)
    index = 0
    x0, x1, y0, y1 = v1_bounds(60)
    # Forêt de Jade : dense, sauf le long du chemin.
    step = 9
    x = x0
    while x < x1:
        y = y0
        while y < y1:
            px, py = x + rng.uniform(-3, 3), y + rng.uniform(-3, 3)
            if (layout.find_zone(px, py) == "Forêt de Jade"
                    and not near_path(px, py, 6) and not near_building(px, py, 4)):
                index += 1
                tree(index, px, py, rng)
            y += step
        x += step
    # Bordures : une ceinture d'arbres sur les talus autour des routes et des villes.
    step = 14
    x = x0
    while x < x1:
        y = y0
        while y < y1:
            px, py = x + rng.uniform(-5, 5), y + rng.uniform(-5, 5)
            distance, _ = layout.zone_distance(px, py)
            if 4 < distance < 45 and layout.height_at(px, py) > layout.SEA_LEVEL + 1:
                index += 1
                tree(index, px, py, rng)
            y += step
        x += step
    unreal.log(f"{index} arbres posés.")


def build_sea():
    # Une grande plaque d'eau au niveau de la mer sous toute la carte, traversable
    # (la vraie eau viendra plus tard).
    half = layout.WORLD_SIZE / 2
    location = unreal.Vector(GROUND.origin.x + half * 100, GROUND.origin.y + half * 100,
                             GROUND.origin.z + (layout.SEA_LEVEL - 0.5) * 100)
    size = layout.WORLD_SIZE + 400
    spawn(CUBE, "Mer", "Mer", location, (size, size, 1), "Water", collision=False)
    # Murs invisibles là où un lieu de la V1 touche l'eau : on ne peut pas encore nager.
    step = 6
    x0, x1, y0, y1 = v1_bounds(20)
    count = 0
    x = x0
    while x < x1:
        y = y0
        while y < y1:
            distance, _ = layout.zone_distance(x, y)
            if distance < 3 and layout.height_at(x, y) < layout.SEA_LEVEL + 1:
                count += 1
                location = GROUND.point(x, y)
                location.z = GROUND.origin.z + (layout.SEA_LEVEL + 2) * 100
                wall = spawn(CUBE, f"Mur_Eau_{count:04d}", "Limites", location,
                             (step, step, 6), "Water")
                wall.set_actor_hidden_in_game(True)
            y += step
        x += step
    unreal.log(f"{count} murs invisibles au bord de l'eau.")


def build_signs():
    """Nom de chaque lieu en grand à son entrée (deux faces, lisible des deux côtés)."""
    for name, x, y in layout.SIGNS:
        for yaw in (0, 180):
            sign = actors.spawn_actor_from_class(
                unreal.TextRenderActor, GROUND.point(x, y, 3.5), unreal.Rotator(yaw=yaw))
            sign.set_actor_label(f"Panneau_{name}_{yaw}")
            sign.set_folder_path(f"{ROOT}/Panneaux")
            sign.set_editor_property("is_spatially_loaded", False)
            text = sign.text_render
            text.set_text(name)
            text.set_world_size(120)
            text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
            text.set_text_render_color(unreal.Color(255, 255, 255, 255))


def place_player_start():
    starts = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    start = starts[0] if starts else actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector())
    x, y = layout.PLAYER_START
    start.set_actor_location(GROUND.point(x, y, 1.2), False, False)
    start.set_actor_rotation(unreal.Rotator(yaw=0), False)


def ensure_actor(cls, label):
    existing = [a for a in actors.get_all_level_actors() if isinstance(a, cls)]
    if existing:
        return existing[0]
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
    actor.set_actor_label(label)
    actor.set_folder_path("Lumière")
    return actor


def setup_lighting():
    """Ciel, soleil d'après-midi, nuages et brume : la base d'un rendu réaliste."""
    sun = ensure_actor(unreal.DirectionalLight, "Soleil")
    # Après-midi : soleil à 40°, de trois quarts (docs/DIRECTION_ARTISTIQUE.md)
    sun.set_actor_rotation(unreal.Rotator(pitch=-40, yaw=135), False)
    sun_light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_light.set_editor_property("atmosphere_sun_light", True)
    sun_light.set_editor_property("intensity", 10.0)
    ensure_actor(unreal.SkyAtmosphere, "Ciel")
    sky = ensure_actor(unreal.SkyLight, "Lumière du ciel")
    sky.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True)
    ensure_actor(unreal.VolumetricCloud, "Nuages")
    fog = ensure_actor(unreal.ExponentialHeightFog, "Brume")
    fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.01)


def landscape_material():
    """Matériau simple du sol : herbe, roche sur les pentes raides, sable près de la mer."""
    path = f"{MATERIAL_DIR}/M_Kanto_Sol"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_Kanto_Sol", MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)

    def color(hex_color, x, y):
        expression = node(unreal.MaterialExpressionConstant3Vector, x, y)
        expression.set_editor_property("constant", srgb_to_linear(hex_color))
        return expression

    def constant(value, x, y):
        expression = node(unreal.MaterialExpressionConstant, x, y)
        expression.set_editor_property("r", value)
        return expression

    grass, rock, sand = color("#5E8C3A", -900, -300), color("#7A7268", -900, -150), color("#CDBB8E", -900, 0)

    # Pente : composante verticale de la normale (1 = plat).
    normal = node(unreal.MaterialExpressionVertexNormalWS, -1300, 200)
    up = node(unreal.MaterialExpressionComponentMask, -1100, 200)
    up.set_editor_property("r", False)
    up.set_editor_property("g", False)
    up.set_editor_property("b", True)
    mel.connect_material_expressions(normal, "", up, "")
    flat = node(unreal.MaterialExpressionSubtract, -900, 200)
    mel.connect_material_expressions(up, "", flat, "A")
    mel.connect_material_expressions(constant(0.8, -1100, 300), "", flat, "B")
    flat_mul = node(unreal.MaterialExpressionMultiply, -750, 200)
    mel.connect_material_expressions(flat, "", flat_mul, "A")
    mel.connect_material_expressions(constant(10.0, -900, 320), "", flat_mul, "B")
    flat_alpha = node(unreal.MaterialExpressionSaturate, -600, 200)
    mel.connect_material_expressions(flat_mul, "", flat_alpha, "")
    slope = node(unreal.MaterialExpressionLinearInterpolate, -450, -200)
    mel.connect_material_expressions(rock, "", slope, "A")
    mel.connect_material_expressions(grass, "", slope, "B")
    mel.connect_material_expressions(flat_alpha, "", slope, "Alpha")

    # Altitude : sable sous 1,5 m.
    position = node(unreal.MaterialExpressionWorldPosition, -1300, 450)
    height = node(unreal.MaterialExpressionComponentMask, -1100, 450)
    height.set_editor_property("r", False)
    height.set_editor_property("g", False)
    height.set_editor_property("b", True)
    mel.connect_material_expressions(position, "", height, "")
    above = node(unreal.MaterialExpressionSubtract, -900, 450)
    mel.connect_material_expressions(height, "", above, "A")
    mel.connect_material_expressions(constant(GROUND.origin.z + 150, -1100, 550), "", above, "B")
    above_mul = node(unreal.MaterialExpressionMultiply, -750, 450)
    mel.connect_material_expressions(above, "", above_mul, "A")
    mel.connect_material_expressions(constant(0.01, -900, 570), "", above_mul, "B")
    sand_alpha = node(unreal.MaterialExpressionSaturate, -600, 450)
    mel.connect_material_expressions(above_mul, "", sand_alpha, "")
    final = node(unreal.MaterialExpressionLinearInterpolate, -250, 0)
    mel.connect_material_expressions(sand, "", final, "A")
    mel.connect_material_expressions(slope, "", final, "B")
    mel.connect_material_expressions(sand_alpha, "", final, "Alpha")

    mel.connect_material_property(final, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(constant(0.9, -250, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def clear_previous_blockout():
    for actor in actors.get_all_level_actors():
        if str(actor.get_folder_path()).startswith(ROOT):
            actors.destroy_actor(actor)


def main():
    global GROUND
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if MAP_NAME not in world.get_path_name():
        unreal.log_error(f"Ouvre d'abord la map {MAP_NAME} : ce script ne touche qu'à elle.")
        return
    landscapes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.Landscape)]
    if not landscapes:
        unreal.log_error("Pas de Landscape : importe d'abord kanto_v1_heightmap.png (docs/MONDE_OUVERT_V1.md).")
        return
    GROUND = Ground(landscapes[0])

    with unreal.ScopedSlowTask(9, "Blockout de Kanto (V1)") as task:
        task.make_dialog(True)
        steps = [
            ("Nettoyage", clear_previous_blockout),
            ("Lumière", setup_lighting),
            ("Mer", build_sea),
            ("Chemins", build_paths),
            ("Bâtiments", build_buildings),
            ("Postes", build_gates),
            ("Hautes herbes", build_tall_grass),
            ("Arbres", build_trees),
            ("Panneaux et départ", lambda: (build_signs(), place_player_start())),
        ]
        for label, step in steps:
            task.enter_progress_frame(1, label)
            step()

    try:
        landscapes[0].set_editor_property("landscape_material", landscape_material())
    except Exception as error:  # le matériau du sol n'est pas bloquant
        unreal.log_warning(f"Matériau du sol non appliqué ({error}) : choisir M_Kanto_Sol "
                           "dans Landscape Material, panneau Détails du Landscape.")

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("Blockout de Kanto (V1) posé et enregistré.")


main()
