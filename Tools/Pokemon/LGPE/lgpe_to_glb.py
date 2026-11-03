"""Pokémon Let's Go (fichiers bruts du jeu) -> .glb, pour passer ensuite dans convert_pokemon_glb.py.

    blender --background --python Tools/Pokemon/LGPE/lgpe_to_glb.py -- <dossier pmXXXX_00> <sortie.glb> [--rare]
            [--anims <dossier contenant pmXXXX_00/anm/>] [--upscale <RealESRGAN_x4plus_anime_6B.pth>]

Lit pmXXXX_00/mdl/pmXXXX_00(.rare).gfbmdl (gfbmdl.py) et ses textures .bntx (bntx_to_png.py) :
  - squelette (os en Y vers le haut, remis en Z vers le haut comme Blender) et poids des sommets ;
  - une texture de couleur par matériau (Col0Tex ; version « _rare » pour le chromatique).
Le jeu répète souvent la texture en miroir (moitié gauche peinte, la droite en reflet) avec un facteur
d'échelle des UV : on recopie le reflet dans l'image (largeur ×2) et on reporte l'échelle dans les UV,
pour n'avoir que de la répétition simple (ce que gardent glTF, FBX et Unreal sans réglage).
"""

import math
import os
import sys

import bpy
from mathutils import Euler, Matrix, Vector
from PIL import Image

# Maillages rangés sous ces os : invisibles en jeu sauf pendant certaines attaques (lianes de Bulbizarre...)
HIDDEN_MESHES = ("Tuta",)
# Part de la couleur d'ombre du jeu mélangée à la couleur éclairée (voir texture_png)
SHADOW_MIX = 0.4
# Flammes (Salamèche, Reptincel...) : shader spécial du jeu, remplacé par une couleur de flamme
FIRE_COLOR = (1.0, 0.42, 0.08, 1.0)

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bntx_to_png  # noqa: E402
import gfbanm  # noqa: E402
import gfbmdl  # noqa: E402

sys.path.insert(0, os.path.dirname(HERE))
from convert_pokemon_glb import bake_rest_pose  # noqa: E402

# Animations reprises du jeu : nom Unreal (A_<Espèce>_<Nom>) -> fichiers candidats dans anm/, par ordre de préférence.
# Idle sert aussi de pose de repos (sa première image), à la place de la pose en T du fichier de modèle.
ANIMATIONS = [
    ("Idle", ("fi01_wait01", "ba10_waitA01", "kw01_wait01")),
    ("BattleIdle", ("ba10_waitA01",)),
    ("Walk", ("fi20_walk01",)),
    ("Run", ("fi21_run01",)),
    ("Attack", ("ba20_buturi01",)),
    ("Special", ("ba21_tokusyu01",)),
    ("Hit", ("ba30_damageS01",)),
    ("Faint", ("ba41_down01",)),
    ("Happy", ("kw32_happyA01",)),
    ("Sleep", ("kw21_sleepA01",)),
]


def load_bntx(mdl_dir, name, rare):
    src = os.path.join(mdl_dir, name + ("_rare" if rare else "") + ".bntx")
    if not os.path.exists(src):
        src = os.path.join(mdl_dir, name + ".bntx")
    with open(src, "rb") as fh:
        return src, bntx_to_png.read_bntx(fh.read())[0][1]


# Agrandissement ×4 des textures par IA (Tools/Pokemon/upscale_textures.py), activé par --upscale <poids .pth>
UPSCALE = {"weights": None, "model": None, "cache": None}


def upscaled(img, key, shadow, mdl_dir, rare):
    """Texture (mélangée à son ombre) agrandie par Real-ESRGAN, gardée en cache (même clé = même résultat)."""
    import hashlib
    if shadow:
        try:
            _, sdw = load_bntx(mdl_dir, shadow, rare)
            if sdw.size == img.size:
                rgba = img.convert("RGBA")
                img = Image.blend(rgba, sdw.convert("RGBA"), SHADOW_MIX)
                img.putalpha(rgba.getchannel("A"))
        except (OSError, KeyError, ValueError) as err:
            print(f"  ombre {shadow} ignorée : {err}")
    with open(key.split("+")[0], "rb") as fh:
        digest = hashlib.sha1(fh.read() + key.encode()).hexdigest()[:16]
    cached = os.path.join(UPSCALE["cache"], digest + ".png")
    if os.path.exists(cached):
        return Image.open(cached).copy()
    if UPSCALE["model"] is None:
        sys.path.insert(0, os.path.dirname(HERE))
        import upscale_textures
        UPSCALE["module"] = upscale_textures
        UPSCALE["model"] = upscale_textures.load_model(UPSCALE["weights"])
    out = UPSCALE["module"].upscale_image(UPSCALE["model"], img)
    os.makedirs(UPSCALE["cache"], exist_ok=True)
    out.save(cached)
    print(f"  {os.path.basename(key.split('+')[0])} agrandie : {img.size} -> {out.size}", flush=True)
    return out


