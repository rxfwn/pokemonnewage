# -*- coding: utf-8 -*-
"""
Import des ressources du menu de création de personnage étudié en cours (exercice du prof, dossier local de L'amie)
dans le jeu : corps, têtes, vêtements, accessoires, textures, icônes, déformation du corps par os.

À lancer DANS Unreal : Outils > Exécuter un script Python (Tools > Execute Python Script) > ce fichier.
Relançable : ce qui est déjà importé est sauté. Voir docs/INTEGRATION_INZOI.md.

Tout arrive dans /Game/ImportedAssets/InZOI (Content/ImportedAssets/ est EXCLU de Git, voir .gitignore) :
les ressources du jeu étudié ne doivent jamais partir sur GitHub. Le jeu marche sans ce dossier
(il reprend alors Content/Characters/Player/DA_TrainerAppearance).
"""
import csv
import json
import os
import re
from collections import OrderedDict

import unreal

# ---------------------------------------------------------------- Réglages (à modifier si besoin)

# Dossier de l'exercice sur le PC (celui qui contient Output/ et Index_Complet/)
SOURCE = r"D:\Dev\SourceArt\CCMenu\Exercice Compilation"

# Âges importés : "Adult" (bases Homme / Femme) ; ajouter "Youth" pour les bases Garçon / Fille (corps d'enfant)
AGES = ["Adult"]

# Nombre de modèles différents par emplacement et par base (0 = tout). 15 pour un premier essai rapide (~15 min),
# 0 ensuite pour tout importer (plusieurs heures, plusieurs Go).
MAX_MESHES_PER_SLOT = 15

# Variantes de couleur gardées par modèle (0 = toutes)
MAX_VARIANTS_PER_MESH = 4

# Taille maximale des textures dans Unreal (pixels, 0 = taille d'origine). L'amie veut la qualité d'origine :
# on ne réduit rien. Les personnages utilisent le streaming de textures (Unreal ne charge que ce qui sert).
MAX_TEXTURE_SIZE = 0
MAX_ICON_SIZE = 0

# Libère la mémoire tous les N modèles importés (évite « mémoire vidéo insuffisante » pendant un long import)
GARBAGE_EVERY = 10

# ---------------------------------------------------------------- Chemins

DEST = "/Game/ImportedAssets/InZOI"
EXPORTS = os.path.join(SOURCE, "Output", "Exports", "BlueClient", "Content")
TABLES = os.path.join(SOURCE, "Index_Complet", "Tables_CSV")
MORPHO_BODY = os.path.join(SOURCE, "Index_Complet", "Morphologie_Corps_Os.csv")
DATA_ASSET = DEST + "/DA_TrainerAppearance_InZOI"
MASTER = DEST + "/Materials/M_InZOI_Part"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

BASES = {
    # (genre, âge) : id, libellé, homme ?, corps
    ("Male", "Adult"): ("Homme", "Homme", True, "Characters/AvatarParts/Body/Male/Mesh/SKM_Male_Base"),
    ("Female", "Adult"): ("Femme", "Femme", False, "Characters/AvatarParts/Body/Female/Mesh/SKM_Female_Base"),
    ("Male", "Youth"): ("Garcon", "Garçon", True, "Characters/AvatarParts/Body/Child/Mesh/SKM_Child_Body"),
    ("Female", "Youth"): ("Fille", "Fille", False, "Characters/AvatarParts/Body/Child/Mesh/SKM_Child_Body"),
}

# Emplacement, libellé, onglet de l'éditeur, obligatoire, remplace, table
SLOTS = [
    ("Head", "Tête", "Face", True, [], "AppearanceHead.csv"),
    ("Top", "Haut", "Outfit", False, [], "GarmentTopMesh.csv"),
    ("Outer", "Veste", "Outfit", False, [], "GarmentJacketMesh.csv"),
    ("Bottom", "Bas", "Outfit", False, [], "GarmentBottomMesh.csv"),
    ("Full", "Tenue complète", "Outfit", False, ["Top", "Bottom"], "GarmentFullMesh.csv"),
    ("Socks", "Chaussettes", "Outfit", False, [], "GarmentSocksMesh.csv"),
    ("Shoes", "Chaussures", "Outfit", False, [], "GarmentShoesMesh.csv"),
    ("Hat", "Chapeau", "Accessories", False, [], "GarmentHatMesh.csv"),
    ("Scarf", "Écharpe", "Accessories", False, [], "GarmentScarfMesh.csv"),
    ("Gloves", "Gants", "Accessories", False, [], "GarmentGlovesMesh.csv"),
    ("Acc", "Bijoux et sacs", "Accessories", False, [], "SkinnedAccessoryMesh.csv"),
]

# Pièces portées au départ (si la base n'a rien de choisi)
DEFAULT_SLOTS = ["Head", "Top", "Bottom", "Shoes"]

CATEGORY_FR = {
    "TShirt": "T-shirts", "Shirt": "Chemises", "Blouse": "Blouses", "Sweater": "Pulls", "Vest": "Gilets", "Apron": "Tabliers",
    "Hoodie": "Sweats à capuche", "Jacket": "Vestes", "Coat": "Manteaux", "Padding": "Doudounes", "Cardigan": "Cardigans",
    "Jeans": "Jeans", "Pants": "Pantalons", "Shorts": "Shorts", "Skirt": "Jupes", "Slacks": "Pantalons de ville",
    "Jogger": "Joggings", "Dress": "Robes", "Jumpsuit": "Combinaisons", "Sneakers": "Baskets", "Boots": "Bottes",
    "Loafers": "Mocassins", "Sandals": "Sandales", "Heels": "Talons", "Slippers": "Chaussons", "Cap": "Casquettes",
    "Hat": "Chapeaux", "Beanie": "Bonnets", "Necklace": "Colliers", "Earring": "Boucles d'oreilles", "Bag": "Sacs",
    "Glasses": "Lunettes", "Watch": "Montres", "Bracelet": "Bracelets", "Ring": "Bagues", "Socks": "Chaussettes",
    "Gloves": "Gants", "Scarf": "Écharpes", "Long": "Manches longues", "Short": "Manches courtes", "Sleeveless": "Sans manches",
}

