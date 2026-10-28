"""
Prépare le modèle 3D du Pokédex de L'amie (Pokedex.blend, export Sketchfab) pour le menu en jeu (SCRUM-64).

Lancer (sans ouvrir Blender) :
    blender --background --python Tools/Pokedex/prepare_pokedex_model.py -- <Pokedex.blend> <dossier_de_sortie> [--render]

Le modèle d'origine est fait de 5 gros morceaux (un par matériau Udim0001 à Udim0005). Le menu a besoin
de pièces séparées : chaque bouton s'enfonce seul, le volet tourne sur sa charnière, les écrans reçoivent
l'interface. Le script :
  1. découpe chaque morceau en éléments séparés et les regroupe en pièces nommées « Pokedex_<Pièce> »
     d'après leur position (table PIECES ci-dessous) ;
  2. place l'origine sur l'axe de la charnière, face avant vers -X dans Unreal (même repère que le C++) ;
  3. ramène les UV de chaque matériau dans la case 0-1 (le modèle utilisait des UDIM) ;
  4. enregistre les textures du .blend (couleur, ORM, normales, émissif) en PNG ;
  5. exporte Pokedex.fbx (+ Pokedex_prepare.blend et un aperçu avec --render).

Sortie : <dossier>/Pokedex.fbx, <dossier>/textures/T_Pokedex_<Udim>_<Carte>.png
"""

import math
import os
import sys

import bpy
import mathutils

# Repère du modèle d'origine (cm) : X vers la droite, Y vers l'arrière (face avant vers -Y), Z vers le haut.
HINGE_X = 0.24        # axe de la charnière (cylindre central)
AXIS_Y = -0.5         # axe de rotation du volet, un peu devant la face avant : fermé, il passe devant l'objectif
CENTER_Z = 0.78       # milieu de la hauteur de l'appareil

# Pièces reconnues : (pièce, matériau, centre X, centre Z, largeur X) en cm dans le repère d'origine.
# Les éléments qui ne sont dans aucune ligne vont au boîtier (à gauche de la charnière) ou au volet (à droite).
PIECES = [
    ("Lens", "Udim0003", -7.50, 7.10, 1.93),
    ("LensRing", "Udim0003", -7.50, 7.10, 2.24),
    ("LedRed", "Udim0003", -6.04, 7.77, 0.58),
    ("LedYellow", "Udim0003", -5.24, 7.77, 0.58),
    ("LedGreen", "Udim0003", -4.45, 7.77, 0.58),
    ("ScreenLeds", "Udim0003", -5.15, 4.31, 0.35),
    ("ScreenLeds", "Udim0003", -4.65, 4.31, 0.35),
    ("BackButton", "Udim0003", -6.68, -0.95, 0.74),      # petit bouton rouge dans le cadre de l'écran
    ("ConfirmButton", "Udim0003", -7.56, -3.36, 1.01),   # bouton rond noir
    ("HomeButton", "Udim0003", -5.75, -2.39, 1.59),      # languette jaune
    ("DexButton", "Udim0003", -3.59, -2.39, 1.59),       # languette bleue
    ("DPad", "Udim0003", -2.49, -4.55, 1.88),
    ("Screen", "Udim0005", -4.67, 1.88, 6.40),
    ("MiniScreen0", "Udim0005", -5.82, -5.03, 2.92),
    ("LidScreen", "Udim0005", 5.31, 2.82, 5.74),
    ("MiniScreen1", "Udim0005", 3.61, -3.57, 2.87),
    ("MiniScreen2", "Udim0005", 6.99, -3.57, 2.87),
    ("LidLeds", "Udim0003", 3.01, -1.07, 0.45),
    ("LidLeds", "Udim0003", 3.58, -1.07, 0.45),
    ("PrevButton", "Udim0003", 3.26, -1.98, 0.94),
    ("NextButton", "Udim0003", 4.21, -1.98, 0.94),
    ("LidButton", "Udim0003", 7.72, -1.91, 0.83),        # bouton rond jaune du volet
]
# Les 10 boutons du volet : 2 rangées de 5, numérotés de gauche à droite puis de haut en bas
MENU_ROWS = (0.63, -0.18)
MENU_COLUMNS = (2.59, 4.00, 5.42, 6.83, 8.24)
# Pièces du volet (tournent avec la charnière) : le reste est accroché au boîtier
LID_SIDE_MIN_X = 0.9
HINGE_BAND = 0.9