def texture_png(mdl_dir, name, rare, tex_dir, wrap_u, wrap_v, shadow=None):
    """PNG de la texture, avec le miroir recopié. Renvoie (chemin, facteur U, facteur V).

    shadow : nom de la texture d'ombre du jeu (ColSdw0Tex). Le jeu passe de la couleur claire (éclairée) à
    cette couleur foncée selon la lumière ; Unreal fait son propre éclairage, donc on prend une couleur entre
    les deux (SHADOW_MIX), sinon le Pokémon paraît délavé.
    """
    src, img = load_bntx(mdl_dir, name, rare)
    if UPSCALE["weights"] and "Iris" not in name:
        img = upscaled(img, src + (f"+{shadow}" if shadow else ""), shadow, mdl_dir, rare)
        shadow = None  # déjà mélangée avant l'agrandissement
    if shadow:
        try:
            _, sdw = load_bntx(mdl_dir, shadow, rare)
            if sdw.size == img.size:
                rgba = img.convert("RGBA")
                mixed = Image.blend(rgba, sdw.convert("RGBA"), SHADOW_MIX)
                mixed.putalpha(rgba.getchannel("A"))
                img = mixed
        except (OSError, KeyError, ValueError) as err:
            print(f"  ombre {shadow} ignorée : {err}")
    fu = 2 if wrap_u == "mirror" else 1
    fv = 2 if wrap_v == "mirror" else 1
    if fu == 2:
        both = Image.new("RGBA", (img.width * 2, img.height))
        both.paste(img, (0, 0))
        both.paste(img.transpose(Image.FLIP_LEFT_RIGHT), (img.width, 0))
        img = both
    if fv == 2:
        both = Image.new("RGBA", (img.width, img.height * 2))
        both.paste(img, (0, 0))
        both.paste(img.transpose(Image.FLIP_TOP_BOTTOM), (0, img.height))
        img = both
    os.makedirs(tex_dir, exist_ok=True)
    path = os.path.join(tex_dir, os.path.basename(src)[:-5] + ".png")
    img.save(path)
    return path, fu, fv


def build_materials(model, mdl_dir, rare, tex_dir):
    mats = []
    for m in model["materials"]:
        mat = bpy.data.materials.new(m["name"])
        mat.use_nodes = True
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        bsdf = nodes.get("Principled BSDF")
        bsdf.inputs["Roughness"].default_value = 0.7
        uv = {"su": 1.0, "sv": 1.0, "tu": 0.0, "tv": 0.0, "fu": 1, "fv": 1}
        col = next((mp for mp in m["maps"] if mp[0] == "Col0Tex"), None)
        sdw = next((mp for mp in m["maps"] if mp[0] == "ColSdw0Tex"), None)
        sdw_name = model["textures"][sdw[1]] if sdw is not None and 0 <= sdw[1] < len(model["textures"]) else None
        if sdw_name and "dummy" in sdw_name:
            sdw_name = None
        if "Fire" in m["shader"]:
            bsdf.inputs["Base Color"].default_value = FIRE_COLOR
            bsdf.inputs["Emission Color"].default_value = FIRE_COLOR
            bsdf.inputs["Emission Strength"].default_value = 2.0
        if col is not None and 0 <= col[1] < len(model["textures"]):
            path, fu, fv = texture_png(mdl_dir, model["textures"][col[1]], rare, tex_dir, col[2], col[3], sdw_name)
            node = nodes.new("ShaderNodeTexImage")
            node.image = bpy.data.images.load(path)
            links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
            if node.image.depth == 32 and m["values"].get("ConstantAlpha", 1.0) < 1.0:
                links.new(node.outputs["Alpha"], bsdf.inputs["Alpha"])
            v = m["values"]
            # ColorUVTranslate choisit l'expression des yeux (planche de paupières) : piloté par les animations
            # du jeu. Au repos on prend la première case (yeux ouverts), donc pas de décalage.
            uv = {"su": v.get("ColorUVScaleU", 1.0), "sv": v.get("ColorUVScaleV", 1.0),
                  "tu": 0.0, "tv": 0.0, "fu": fu, "fv": fv}
        # Calque 1 (iris des yeux) : multiplié sur la couleur, avec ses propres UV (uv2) ; cuit plus bas
        layer = next((mp for mp in m["maps"] if mp[0] == "LyCol0Tex"), None)
        v = m["values"]
        if (layer is not None and col is not None and v.get("Layer1OverLerpValue", 0.0) > 0
                and 0 <= layer[1] < len(model["textures"]) and "dummy" not in model["textures"][layer[1]]):
            lpath, lfu, lfv = texture_png(mdl_dir, model["textures"][layer[1]], rare, tex_dir, layer[2], layer[3])
            uv["layer"] = {"path": lpath, "su": v.get("Layer1UVScaleU", 1.0), "sv": v.get("Layer1UVScaleV", 1.0),
                           "tu": v.get("Layer1UVTranslateU", 0.0), "tv": v.get("Layer1UVTranslateV", 0.0),
                           "fu": lfu, "fv": lfv}
        mats.append((mat, uv))
    return mats