TEXTURE_PARAMS = {"color": "BaseColorMap", "normal": "NormalMap", "arm": "ARMMap", "mask": "ColorMask"}

stats = {"textures": 0, "meshes": 0, "materials": 0, "missing": [], "skipped": []}

# Modèle qui fait planter Unreal : le script note le modèle en cours dans Saved/ ; si Unreal plante pendant son
# import, il est mis de côté au lancement suivant (liste dans Saved/InZOI_ignores.txt, à vider pour réessayer).
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
IN_PROGRESS = os.path.join(SAVED, "InZOI_en_cours.txt")
IGNORED_FILE = os.path.join(SAVED, "InZOI_ignores.txt")


def load_ignored():
    ignored = set()
    if os.path.isfile(IGNORED_FILE):
        with open(IGNORED_FILE, encoding="utf-8") as handle:
            ignored = {line.strip() for line in handle if line.strip()}
    if os.path.isfile(IN_PROGRESS):
        with open(IN_PROGRESS, encoding="utf-8") as handle:
            crashed = handle.read().strip()
        os.remove(IN_PROGRESS)
        if crashed and crashed not in ignored:
            ignored.add(crashed)
            with open(IGNORED_FILE, "a", encoding="utf-8") as handle:
                handle.write(crashed + "\n")
            unreal.log_warning("[InZOI] Unreal a planté pendant l'import de %s : modèle mis de côté (%s)" % (crashed, IGNORED_FILE))
    return ignored


# Modèles connus pour faire planter la carte graphique à l'import (nom du fichier sans extension)
SKIP_MESHES = {"SKM_M_Costume_Outer_JK009"}

IGNORED = load_ignored() | SKIP_MESHES

# .glb réparés (morphs vides retirés) : les têtes d'avant ce correctif sont réimportées une fois
FIXED_DIR = os.path.join(SAVED, "InZOI_glb")
FIXED_FILE = os.path.join(SAVED, "InZOI_glb_repares.txt")
FIXED = set()
if os.path.isfile(FIXED_FILE):
    with open(FIXED_FILE, encoding="utf-8") as _handle:
        FIXED = {line.strip() for line in _handle if line.strip()}


def read_glb(filename):
    with open(filename, "rb") as handle:
        data = handle.read()
    if data[:4] != b"glTF":
        return None, data
    length = int.from_bytes(data[12:16], "little")
    return json.loads(data[20:20 + length].decode("utf-8")), data


def glb_needs_fix(filename):
    """Vrai si une partie du modèle a des morphs sans données : Unreal la jette (« primitive non valide »),
    c'est ce qui faisait disparaître les yeux et les dents des têtes"""
    try:
        with open(filename, "rb") as handle:
            head = handle.read(20)
            length = int.from_bytes(head[12:16], "little")
            gltf = json.loads(handle.read(length).decode("utf-8"))
    except Exception:
        return False
    # Tous les morphs sont retirés : vides, ils font jeter des parties (yeux, dents) ; « creux » (sparse, comme
    # Body_Muscle du corps), Unreal les lit mal et la peau se couvre de traînées sombres le long des muscles
    for mesh in gltf.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            if primitive.get("targets"):
                return True
    return False


def fixed_glb(filename):
    """Copie du .glb sans les morphs (Saved/InZOI_glb), même nom de fichier"""
    gltf, data = read_glb(filename)
    if gltf is None:
        return filename
    for mesh in gltf.get("meshes", []):
        mesh.pop("weights", None)
        if isinstance(mesh.get("extras"), dict):
            mesh["extras"].pop("targetNames", None)
        for primitive in mesh.get("primitives", []):
            primitive.pop("targets", None)
    length = int.from_bytes(data[12:16], "little")
    rest = data[20 + length:]
    chunk = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    chunk += b" " * ((4 - len(chunk) % 4) % 4)
    body = len(chunk).to_bytes(4, "little") + b"JSON" + chunk + rest
    os.makedirs(FIXED_DIR, exist_ok=True)
    out = os.path.join(FIXED_DIR, os.path.basename(filename))
    with open(out, "wb") as handle:
        handle.write(data[:8] + (12 + len(body)).to_bytes(4, "little") + body)
    return out


# ---------------------------------------------------------------- Outils

def log(message):
    unreal.log("[InZOI] " + message)


def game_rel(path):
    """« /Script/Engine.SkeletalMesh'/Game/Characters/X/SKM_Y.SKM_Y' » ou « /Game/Characters/X/T_Y.0 » -> « Characters/X/SKM_Y »"""
    match = re.search(r"/Game/([^'\".]+)", path or "")
    return match.group(1) if match else None


def dest(rel):
    return DEST + "/" + rel


def split(asset_path):
    folder, name = asset_path.rsplit("/", 1)
    return folder, name


def src(rel, ext):
    return os.path.join(EXPORTS, *rel.split("/")) + ext