TOLERANCE = 0.35  # cm


def classify(material, center, size_x):
    cx, cz = center.x, center.z
    if material == "Udim0003":
        for row, z in enumerate(MENU_ROWS):
            for column, x in enumerate(MENU_COLUMNS):
                if abs(cx - x) < TOLERANCE and abs(cz - z) < TOLERANCE and size_x > 1.2:
                    return "MenuButton%d" % (row * len(MENU_COLUMNS) + column)
    best, best_score = None, None
    for name, mat, x, z, width in PIECES:
        distance = math.hypot(cx - x, cz - z)
        if mat != material or distance > TOLERANCE:
            continue
        score = distance + abs(size_x - width)
        if best_score is None or score < best_score:
            best, best_score = name, score
    if best:
        return best
    if abs(cx - HINGE_X) < 0.2 and size_x < 1.3:
        return "Hinge"
    return "Lid" if cx > LID_SIDE_MIN_X else "Body"


def world_bounds_cm(obj):
    corners = [obj.matrix_world @ mathutils.Vector(c) for c in obj.bound_box]
    lo = mathutils.Vector([min(v[i] for v in corners) for i in range(3)]) * 100.0
    hi = mathutils.Vector([max(v[i] for v in corners) for i in range(3)]) * 100.0
    return lo, hi