def bake_layers(objects, mats):
    """Cuit le calque 1 (iris, visible où la couleur est transparente) dans la texture de couleur : FBX et Unreal ne gardent qu'une texture par matériau."""
    layered = [(mat, t) for mat, t in mats if "layer" in t]
    if not layered:
        return
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 1
    sc.render.bake.use_clear = False
    sc.render.bake.margin = 2
    for mat, t in layered:
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        base = next(n for n in nodes if n.type == "TEX_IMAGE")
        target = bpy.data.images.new(base.image.name + "_bake", *base.image.size, alpha=True)
        target.pixels = base.image.pixels[:]
        uv1 = nodes.new("ShaderNodeUVMap")
        uv1.uv_map = "UVMap"
        uv2 = nodes.new("ShaderNodeUVMap")
        uv2.uv_map = "UVLayer"
        links.new(uv1.outputs[0], base.inputs[0])
        lay = nodes.new("ShaderNodeTexImage")
        lay.image = bpy.data.images.load(t["layer"]["path"])
        links.new(uv2.outputs[0], lay.inputs[0])
        # L'iris apparaît là où la couleur de l'œil est transparente (ouverture de la paupière)
        base.image.alpha_mode = "STRAIGHT"
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        links.new(base.outputs["Alpha"], mix.inputs[0])
        links.new(lay.outputs["Color"], mix.inputs[6])
        links.new(base.outputs["Color"], mix.inputs[7])
        emit = nodes.new("ShaderNodeEmission")
        links.new(mix.outputs[2], emit.inputs[0])
        out = nodes.get("Material Output")
        old = [l for l in links if l.to_node == out]
        bsdf_link = old[0].from_socket if old else None
        links.new(emit.outputs[0], out.inputs[0])
        holder = nodes.new("ShaderNodeTexImage")
        holder.image = target
        nodes.active = holder
        users = [o for o in objects if mat.name in [m.name for m in o.data.materials]]
        for o in users:
            o.data.uv_layers.active = o.data.uv_layers["UVMap"]
            bpy.ops.object.select_all(action="DESELECT")
            o.select_set(True)
            bpy.context.view_layer.objects.active = o
            # Cycles cuit chaque matériau de l'objet dans SON image active : les autres matériaux reçoivent
            # une image jetable, sinon leur vraie texture serait écrasée
            trash = []
            for other in o.data.materials:
                if other != mat:
                    n = other.node_tree.nodes.new("ShaderNodeTexImage")
                    n.image = bpy.data.images.new("jetable", 8, 8)
                    other.node_tree.nodes.active = n
                    trash.append((other, n))
            bpy.ops.object.bake(type="EMIT", use_clear=False, margin=2)
            for other, n in trash:
                bpy.data.images.remove(n.image)
                other.node_tree.nodes.remove(n)
        # Matériau final : la texture cuite à la place de la couleur, sans le calque
        target.filepath_raw = os.path.join(os.path.dirname(t["layer"]["path"]), mat.name + "_iris.png")
        target.file_format = "PNG"
        target.save()
        base.image = target
        for n in (lay, mix, emit, holder, uv2):
            nodes.remove(n)
        if bsdf_link is not None:
            links.new(bsdf_link, out.inputs[0])


