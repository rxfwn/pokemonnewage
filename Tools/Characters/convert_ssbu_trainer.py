"""Prépare le Dresseur Pokémon de Smash Bros Ultimate pour Unreal (SCRUM-65).

Entrée : ptrainer.fbx (Red) ou ptrainer_female.fbx (Leaf) du zip « Pokemon_Trainer_SSBU »
(ripé par Centrixe, The Models Resource), avec son dossier images/ à côté.

Sortie, dans le dossier de sortie :
- SK_Trainer.fbx (ou SK_TrainerFemale.fbx) : un seul mesh skinné, Z en haut, 1,65 m,
  os renommés comme le mannequin d'Unreal (pelvis, thigh_l, spine_01, hand_r, head…)
  pour que le retargeting des animations du modèle Third Person se fasse presque seul ;
- Textures/T_Trainer_<Matériau>_D/_N/_ORM.png, à brancher sur les matériaux dans Unreal.

Ce que le script corrige par rapport au fichier d'origine :
1. Smash garde toutes les expressions du visage les unes sur les autres (yeux fermés,
   bouche qui crie…) : on ne garde que le visage neutre, yeux ouverts.
2. Les Poké Balls tenues en main et le squelette d'animation « Throw » sont retirés.
3. Le modèle mesure 45 cm, couché sur l'axe Y : on le redresse et on le met à l'échelle.
   Certaines UV dépassent 1 : c'est voulu, la texture se répète (comme par défaut dans Unreal).
4. Les yeux pointent vers la texture du blanc seul : on met celle avec l'iris.

Lancement (Blender 4.x ou plus, sans ouvrir l'interface) :
    blender --background --python Tools/Characters/convert_ssbu_trainer.py -- \
        "C:/chemin/Pokemon Trainer/ptrainer.fbx" "C:/chemin/sortie"
"""

import math
import os
import sys

import bpy
import numpy as np
from mathutils import Matrix

TARGET_HEIGHT_M = 1.65

# Os de Smash -> os du mannequin UE5. Les os absents gardent leur nom (sac, cheveux, jupe).
BONE_NAMES = {
    "Trans": "root",
    "Hip": "pelvis",
    "LegL": "thigh_l", "KneeL": "calf_l", "FootL": "foot_l", "ToeL": "ball_l",
    "LegR": "thigh_r", "KneeR": "calf_r", "FootR": "foot_r", "ToeR": "ball_r",
    "Waist": "spine_01", "Bust": "spine_02",
    "ClavicleL": "clavicle_l", "ShoulderL": "upperarm_l", "ArmL": "lowerarm_l", "HandL": "hand_l",
    "ClavicleR": "clavicle_r", "ShoulderR": "upperarm_r", "ArmR": "lowerarm_r", "HandR": "hand_r",
    "FingerL11": "index_01_l", "FingerL12": "index_02_l",
    "FingerL21": "middle_01_l", "FingerL22": "middle_02_l",
    "FingerL51": "thumb_01_l", "FingerL52": "thumb_02_l",
    "FingerR11": "index_01_r", "FingerR12": "index_02_r",
    "FingerR21": "middle_01_r", "FingerR22": "middle_02_r",
    "FingerR51": "thumb_01_r", "FingerR52": "thumb_02_r",
    "Neck": "neck_01", "Head": "head",
}

# Matériaux de Smash -> nom de base dans Unreal. Les yeux gauche et droit partagent un matériau.
MATERIALS = {
    "def_ptrainer": "Body", "skin_ptrainer": "Skin", "alp_ptrainer": "Hair",
    "metal_ptrainer": "Metal", "eye": "Eye",
}

# Morceaux du visage à garder : yeux ouverts, bouche neutre, globes oculaires.
KEEP_FACE = ("_Openblink_", "_faceN_Mouth_", "_eyeL_", "_eyeR_")


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(argv) != 2:
        sys.exit("Usage : blender --background --python convert_ssbu_trainer.py -- <entrée.fbx> <dossier de sortie>")
    return os.path.abspath(argv[0]), os.path.abspath(argv[1])


