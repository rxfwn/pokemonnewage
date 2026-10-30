"""Convertit les modèles .glb des Pokémon (dépôt Pokemon-3D-api/assets) en FBX pour Unreal.

Entrée : un dossier de .glb nommés comme dans le dépôt, rangés par catégorie :
    <entrée>/regular/1.glb, <entrée>/shiny/1.glb ...
(download_pokemon_glb.py les télécharge exactement comme ça.)
Ces fichiers sont compressés en Draco avec des textures WebP : Unreal ne les lit pas tels quels.

Sortie, un dossier par espèce (nom de ligne de DT_Species) :
    <sortie>/<Espèce>/SK_<Espèce>.fbx          mesh skinné + animations éventuelles
    <sortie>/<Espèce>/SK_<Espèce>_Shiny.fbx    version chromatique, quand elle existe
    <sortie>/<Espèce>/Textures/T_<Espèce>[_Shiny]_<n>.png
    <sortie>/models.json                       récapitulatif (taille, os, animations)

Ce que le script corrige :
1. Taille réelle : chaque Pokémon est mis à la taille de son Pokédex (HeightM de DT_Species),
   pieds à Z = 0 et centré. Pour les Pokémon allongés (Onix…), c'est la longueur qui compte.
2. Unités : os en centimètres (même piège que le Dresseur, voir docs/PERSONNAGE_DRESSEUR.md).
3. Textures WebP réécrites en PNG, et embarquées dans le FBX.
4. Plusieurs meshes fusionnés en un seul Skeletal Mesh (sans la sphère d'affichage des os
   que l'import glTF ajoute, ni les lianes dépliées de Bulbizarre chromatique, ni la coque
   noire de contour d'Onix).
5. Texture branchée en direct sur Base Color (sinon certains matériaux arrivent blancs).
6. Animations renommées (Idle, Walk, Run…) pour que l'import Unreal les retrouve.

Lancement (Blender 4.2 ou plus, ou `pip install bpy`) :
    blender --background --python Tools/Pokemon/convert_pokemon_glb.py -- <entrée> <sortie>
    python Tools/Pokemon/convert_pokemon_glb.py <entrée> <sortie>      (avec le module bpy)
"""

import json
import os
import re
import sys

import bpy
from mathutils import Matrix, Vector

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SPECIES_JSON = os.path.join(REPO_ROOT, "Data", "Pokemon", "DT_Species.json")

# Pokémon dont la taille du Pokédex est une longueur plutôt qu'une hauteur.
MEASURE_LENGTH = {"Onix", "Caterpie", "Weedle", "Ekans", "Arbok", "Dratini", "Dragonair", "Gyarados"}

# Morceaux à retirer : les lianes de Bulbizarre chromatique, dépliées vers le ciel en pose de repos.
DROP_MESHES = re.compile(r"Vine", re.IGNORECASE)

# Noms d'animation du dépôt -> nom court utilisé dans Unreal (A_<Espèce>_<Nom>).
ANIM_NAMES = [
    (r"idle", "Idle"), (r"walk", "Walk"), (r"run", "Run"), (r"ko|faint|down", "Faint"),
    (r"fight_?b|attack|impactrueno|thunder", "Attack"), (r"fight_?d|damage|hit", "Hit"),
    (r"jump_?s", "JumpStart"), (r"jump_?l", "JumpLoop"), (r"jump_?e", "JumpEnd"),
]


def is_outline_shell(obj):
    """Coque noire sans texture posée sur le modèle (contour façon dessin animé, Onix).

    Elle est faite pour être vue de dos uniquement ; dans Blender et Unreal elle recouvre tout en noir.
    """
    mats = [m for m in obj.data.materials if m]
    if not mats:
        return False
    for mat in mats:
        if not mat.use_nodes:
            return False
        if any(n.type == "TEX_IMAGE" for n in mat.node_tree.nodes):
            return False
        bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is None or max(bsdf.inputs["Base Color"].default_value[:3]) > 0.02:
            return False
    return True


