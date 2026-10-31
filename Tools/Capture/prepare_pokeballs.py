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
    return top, bottom


def export(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", add_leaf_bones=False, path_mode="STRIP", embed_textures=False)


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