def find_input_image(material, socket_names):
    """Image branchée (directement ou via Separate Color / Normal Map) sur une entrée du Principled BSDF."""
    tree = material.node_tree
    bsdf = next((n for n in tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if not bsdf:
        return None
    for socket_name in socket_names:
        socket = bsdf.inputs.get(socket_name)
        if not socket or not socket.is_linked:
            continue
        node = socket.links[0].from_node
        while node and node.type != "TEX_IMAGE":
            linked = [i for i in node.inputs if i.is_linked]
            node = linked[0].links[0].from_node if linked else None
        if node and node.image:
            return node.image
    return None


def save_textures(out_dir):
    tex_dir = os.path.join(out_dir, "textures")
    os.makedirs(tex_dir, exist_ok=True)
    maps = {
        "BaseColor": ["Base Color"],
        "ORM": ["Roughness", "Metallic"],
        "Normal": ["Normal"],
        "Emissive": ["Emission Color"],
    }
    written = []
    for material in bpy.data.materials:
        if not material.name.startswith("Udim") or not material.use_nodes:
            continue
        for suffix, sockets in maps.items():
            image = find_input_image(material, sockets)
            if not image:
                continue
            path = os.path.join(tex_dir, "T_Pokedex_%s_%s.png" % (material.name, suffix))
            image.filepath_raw = path
            image.file_format = "PNG"
            image.save()
            written.append(path)
    print("Textures écrites :", len(written))
    return written


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(argv) < 2:
        raise SystemExit("Usage : blender --background --python prepare_pokedex_model.py -- Pokedex.blend sortie [--render]")
    source, out_dir = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    os.makedirs(out_dir, exist_ok=True)

    bpy.ops.wm.open_mainfile(filepath=source)
    save_textures(out_dir)

    for obj in list(bpy.data.objects):
        if obj.type in ("CAMERA", "LIGHT"):
            bpy.data.objects.remove(obj, do_unlink=True)

    # 1. Morceaux d'origine -> éléments séparés, en coordonnées du monde
    sources = [o for o in bpy.data.objects if o.type == "MESH"]
    for obj in sources:
        world = obj.matrix_world.copy()
        obj.parent = None
        obj.matrix_world = world
    bpy.ops.object.select_all(action="DESELECT")
    for obj in sources:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = sources[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for obj in list(bpy.data.objects):
        if obj.type == "EMPTY":
            bpy.data.objects.remove(obj, do_unlink=True)

    for obj in sources:
        material = obj.material_slots[0].material.name
        # UV : chaque matériau était dans sa case UDIM (1001 -> u 0-1, 1002 -> u 1-2...)
        tile = int(material[-1]) - 1
        for layer in obj.data.uv_layers:
            for loop in layer.data:
                loop.uv.x -= tile
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.mesh.separate(type="LOOSE")

    # 2. Regroupement en pièces
    groups = {}
    for obj in [o for o in bpy.data.objects if o.type == "MESH"]:
        material = obj.material_slots[0].material.name
        lo, hi = world_bounds_cm(obj)
        key = classify(material, (lo + hi) / 2.0, hi.x - lo.x)
        groups.setdefault(key, []).append(obj)

    # Repère final (celui de make_pokedex_model.py) : X Unreal = Y d'origine (vers l'arrière),
    # Y Unreal = X d'origine (vers la droite), origine sur l'axe de la charnière ; Blender écrit y -> -y.
    to_unreal = mathutils.Matrix((
        (0.0, 1.0, 0.0, -AXIS_Y / 100.0),
        (-1.0, 0.0, 0.0, HINGE_X / 100.0),
        (0.0, 0.0, 1.0, -CENTER_Z / 100.0),
        (0.0, 0.0, 0.0, 1.0),
    ))

    parts = []
    for key, objects in sorted(groups.items()):
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        if len(objects) > 1:
            bpy.ops.object.join()
        part = bpy.context.view_layer.objects.active
        part.name = "Pokedex_" + key
        part.data.name = "Pokedex_" + key
        part.matrix_world = to_unreal @ part.matrix_world
        parts.append(part)
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    for material in bpy.data.materials:
        if material.name.startswith("Udim"):
            material.name = "MAT_" + material.name

    print("Pièces :", ", ".join(sorted(p.name[8:] for p in parts)))
    expected = {"Body", "Lid", "Hinge", "Lens", "LensRing", "Screen", "LidScreen", "DPad"} | {"MenuButton%d" % i for i in range(10)}
    missing = expected - {p.name[8:] for p in parts}
    if missing:
        print("ATTENTION pièces manquantes :", ", ".join(sorted(missing)))

    fbx_path = os.path.join(out_dir, "Pokedex.fbx")
    bpy.ops.export_scene.fbx(
        filepath=fbx_path,
        use_selection=True,
        object_types={"MESH"},
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        bake_space_transform=False,
        axis_forward="-Z",
        axis_up="Y",
        path_mode="STRIP",
        embed_textures=False,
    )
    print("FBX écrit :", fbx_path)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "Pokedex_prepare.blend"))

    if "--render" in argv:
        render_preview(parts, os.path.join(out_dir, "pokedex_apercu.png"))


def render_preview(parts, path):
    """Aperçu : appareil ouvert vu de face (couleurs des pièces reconnues pour vérifier la découpe)."""
    scene = bpy.context.scene
    for obj in list(bpy.data.objects):
        if obj.type in ("CAMERA", "LIGHT"):
            bpy.data.objects.remove(obj, do_unlink=True)
    target = mathutils.Vector((0.0, 0.0, 0.0))
    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    scene.collection.objects.link(cam)
    cam.location = mathutils.Vector((-0.42, 0.04, 0.12))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 60
    scene.camera = cam
    for loc, energy in (((-0.4, 0.3, 0.4), 25.0), ((-0.3, -0.4, 0.1), 10.0)):
        light = bpy.data.objects.new("Lumiere", bpy.data.lights.new("Lumiere", "AREA"))
        light.data.energy = energy
        light.data.size = 0.3
        light.location = loc
        light.rotation_euler = (target - mathutils.Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        scene.collection.objects.link(light)
    world = bpy.data.worlds.new("Fond")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.25, 0.27, 0.3, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
    scene.world = world
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 32
    scene.render.resolution_x, scene.render.resolution_y = 900, 640
    scene.view_settings.view_transform = "Standard"
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("Aperçu :", path)


if __name__ == "__main__":
    main()