def material_key(name):
    for prefix, key in MATERIALS.items():
        if name.lower().startswith(prefix):
            return key
    return None


def keep_object(obj):
    name = obj.name
    if "_VIS_O_OBJ" not in name:
        return True
    if "pokeball" in name.lower() or "mball" in name.lower():
        return False
    return any(tag in name for tag in KEEP_FACE)


def clean_scene(arm):
    for obj in [o for o in bpy.data.objects if o.type == "MESH" and not keep_object(o)]:
        bpy.data.objects.remove(obj)
    # Les morceaux de sac de Leaf (« _Gamemodel_bag… ») ne portent pas _VIS_O_OBJ : on les garde.
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for bone in [b for b in arm.data.edit_bones if b.name in ("Throw",)]:
        arm.data.edit_bones.remove(bone)
    bpy.ops.object.mode_set(mode="OBJECT")


def fix_orientation_and_scale(arm, meshes):
    """Redresse le modèle (Z en haut, face vers -Y de Blender), le pose au sol et le met à 1,65 m."""
    bpy.ops.object.select_all(action="DESELECT")
    for obj in [arm, *meshes]:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = arm
    # Même après l'import, le fichier reste en Y vers le haut et regarde vers +Z : un quart de tour
    # autour de X le met debout, face vers -Y (la vue de face de Blender, l'avant pour Unreal).
    arm.matrix_world = Matrix.Rotation(math.radians(90), 4, "X") @ arm.matrix_world
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.context.view_layer.update()

    zs = [(o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices]
    height = max(zs) - min(zs)
    factor = TARGET_HEIGHT_M / height
    arm.matrix_world = Matrix.Translation((0, 0, -min(zs) * factor)) @ Matrix.Scale(factor, 4)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return height, factor


def rename_bones(arm):
    for bone in arm.data.bones:
        if bone.name in BONE_NAMES:
            bone.name = BONE_NAMES[bone.name]  # Blender renomme aussi les groupes de vertex.


def load_image(path):
    return bpy.data.images.load(path, check_existing=True) if os.path.exists(path) else None


def save_png(pixels, size, path):
    img = bpy.data.images.new(os.path.basename(path), size[0], size[1], alpha=False)
    img.pixels.foreach_set(pixels.ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img


def read_pixels(img, size):
    if tuple(img.size) != tuple(size):
        img.scale(size[0], size[1])
    px = np.empty(size[0] * size[1] * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    return px.reshape(-1, 4)


def build_textures(images_dir, smash_name, key, tex_dir, prefix):
    """Écrit _D, _N, _ORM pour un matériau. Renvoie les chemins (None si absent)."""
    base = smash_name.lower()
    if key == "Eye":
        stem = "eye_ptrainer_female_b" if "female" in prefix.lower() else "eye_ptrainer_b"
        col = load_image(os.path.join(images_dir, stem + "_col.png"))
        # Le blanc de l'œil et l'iris sont dans la même image ; pas de normal ni d'ORM utiles.
        if col is None:
            return {}
        size = tuple(col.size)
        out = os.path.join(tex_dir, f"T_{prefix}_Eye_D.png")
        save_png(read_pixels(col, size), size, out)
        return {"D": out}

    col = load_image(os.path.join(images_dir, base + "_col.png"))
    if col is None:
        return {}
    size = tuple(col.size)
    out = {}

    d = read_pixels(col, size).copy()
    d[:, 3] = 1.0
    out["D"] = os.path.join(tex_dir, f"T_{prefix}_{key}_D.png")
    save_png(d, size, out["D"])

    nor = load_image(os.path.join(images_dir, base + "_nor.png"))
    if nor is not None:
        n = read_pixels(nor, size).copy()
        # Smash ne stocke que X et Y : on recalcule Z pour une normal map classique.
        x, y = n[:, 0] * 2 - 1, n[:, 1] * 2 - 1
        n[:, 2] = (np.sqrt(np.clip(1 - x * x - y * y, 0, 1)) + 1) / 2
        n[:, 3] = 1.0
        out["N"] = os.path.join(tex_dir, f"T_{prefix}_{key}_N.png")
        save_png(n, size, out["N"])

    channels = []
    for suffix, default in (("_ao", 1.0), ("_rgh", 0.6), ("_mtl", 0.0)):
        img = load_image(os.path.join(images_dir, base + suffix + ".png"))
        channels.append(read_pixels(img, size)[:, 0] if img else np.full(size[0] * size[1], default, np.float32))
    if key == "Skin":
        # La peau n'est pas du métal : Smash range autre chose (un masque) dans ce canal.
        channels[2][:] = 0.0
    orm = np.stack([*channels, np.ones_like(channels[0])], axis=1)
    out["ORM"] = os.path.join(tex_dir, f"T_{prefix}_{key}_ORM.png")
    save_png(orm, size, out["ORM"])
    return out


def setup_material(mat, textures):
    """Branche les textures dans le Principled BSDF : l'import FBX d'Unreal les retrouve."""
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    output = nodes.new("ShaderNodeOutputMaterial")
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
    if "D" in textures:
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(textures["D"])
        links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    if "N" in textures:
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(textures["N"])
        tex.image.colorspace_settings.name = "Non-Color"
        nmap = nodes.new("ShaderNodeNormalMap")
        links.new(tex.outputs["Color"], nmap.inputs["Color"])
        links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Metallic"].default_value = 0.0
    bsdf.inputs["Roughness"].default_value = 0.6


def main():
    src, out_dir = parse_args()
    images_dir = os.path.join(os.path.dirname(src), "images")
    female = "female" in os.path.basename(src).lower()
    prefix = "TrainerFemale" if female else "Trainer"
    tex_dir = os.path.join(out_dir, "Textures")
    os.makedirs(tex_dir, exist_ok=True)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")

    clean_scene(arm)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    height, factor = fix_orientation_and_scale(arm, meshes)
    rename_bones(arm)

    # Un matériau Unreal par matériau Smash réellement utilisé.
    done = {}
    for obj in meshes:
        me = obj.data
        used = {me.materials[p.material_index].name for p in me.polygons}
        for i, mat in enumerate(me.materials):
            if mat is None or mat.name not in used:
                continue
            key = material_key(mat.name)
            if key is None:
                continue
            if key not in done:
                new = bpy.data.materials.new(f"M_{prefix}_{key}")
                setup_material(new, build_textures(images_dir, mat.name, key, tex_dir, prefix))
                done[key] = new
            me.materials[i] = done[key]

    # Un seul mesh : un seul Skeletal Mesh dans Unreal.
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active
    body.name = body.data.name = f"SK_{prefix}"
    # Retire les emplacements de matériaux devenus vides.
    bpy.ops.object.material_slot_remove_unused()
    for mod in body.modifiers:
        if mod.type == "ARMATURE":
            mod.object = arm
    arm.name = "Armature"  # Unreal ignore un nœud racine nommé « Armature » : la racine sera « root ».

    fbx = os.path.join(out_dir, f"SK_{prefix}.fbx")
    bpy.ops.export_scene.fbx(
        filepath=fbx,
        use_selection=False,
        object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_NONE",
        mesh_smooth_type="FACE",
        add_leaf_bones=False,
        bake_anim=False,
        path_mode="COPY",
        embed_textures=True,
    )
    print(f"Modèle d'origine : {height * 100:.1f} cm, mis à l'échelle x{factor:.2f}")
    print(f"Matériaux : {', '.join(m.name for m in body.data.materials)}")
    print(f"Os : {len(arm.data.bones)}")
    print(f"Écrit : {fbx}")


main()