def read_csv(path):
    if not os.path.isfile(path):
        log("Table absente : " + path)
        return []
    with open(path, encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def load(asset_path):
    return unreal.load_asset(asset_path) if EAL.does_asset_exist(asset_path) else None


def set_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as error:  # noqa: BLE001 - un réglage absent ne doit pas tout arrêter
        log("Réglage ignoré %s.%s : %s" % (type(obj).__name__, name, error))
        return False


def hex_color(text):
    text = (text or "").strip()
    if len(text) < 6:
        return None
    return unreal.LinearColor(int(text[0:2], 16) / 255.0, int(text[2:4], 16) / 255.0, int(text[4:6], 16) / 255.0, 1.0)


# ---------------------------------------------------------------- Textures

def configure_texture(texture, role):
    TCS = unreal.TextureCompressionSettings
    limit = MAX_ICON_SIZE if role == "ui" else MAX_TEXTURE_SIZE
    if limit:
        set_prop(texture, "max_texture_size", limit)
    if role == "ui":
        set_prop(texture, "compression_settings", TCS.TC_EDITOR_ICON)
        set_prop(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        set_prop(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        set_prop(texture, "never_stream", True)
    elif role == "normal":
        set_prop(texture, "compression_settings", TCS.TC_NORMALMAP)
        set_prop(texture, "srgb", False)
        set_prop(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP)
    elif role in ("arm", "mask"):
        set_prop(texture, "compression_settings", TCS.TC_MASKS)
        set_prop(texture, "srgb", False)
        set_prop(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_SPECULAR)
    else:
        set_prop(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)


def import_texture(rel, role):
    if not rel:
        return None
    asset_path = dest(rel)
    existing = load(asset_path)
    if existing:
        return existing
    filename = src(rel, ".png")
    if not os.path.isfile(filename):
        stats["missing"].append(rel + ".png")
        return None
    folder, name = split(asset_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    asset_tools.import_asset_tasks([task])
    texture = load(asset_path)
    if texture:
        configure_texture(texture, role)
        EAL.save_loaded_asset(texture)
        stats["textures"] += 1
    return texture


def import_ui_folder(rel_folder):
    """Toutes les images d'un dossier d'interface (icônes, fonds du studio...)"""
    root = os.path.join(EXPORTS, *rel_folder.split("/"))
    if not os.path.isdir(root):
        return
    for current, _dirs, files in os.walk(root):
        for filename in files:
            if filename.lower().endswith(".png"):
                full = os.path.join(current, filename)
                rel = os.path.relpath(full, EXPORTS).replace("\\", "/")[:-4]
                import_texture(rel, "ui")


def texture_role(rel):
    name = rel.rsplit("/", 1)[-1].upper()
    if name.endswith("_N") or "NORMAL" in name:
        return "normal"
    if name.endswith(("_ARM", "_ARMO", "_ORM")):
        return "arm"
    if name.endswith(("_M", "_MASK", "_MASKD")):
        return "mask"
    if name.endswith(("_D", "_BC", "_BASECOLOR", "_ALBEDO")):
        return "color"
    return None


# ---------------------------------------------------------------- Matériaux

def build_master():
    """M_InZOI_Part : texture de couleur x 3 teintes par masque (R, V, B) x Tint x SkinTint, normale, ARM"""
    existing = load(MASTER)
    if existing:
        return existing
    folder, name = split(MASTER)
    material = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    set_prop(material, "used_with_skeletal_mesh", True)

    default_arm = import_texture("Characters/Materials/DefaultTextures/T_Default_ARM", "arm")
    default_mask = import_texture("Characters/Materials/DefaultTextures/T_Default_Black_M", "mask")
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    flat = unreal.load_asset("/Engine/EngineMaterials/FlatNormal")
    ST = unreal.MaterialSamplerType

    def sampler(param, texture, sampler_type, x, y):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        set_prop(node, "parameter_name", param)
        if texture:
            set_prop(node, "texture", texture)
        set_prop(node, "sampler_type", sampler_type)
        return node

    def vector(param, x, y):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
        set_prop(node, "parameter_name", param)
        set_prop(node, "default_value", unreal.LinearColor(1, 1, 1, 1))
        return node

    def multiply(a, a_out, b, b_out, x, y):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, x, y)
        MEL.connect_material_expressions(a, a_out, node, "A")
        MEL.connect_material_expressions(b, b_out, node, "B")
        return node

    base = sampler("BaseColorMap", white, ST.SAMPLERTYPE_COLOR, -1400, -300)
    mask = sampler("ColorMask", default_mask, ST.SAMPLERTYPE_MASKS, -1400, 0)
    current, current_out = base, "RGB"
    for index, channel in enumerate(["R", "G", "B"]):
        color = vector("Color%d" % (index + 1), -1100, 200 + index * 150)
        lerp = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -900, 200 + index * 150)
        set_prop(lerp, "const_a", 1.0)
        MEL.connect_material_expressions(color, "", lerp, "B")
        MEL.connect_material_expressions(mask, channel, lerp, "Alpha")
        current = multiply(current, current_out, lerp, "", -700 + index * 120, -200)
        current_out = ""
    current = multiply(current, "", vector("Tint", -500, 100), "", -250, -200)
    current = multiply(current, "", vector("SkinTint", -250, 100), "", -50, -200)
    MEL.connect_material_property(current, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = sampler("NormalMap", flat, ST.SAMPLERTYPE_NORMAL, -500, 400)
    MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    arm = sampler("ARMMap", default_arm, ST.SAMPLERTYPE_MASKS, -500, 700)
    MEL.connect_material_property(arm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(arm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(arm, "B", unreal.MaterialProperty.MP_METALLIC)

    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    log("Matériau maître créé : " + MASTER)
    return material


def read_material_json(rel, depth=0):
    """Textures et couleurs d'une Material Instance exportée en JSON (avec celles de ses parents)"""
    textures, colors = {}, {}
    filename = src(rel, ".json") if rel else ""
    if not rel or not os.path.isfile(filename):
        return textures, colors
    try:
        with open(filename, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return textures, colors
    objects = data if isinstance(data, list) else [data]
    props = {}
    for obj in objects:
        if "Material" in obj.get("Type", ""):
            props = obj.get("Properties", {})
            break
    parent = props.get("Parent")
    if isinstance(parent, dict) and depth < 4:
        textures, colors = read_material_json(game_rel(parent.get("ObjectPath", "")), depth + 1)
    for entry in props.get("TextureParameterValues", []) or []:
        value = entry.get("ParameterValue")
        path = value.get("ObjectPath", "") if isinstance(value, dict) else str(value)
        texture_rel = game_rel(path)
        role = texture_role(texture_rel) if texture_rel else None
        if role:
            textures[role] = texture_rel
    for entry in props.get("VectorParameterValues", []) or []:
        name = (entry.get("ParameterInfo") or {}).get("Name", "")
        value = entry.get("ParameterValue") or {}
        if isinstance(value, dict) and "R" in value:
            colors[name] = unreal.LinearColor(value["R"], value["G"], value["B"], 1.0)
    return textures, colors


def mask_colors(colors):
    """Couleurs des canaux du masque : paramètres « Custom(R) », « Color1 »... -> Color1, Color2, Color3"""
    result = {}
    for name, color in colors.items():
        lower = name.lower()
        for index, keys in enumerate([("(r)", "color1", "_r"), ("(g)", "color2", "_g"), ("(b)", "color3", "_b")]):
            if any(key in lower for key in keys):
                result["Color%d" % (index + 1)] = color
    return result


def make_instance(asset_path, textures, colors, skin):
    existing = load(asset_path)
    if existing:
        return existing
    master = build_master()
    folder, name = split(asset_path)
    instance = asset_tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(instance, master)
    for role, texture_rel in textures.items():
        if skin and role == "mask":
            continue  # les masques de peau ne sont pas des masques de couleur
        texture = import_texture(texture_rel, role)
        if texture:
            MEL.set_material_instance_texture_parameter_value(instance, TEXTURE_PARAMS[role], texture)
    if "mask" in textures and not skin:
        for param, color in mask_colors(colors).items():
            MEL.set_material_instance_vector_parameter_value(instance, param, color)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance)
    stats["materials"] += 1
    return instance


def part_material(mi_rel):
    """Material Instance de notre matériau maître d'après celle du jeu étudié (même chemin, préfixe conservé)"""
    textures, colors = read_material_json(mi_rel)
    if not textures:
        # Pas de JSON lisible : on cherche les textures rangées à côté (dossier Texture/ du vêtement)
        textures = scan_textures(mi_rel.rsplit("/Materials/", 1)[0] if "/Materials/" in mi_rel else mi_rel.rsplit("/", 1)[0])
    return make_instance(dest(mi_rel), textures, colors, skin=False)


def scan_textures(rel_folder, prefer=("Main_D", "_D")):
    """Textures d'un dossier d'asset (Texture/ ou Textures/) classées par rôle"""
    found = {}
    for sub in ("Texture", "Textures", ""):
        root = os.path.join(EXPORTS, *rel_folder.split("/"), sub) if sub else os.path.join(EXPORTS, *rel_folder.split("/"))
        if not os.path.isdir(root):
            continue
        names = sorted(f[:-4] for f in os.listdir(root) if f.lower().endswith(".png"))
        for wanted in prefer:
            match = next((n for n in names if n.endswith(wanted)), None)
            if match and "color" not in found:
                found["color"] = (rel_folder + "/" + sub + "/" + match).replace("//", "/")
        for name in names:
            role = texture_role(name)
            if role in ("normal", "arm") and role not in found:
                found[role] = (rel_folder + "/" + sub + "/" + name).replace("//", "/")
    return found


# ---------------------------------------------------------------- Modèles

def find_mesh(asset_path):
    """Le Skeletal Mesh importé. Interchange (glTF) le range dans <nom>/SkeletalMeshes/<nom>
    au lieu de <nom> directement : on cherche aux deux endroits, puis dans tout le dossier <nom>."""
    folder, name = split(asset_path)
    for candidate in (asset_path, "%s/%s/SkeletalMeshes/%s" % (folder, name, name)):
        asset = load(candidate)
        if isinstance(asset, unreal.SkeletalMesh):
            return asset
    sub = folder + "/" + name
    if EAL.does_directory_exist(sub):
        for path in EAL.list_assets(sub, recursive=True, include_folder=False):
            data = EAL.find_asset_data(path)
            if str(data.asset_class_path.asset_name) == "SkeletalMesh":
                return unreal.load_asset(path)
    return None


USAGE_CHECKED = set()


def ensure_skeletal_usage(mesh):
    """Les matériaux créés par l'import (vêtements, chaussures...) n'ont pas l'usage « Skeletal Mesh »
    quand on importe sans rendu (-nullrhi) : en jeu Unreal met alors le damier à leur place.
    On coche l'usage sur le matériau de base (s'il est dans le projet) et on réenregistre."""
    try:
        slots = mesh.get_editor_property("materials")
    except Exception:  # noqa: BLE001
        return
    for slot in slots:
        material = slot.get_editor_property("material_interface")
        if not material:
            continue
        path = material.get_path_name()
        if path in USAGE_CHECKED:
            continue
        USAGE_CHECKED.add(path)
        fixed = False
        setter = getattr(MEL, "set_material_usage", None)
        if setter and hasattr(unreal, "MaterialUsage"):
            try:
                setter(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
                fixed = True
            except Exception:  # noqa: BLE001
                pass
        base = material.get_base_material() if hasattr(material, "get_base_material") else material
        if base and base.get_path_name().startswith("/Game/"):
            try:
                if not base.get_editor_property("used_with_skeletal_mesh"):
                    base.set_editor_property("used_with_skeletal_mesh", True)
                    MEL.recompile_material(base)
                    EAL.save_loaded_asset(base, only_if_is_dirty=False)
                    log("Usage Skeletal Mesh ajouté : " + base.get_path_name())
                fixed = True
            except Exception as error:  # noqa: BLE001
                log("Usage Skeletal Mesh impossible sur %s : %s" % (base.get_path_name(), error))
        if path.startswith("/Game/") and material != base:
            material.modify()
            EAL.save_loaded_asset(material, only_if_is_dirty=False)
        if not fixed:
            log("Matériau sans usage Skeletal Mesh (damier possible en jeu) : %s, base %s" % (
                path, base.get_path_name() if base else "?"))


def import_mesh(rel):
    if not rel:
        return None
    asset_path = dest(rel)
    filename = src(rel, ".glb")
    existing = find_mesh(asset_path)
    needs_fix = os.path.isfile(filename) and glb_needs_fix(filename)
    if existing and (not needs_fix or rel in FIXED):
        ensure_skeletal_usage(existing)
        return existing
    if not os.path.isfile(filename):
        stats["missing"].append(rel + ".glb")
        return None
    if rel in IGNORED or rel.rsplit("/", 1)[-1] in IGNORED:
        stats["skipped"].append(rel)
        return None
    with open(IN_PROGRESS, "w", encoding="utf-8") as handle:
        handle.write(rel)
    folder, name = split(asset_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fixed_glb(filename) if needs_fix else filename)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    if os.path.isfile(IN_PROGRESS):
        os.remove(IN_PROGRESS)
    mesh = find_mesh(asset_path)
    if mesh and needs_fix:
        FIXED.add(rel)
        with open(FIXED_FILE, "a", encoding="utf-8") as handle:
            handle.write(rel + "\n")
        if existing:
            log("Réimporté sans morphs (yeux, dents, peau) : " + name)
    if mesh:
        ensure_skeletal_usage(mesh)
        stats["meshes"] += 1
        if GARBAGE_EVERY and stats["meshes"] % GARBAGE_EVERY == 0:
            EAL.save_directory(DEST, only_if_is_dirty=True, recursive=True)
            unreal.SystemLibrary.collect_garbage()
    else:
        log("Import raté : " + filename)
    return mesh


def material_slot_names(mesh):
    try:
        return [str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("materials")]
    except Exception:  # noqa: BLE001
        return []


SKIN_MASTER = DEST + "/Materials/M_InZOI_Skin"
EYE_MASTER = DEST + "/Materials/M_InZOI_Eye"
TEETH_MATERIAL = DEST + "/Materials/M_InZOI_Teeth"
HIDDEN_MATERIAL = DEST + "/Materials/M_InZOI_Hidden"
EYE_SLOTS = set()


def new_material(asset_path):
    folder, name = split(asset_path)
    material = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    set_prop(material, "used_with_skeletal_mesh", True)
    return material


def finish_material(material, asset_path):
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    log("Matériau créé : " + asset_path)
    return material


def constant(material, value, x, y):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    set_prop(node, "r", value)
    return node


def build_skin_master():
    """M_InZOI_Skin : la couleur _Main_D d'inZOI (déjà une vraie couleur de peau) x SkinTint, rien d'autre.
    Les _Main_M d'inZOI sont des normales de détail encodées à leur façon : on ne s'en sert pas pour l'instant."""
    existing = load(SKIN_MASTER)
    if existing:
        return existing
    material = new_material(SKIN_MASTER)
    base = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
    set_prop(base, "parameter_name", "BaseColorMap")
    set_prop(base, "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    tint = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 50)
    set_prop(tint, "parameter_name", "SkinTint")
    set_prop(tint, "default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, -100)
    MEL.connect_material_expressions(base, "RGB", mul, "A")
    MEL.connect_material_expressions(tint, "", mul, "B")
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(constant(material, 0.55, -400, 150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(constant(material, 0.35, -400, 250), "", unreal.MaterialProperty.MP_SPECULAR)
    return finish_material(material, SKIN_MASTER)


EYE_VERSION = 2  # à augmenter quand le dessin de l'œil change : le matériau est alors refait


def build_eye_master():
    """M_InZOI_Eye : blanc de l'œil + iris (IrisColor) + pupille, dessinés d'après les UV du globe
    (iris centré en 0,5 0,5 sur les têtes inZOI). La couleur des yeux du joueur passe par IrisColor."""
    existing = load(EYE_MASTER)
    if existing and EAL.get_metadata_tag(existing, "InZOIVersion") == str(EYE_VERSION):
        return existing
    if existing:
        # Version précédente : on vide le graphe et on le refait au même endroit (les têtes gardent le lien)
        MEL.delete_all_material_expressions(existing)
        material = existing
    else:
        material = None
    material = material or new_material(EYE_MASTER)
    EAL.set_metadata_tag(material, "InZOIVersion", str(EYE_VERSION))
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -200)
    iris = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, 0)
    set_prop(iris, "parameter_name", "IrisColor")
    set_prop(iris, "default_value", unreal.LinearColor(0.25, 0.15, 0.08, 1))
    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -500, -100)
    set_prop(custom, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    set_prop(custom, "code", (
        "float d = distance(UV, float2(0.5, 0.5));\n"
        "float irisMask = 1.0 - smoothstep(0.115, 0.13, d);\n"
        "float pupil = 1.0 - smoothstep(0.035, 0.045, d);\n"
        "float ring = smoothstep(0.08, 0.125, d);\n"
        "float3 sclera = float3(0.80, 0.78, 0.76) * (1.0 - 0.25 * smoothstep(0.2, 0.45, d));\n"
        "float3 irisColor = Iris.rgb * lerp(1.0, 0.45, ring);\n"
        "float3 c = lerp(sclera, irisColor, irisMask);\n"
        "return lerp(c, float3(0.01, 0.01, 0.01), pupil);"))
    inputs = []
    for input_name in ("UV", "Iris"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        inputs.append(entry)
    set_prop(custom, "inputs", inputs)
    MEL.connect_material_expressions(uv, "", custom, "UV")
    MEL.connect_material_expressions(iris, "", custom, "Iris")
    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(constant(material, 0.08, -500, 150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(constant(material, 0.5, -500, 250), "", unreal.MaterialProperty.MP_SPECULAR)
    return finish_material(material, EYE_MASTER)


def build_teeth():
    existing = load(TEETH_MATERIAL)
    if existing:
        return existing
    material = new_material(TEETH_MATERIAL)
    color = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, -100)
    set_prop(color, "constant", unreal.LinearColor(0.78, 0.75, 0.68, 1))
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(constant(material, 0.35, -500, 100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish_material(material, TEETH_MATERIAL)


def build_hidden():
    """Pour les couches d'effet d'inZOI (larmes, ombre des yeux, salive) qu'on ne sait pas reproduire : invisibles"""
    existing = load(HIDDEN_MATERIAL)
    if existing:
        return existing
    material = new_material(HIDDEN_MATERIAL)
    set_prop(material, "blend_mode", unreal.BlendMode.BLEND_MASKED)
    MEL.connect_material_property(constant(material, 0.0, -500, 0), "", unreal.MaterialProperty.MP_OPACITY_MASK)
    return finish_material(material, HIDDEN_MATERIAL)


def force_color_texture(texture):
    """Une texture de couleur importée plus tôt avec un autre rôle (masque, normale...) garde ces réglages :
    on les remet en couleur sRGB et on note ce qui a été corrigé dans le journal"""
    TCS = unreal.TextureCompressionSettings
    before = (str(texture.get_editor_property("compression_settings")), texture.get_editor_property("srgb"),
              str(texture.get_editor_property("address_x")), str(texture.get_editor_property("lod_group")))
    changed = False
    if texture.get_editor_property("compression_settings") != TCS.TC_DEFAULT:
        set_prop(texture, "compression_settings", TCS.TC_DEFAULT)
        changed = True
    if not texture.get_editor_property("srgb"):
        set_prop(texture, "srgb", True)
        changed = True
    for axis in ("address_x", "address_y"):
        if texture.get_editor_property(axis) != unreal.TextureAddress.TA_WRAP:
            set_prop(texture, axis, unreal.TextureAddress.TA_WRAP)
            changed = True
    log("Texture de peau %s : %s%s" % (texture.get_name(), before, " -> corrigée" if changed else ""))
    if changed:
        EAL.save_loaded_asset(texture)


def skin_instance(asset_path, color_rel):
    existing = load(asset_path)
    if existing:
        texture = MEL.get_material_instance_texture_parameter_value(existing, "BaseColorMap")
        if texture:
            force_color_texture(texture)
        return existing
    folder, name = split(asset_path)
    instance = asset_tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(instance, build_skin_master())
    texture = import_texture(color_rel, "color") if color_rel else None
    if texture:
        force_color_texture(texture)
        MEL.set_material_instance_texture_parameter_value(instance, "BaseColorMap", texture)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance)
    stats["materials"] += 1
    return instance


def apply_skin(mesh, rel_folder, age, only_skin_slots):
    """Peau sur le corps ou la tête, plus yeux et dents des têtes ; renvoie les emplacements teintés par la couleur de peau"""
    prefer = ("Youth_D", "Main_D", "_D") if age == "Youth" else ("Main_D", "_D")
    textures = scan_textures(rel_folder, prefer)
    mi_folder = dest(rel_folder + "/Materials")
    # Nouveau nom (MI_SkinV2_) : les anciennes instances mélangeaient d'autres textures
    instance = skin_instance(mi_folder + "/MI_SkinV2_" + mesh.get_name(), textures.get("color"))
    mesh.modify()
    materials = mesh.get_editor_property("materials")
    tinted = []
    for index, slot in enumerate(materials):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        lower = slot_name.lower()
        if "eyeball" in lower:
            slot.set_editor_property("material_interface", build_eye_master())
            EYE_SLOTS.add(slot_name)
            continue
        if any(key in lower for key in ("saliva", "eyeao", "eyetear", "eye_ao", "eye_tear", "lash", "cartilage")):
            slot.set_editor_property("material_interface", build_hidden())
            continue
        if "teeth" in lower or "tooth" in lower or "tongue" in lower:
            slot.set_editor_property("material_interface", build_teeth())
            continue
        is_skin = any(key in lower for key in ("skin", "head", "body", "face"))
        if only_skin_slots and not is_skin and not (index == 0 and len(materials) == 1):
            continue
        slot.set_editor_property("material_interface", instance)
        tinted.append(slot_name)
    mesh.set_editor_property("materials", materials)
    # Sans only_if_is_dirty=False, Unreal ne voyait pas le changement (« Tous les fichiers sont déjà
    # enregistrés ») et le mesh gardait le matériau d'Interchange, avec sa normale inZOI qui strie la peau
    mesh.modify()
    EAL.save_loaded_asset(mesh, only_if_is_dirty=False)
    placed = [m.get_editor_property("material_interface") for m in mesh.get_editor_property("materials")]
    log("Matériaux enregistrés sur %s : %s" % (mesh.get_name(), ", ".join(m.get_name() if m else "aucun" for m in placed)))
    texture = MEL.get_material_instance_texture_parameter_value(instance, "BaseColorMap")
    log("Peau sur %s : %s (emplacements du mesh : %s) ; matériau %s, texture %s" % (
        mesh.get_name(), ", ".join(tinted) or "aucun", ", ".join(material_slot_names(mesh)),
        instance.get_path_name(), texture.get_path_name() if texture else "AUCUNE"))
    return tinted


# ---------------------------------------------------------------- Déformation du corps

GROUP_FR = OrderedDict([
    ("Head", ("Tête", "Taille de la tête")),
    ("Neck", ("Haut du corps", "Cou")),
    ("Shoulder", ("Haut du corps", "Largeur des épaules")),
    ("Shoulder_Y", ("Haut du corps", "Épaules en avant / en arrière")),
    ("Shoulder_Z", ("Haut du corps", "Hauteur des épaules")),
    ("RibCage", ("Haut du corps", "Largeur du torse")),
    ("RibCage_Y", ("Haut du corps", "Épaisseur du torse")),
    ("Chest", ("Haut du corps", "Poitrine")),
    ("BreastMoveX", ("Haut du corps", "Écart de la poitrine")),
    ("BreastMoveZ", ("Haut du corps", "Hauteur de la poitrine")),
    ("Belly", ("Ventre et hanches", "Ventre")),
    ("Belly_X", ("Ventre et hanches", "Hauteur du ventre")),
    ("Belly_Y", ("Ventre et hanches", "Ventre en avant")),
    ("Pelvis", ("Ventre et hanches", "Bassin")),
    ("Hip_Side", ("Ventre et hanches", "Largeur des hanches")),
    ("Hip_Y", ("Ventre et hanches", "Fessier")),
    ("Hip_Z", ("Ventre et hanches", "Hauteur des hanches")),
    ("UpperArm", ("Bras et jambes", "Bras")),
    ("LowerArm", ("Bras et jambes", "Avant-bras")),
    ("Hand", ("Bras et jambes", "Mains")),
    ("Thigh", ("Bras et jambes", "Cuisses")),
    ("Calf", ("Bras et jambes", "Mollets")),
])


def float_of(row, key, default=0.0):
    try:
        return float(row.get(key) or default)
    except ValueError:
        return default


def body_sliders(gender, age):
    """Curseurs du corps d'après Morphologie_Corps_Os.csv (groupes d'os positifs / négatifs, coefficients muscles / masse)"""
    groups = OrderedDict()
    for row in read_csv(MORPHO_BODY):
        if row.get("genre") != gender or row.get("age") != age:
            continue
        group = groups.setdefault(row["groupe"], {"pos": [], "neg": [], "muscle": None, "mass": None})
        offset = unreal.TrainerBoneOffset()
        offset.set_editor_property("bone", row["os"])
        offset.set_editor_property("location", unreal.Vector(float_of(row, "loc_x"), float_of(row, "loc_y"), float_of(row, "loc_z")))
        offset.set_editor_property("rotation", unreal.Quat(float_of(row, "rot_x"), float_of(row, "rot_y"), float_of(row, "rot_z"), float_of(row, "rot_w", 1.0)))
        offset.set_editor_property("scale", unreal.Vector(float_of(row, "scale_x"), float_of(row, "scale_y"), float_of(row, "scale_z")))
        group["pos" if row.get("signe") == "+" else "neg"].append(offset)
        if group["muscle"] is None:
            group["muscle"] = float_of(row, "delta_muscle")
            group["mass"] = float_of(row, "delta_masse")

    sliders = []

    def slider(slider_id, label, section, signed=False):
        definition = unreal.TrainerSliderDef()
        definition.set_editor_property("id", slider_id)
        definition.set_editor_property("label", label)
        definition.set_editor_property("section", section)
        definition.set_editor_property("group", unreal.TrainerSliderGroup.BODY)
        definition.set_editor_property("signed", signed)
        return definition

    sliders.append(slider("Muscle", "Muscles", "Silhouette", True))
    sliders.append(slider("Masse", "Corpulence", "Silhouette", True))
    ordered = [name for name in GROUP_FR if name in groups] + [name for name in groups if name not in GROUP_FR]
    for name in ordered:
        group = groups[name]
        section, label = GROUP_FR.get(name, ("Autres", name))
        definition = slider("Corps_" + name, label, section)
        definition.set_editor_property("bones_positive", group["pos"])
        definition.set_editor_property("bones_negative", group["neg"])
        definition.set_editor_property("muscle_coef", group["muscle"] or 0.0)
        definition.set_editor_property("mass_coef", group["mass"] or 0.0)
        sliders.append(definition)
    return sliders


# ---------------------------------------------------------------- Catalogue

def category_of(mesh_rel, slot):
    parts = mesh_rel.split("/")
    # Characters/Costumes/Female/Top/TShirt/Long/TS_L001/Mesh/SKM_...
    if len(parts) > 5 and parts[1] == "Costumes":
        key = parts[4]
        label = CATEGORY_FR.get(key, key)
        if key == "TShirt" and len(parts) > 6:
            label = "T-shirts " + CATEGORY_FR.get(parts[5], parts[5]).lower()
        return label
    return ""


def parse_list(text):
    try:
        value = json.loads(text) if text and text.strip().startswith("[") else []
    except ValueError:
        value = []
    return [v for v in value if isinstance(v, str)]


def build_parts(slow):
    parts = []
    defaults = {}  # base id -> {slot: part id}
    for slot, slot_label, _tab, _required, _replaces, table in SLOTS:
        rows = read_csv(os.path.join(TABLES, table))
        for (gender, age), (base_id, _label, _male, _body) in BASES.items():
            if age not in AGES:
                continue
            meshes = OrderedDict()
            for row in rows:
                if row.get("GenderType") != gender or row.get("BodyAgeType") != age:
                    continue
                if row.get("HiddenFromUI") == "True" or row.get("Mesh", "None") == "None":
                    continue
                mesh_rel = game_rel(row.get("Mesh"))
                if not mesh_rel:
                    continue
                meshes.setdefault(mesh_rel, []).append(row)
            mesh_items = list(meshes.items())
            if MAX_MESHES_PER_SLOT:
                mesh_items = mesh_items[:MAX_MESHES_PER_SLOT]
            counters = {}
            for mesh_rel, variants in mesh_items:
                if slow.should_cancel():
                    return parts, defaults
                slow.enter_progress_frame(1, "%s %s : %s" % (base_id, slot_label, mesh_rel.rsplit("/", 1)[-1]))
                mesh = import_mesh(mesh_rel)
                if not mesh:
                    continue
                if slot == "Head":
                    apply_skin(mesh, mesh_rel.rsplit("/Mesh/", 1)[0], age, True)
                if MAX_VARIANTS_PER_MESH:
                    variants = variants[:MAX_VARIANTS_PER_MESH]
                category = category_of(mesh_rel, slot)
                for row in variants:
                    counters[category] = counters.get(category, 0) + 1
                    part = unreal.TrainerPartDef()
                    part.set_editor_property("id", row["Id"])
                    part.set_editor_property("label", "%s %d" % (category or slot_label, counters[category]))
                    part.set_editor_property("slot", slot)
                    part.set_editor_property("category", category)
                    part.set_editor_property("bases", [base_id])
                    part.set_editor_property("mesh", mesh)
                    if slot == "Head":
                        # Têtes : peau, yeux et dents sont déjà posés sur le mesh (apply_skin). Les matériaux
                        # « OverrideMaterial » d'inZOI recouvraient la peau (taches) et les yeux (damier)
                        materials = []
                    else:
                        materials = [part_material(rel) for rel in (game_rel(p) for p in parse_list(row.get("OverrideMaterial"))) if rel]
                    part.set_editor_property("materials", [m for m in materials if m])
                    thumbnail = import_texture(game_rel(row.get("Thumbnail")), "ui")
                    if thumbnail:
                        part.set_editor_property("thumbnail", thumbnail)
                    parts.append(part)
                    defaults.setdefault(base_id, {}).setdefault(slot, row["Id"])
            log("%s / %s : %d modèles" % (base_id, slot_label, len(mesh_items)))
    return parts, defaults


# Onglet Modèles : un personnage complet par tête importée (base + tête + teint + silhouette)
PRESET_SKINS = [(0.98, 0.86, 0.76), (0.86, 0.68, 0.52), (0.64, 0.46, 0.33), (0.93, 0.78, 0.64),
                (0.42, 0.28, 0.19), (1.0, 0.93, 0.88), (0.76, 0.57, 0.42), (0.52, 0.36, 0.25)]
PRESET_SHAPES = [(0.0, 0.0), (0.4, 0.1), (-0.2, 0.3), (0.2, -0.3), (0.6, 0.2), (-0.1, -0.1)]


def full_presets(bases, parts):
    presets = []
    for base in bases:
        base_id = str(base.get_editor_property("id"))
        base_label = str(base.get_editor_property("label"))
        heads = [p for p in parts if str(p.get_editor_property("slot")) == "Head"
                 and base_id in [str(b) for b in p.get_editor_property("bases")]]
        for index, head in enumerate(heads):
            look = unreal.TrainerAppearance()
            look.set_editor_property("base", base_id)
            look.set_editor_property("outfit", [head.get_editor_property("id")])
            r, g, b = PRESET_SKINS[index % len(PRESET_SKINS)]
            look.set_editor_property("skin_color", unreal.LinearColor(r, g, b, 1.0))
            muscle, mass = PRESET_SHAPES[index % len(PRESET_SHAPES)]
            values = []
            for slider_id, value in (("Muscle", muscle), ("Masse", mass)):
                entry = unreal.TrainerSliderValue()
                entry.set_editor_property("slider", slider_id)
                entry.set_editor_property("value", value)
                values.append(entry)
            look.set_editor_property("sliders", values)
            preset = unreal.TrainerPresetDef()
            preset.set_editor_property("label", "%s %d" % (base_label, index + 1))
            thumbnail = head.get_editor_property("thumbnail")
            if thumbnail:
                preset.set_editor_property("thumbnail", thumbnail)
            preset.set_editor_property("look", look)
            presets.append(preset)
    log("Modèles complets : %d" % len(presets))
    return presets


def count_work():
    total = 0
    for _slot, _label, _tab, _req, _rep, table in SLOTS:
        rows = read_csv(os.path.join(TABLES, table))
        for (gender, age) in BASES:
            if age not in AGES:
                continue
            meshes = {game_rel(r.get("Mesh")) for r in rows if r.get("GenderType") == gender and r.get("BodyAgeType") == age}
            total += min(len(meshes), MAX_MESHES_PER_SLOT) if MAX_MESHES_PER_SLOT else len(meshes)
    return total + 10


def main():
    if not os.path.isdir(EXPORTS):
        unreal.log_error("[InZOI] Dossier introuvable : %s (corrige SOURCE en haut du script)" % EXPORTS)
        return

    already = len(EAL.list_assets(DEST, recursive=True, include_folder=False)) if EAL.does_directory_exist(DEST) else 0
    log("Déjà importé : %d assets (ils sont sautés, la barre repart de 0 mais passe vite dessus)" % already)

    with unreal.ScopedSlowTask(count_work(), "Import des ressources du créateur de personnage") as slow:
        slow.make_dialog(True)

        slow.enter_progress_frame(1, "Images du menu")
        for folder in ("UI/CharacterCustomize/Tex", "UI/CharacterCustomize/Sprite", "UI/ColorPickers"):
            import_ui_folder(folder)
        build_master()

        bases = []
        skin_slots = set()
        for (gender, age), (base_id, label, male, body_rel) in BASES.items():
            if age not in AGES:
                continue
            slow.enter_progress_frame(1, "Corps : " + label)
            body = import_mesh(body_rel)
            if not body:
                continue
            skin_slots.update(apply_skin(body, body_rel.rsplit("/Mesh/", 1)[0], age, False))
            base = unreal.TrainerBaseDef()
            base.set_editor_property("id", base_id)
            base.set_editor_property("label", label)
            base.set_editor_property("mesh", body)
            base.set_editor_property("male", male)
            base.set_editor_property("anim_class", unreal.TrainerAnimInstance.static_class())
            base.set_editor_property("sliders", body_sliders(gender, age))
            bases.append(base)

        parts, defaults = build_parts(slow)

        # Peau de la tête teintée aussi
        for part in parts:
            if str(part.get_editor_property("slot")) == "Head":
                mesh = part.get_editor_property("mesh")
                if mesh:
                    for slot in mesh.get_editor_property("materials"):
                        interface = slot.get_editor_property("material_interface")
                        if interface and interface.get_name().startswith("MI_Skin"):
                            skin_slots.add(str(slot.get_editor_property("material_slot_name")))

        for base in bases:
            base_defaults = defaults.get(str(base.get_editor_property("id")), {})
            base.set_editor_property("default_parts", [base_defaults[s] for s in DEFAULT_SLOTS if s in base_defaults])

        slot_defs = []
        for slot, slot_label, tab, required, replaces, _table in SLOTS:
            definition = unreal.TrainerPartSlotDef()
            definition.set_editor_property("slot", slot)
            definition.set_editor_property("label", slot_label)
            definition.set_editor_property("tab", tab)
            definition.set_editor_property("required", required)
            definition.set_editor_property("replaces", replaces)
            slot_defs.append(definition)

        presets = full_presets(bases, parts)

        data = load(DATA_ASSET)
        if not data:
            folder, name = split(DATA_ASSET)
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.TrainerAppearanceData)
            data = asset_tools.create_asset(name, folder, unreal.TrainerAppearanceData, factory)
        data.set_editor_property("bases", bases)
        data.set_editor_property("common_sliders", [])
        data.set_editor_property("part_slots", slot_defs)
        data.set_editor_property("full_presets", presets)
        data.set_editor_property("parts", parts)
        data.set_editor_property("skin_material_slots", sorted(skin_slots))
        data.set_editor_property("skin_color_parameter", "SkinTint")
        data.set_editor_property("eye_material_slots", sorted(EYE_SLOTS))
        data.set_editor_property("eye_color_parameter", "IrisColor")
        data.set_editor_property("drag_handles", [])  # les poignées de CC visaient des os CC_Base_*
        EAL.save_loaded_asset(data)
        EAL.save_directory(DEST, only_if_is_dirty=True, recursive=True)

    log("Terminé : %d textures, %d modèles, %d matériaux, %d pièces, %d bases." % (
        stats["textures"], stats["meshes"], stats["materials"], len(parts), len(bases)))
    if stats["skipped"]:
        log("%d modèles mis de côté car ils ont fait planter Unreal : %s" % (len(stats["skipped"]), ", ".join(r.rsplit("/", 1)[-1] for r in stats["skipped"])))
    if stats["missing"]:
        log("%d fichiers absents de l'export (ex. %s)" % (len(stats["missing"]), ", ".join(stats["missing"][:5])))
    log("Catalogue : " + DATA_ASSET + " (le jeu le prend automatiquement à la place de DA_TrainerAppearance)")


main()
