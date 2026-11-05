"""Prépare les modèles de Poké Balls de L'amie pour Unreal (SCRUM-75).

Lancer avec le Python de Blender (ou le module bpy : pip install bpy) :
    python Tools/Capture/prepare_pokeballs.py <dossier des Balls> [--apercu image.png]

<dossier des Balls> contient un dossier par Ball, tel que livré (Pokeball_Part1/Pokeball_Part2 acceptés) :
    Pokeball/Pokeball.fbx
    Pokeball/Pokeball_textures/2K/Pokeball_Base_color.png ...

Chaque FBX d'origine a deux objets, <nom>_top et <nom>_bot (moitié haute avec le bouton, moitié basse),
centrés sur l'origine, 7 cm de diamètre, bouton vers -Y de Blender. Le script :
  - tourne la Ball d'un quart de tour pour que le bouton regarde +X (l'avant dans Unreal) ;
  - exporte les deux moitiés à part, pivot au centre de la Ball, un seul emplacement de matériau :
        Tools/Capture/Balls/<Id>/SM_<Id>_Haut.fbx et SM_<Id>_Bas.fbx
    <Id> = nom de ligne de DT_Items (PokeBall, GreatBall, UltraBall...).
Les FBX produits sont petits et versionnés dans Git ; les textures restent dans le dossier de L'amie
(import_pokeball_unreal.py les lit dans Tools/Capture/Generated/Sources).
"""

import math
import os
import sys

import bpy
import mathutils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Balls")


def ball_id(folder_name):
    """Pokeball -> PokeBall, Greatball -> GreatBall (noms de lignes de DT_Items)."""
    base = folder_name[:-4] if folder_name.lower().endswith("ball") else folder_name
    return base[:1].upper() + base[1:].lower() + "Ball"


def find_fbx(root):
    """Tous les <Nom>/<Nom>.fbx sous root (un niveau de dossier en plus toléré : Pokeball_Part1/...)."""
    found = []
    for current, _dirs, files in os.walk(root):
        for name in files:
            stem, ext = os.path.splitext(name)
            if ext.lower() == ".fbx" and os.path.basename(current).lower() == stem.lower():
                found.append(os.path.join(current, name))
    return sorted(found, key=lambda path: os.path.basename(path).lower())


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def prepare(fbx_path):
    reset_scene()
    bpy.ops.import_scene.fbx(filepath=fbx_path)
    meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    top = next((obj for obj in meshes if obj.name.lower().endswith("_top")), None)
    bottom = next((obj for obj in meshes if obj.name.lower().endswith("_bot")), None)
    if not top or not bottom:
        raise RuntimeError(f"{fbx_path} : objets *_top et *_bot introuvables ({[obj.name for obj in meshes]})")

    base_color = load_base_color(fbx_path)

    # Bouton : -Y de Blender -> +X (un quart de tour autour de Z), transformations appliquées
    turn = mathutils.Matrix.Rotation(math.radians(90.0), 4, "Z")
    for obj in (top, bottom):
        obj.data = obj.data.copy()
        obj.data.transform(turn @ obj.matrix_world)
        obj.matrix_world = mathutils.Matrix.Identity(4)
        # Un seul matériau, même nom partout (les textures sont branchées dans Unreal)
        material = bpy.data.materials.get("M_Ball") or bpy.data.materials.new("M_Ball")
        obj.data.materials.clear()
        obj.data.materials.append(material)
        mark_shell(obj, base_color)
    return top, bottom


# ---------------------------------------------------------------- Masque de la coque (couleur de sommet R)
# Les textures livrées (2K) projettent chaque coque à plat, vue de dessus : près de la bande, un pixel de texture
# couvre une grande bande de coque et le grain, les rayures et le relief y sont étirés et flous en gros plan.
# On marque donc les faces de la coque extérieure dans les couleurs de sommets (attribut « Coque ») :
#   - A = 0 : pas la coque (bande, bouton, mécanisme) -> textures d'origine ;
#   - A = 0,5 : coque avec motif (Super Ball, Hyper Ball...) -> relief et rugosité lissés, couleur de la texture ;
#   - A = 1 : coque d'une seule couleur -> lissée ET couleur unie RGB (sRGB), mesurée au centre de la coque, là
#     où la texture n'est pas étirée. Plus de dégradé flou contre la bande.
# M_Ball (import_pokeball_unreal.py) lit ce masque.
SHELL_TOLERANCE = 0.001   # écart au rayon de la coque, en fraction du rayon (0,1 % : 0,035 mm sur 7 cm)
SHELL_DEPTH = 0.02        # la coque va jusqu'à 2 % sous ce rayon (arrondi du bord contre la bande, autour du bouton)
SHELL_MIN_SHARE = 0.25    # la coque fait au moins 25 % des faces d'une moitié (Poké Ball : 37 %)
SHELL_MIN_ISLAND = 0.2    # un morceau de coque d'un seul tenant fait au moins 20 % des faces candidates
SHELL_MIN_DOT = 0.97      # une face de coque regarde vers l'extérieur de la Ball (cos 14°) : exclut bouton et bande
UNIFORM_MAX_SPREAD = 0.08 # écart (sRGB, 0-1) toléré entre les couleurs de la coque : au-delà, elle a un motif
UNIFORM_PERCENTILE = 99   # 1 % des points peut s'écarter (rayures, ombre du bord)