def build_armature(model, name):
    arm = bpy.data.armatures.new(name)
    obj = bpy.data.objects.new(name, arm)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bones = []
    world = []
    for b in model["bones"]:
        local = Matrix.LocRotScale(Vector(b["translation"]), Euler(b["rotation"], "XYZ"), Vector(b["scale"]))
        m = world[b["parent"]] @ local if b["parent"] >= 0 else local
        world.append(m)
        eb = arm.edit_bones.new(b["name"])
        eb.head = (0, 0, 0)
        eb.tail = (0, 0, 2.0)
        eb.matrix = m
        if b["parent"] >= 0:
            eb.parent = arm.edit_bones[b["parent"]]
        bones.append(eb.name)
    bpy.ops.object.mode_set(mode="OBJECT")
    skin = [b["name"] for b in model["bones"] if not b["rigid"]]
    return obj, skin


def build_mesh(model, index, mats, arm, skin):
    data = model["meshes"][index]
    s = data["streams"]
    faces, face_mats = [], []
    for poly in data["polygons"]:
        f = poly["faces"]
        for i in range(0, len(f) - 2, 3):
            faces.append((f[i], f[i + 1], f[i + 2]))
            face_mats.append(poly["material"])
    me = bpy.data.meshes.new(f"mesh{index}")
    me.from_pydata([tuple(p[:3]) for p in s["position"]], [], faces)
    used = sorted(set(face_mats))
    for mi in used:
        me.materials.append(mats[mi][0])
    slot = {mi: k for k, mi in enumerate(used)}
    uv_layer = me.uv_layers.new(name="UVMap")
    lay_layer = me.uv_layers.new(name="UVLayer")
    uvs = s.get("uv1")
    uvs2 = s.get("uv2") or uvs
    for poly, mi in zip(me.polygons, face_mats):
        poly.material_index = slot[mi]
        poly.use_smooth = True
        t = mats[mi][1]
        # Convention du jeu = celle de Blender pour V (vérifié en rendu) : pas d'inversion
        coords = []
        for vi in poly.vertices:
            u, v = uvs[vi][:2] if uvs else (0.0, 0.0)
            coords.append(((u * t["su"] + t["tu"]) / t["fu"], (v * t["sv"] + t["tv"]) / t["fv"]))
        if "layer" in t:
            # Cuisson : la face doit tomber dans l'image (0-1) ; la texture se répète, donc on la ramène
            du, dv = math.floor(min(c[0] for c in coords)), math.floor(min(c[1] for c in coords))
            coords = [(u - du, v - dv) for u, v in coords]
            lt = t["layer"]
        for k, (vi, li) in enumerate(zip(poly.vertices, poly.loop_indices)):
            uv_layer.data[li].uv = coords[k]
            if "layer" in t:
                u2, v2 = uvs2[vi][:2]
                # Le jeu décale AVANT l'échelle : (uv + décalage) × échelle (sinon les iris partent vers l'extérieur)
                lay_layer.data[li].uv = (((u2 + lt["tu"]) * lt["su"]) / lt["fu"], ((v2 + lt["tv"]) * lt["sv"]) / lt["fv"])
    me.update()
    if "normal" in s:
        me.normals_split_custom_set_from_vertices([Vector(n[:3]).normalized() for n in s["normal"]])
    obj = bpy.data.objects.new(f"mesh{index}", me)
    bpy.context.collection.objects.link(obj)
    obj.parent = arm
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    if "bone_ids" in s and "weights" in s:
        groups = {}
        for vi, (ids, ws) in enumerate(zip(s["bone_ids"], s["weights"])):
            for bid, w in zip(ids, ws):
                if w > 0 and bid < len(skin):
                    name = skin[bid]
                    if name not in groups:
                        groups[name] = obj.vertex_groups.new(name=name)
                    groups[name].add([vi], w, "REPLACE")
    return obj


def find_anim_dir(folder, anim_root):
    base = os.path.basename(os.path.normpath(folder))
    for cand in ([os.path.join(anim_root, base, "anm")] if anim_root else []) + [os.path.join(folder, "anm")]:
        if os.path.isdir(cand):
            return cand
    return None