def plug_base_color(mat):
    """Branche la texture directement sur Base Color.

    Certains modèles multiplient la texture par la couleur des sommets (nœud Mix) : l'export FBX
    ne suit pas ce détour et le matériau arrive blanc dans Unreal.
    """
    if not mat.use_nodes:
        return
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None or not bsdf.inputs["Base Color"].is_linked:
        return
    src = bsdf.inputs["Base Color"].links[0].from_node
    if src.type == "TEX_IMAGE":
        return
    tex = next((n for n in nodes if n.type == "TEX_IMAGE"
                and any(l.to_node == src for l in n.outputs["Color"].links)), None)
    if tex:
        links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if len(argv) < 2:
        sys.exit("Usage : convert_pokemon_glb.py <dossier .glb> <dossier de sortie> [Espèce ...]")
    return argv[0], argv[1], set(argv[2:])


def load_species():
    with open(SPECIES_JSON, encoding="utf-8") as f:
        return {row["NationalDex"]: row for row in json.load(f)}


def anim_short_name(name, used):
    low = name.lower().split("|")[-1]
    short = next((n for pat, n in ANIM_NAMES if re.search(pat, low)), None)
    if short is None:
        short = re.sub(r"[^A-Za-z0-9]", "", low.title()) or "Anim"
    base, i = short, 2
    while short in used:
        short, i = f"{base}{i}", i + 1
    used.add(short)
    return short


def world_bounds(meshes):
    pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def rest_bounds(meshes):
    """Boîte englobante des sommets déformés (pose de repos), plus juste que bound_box."""
    deps = bpy.context.evaluated_depsgraph_get()
    xs, ys, zs = [], [], []
    for o in meshes:
        ev = o.evaluated_get(deps)
        me = ev.to_mesh()
        for v in me.vertices:
            p = ev.matrix_world @ v.co
            xs.append(p.x), ys.append(p.y), zs.append(p.z)
        ev.to_mesh_clear()
    return Vector((min(xs), min(ys), min(zs))), Vector((max(xs), max(ys), max(zs)))