def load_base_color(fbx_path):
    """Texture Base_color de la Ball (2K, sinon 1K) en tableau numpy H×W×3 (valeurs sRGB 0-1), ou None."""
    import numpy as np

    folder = textures_dir(fbx_path)
    stem = os.path.splitext(os.path.basename(fbx_path))[0]
    path = texture(folder, stem, "Base_color") if folder else None
    if not path:
        return None
    image = bpy.data.images.load(path)
    width, height = image.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)  # PNG 8 bits : valeurs sRGB brutes, première ligne en bas
    bpy.data.images.remove(image)
    return pixels.reshape(height, width, 4)[:, :, :3]


def shell_color(mesh, faces, base_color):
    """Couleur unie (sRGB) de la coque si elle n'a pas de motif, sinon None."""
    import numpy as np

    if base_color is None or not faces:
        return None
    uv_layer = mesh.uv_layers.active
    if uv_layer is None:
        return None
    height, width, _ = base_color.shape
    samples = []
    for index in faces:
        polygon = mesh.polygons[index]
        for loop in polygon.loop_indices:
            u, v = uv_layer.data[loop].uv
            x = min(max(int(u % 1.0 * width), 0), width - 1)
            y = min(max(int(v % 1.0 * height), 0), height - 1)
            samples.append(base_color[y, x])
    if len(samples) < 20:
        return None
    samples = np.array(samples)
    # Couleur de référence : la médiane (la majorité de la coque)
    median = np.median(samples, axis=0)
    # Toute la coque doit en être proche : un motif (M de la Master Ball, bandes, points) la fait refuser
    spread = np.percentile(np.abs(samples - median).max(axis=1), UNIFORM_PERCENTILE)
    return tuple(float(c) for c in median) if spread <= UNIFORM_MAX_SPREAD else None


def mark_shell(obj, base_color=None):
    """Couleur de sommet « Coque » par coin de face (voir plus haut). Le centre de la Ball est l'origine
    (transformations déjà appliquées). Renvoie (faces de coque, couleur unie ou None)."""
    mesh = obj.data
    for attribute in list(mesh.color_attributes):
        mesh.color_attributes.remove(attribute)
    radii = [vert.co.length for vert in mesh.vertices]
    radius = max(radii)
    # Le rayon de coque : le plus grand rayon porté par beaucoup de sommets (pas un picot qui dépasse)
    candidates = sorted(radii, reverse=True)
    for value in candidates:
        if sum(1 for r in radii if abs(r - value) <= value * SHELL_TOLERANCE) >= 0.1 * len(radii):
            radius = value
            break
    on_shell = [radius * (1.0 - SHELL_DEPTH) <= r <= radius * (1.0 + SHELL_TOLERANCE) for r in radii]
    shell_faces = set()
    for polygon in mesh.polygons:
        center = polygon.center
        if all(on_shell[index] for index in polygon.vertices) and center.length > 1e-9 and \
                polygon.normal.dot(center.normalized()) >= SHELL_MIN_DOT:
            shell_faces.add(polygon.index)

    # Garde seulement les grands morceaux d'un seul tenant : la plaque du bouton affleure la coque (même
    # rayon, tournée vers l'extérieur) mais forme un petit îlot à part, qui garde sa texture.
    edge_faces = {}
    for polygon in mesh.polygons:
        if polygon.index in shell_faces:
            for key in polygon.edge_keys:
                edge_faces.setdefault(key, []).append(polygon.index)
    neighbours = {index: set() for index in shell_faces}
    for faces in edge_faces.values():
        for a in faces:
            neighbours[a].update(f for f in faces if f != a)
    kept, seen = set(), set()
    for start in shell_faces:
        if start in seen:
            continue
        island, stack = [], [start]
        seen.add(start)
        while stack:
            face = stack.pop()
            island.append(face)
            for other in neighbours[face]:
                if other not in seen:
                    seen.add(other)
                    stack.append(other)
        if len(island) >= SHELL_MIN_ISLAND * len(shell_faces):
            kept.update(island)

    # Coque introuvable (forme spéciale : ailerons de la Super Ball au-dessus de la coque...) : rien de marqué,
    # la moitié garde ses textures telles quelles
    if len(kept) < SHELL_MIN_SHARE * len(mesh.polygons):
        kept = set()
    flat = shell_color(mesh, kept, base_color)
    shell_value = (*flat, 1.0) if flat else (1.0, 1.0, 1.0, 0.5)
    colors = mesh.color_attributes.new("Coque", "BYTE_COLOR", "CORNER")
    for polygon in mesh.polygons:
        value = shell_value if polygon.index in kept else (0.0, 0.0, 0.0, 0.0)
        for loop in polygon.loop_indices:
            colors.data[loop].color_srgb = value
    mesh.color_attributes.active_color = colors
    return len(kept), flat


