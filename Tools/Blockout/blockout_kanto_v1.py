"""Blockout du monde ouvert : tout Kanto (SCRUM-62, puis blockout de tout Kanto).

À lancer dans l'éditeur Unreal, avec la map L_Kanto ouverte et le Landscape déjà importé
(voir docs/MONDE_OUVERT_V1.md) : Outils > Exécuter un script Python... > choisir ce fichier.

Pose sur le relief : les chemins, les bâtiments à leur vraie taille, les postes de passage,
les hautes herbes, les entrées de grottes, la mer, les panneaux de chaque lieu, le point de départ,
la lumière réaliste (ciel, soleil, nuages, brume) et un matériau simple pour le sol.
Pas d'arbres : ils rallongeaient le chargement pour rien, le décor viendra avec le biome.
Relançable sans risque : il efface d'abord tout ce qu'il a posé (dossier « Kanto_Blockout »).

Le plan vient de kanto_v1_layout.py (même dossier) : on modifie le plan là, pas ici.
"""

import importlib
import json
import math
import os
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
    "House": "#E9DFC9",
    "Roof": "#B5523A",
    "RoofRed": "#B5463A",
    "RoofBlue": "#3F6EA8",
    "RoofGreen": "#4F7F4A",
    "RoofGrey": "#7C8088",
    "RoofBrown": "#7A4E33",
    "Lab": "#D9DDE3",
    "Center": "#D9534F",
    "Mart": "#3E7CB1",
    "Gym": "#8C8C8C",
    "Gate": "#B9A88F",
    "Door": "#4A3528",
    "TallGrass": "#4E8F2A",
    "Leaves": "#2F5E24",
    "Cave": "#1C1916",
    "Water": "#2F6E9E",
    "Flowers": "#E58FB0",
    "Fence": "#F2F0EA",
    "Wood": "#8A6340",
    "Sand": "#D8C79A",
    "Stone": "#9C9890",
}

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
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
        self.scale_z = scale.z / layout.LANDSCAPE_SCALE_Z
        if abs(self.scale_xy - 1) > 0.01 or abs(self.scale_z - 1) > 0.01:
            unreal.log_warning(
                f"Échelle du Landscape inattendue ({scale.x}, {scale.y}, {scale.z}) : "
                f"il faut {layout.METERS_PER_PIXEL * 100:.0f}, {layout.METERS_PER_PIXEL * 100:.0f}, "
                f"{layout.LANDSCAPE_SCALE_Z:.0f}.")

    def point(self, x, y, z_offset=0.0):
        """Point au sol (x, y en mètres) + décalage vertical en mètres, en centimètres."""
        z = layout.height_at(x, y) * self.scale_z + z_offset
        return unreal.Vector(self.origin.x + x * 100 * self.scale_xy,
                             self.origin.y + y * 100 * self.scale_xy,
                             self.origin.z + z * 100)

    def highest(self, x, y, size_x, size_y):
        """Hauteur la plus haute sous une emprise, pour qu'un sol ne soit pas traversé."""
        samples = [layout.height_at(x + dx * size_x / 2, y + dy * size_y / 2)
                   for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
        return max(samples) * self.scale_z

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
            # Morceaux de 12 m au plus (moins d'acteurs), plus courts dans les pentes.
            climb = abs(layout.height_at(x1, y1) - layout.height_at(x0, y0))
            pieces = max(1, int(length // 12), int(climb / 0.4))
            yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
            for piece in range(pieces):
                t = (piece + 0.5) / pieces
                x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
                width = 3.0 if "Forêt" in name else 4.0
                box_on_ground(f"{name}_{index}_{piece}", "Chemins", x, y,
                              (length / pieces + 1.0, width, 0.6), "Dirt", sink=0.55, yaw=yaw)


def box_at(label, folder, x, y, bottom, size, color, collision=True):
    """Pavé dont le bas est à l'altitude « bottom » (mètres, comme GROUND.lowest)."""
    sx, sy, sz = size
    location = GROUND.point(x, y)
    location.z = GROUND.origin.z + (bottom + sz / 2) * 100
    return spawn(CUBE, label, folder, location, (sx, sy, sz), color, collision=collision)


def spawn_class(path, label, folder, location, rotation=None):
    """Acteur d'une classe C++ du jeu (porte, volume d'intérieur). None si le code n'est pas
    compilé : le blockout reste utilisable, sans portes animées."""
    cls = unreal.load_class(None, path)
    if cls is None:
        return None
    actor = actors.spawn_actor_from_class(cls, location, rotation or unreal.Rotator())
    actor.set_actor_label(label)
    actor.set_folder_path(f"{ROOT}/{folder}")
    actor.set_editor_property("is_spatially_loaded", False)
    return actor


def place_door_and_interior(name, folder, x, y, depth, width, floor, ceiling):
    """Porte (pivot au milieu du bas du passage, axe X vers le sud) et volume d'intérieur
    (caméra rapprochée). False si les classes C++ ne sont pas compilées."""
    t = layout.WALL_THICKNESS
    door_location = GROUND.point(x - depth / 2 + t / 2, y)
    door_location.z = GROUND.origin.z + floor * 100
    door = spawn_class("/Script/PokemonNewAge.Door", f"{name}_Porte", folder, door_location,
                       unreal.Rotator(yaw=180))
    if door is None:
        return False
    # La taille du passage par défaut (1,4 × 2,4 m) est celle du blockout : seul le verrou change.
    door.set_editor_property("locked", name in layout.LOCKED)
    for component in door.get_components_by_class(unreal.StaticMeshComponent):
        component.set_material(0, material("Door"))

    inside = GROUND.point(x, y)
    inside.z = GROUND.origin.z + (floor + ceiling) / 2 * 100
    volume = spawn_class("/Script/PokemonNewAge.InteriorVolume", f"{name}_Intérieur", folder, inside)
    if volume is not None:
        # Le volume est mis à l'échelle (la boîte de base fait 8 × 8 × 3 m).
        volume.set_actor_scale3d(unreal.Vector((depth - 2 * t) / 8.0, (width - 2 * t) / 8.0,
                                               (ceiling - floor) / 3.0))
    return True


def build_buildings():
    """Bâtiments creux (SCRUM-25) : murs, porte au sud, sol, plafond, toit et meubles.
    On y entre à pied, sans chargement."""
    t = layout.WALL_THICKNESS
    missing_code = False
    for zone, name, x, y, depth, width, height, wall, roof_color, style in layout.BUILDINGS:
        folder = f"Bâtiments/{zone}/{name}"
        base = GROUND.lowest(x, y, depth, width) - 0.5
        floor = GROUND.highest(x, y, depth, width) + 0.15
        top = floor + height
        ceiling = floor + min(layout.CEILINGS[style], height - 1.0)
        door_w, door_h = layout.DOOR_WIDTH, layout.DOOR_HEIGHT
        front, back = x - depth / 2 + t / 2, x + depth / 2 - t / 2

        # Murs : nord, ouest, est, et le mur sud en deux morceaux autour de la porte + linteau.
        box_at(f"{name}_Mur_nord", folder, back, y, base, (t, width, top - base), wall)
        for side, label in ((-1, "ouest"), (1, "est")):
            box_at(f"{name}_Mur_{label}", folder, x, y + side * (width / 2 - t / 2), base,
                   (depth, t, top - base), wall)
            piece = width / 2 - door_w / 2
            box_at(f"{name}_Mur_sud_{label}", folder, front, y + side * (door_w / 2 + piece / 2), base,
                   (t, piece, top - base), wall)
        box_at(f"{name}_Linteau", folder, front, y, floor + door_h, (t, door_w, top - floor - door_h), wall)
        # Seuil sous la porte (le sol peut être un peu plus haut que le terrain devant).
        box_at(f"{name}_Seuil", folder, front - 0.6, y, base, (1.5, door_w + 0.6, floor - base), "Stone")

        # Sol et plafond.
        box_at(f"{name}_Sol", folder, x, y, base, (depth - 2 * t, width - 2 * t, floor - base), "Wood")
        box_at(f"{name}_Plafond", folder, x, y, ceiling, (depth - 2 * t, width - 2 * t, 0.25), "House")

        # Toit.
        roof = GROUND.point(x, y)
        roof.z = GROUND.origin.z + top * 100
        if style == "pignon":
            # Toit à deux pans qui déborde : un cube tourné de 45° dont seule la moitié haute dépasse.
            side = (width + 1.6) / math.sqrt(2)
            spawn(CUBE, f"{name}_Toit", folder, roof, (depth + 1.0, side, side), roof_color,
                  unreal.Rotator(roll=45))
        else:
            # Toit plat qui déborde un peu.
            roof.z += 25
            spawn(CUBE, f"{name}_Toit", folder, roof, (depth + 1.0, width + 1.0, 0.5), roof_color)

        # Fenêtres de part et d'autre de la porte, comme dans le jeu d'origine.
        for side in (-1, 1):
            window = GROUND.point(x - depth / 2 - 0.05, y + side * width / 4)
            window.z = GROUND.origin.z + (floor + 1.9) * 100
            spawn(CUBE, f"{name}_Fenêtre{side:+d}", folder, window, (0.15, 2.0, 1.2), "Mart",
                  collision=False)

        # Meubles.
        for label, dx, dy, fx, fy, fz, color in layout.INTERIORS.get(name, []):
            box_at(f"{name}_{label}", folder, x + dx, y + dy, floor, (fx, fy, fz), color)

        # Porte animée (C++ ADoor) et volume d'intérieur (C++ AInteriorVolume). Une erreur ici
        # ne doit jamais arrêter le blockout : on la signale et on continue.
        try:
            missing_code |= not place_door_and_interior(name, folder, x, y, depth, width, floor, ceiling)
        except Exception as error:
            unreal.log_warning(f"{name} : porte ou intérieur non posé ({error}).")
    if missing_code:
        unreal.log_warning("Classes Door / InteriorVolume introuvables : compiler le code C++ "
                           "(PokemonNewAgeEditor Win64 Development Build) puis relancer le script. "
                           "Les bâtiments sont posés, mais sans portes.")


def build_decor():
    """Étangs, fleurs, barrières, panneaux, boîtes aux lettres et sable des villes."""
    for index, (zone, kind, x, y, depth, width) in enumerate(layout.DECOR):
        folder = f"Décor/{zone}"
        label = f"{kind}_{zone}_{index}"
        if kind == "Water":
            # L'étang est creusé dans le relief : une plaque d'eau et un muret de pierre.
            rim = 2.6
            # Niveau de l'eau : 40 cm sous le sol autour de l'étang (pas sous le fond creusé).
            ring = [layout.height_at(x + i * (depth / 2 + rim), y + j * (width / 2 + rim))
                    for i in (-1, 0, 1) for j in (-1, 0, 1) if i or j]
            level = min(ring) - 0.4
            water = GROUND.point(x, y)
            water.z = GROUND.origin.z + (level * GROUND.scale_z - 0.05) * 100
            spawn(CUBE, f"{label}_Eau", folder, water, (depth + 0.5, width + 0.5, 0.1), "Water",
                  collision=False)
            box_on_ground(f"{label}_Bord_sud", folder, x - depth / 2 - rim, y,
                          (0.8, width + 2 * rim + 0.8, 0.9), "Stone", sink=0.4)
            box_on_ground(f"{label}_Bord_nord", folder, x + depth / 2 + rim, y,
                          (0.8, width + 2 * rim + 0.8, 0.9), "Stone", sink=0.4)
            for side in (-1, 1):
                box_on_ground(f"{label}_Bord{side:+d}", folder, x + rim / 2, y + side * (width / 2 + rim),
                              (depth + rim, 0.8, 0.9), "Stone", sink=0.4)
        elif kind == "Flowers":
            box_on_ground(label, folder, x, y, (depth, width, 0.5), "Flowers", sink=0.3,
                          collision=False)
        elif kind == "Fence":
            box_on_ground(label, folder, x, y, (depth, width, 1.1), "Fence", sink=0.1)
        elif kind == "Sign":
            box_on_ground(f"{label}_Poteau", folder, x, y, (0.15, 0.15, 1.0), "Wood", sink=0.1)
            board = GROUND.point(x, y, 1.2)
            spawn(CUBE, label, folder, board, (depth, width, 0.7), "Wood")
        elif kind == "Mailbox":
            box_on_ground(label, folder, x, y, (depth, width, 1.2), "Fence", sink=0.1)
        elif kind == "Sand":
            box_on_ground(label, folder, x, y, (depth, width, 0.6), "Sand", sink=0.55,
                          collision=False)


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
    # Les zones de rencontres (SCRUM-39) sont posées à part : Tools/Capture/place_grass_zones.py
    for index, (zone, x, y, depth, width, _rate) in enumerate(layout.TALL_GRASS):
        # Herbe haute de 70 cm, traversable.
        box_on_ground(f"Herbes_{zone}_{index}", "Hautes herbes", x, y, (depth, width, 0.7),
                      "TallGrass", sink=0.4, collision=False)


def cave_spot(x, y, angle):
    """Pied de pente le plus proche du point de la carte FRLG : le porche s'y adosse,
    l'ouverture tournée vers le bas. Sur l'eau (Îles Écume), on cherche d'abord la terre."""
    directions = [i * 22.5 for i in range(16)]
    if layout.height_at(x, y) < layout.SEA_LEVEL + 1.5:
        for distance in range(5, 300, 5):
            land = [(x + math.cos(math.radians(a)) * distance, y + math.sin(math.radians(a)) * distance)
                    for a in directions]
            land = [p for p in land if layout.height_at(*p) > layout.SEA_LEVEL + 1.5]
            if land:
                x, y = land[0]
                break
    base = layout.height_at(x, y)
    best = None
    for a in directions:
        for distance in range(4, 124, 4):
            px = x + math.cos(math.radians(a)) * distance
            py = y + math.sin(math.radians(a)) * distance
            if layout.height_at(px, py) > base + 5:
                if best is None or distance < best[0]:
                    best = (distance, a)
                break
    if best is None:
        return x, y, angle
    distance, a = best
    foot = max(0, distance - 6)
    return (x + math.cos(math.radians(a)) * foot, y + math.sin(math.radians(a)) * foot,
            (a + 180) % 360)


def build_caves():
    """Entrées de grottes : un porche sombre (deux piliers, un linteau, un fond noir) posé
    au pied de la pente, l'ouverture tournée vers la route. L'intérieur viendra plus tard."""
    for name, column, row, angle in layout.CAVES:
        x, y, angle = cave_spot(*layout.from_frlg(column, row), angle)
        folder = f"Grottes/{name}"
        forward = (math.cos(math.radians(angle)), math.sin(math.radians(angle)))
        side = (-forward[1], forward[0])
        bottom = GROUND.lowest(x, y, 8, 8) - 0.5
        for sign in (-1, 1):
            px, py = x + side[0] * 3.5 * sign, y + side[1] * 3.5 * sign
            pillar = box_at(f"{name}_Pilier{sign:+d}", folder, px, py, bottom, (3, 2, 7), "Stone")
            pillar.set_actor_rotation(unreal.Rotator(yaw=angle), False)
        lintel = box_at(f"{name}_Linteau", folder, x, y, bottom + 6, (3, 9, 2), "Stone")
        lintel.set_actor_rotation(unreal.Rotator(yaw=angle), False)
        back = box_at(f"{name}_Fond", folder, x - forward[0] * 3, y - forward[1] * 3, bottom,
                      (4, 6, 6.5), "Cave", collision=True)
        back.set_actor_rotation(unreal.Rotator(yaw=angle), False)
    unreal.log(f"{len(layout.CAVES)} entrées de grottes posées.")


def build_sea():
    # Une grande plaque d'eau au niveau de la mer sous toute la carte, traversable
    # (la vraie eau viendra plus tard).
    half = layout.WORLD_SIZE / 2
    location = unreal.Vector(GROUND.origin.x + half * 100, GROUND.origin.y + half * 100,
                             GROUND.origin.z + (layout.SEA_LEVEL - 0.5) * 100)
    size = layout.WORLD_SIZE + 400
    spawn(CUBE, "Mer", "Mer", location, (size, size, 1), "Water", collision=False)
    # Murs invisibles au bord de l'eau, sur les routes (on ne peut pas encore nager).
    # Ailleurs, le relief bloque. Les cases voisines d'une même ligne forment un seul mur,
    # pour garder peu d'acteurs et un chargement rapide.
    step = 6
    count = 0
    x = 0.0
    while x < layout.WORLD_SIZE:
        run = None
        y = 0.0
        while y <= layout.WORLD_SIZE + step:
            ground = layout.height_at(x, y) if y <= layout.WORLD_SIZE else 99.0
            at_sea = (-1.5 < ground < layout.SEA_LEVEL + 0.3
                      and layout.corridor_depth(x, y) > -6)
            if at_sea and run is None:
                run = y
            elif not at_sea and run is not None:
                count += 1
                length = y - run
                location = GROUND.point(x, run + length / 2 - step / 2, 3)
                location.z = GROUND.origin.z + (layout.SEA_LEVEL + 3) * 100
                wall = spawn(CUBE, f"Limite_{count:04d}", "Limites", location,
                             (step + 1.5, length + 1.5, 10), "Water")
                wall.set_actor_hidden_in_game(True)
                run = None
            y += step
        x += step
    unreal.log(f"{count} murs invisibles au bord de l'eau.")
    build_lakes()


def build_lakes():
    """Lacs et rivières en hauteur (kanto_lacs.json) : une plaque d'eau à leur niveau."""
    with open(layout.LAKES_PATH, encoding="utf-8") as handle:
        lakes = json.load(handle)
    count = 0
    for lake in lakes:
        count += 1
        cx, cy = (lake["x0"] + lake["x1"]) / 2, (lake["y0"] + lake["y1"]) / 2
        location = GROUND.point(cx, cy)
        location.z = GROUND.origin.z + lake["niveau"] * GROUND.scale_z * 100
        spawn(CUBE, f"Lac_{count:02d}", "Lacs", location,
              (lake["x1"] - lake["x0"], lake["y1"] - lake["y0"], 0.1), "Water", collision=False)
    unreal.log(f"{count} lacs posés.")


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
        # On le refait à chaque lancement, pour que les réglages ci-dessous s'appliquent.
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
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
    mel.connect_material_expressions(constant(0.65, -1100, 300), "", flat, "B")
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

    with unreal.ScopedSlowTask(10, "Blockout de Kanto") as task:
        task.make_dialog(True)
        steps = [
            ("Nettoyage", clear_previous_blockout),
            ("Lumière", setup_lighting),
            ("Mer", build_sea),
            ("Chemins", build_paths),
            ("Bâtiments", build_buildings),
            ("Décor des villes", build_decor),
            ("Postes", build_gates),
            ("Hautes herbes", build_tall_grass),
            ("Grottes", build_caves),
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
    unreal.log("Blockout de Kanto posé et enregistré.")


main()