def convert(glb, out_dir, species, suffix):
    name = species["Name"]
    tex_dir = os.path.join(out_dir, "Textures")
    os.makedirs(tex_dir, exist_ok=True)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=glb, import_shading="NORMALS", bone_heuristic="TEMPERANCE")

    # Pas de pose animée : on mesure et on exporte la pose de repos.
    for o in bpy.data.objects:
        if o.animation_data:
            o.animation_data.action = None
    bpy.context.scene.frame_set(0)

    # L'import glTF crée une sphère (« Icosphere ») pour dessiner les os : ce n'est pas le modèle.
    shapes = {pb.custom_shape for o in bpy.data.objects if o.type == "ARMATURE"
              for pb in o.pose.bones if pb.custom_shape}
    for o in list(bpy.data.objects):
        if o in shapes or (o.type == "MESH" and (not o.data.polygons or DROP_MESHES.search(o.name)
                                                  or is_outline_shell(o))):
            bpy.data.objects.remove(o)
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for m in meshes:
        for mat in m.data.materials:
            if mat:
                plug_base_color(mat)
    for o in list(bpy.data.objects):
        if o.type not in {"ARMATURE", "MESH"}:
            # Les nœuds vides du glTF (racines, caméras) : on garde leur transformation.
            for child in o.children:
                mw = child.matrix_world.copy()
                child.parent = None
                child.matrix_world = mw
            bpy.data.objects.remove(o)
    if not meshes:
        raise RuntimeError(f"{glb} : aucun mesh")
    if len(arms) > 1:
        # Certains modèles ont un squelette par morceau (Sandshrew) : on les réunit.
        bpy.ops.object.select_all(action="DESELECT")
        for a in arms:
            a.select_set(True)
        bpy.context.view_layer.objects.active = arms[0]
        bpy.ops.object.join()
        arms = [arms[0]]
        for m in meshes:
            for mod in m.modifiers:
                if mod.type == "ARMATURE":
                    mod.object = arms[0]
    arm = arms[0] if arms else None
    # Sans squelette (quelques versions chromatiques), on exporte un Static Mesh.
    asset = f"{'SK' if arm else 'SM'}_{name}{suffix}"

    # Mise à l'échelle sur la taille du Pokédex.
    lo, hi = rest_bounds(meshes)
    size = hi - lo
    measured = max(size.x, size.y, size.z) if name in MEASURE_LENGTH else size.z
    target = float(species["HeightM"]) or 0.5
    factor = target / measured
    center = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    roots = [arm] if arm else []
    roots += [m for m in meshes if m.parent is None or m.parent.type != "ARMATURE"]
    fix = Matrix.Scale(factor, 4) @ Matrix.Translation(-center)
    for r in roots:
        r.matrix_world = fix @ r.matrix_world

    # Unreal compte en centimètres : on applique ×100 et on déclare 1 unité = 1 cm.
    bpy.context.scene.unit_settings.scale_length = 0.01
    for r in roots:
        r.matrix_world = Matrix.Scale(100, 4) @ r.matrix_world
    bpy.ops.object.select_all(action="DESELECT")
    for o in roots + meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = roots[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Un seul mesh : un seul Skeletal Mesh.
    if len(meshes) > 1:
        bpy.ops.object.select_all(action="DESELECT")
        for m in meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active if len(meshes) > 1 else meshes[0]
    body.name = body.data.name = asset
    if arm:
        arm.name = "Armature"  # nœud ignoré par Unreal : la racine du squelette reste le 1er os.

    # Textures : WebP -> PNG, noms Unreal.
    textures = []
    for i, img in enumerate(sorted(bpy.data.images, key=lambda im: im.name)):
        if img.size[0] == 0:
            continue
        path = os.path.join(tex_dir, f"T_{name}{suffix}_{i + 1}.png")
        img.file_format = "PNG"
        img.save(filepath=path)
        # L'image reste emballée en WebP : on la relie au PNG pour que le FBX embarque le PNG.
        if img.packed_file:
            img.unpack(method="REMOVE")
        img.filepath = path
        img.reload()
        textures.append(os.path.basename(path))
    for i, mat in enumerate(body.data.materials):
        if mat:
            mat.name = f"M_{name}{suffix}_{i + 1}"

    # Animations, renommées et rattachées à l'armature pour l'export.
    used, anims = set(), []
    actions = list(bpy.data.actions)
    for act in actions:
        # Une seule animation au nom générique (« Take 001 ») : c'est la boucle de repos du modèle.
        if len(actions) == 1 and re.search(r"take|armatureaction", act.name, re.IGNORECASE):
            act.name = "idle"
        short = anim_short_name(act.name, used)
        act.name = short
        act.use_fake_user = True
        anims.append(short)
    if arm and anims:
        arm.animation_data_create()
        arm.animation_data.action = bpy.data.actions[anims[0]]

    fbx = os.path.join(out_dir, f"{asset}.fbx")
    bpy.ops.export_scene.fbx(
        filepath=fbx,
        object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_NONE",
        mesh_smooth_type="FACE",
        add_leaf_bones=False,
        primary_bone_axis="Y",
        secondary_bone_axis="X",
        bake_anim=bool(anims),
        bake_anim_use_all_actions=True,
        bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True,
        path_mode="COPY",
        embed_textures=True,
    )
    return {
        "asset": asset,
        "skeletal": arm is not None,
        "fbx": os.path.relpath(fbx, os.path.dirname(out_dir)),
        "height_m": target,
        "source_size": [round(v, 4) for v in size],
        "scale": round(factor, 4),
        "bones": len(arm.data.bones) if arm else 0,
        "materials": len(body.data.materials),
        "textures": textures,
        "animations": anims,
        "triangles": sum(len(p.vertices) - 2 for p in body.data.polygons),
    }


def main():
    src, out, only = parse_args()
    species_by_dex = load_species()
    summary = {}
    for category, suffix in (("regular", ""), ("shiny", "_Shiny")):
        folder = os.path.join(src, category)
        if not os.path.isdir(folder):
            continue
        for fname in sorted(os.listdir(folder), key=lambda f: (len(f), f)):
            m = re.fullmatch(r"(\d+)\.glb", fname)
            if not m or int(m.group(1)) not in species_by_dex:
                continue
            species = species_by_dex[int(m.group(1))]
            if only and species["Name"] not in only:
                continue
            out_dir = os.path.join(out, species["Name"])
            info = convert(os.path.join(folder, fname), out_dir, species, suffix)
            summary.setdefault(species["Name"], {})[category] = info
            print(f"{info['asset']}: {info['height_m']} m, x{info['scale']}, {info['bones']} os, "
                  f"{info['triangles']} triangles, anims {info['animations']}")
    with open(os.path.join(out, "models.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    print(f"{len(summary)} espèces converties dans {out}")


main()