def export(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", add_leaf_bones=False, colors_type="SRGB", path_mode="STRIP", embed_textures=False)


def textures_dir(fbx_path):
    stem = os.path.splitext(os.path.basename(fbx_path))[0]
    for size in ("2K", "1K"):
        path = os.path.join(os.path.dirname(fbx_path), f"{stem}_textures", size)
        if os.path.isdir(path):
            return path
    return None


def texture(folder, stem, kind):
    """Base_color, Metallic, Roughness, Normal_OpenGL ; tolère <Nom>_Base_<kind> (Ultraball)."""
    for name in (f"{stem}_{kind}.png", f"{stem}_Base_{kind}.png"):
        path = os.path.join(folder, name)
        if os.path.exists(path):
            return path
    return None


def render_preview(entries, out_path, cell=256):
    """Planche des Balls (vue de 3/4 face), pour vérifier modèles et textures."""
    from PIL import Image, ImageDraw  # pip install pillow

    columns = 7
    rows = math.ceil(len(entries) / columns)
    sheet = Image.new("RGB", (columns * cell, rows * (cell + 22)), (236, 236, 232))
    draw = ImageDraw.Draw(sheet)
    tmp = os.path.join(OUT, "_apercu_tmp.png")
    for index, (identifier, fbx_path) in enumerate(entries):
        reset_scene()
        folder = os.path.join(OUT, identifier)
        for part in ("Haut", "Bas"):
            bpy.ops.import_scene.fbx(filepath=os.path.join(folder, f"SM_{identifier}_{part}.fbx"))
        stem = os.path.splitext(os.path.basename(fbx_path))[0]
        tex = textures_dir(fbx_path)
        material = bpy.data.materials.new("Apercu")
        material.use_nodes = True
        nodes = material.node_tree.nodes
        links = material.node_tree.links
        bsdf = nodes["Principled BSDF"]

        def image(kind, non_color):
            path = texture(tex, stem, kind) if tex else None
            if not path:
                return None
            node = nodes.new("ShaderNodeTexImage")
            node.image = bpy.data.images.load(path)
            if non_color:
                node.image.colorspace_settings.name = "Non-Color"
            return node

        for kind, socket, non_color in (("Base_color", "Base Color", False), ("Metallic", "Metallic", True),
                                        ("Roughness", "Roughness", True)):
            node = image(kind, non_color)
            if node:
                links.new(node.outputs[0], bsdf.inputs[socket])
        node = image("Normal_OpenGL", True)
        if node:
            normal = nodes.new("ShaderNodeNormalMap")
            links.new(node.outputs[0], normal.inputs["Color"])
            links.new(normal.outputs[0], bsdf.inputs["Normal"])
        for obj in bpy.data.objects:
            if obj.type == "MESH":
                obj.data.materials.clear()
                obj.data.materials.append(material)

        scene = bpy.context.scene
        scene.render.engine = "CYCLES"
        scene.cycles.samples = 24
        scene.render.resolution_x = scene.render.resolution_y = cell
        world = bpy.data.worlds.new("Fond")
        scene.world = world
        world.use_nodes = True
        world.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.63, 0.66, 1.0)
        sun = bpy.data.objects.new("Soleil", bpy.data.lights.new("Soleil", "SUN"))
        sun.data.energy = 3.0
        sun.rotation_euler = (math.radians(50), 0, math.radians(120))
        scene.collection.objects.link(sun)
        camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
        camera.data.type = "ORTHO"
        camera.data.ortho_scale = 0.09
        scene.collection.objects.link(camera)
        scene.camera = camera
        # Bouton vers +X : la caméra regarde la Ball de 3/4 face, un peu au-dessus
        direction = mathutils.Vector((1.0, -0.55, 0.45)).normalized()
        camera.location = direction * 0.5
        camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = tmp
        bpy.ops.render.render(write_still=True)

        x = (index % columns) * cell
        y = (index // columns) * (cell + 22)
        sheet.paste(Image.open(tmp).convert("RGB"), (x, y + 22))
        draw.text((x + 8, y + 5), identifier, fill=(30, 30, 30))
    os.remove(tmp)
    sheet.save(out_path)
    print(f"Aperçu : {out_path}")


def main():
    args = [arg for arg in sys.argv[1:]]
    preview = None
    if "--apercu" in args:
        index = args.index("--apercu")
        preview = args[index + 1]
        del args[index:index + 2]
    if not args:
        print(__doc__)
        sys.exit(1)

    entries = []
    for fbx_path in find_fbx(args[0]):
        identifier = ball_id(os.path.splitext(os.path.basename(fbx_path))[0])
        top, bottom = prepare(fbx_path)
        folder = os.path.join(OUT, identifier)
        os.makedirs(folder, exist_ok=True)
        export(top, os.path.join(folder, f"SM_{identifier}_Haut.fbx"))
        export(bottom, os.path.join(folder, f"SM_{identifier}_Bas.fbx"))
        entries.append((identifier, fbx_path))
        print(f"{identifier} : {folder}")
    print(f"{len(entries)} Balls préparées dans {OUT}")
    if preview:
        render_preview(entries, preview)


if __name__ == "__main__":
    main()