def import_animations(arm, anim_dir, base):
    """Crée une action Blender par animation retenue. Les pistes du jeu donnent la transformation locale de
    chaque os (position, rotation) ; on en déduit la pose « basis » de Blender. Les échelles animées sont
    ignorées : le jeu ne les transmet pas aux os enfants, Unreal si (elles étireraient tout le squelette)."""
    from mathutils import Quaternion
    bones = list(arm.pose.bones)
    order = [pb for pb in bones]  # build_armature crée les parents avant les enfants
    rest_local = {}
    for pb in bones:
        m = pb.bone.matrix_local
        rest_local[pb.name] = pb.parent.bone.matrix_local.inverted() @ m if pb.parent else m.copy()
    made = []
    if arm.animation_data is None:
        arm.animation_data_create()
    for name, candidates in ANIMATIONS:
        path = next((os.path.join(anim_dir, f"{base}_{c}.gfbanm") for c in candidates
                     if os.path.exists(os.path.join(anim_dir, f"{base}_{c}.gfbanm"))), None)
        if path is None:
            continue
        anim = gfbanm.read_animation(path)
        act = bpy.data.actions.new(name)
        arm.animation_data.action = act
        bpy.context.scene.render.fps = anim["fps"]
        for f in range(anim["frames"]):
            world = {}
            for pb in order:
                loc, rot, _ = rest_local[pb.name].decompose()
                tr = anim["tracks"].get(pb.name)
                if tr:
                    if tr["t"]:
                        loc = Vector(tr["t"][f])
                    if tr["r"]:
                        rot = Quaternion(tr["r"][f])
                local = Matrix.LocRotScale(loc, rot, None)
                world[pb.name] = world[pb.parent.name] @ local if pb.parent else local
                rel = rest_local[pb.name]
                parent = world[pb.parent.name] if pb.parent else Matrix.Identity(4)
                pb.rotation_mode = "QUATERNION"
                pb.matrix_basis = rel.inverted() @ parent.inverted() @ world[pb.name]
                for dp in ("location", "rotation_quaternion"):
                    pb.keyframe_insert(dp, frame=f + 1)
        made.append(name)
    arm.animation_data.action = None
    return made


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    folder, out = argv[0], argv[1]
    rare = "--rare" in argv
    base = os.path.basename(os.path.normpath(folder))
    mdl_dir = os.path.join(folder, "mdl")
    path = os.path.join(mdl_dir, base + ("_rare" if rare else "") + ".gfbmdl")
    if not os.path.exists(path):
        path = os.path.join(mdl_dir, base + ".gfbmdl")
    if "--upscale" in argv:
        UPSCALE["weights"] = argv[argv.index("--upscale") + 1]
        UPSCALE["cache"] = os.path.join(os.path.dirname(os.path.abspath(out)), "_agrandies")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    model = gfbmdl.read_model(path)
    tex_dir = os.path.join(os.path.dirname(os.path.abspath(out)), base + ("_rare" if rare else "") + "_tex")
    mats = build_materials(model, mdl_dir, rare, tex_dir)
    arm, skin = build_armature(model, base)
    objects = []
    for g in model["groups"]:
        bone = model["bones"][g["bone"]]["name"] if g["bone"] < len(model["bones"]) else ""
        if any(k in bone for k in HIDDEN_MESHES):
            print(f"  maillage {g['mesh']} ({bone}) retiré : caché en jeu hors attaques")
            continue
        objects.append(build_mesh(model, g["mesh"], mats, arm, skin))
    bake_layers(objects, mats)
    for o in objects:
        o.data.uv_layers.remove(o.data.uv_layers["UVLayer"])

    anim_root = argv[argv.index("--anims") + 1] if "--anims" in argv else None
    anim_dir = find_anim_dir(folder, anim_root)
    anims = import_animations(arm, anim_dir, base) if anim_dir else []
    if anims:
        # Pose de repos = première image de l'attente (sinon Unreal montre la pose en T du fichier)
        arm.animation_data.action = bpy.data.actions[anims[0]]
        bpy.context.scene.frame_set(1)
        arm.animation_data.action = None
        bake_rest_pose(arm, objects)
        print(f"  animations : {', '.join(anims)}")
    # Le jeu est en Y vers le haut : on redresse (le Pokémon regarde alors -Y, comme un glTF importé)
    arm.matrix_world = Matrix.Rotation(math.radians(90), 4, "X")
    bpy.context.view_layer.update()
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_yup=True, export_skins=True,
                              export_animations=bool(anims), export_animation_mode="ACTIONS",
                              export_image_format="AUTO")
    print(f"LGPE {base}{' rare' if rare else ''} -> {out}")


main()
