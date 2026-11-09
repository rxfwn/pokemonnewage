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
import math
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
MORPHO_FACE = os.path.join(SOURCE, "Index_Complet", "Morphologie_Visage_Os.csv")
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

# Vêtements qui pendent loin du corps : échelle des os d'aide bornée à 1 ± valeur (sinon le bas du t-shirt
# s'envolait derrière avec les fessiers agrandis de « Musclé »), voir FTrainerPartSlotDef::HelperScaleLimit
# 0,6 (et non 0,35) : à forte corpulence (0,86) le ventre et la poitrine traversaient le t-shirt (banc BancPerso B7)
LOOSE_SLOTS = {"Top": 0.6, "Outer": 0.6}

# Pièces portées au départ (si la base n'a rien de choisi)
DEFAULT_SLOTS = ["Head", "Hair", "Eyebrows", "Eyelashes", "Top", "Bottom", "Shoes"]
# Tenue de départ « normale » : la première pièce de la base (homme ou femme) dans ces catégories, dans l'ordre
DEFAULT_CATEGORIES = {
    "Top": ["T-shirts manches courtes", "T-shirts manches longues", "Chemises", "Pulls"],
    "Bottom": ["Pantalons", "Jeans", "Jupes mi-longues", "Jupes longues"],
    "Shoes": ["Baskets", "Mocassins", "Bottes"],
}

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


GROOM_REL = "Characters/AvatarParts/Groom"


# Exports FModel faits à part et rangés à la racine de l'exercice (ex. « MakeupV2 » : masques de maquillage, taches de
# rousseur, grains de beauté, cicatrices, tatouages + leurs tables) : chaque <dossier>/BlueClient/Content ou
# <dossier>/Output/Exports/BlueClient/Content est aussi fouillé
EXTRA_EXPORT_DIRS = ["MakeupV2", "makeup", os.path.join("V2", "Exports_depuis_23h")]


def extra_exports():
    roots = []
    for name in EXTRA_EXPORT_DIRS:
        # V2/Exports_depuis_23h (exports du 2026-11-08) : masques de maquillage A-F complets, textures d'yeux, corps .uemodel
        for tail in (("BlueClient", "Content"), ("Output", "Exports", "BlueClient", "Content"), ("Exports", "BlueClient", "Content")):
            root = os.path.join(SOURCE, name, *tail)
            if os.path.isdir(root):
                roots.append(root)
    return roots


def src(rel, ext):
    path = os.path.join(EXPORTS, *rel.split("/")) + ext
    if not os.path.isfile(path):
        for root in extra_exports():
            other = os.path.join(root, *rel.split("/")) + ext
            if os.path.isfile(other):
                return other
    # Poils (barbes) : aussi cherchés dans la copie Groom/ à la racine de l'exercice
    if not os.path.isfile(path) and rel.startswith(GROOM_REL + "/"):
        copy = os.path.join(SOURCE, "Groom", *rel[len(GROOM_REL) + 1:].split("/")) + ext
        if os.path.isfile(copy):
            return copy
    return path


def read_table(name):
    """Table JSON d'inZOI (Data/JsonResult/<name>.json) : export principal puis exports à part. [] si absente"""
    candidates = [os.path.join(EXPORTS, "Data", "JsonResult", name + ".json")]
    candidates += [os.path.join(root, "Data", "JsonResult", name + ".json") for root in extra_exports()]
    for filename in candidates:
        try:
            with open(filename, encoding="utf-8") as handle:
                rows = json.load(handle)
        except (OSError, ValueError):
            continue
        if isinstance(rows, list):
            return rows
    log("Table %s.json introuvable (export principal et %s)" % (name, ", ".join(EXTRA_EXPORT_DIRS)))
    return []


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

CLOTH_INFLATE = 0.35  # cm : poussée des vêtements vers l'extérieur (réglable par vêtement : paramètre Inflate)
PART_VERSION = 3  # à augmenter quand le graphe de M_InZOI_Part change : refait au même endroit


def build_master():
    """M_InZOI_Part. Couleur comme dans inZOI : la texture _D telle quelle, ou, si la variante a des couleurs
    (CustomIntensity > 0, ou une couleur choisie dans le nuancier), chaque zone du masque _M (R, V, B) prend sa couleur
    Color1-3, avec le grain de la texture. Puis x Tint x SkinTint. Normale et ARM classiques."""
    existing = load(MASTER)
    if existing and EAL.get_metadata_tag(existing, "InZOIVersion") == str(PART_VERSION):
        return existing
    if existing:
        MEL.delete_all_material_expressions(existing)
        material = existing
    else:
        material = new_material(MASTER)

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

    base = sampler("BaseColorMap", white, ST.SAMPLERTYPE_COLOR, -1400, -300)
    mask = sampler("ColorMask", default_mask, ST.SAMPLERTYPE_MASKS, -1400, 0)
    intensity = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1100, 650)
    set_prop(intensity, "parameter_name", "CustomIntensity")
    set_prop(intensity, "default_value", 0.0)
    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -500, -100)
    set_prop(custom, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    set_prop(custom, "code", (
        "float picked = (any(C1.rgb < 0.999) || any(C2.rgb < 0.999) || any(C3.rgb < 0.999)) ? 1.0 : 0.0;\n"
        "float amount = saturate(max(I, picked));\n"
        "float3 grain = saturate(D.rgb * 2.2);\n"
        "float3 c = D.rgb;\n"
        "c = lerp(c, C1.rgb * grain, M.r);\n"
        "c = lerp(c, C2.rgb * grain, M.g);\n"
        "c = lerp(c, C3.rgb * grain, M.b);\n"
        "return lerp(D.rgb, c, amount) * Tint.rgb * Skin.rgb;"))
    inputs = []
    for input_name in ("D", "M", "C1", "C2", "C3", "I", "Tint", "Skin"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        inputs.append(entry)
    set_prop(custom, "inputs", inputs)
    MEL.connect_material_expressions(base, "RGB", custom, "D")
    MEL.connect_material_expressions(mask, "RGB", custom, "M")
    for index in range(3):
        MEL.connect_material_expressions(vector("Color%d" % (index + 1), -1100, 200 + index * 150), "", custom, "C%d" % (index + 1))
    MEL.connect_material_expressions(intensity, "", custom, "I")
    MEL.connect_material_expressions(vector("Tint", -800, 650), "", custom, "Tint")
    MEL.connect_material_expressions(vector("SkinTint", -800, 800), "", custom, "Skin")
    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = sampler("NormalMap", flat, ST.SAMPLERTYPE_NORMAL, -500, 400)
    MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    arm = sampler("ARMMap", default_arm, ST.SAMPLERTYPE_MASKS, -500, 700)
    MEL.connect_material_property(arm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(arm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(arm, "B", unreal.MaterialProperty.MP_METALLIC)
    # Vêtement poussé de quelques millimètres le long de sa normale : la peau ne passe plus au travers là où le
    # vêtement colle au corps (inZOI déforme ses vêtements sur le corps, nous non)
    inflate = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, 1000)
    set_prop(inflate, "parameter_name", "Inflate")
    set_prop(inflate, "default_value", CLOTH_INFLATE)
    normal_ws = MEL.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -800, 1100)
    push = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -500, 1050)
    MEL.connect_material_expressions(normal_ws, "", push, "A")
    MEL.connect_material_expressions(inflate, "", push, "B")
    MEL.connect_material_property(push, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    EAL.set_metadata_tag(material, "InZOIVersion", str(PART_VERSION))
    return finish_material(material, MASTER)


def material_json_values(data, textures, colors):
    """Format de l'exercice : {"Textures": {nom: {ObjectPath}}, "Colors": {nom: {R, G, B}}, "Scalars": {nom: valeur}}"""
    for name, value in (data.get("Textures") or {}).items():
        texture_rel = game_rel(value.get("ObjectPath", "")) if isinstance(value, dict) else None
        role = texture_role(texture_rel) if texture_rel else None
        if role and (role not in textures or name in ("Base Color Map", "RGB Mask Map", "ARM Map", "Normal Map")):
            textures[role] = texture_rel
    for name, value in (data.get("Colors") or {}).items():
        if isinstance(value, dict) and "R" in value:
            colors[name] = unreal.LinearColor(value["R"], value["G"], value["B"], 1.0)
    scalars = data.get("Scalars") or {}
    if "Custom Color Intensity" in scalars:
        colors["__intensity"] = float(scalars["Custom Color Intensity"])


def read_material_json(rel, depth=0):
    """Textures et couleurs d'une Material Instance exportée en JSON (avec celles de ses parents).
    « __intensity » = Custom Color Intensity d'inZOI (0 : la texture telle quelle, sans les couleurs du masque)."""
    textures, colors = {}, {}
    filename = src(rel, ".json") if rel else ""
    if not rel or not os.path.isfile(filename):
        return textures, colors
    try:
        with open(filename, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return textures, colors
    if isinstance(data, dict) and ("Textures" in data or "Colors" in data):
        material_json_values(data, textures, colors)
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
    for entry in props.get("ScalarParameterValues", []) or []:
        if (entry.get("ParameterInfo") or {}).get("Name") == "Custom Color Intensity":
            colors["__intensity"] = float(entry.get("ParameterValue", 1.0))
    return textures, colors


def mask_colors(colors):
    """Couleurs des canaux du masque : paramètres « Custom(R) », « Color1 »... -> Color1, Color2, Color3"""
    result = {}
    for name, color in colors.items():
        if name.startswith("__"):
            continue
        lower = name.lower()
        for index, keys in enumerate([("(r)", "color1", "_r"), ("(g)", "color2", "_g"), ("(b)", "color3", "_b")]):
            if any(key in lower for key in keys):
                result["Color%d" % (index + 1)] = color
    return result


def make_instance(asset_path, textures, colors, skin):
    """Material Instance de M_InZOI_Part ; une instance déjà importée est remise à jour (textures, couleurs)"""
    instance = load(asset_path)
    if not instance:
        folder, name = split(asset_path)
        instance = asset_tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        stats["materials"] += 1
    MEL.set_material_instance_parent(instance, build_master())
    for role, texture_rel in textures.items():
        if skin and role == "mask":
            continue  # les masques de peau ne sont pas des masques de couleur
        texture = import_texture(texture_rel, role)
        if texture:
            MEL.set_material_instance_texture_parameter_value(instance, TEXTURE_PARAMS[role], texture)
    found = mask_colors(colors) if ("mask" in textures and not skin) else {}
    for index in range(1, 4):
        color = found.get("Color%d" % index, unreal.LinearColor(1, 1, 1, 1))
        MEL.set_material_instance_vector_parameter_value(instance, "Color%d" % index, color)
    # Sans valeur dans le JSON : couleurs présentes = variante colorée (intensité 1), sinon la texture telle quelle
    intensity = colors.get("__intensity", 1.0 if found else 0.0)
    MEL.set_material_instance_scalar_parameter_value(instance, "CustomIntensity", intensity if found else 0.0)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance)
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
# Chemin du mesh -> matériaux voulus, un par emplacement (rempli par apply_skin)
SKIN_MATERIALS = {}


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


SKIN_VERSION = 17  # à augmenter quand le graphe de la peau change : refait au même endroit (les têtes gardent le lien)
# Maquillage d'inZOI (table AppearanceMakeup) : catégorie, lettre du dossier de masques, emplacement du jeu (= nom du
# paramètre texture de M_InZOI_Skin, couleurs dans <emplacement>_R/_G/_B), libellé, force du mélange (les reflets d'inZOI
# mêlent un éclat blanc et une ombre de contour : à moitié pour rester naturels)
MAKEUP_PARTS = [
    ("Highlight", "A", "MakeupHighlight", "Reflets", 0.5),
    ("BlusherNoseFreckle", "B", "MakeupBlush", "Blush et taches", 1.0),
    ("Eyeshadow", "C", "MakeupEyeshadow", "Fard à paupières", 1.0),
    ("Eyeline", "D", "MakeupEyeliner", "Eye-liner", 1.0),
    ("Lip", "E", "MakeupLips", "Lèvres", 1.0),
    ("Eyebrow", "F", "MakeupBrows", "Sourcils dessinés", 1.0),
]
MAKEUP_MASKS = "Characters/Materials/DefaultTextures/Skin/MakeupMasks"
# Détails de peau d'inZOI (Visage > Maquillage, après le maquillage) : emplacement (= paramètre texture de M_InZOI_Skin),
# libellé, table, sorte. « single » = un masque à une couleur (<emplacement>_R, canaux lus selon <emplacement>_WR/_WG/
# _WB/_WA) ; « scar » = pareil mais la couleur vient de la texture _D de la cicatrice (SkinScarColor) ; « rgb » = comme
# le maquillage, une couleur par canal R, V, B (tatouages, peintures)
SKIN_FEATURES = [
    ("SkinFreckles", "Taches de rousseur", "AppearanceFreckles", "single"),
    ("SkinMoles", "Grains de beauté", "AppearanceMole", "single"),
    ("SkinScars", "Cicatrices", "AppearanceScar", "scar"),
    ("SkinTattoo", "Tatouages", "AppearanceTattoo", "rgb"),
    ("SkinFacePaint", "Peintures", "AppearanceTattoo", "rgb"),
]
SKIN_FEATURE_SINGULAR = {"SkinFreckles": "Taches", "SkinMoles": "Grain de beauté", "SkinScars": "Cicatrice",
                         "SkinTattoo": "Tatouage", "SkinFacePaint": "Peinture"}
SCAR_COLOR_PARAM = "SkinScarColor"
FRECKLE_COLOR = "8A5A3C"  # pas de couleur dans la table d'inZOI : brun clair
# Relief de la peau d'inZOI (dossier Skin de MakeupV2) : normale de détail de la tête (rides, pores, lèvres) par genre,
# et micro-relief répété partout (sinon la peau a l'air en plastique)
SKIN_TEXTURES = "Characters/Materials/DefaultTextures/Skin"
HEAD_DETAIL_NORMAL = {"Female": SKIN_TEXTURES + "/Female_Head_Detail_Full_N", "Male": SKIN_TEXTURES + "/Male_Head_Detail_Full_N"}
# Force du relief de la tête : à 1 les rides et plis (front, joues, cou) étaient bien trop marqués
HEAD_DETAIL_AMOUNT = 0.4
# Rugosité + AO cuite des têtes : les têtes sans _Main_RS propre partagent celle de Female_Head_001 (comme le jeu)
SKIN_RS_SHARED = "Characters/AvatarParts/Heads/Female/Female_Head_001/Textures/Female_Head_001_Main_RS"
# Relief et couleur du corps d'inZOI : (relief fin _Main_M, relief des muscles _Physique_M, couleur musclée _Muscle_D)
BODY_RELIEF = {g: ("Characters/AvatarParts/Body/%s/Textures/%s_Body_Main_M" % (g, g),
                   "Characters/AvatarParts/Body/%s/Textures/%s_Body_Physique_M" % (g, g),
                   "Characters/AvatarParts/Body/%s/Textures/%s_Body_Muscle_D" % (g, g)) for g in ("Male", "Female")}
# Réglés au banc (silhouettes M_Average01, M_Muscular01/03 comparées aux vignettes) : à 1, des abdominaux
# apparaissaient même à Muscles = 0 et les « Musclé » étaient bien plus dessinés que dans inZOI
BODY_RELIEF_AMOUNT = 0.4
BODY_PHYSIQUE_AMOUNT = 1.0
BODY_MUSCLE_COLOR_AMOUNT = 0.7
BODY_FAT_COLOR_AMOUNT = 0.8
SKIN_MICRO_NORMAL = [SKIN_TEXTURES + "/T_SkinMicro_N", "MetaHumans/Common/Materials/Textures/T_SkinMicroNormal"]


def makeup_blend(base, color):
    """Mélange du maquillage qui garde le grain de la peau : moitié couleur pure, moitié « lumière douce » sur la peau"""
    return "lerp(%s, (1.0 - 2.0 * %s) * %s * %s + 2.0 * %s * %s, 0.5)" % (color, color, base, base, color, base)


def default_mask_texture():
    """Une texture en réglage « masque » pour les échantillonneurs vides (sinon le matériau ne compile pas)"""
    candidates = ["Characters/Materials/DefaultTextures/T_Default_Black_M"]
    candidates += ["%s/Makeup_Part_%s/Makeup_Part_%s_001_M" % (MAKEUP_MASKS, letter, letter) for _c, letter, _s, _l, _b in MAKEUP_PARTS]
    candidates += IRIS_MASKS
    candidates += ["Characters/Materials/DefaultTextures/Skin/SkinFeatures/Mole/Head/Head_Mole_001_GRY"]
    for rel in candidates:
        texture = import_texture(rel, "mask")
        if texture:
            return texture
    return None


def build_skin_master():
    """M_InZOI_Skin : la couleur _Main_D d'inZOI (déjà une vraie couleur de peau) x SkinTint, puis le maquillage :
    pour chaque catégorie (MAKEUP_PARTS), un masque dont les canaux R, V, B prennent chacun une couleur (alpha = intensité).
    Les couleurs sont à alpha 0 par défaut : le corps (même matériau, autres UV) ne reçoit jamais de maquillage.
    Les _Main_M d'inZOI sont des normales de détail encodées à leur façon : on ne s'en sert pas pour l'instant."""
    existing = load(SKIN_MASTER)
    if existing and EAL.get_metadata_tag(existing, "InZOIVersion") == str(SKIN_VERSION):
        return existing
    if existing:
        MEL.delete_all_material_expressions(existing)
        material = existing
    else:
        material = new_material(SKIN_MASTER)
    base = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1400, -400)
    set_prop(base, "parameter_name", "BaseColorMap")
    set_prop(base, "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    tint = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1400, -150)
    set_prop(tint, "parameter_name", "SkinTint")
    set_prop(tint, "default_value", unreal.LinearColor(1, 1, 1, 1))

    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -400, -100)
    set_prop(custom, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    # Corps musclé d'inZOI (Male_Body_Muscle_D, ombres des abdominaux et des pectoraux) mélangé selon le curseur
    # Muscles (MuscleDefinition, posé par le jeu) ; MuscleColorAmount à 0 sur les têtes et le corps de femme
    muscle_color = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1400, -650)
    set_prop(muscle_color, "parameter_name", "MuscleColorMap")
    set_prop(muscle_color, "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    shared_source = getattr(getattr(unreal, "SamplerSourceMode", None), "SSM_WRAP_WORLD_GROUP_SETTINGS", None)
    if shared_source is not None:
        set_prop(muscle_color, "sampler_source", shared_source)
    muscle = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1400, -800)
    set_prop(muscle, "parameter_name", "MuscleDefinition")
    set_prop(muscle, "default_value", 0.0)
    muscle_amount = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1400, -900)
    set_prop(muscle_amount, "parameter_name", "MuscleColorAmount")
    set_prop(muscle_amount, "default_value", 0.0)
    # Corps fort d'inZOI (_Body_Fat_D : plis et poids du ventre) selon la corpulence (BodyFat, posé par le jeu)
    fat_color = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1700, -650)
    set_prop(fat_color, "parameter_name", "FatColorMap")
    set_prop(fat_color, "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    if shared_source is not None:
        set_prop(fat_color, "sampler_source", shared_source)
    fat = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1700, -800)
    set_prop(fat, "parameter_name", "BodyFat")
    set_prop(fat, "default_value", 0.0)
    fat_amount = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1700, -900)
    set_prop(fat_amount, "parameter_name", "FatColorAmount")
    set_prop(fat_amount, "default_value", 0.0)
    links = [(base, "RGB", "D"), (tint, "", "Tint"), (muscle_color, "RGB", "MD"), (muscle, "", "Mus"), (muscle_amount, "", "MusA"),
             (fat_color, "RGB", "FD"), (fat, "", "Fat"), (fat_amount, "", "FatA")]
    gloss_inputs = None
    lip_inputs = None
    code = ["float3 d = lerp(D.rgb, MD.rgb, saturate(Mus * MusA));",
            "d = lerp(d, FD.rgb, saturate(Fat * FatA));",
            "float3 c = d * Tint.rgb;"]
    mask_texture = default_mask_texture()
    if mask_texture:
        for index, (_category, _letter, slot, _label, blend) in enumerate(MAKEUP_PARTS):
            y = index * 450
            sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, y)
            set_prop(sample, "parameter_name", slot)
            set_prop(sample, "texture", mask_texture)
            set_prop(sample, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            # Échantillonneur partagé : 7 textures dans un seul matériau sans dépasser la limite de 16
            shared = getattr(getattr(unreal, "SamplerSourceMode", None), "SSM_WRAP_WORLD_GROUP_SETTINGS", None)
            if shared is not None:
                set_prop(sample, "sampler_source", shared)
            links.append((sample, "RGBA", "M%d" % index))
            for offset, channel in enumerate("RGB"):
                color = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -800, y + offset * 130)
                set_prop(color, "parameter_name", "%s_%s" % (slot, channel))
                set_prop(color, "default_value", unreal.LinearColor(0, 0, 0, 0))
                links.append((color, "", "%s%d" % (channel, index)))
                links.append((color, "A", "%s%da" % (channel, index)))  # alpha à part : la sortie principale n'a que RVB
                if slot == "MakeupLips" and channel == "B":
                    # Canal B du masque des lèvres = brillance (lipstick_Spec_Roughness / _Spec_Intensity du modèle) :
                    # R de la « couleur » = rugosité, alpha = intensité ; jamais affiché en couleur
                    lip_inputs = (sample, color)
                    continue
                if slot == "MakeupHighlight" and channel == "G":
                    # Canal G de Makeup_Part_A = BRILLANCE du highlight (pas une couleur) : l'afficher en couleur
                    # faisait des plaques claires sur les joues, le nez et le menton (peaux foncées 702, 706)
                    gloss_inputs = (sample, color)
                    continue
                code.append("c = lerp(c, %s, saturate(M%d.%s * %s%da * %.2f));" % (
                    makeup_blend("c", "%s%d.rgb" % (channel, index)), index, channel.lower(), channel, index, blend))
        shared = getattr(getattr(unreal, "SamplerSourceMode", None), "SSM_WRAP_WORLD_GROUP_SETTINGS", None)
        for index, (slot, _label, _table, kind) in enumerate(SKIN_FEATURES):
            y = (len(MAKEUP_PARTS) + index) * 450
            name = "F%d" % index
            sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, y)
            set_prop(sample, "parameter_name", slot)
            set_prop(sample, "texture", mask_texture)
            set_prop(sample, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            if shared is not None:
                set_prop(sample, "sampler_source", shared)
            links.append((sample, "RGBA", name))
            channels = "RGB" if kind == "rgb" else "R"
            for offset, channel in enumerate(channels):
                color = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -800, y + offset * 130)
                set_prop(color, "parameter_name", "%s_%s" % (slot, channel))
                default = unreal.LinearColor(1, 1, 1, 0) if kind == "scar" else unreal.LinearColor(0, 0, 0, 0)
                set_prop(color, "default_value", default)
                links.append((color, "", "%s%s" % (name, channel)))
                links.append((color, "A", "%s%sa" % (name, channel)))
            if kind == "rgb":
                for channel in "RGB":
                    code.append("c = lerp(c, %s, saturate(%s.%s * %s%sa));" % (
                        makeup_blend("c", "%s%s.rgb" % (name, channel)), name, channel.lower(), name, channel))
                continue
            # Canaux du masque qui portent le dessin (trouvés à l'import, R par défaut)
            for offset, channel in enumerate("RGBA"):
                weight = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, y + 150 + offset * 70)
                set_prop(weight, "parameter_name", "%s_W%s" % (slot, channel))
                set_prop(weight, "default_value", 1.0 if channel == "R" else 0.0)
                links.append((weight, "", "%sW%s" % (name, channel)))
            # Poids > 0 : le dessin est clair sur fond noir ; < 0 : sombre sur fond blanc (masque inversé)
            terms = ["saturate(%s.%s * %sW%s) + saturate((1.0 - %s.%s) * -%sW%s)" % (
                name, c.lower(), name, c, name, c.lower(), name, c) for c in "RGBA"]
            code.append("float m%d = max(max(%s, %s), max(%s, %s));" % (index, terms[0], terms[1], terms[2], terms[3]))
            if kind == "scar":
                scar = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, y + 250)
                set_prop(scar, "parameter_name", SCAR_COLOR_PARAM)
                set_prop(scar, "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
                if shared is not None:
                    set_prop(scar, "sampler_source", shared)
                links.append((scar, "RGB", "%sD" % name))
                # Couleur de la cicatrice (texture _D d'inZOI) teintée dans le nuancier (blanc = telle quelle)
                code.append("c = lerp(c, %sD.rgb * %sR.rgb, saturate(m%d * %sRa));" % (name, name, index, name))
            else:
                code.append("c = lerp(c, %s, saturate(m%d * %sRa));" % (makeup_blend("c", "%sR.rgb" % name), index, name))
    else:
        log("Peau : aucune texture de masque trouvée, maquillage désactivé")
    code.append("return c;")
    set_prop(custom, "code", "\n".join(code))
    entries = []
    for _node, _output, input_name in links:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        entries.append(entry)
    set_prop(custom, "inputs", entries)
    for node, output, input_name in links:
        MEL.connect_material_expressions(node, output, custom, input_name)
    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    gloss = None
    if gloss_inputs:
        gloss = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -800, 3300)
        set_prop(gloss, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        set_prop(gloss, "code", "return saturate(M.g * A);")
        entries = []
        for input_name in ("M", "A"):
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", input_name)
            entries.append(entry)
        set_prop(gloss, "inputs", entries)
        MEL.connect_material_expressions(gloss_inputs[0], "RGBA", gloss, "M")
        MEL.connect_material_expressions(gloss_inputs[1], "A", gloss, "A")
    lips = None
    if lip_inputs:
        # Rugosité propre au rouge à lèvres (lipstick_Spec_Roughness d'inZOI) : mélange selon masque x intensité
        lips = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -800, 3500)
        set_prop(lips, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        set_prop(lips, "code", "return saturate(M.b * A);")
        entries = []
        for input_name in ("M", "A"):
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", input_name)
            entries.append(entry)
        set_prop(lips, "inputs", entries)
        MEL.connect_material_expressions(lip_inputs[0], "RGBA", lips, "M")
        MEL.connect_material_expressions(lip_inputs[1], "A", lips, "A")
    build_skin_relief(material, gloss, lips, lip_inputs[1] if lip_inputs else None)
    # 7) Fausse lumière d'inZOI (T_Head_FakeLightMask_M, R = dégradé du centre du visage) : un peu d'émissif de la
    # couleur de peau ; FakeLightAmount à 0 par défaut (corps), posé par set_head_relief sur les têtes
    fake_texture = import_texture(FAKE_LIGHT_MASK, "mask")
    if fake_texture:
        fake_sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, 3900)
        set_prop(fake_sample, "parameter_name", "FakeLightMask")
        set_prop(fake_sample, "texture", fake_texture)
        set_prop(fake_sample, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        if shared_source is not None:
            set_prop(fake_sample, "sampler_source", shared_source)
        fake_amount = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1100, 4100)
        set_prop(fake_amount, "parameter_name", "FakeLightAmount")
        set_prop(fake_amount, "default_value", 0.0)
        fake = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -400, 4000)
        set_prop(fake, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        set_prop(fake, "code", "return C * F.r * FA;")
        entries = []
        for input_name in ("C", "F", "FA"):
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", input_name)
            entries.append(entry)
        set_prop(fake, "inputs", entries)
        MEL.connect_material_expressions(custom, "", fake, "C")
        MEL.connect_material_expressions(fake_sample, "RGBA", fake, "F")
        MEL.connect_material_expressions(fake_amount, "", fake, "FA")
        MEL.connect_material_property(fake, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # 4) Peaux foncées : profil de diffusion sous la peau d'inZOI (SSP_Head : rayon 0,425 / 0,052 / 0,018), plus
    # court que celui par défaut du moteur : moins d'orange vif sur le nez, les joues, les oreilles et les lèvres
    # Profil SSP_Head testé au banc (9 nov.) : taches orange vif près des oreilles et du cou ; profil par défaut gardé
    profile = skin_subsurface_profile() if USE_SKIN_PROFILE else None
    # Le matériau est refait au même endroit : sans profil, on retire celui d'un import précédent
    set_prop(material, "subsurface_profile", profile)
    MEL.connect_material_property(constant(material, 0.3, -400, 350), "", unreal.MaterialProperty.MP_SPECULAR)
    # Peau « Subsurface Profile » : la lumière diffuse sous la surface (oreilles, nez, lèvres) ; en éclairage normal la
    # peau paraissait en plastique. Grain : micro-relief des pores plus fin (40 répétitions) et plus marqué (0,5).
    set_prop(material, "shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE_PROFILE)
    EAL.set_metadata_tag(material, "InZOIVersion", str(SKIN_VERSION))
    return finish_material(material, SKIN_MASTER)


SKIN_PROFILE = DEST + "/Materials/SSP_InZOI_Skin"
USE_SKIN_PROFILE = False
FAKE_LIGHT_MASK = "Characters/Materials/DefaultTextures/Skin/T_Head_FakeLightMask_M"
FAKE_LIGHT_AMOUNT = 0.0  # 0,15 testé au banc (9 nov.) : visages pâles, délavés ; gardé disponible mais coupé


def skin_subsurface_profile():
    """Profil de diffusion de la peau d'après SSP_Head d'inZOI ; nul si l'API Python ne le permet pas"""
    profile = load(SKIN_PROFILE)
    try:
        if not profile:
            folder, name = split(SKIN_PROFILE)
            profile = asset_tools.create_asset(name, folder, unreal.SubsurfaceProfile, unreal.SubsurfaceProfileFactory())
        settings = profile.get_editor_property("settings")
        settings.set_editor_property("mean_free_path_color", unreal.LinearColor(1.0, 0.122, 0.042, 1.0))
        settings.set_editor_property("mean_free_path_distance", 6.0)
        profile.set_editor_property("settings", settings)
        EAL.save_loaded_asset(profile)
        return profile
    except Exception as error:
        log("Peau : profil de diffusion non créé (%s)" % error)
        return None


def build_skin_relief(material, gloss=None, lips=None, lip_rough=None):
    """Normale et rugosité de la peau : normale de détail de la tête (DetailNormal, force DetailNormalAmount, 0 sur le
    corps qui a d'autres UV) + micro-relief répété (SkinMicroNormal, MicroTiling fois), rugosité un peu variée"""
    shared = getattr(getattr(unreal, "SamplerSourceMode", None), "SSM_WRAP_WORLD_GROUP_SETTINGS", None)
    flat = unreal.load_asset("/Engine/EngineMaterials/FlatNormal")
    micro_texture = None
    for rel in SKIN_MICRO_NORMAL:
        micro_texture = import_texture(rel, "normal")
        if micro_texture:
            break

    def scalar(name, value, x, y):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
        set_prop(node, "parameter_name", name)
        set_prop(node, "default_value", value)
        return node

    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1700, 1800)
    tiling = scalar("MicroTiling", 40.0, -1700, 1950)
    tiled = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -1450, 1850)
    MEL.connect_material_expressions(uv, "", tiled, "A")
    MEL.connect_material_expressions(tiling, "", tiled, "B")
    samples = []
    for name, texture, uv_node, y in (("DetailNormal", flat, uv, 1600), ("SkinMicroNormal", micro_texture or flat, tiled, 1850)):
        sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, y)
        set_prop(sample, "parameter_name", name)
        set_prop(sample, "texture", texture)
        set_prop(sample, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        if shared is not None:
            set_prop(sample, "sampler_source", shared)
        MEL.connect_material_expressions(uv_node, "", sample, "UVs")
        samples.append(sample)
    # Relief du corps d'inZOI (_Main_M fin, _Physique_M des muscles) : canaux R, V = pente (0,5 = plat).
    # Lus en linéaire ; force à 0 sur les têtes (autres UV), posée sur le corps par set_body_relief.
    body_default = import_texture(BODY_RELIEF["Male"][1], "mask")
    for name, y in (("BodyRelief", 2400), ("PhysiqueRelief", 2650)):
        sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, y)
        set_prop(sample, "parameter_name", name)
        if body_default:
            set_prop(sample, "texture", body_default)
            set_prop(sample, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)  # texture « mask »
        else:
            set_prop(sample, "texture", flat)
            set_prop(sample, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        if shared is not None:
            set_prop(sample, "sampler_source", shared)
        MEL.connect_material_expressions(uv, "", sample, "UVs")
        samples.append(sample)
    body_amount = scalar("BodyReliefAmount", 0.0, -1100, 2900)
    physique_amount = scalar("PhysiqueStrength", 0.0, -1100, 3000)
    muscle_relief = scalar("MuscleDefinition", 0.0, -1100, 3100)
    detail_amount = scalar("DetailNormalAmount", 0.0, -1100, 2100)
    micro_amount = scalar("MicroNormalAmount", 0.75, -1100, 2200)
    rough_base = scalar("SkinRoughness", 0.66, -1100, 2300)
    # <Tête>_Main_RS d'inZOI : R = rugosité (0,399 à 0,95 dans le jeu), A = occlusion cuite (orbites, narines, bouche,
    # oreilles). Force SkinRSAmount à 0 par défaut, posée à 1 par set_skin_rs (texture propre ou partagée)
    rs_default = import_texture(SKIN_RS_SHARED, "mask")
    rs_sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, 3500)
    set_prop(rs_sample, "parameter_name", "SkinRS")
    set_prop(rs_sample, "texture", rs_default or flat)
    set_prop(rs_sample, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if rs_default else unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    if shared is not None:
        set_prop(rs_sample, "sampler_source", shared)
    MEL.connect_material_expressions(uv, "", rs_sample, "UVs")
    rs_amount = scalar("SkinRSAmount", 0.0, -1100, 3700)
    ao_node = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -600, 2300)
    set_prop(ao_node, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    set_prop(ao_node, "code", "return lerp(1.0, RS.a * RS.a, RSA);")  # alpha au carré : AO cuite plus marquée
    ao_entries = []
    for input_name in ("RS", "RSA"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        ao_entries.append(entry)
    set_prop(ao_node, "inputs", ao_entries)
    MEL.connect_material_expressions(rs_sample, "RGBA", ao_node, "RS")
    MEL.connect_material_expressions(rs_amount, "", ao_node, "RSA")
    MEL.connect_material_property(ao_node, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    normal = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -600, 1800)
    set_prop(normal, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    set_prop(normal, "code", "float2 xy = D.xy * DA + M.xy * MA + (B.rg * 2.0 - 1.0) * BA + (P.rg * 2.0 - 1.0) * PA * Mus;\nreturn normalize(float3(xy, 1.0));")
    rough = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -600, 2050)
    set_prop(rough, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    # Les creux du micro-relief (pores) un peu plus mats que les bosses
    # G = brillance du highlight du maquillage (canal G de Makeup_Part_A) : peau plus lisse à ces endroits
    set_prop(rough, "code", "float r = lerp(R, lerp(0.399, 0.95, RS.r), RSA);\nr = lerp(r, LR, L);\nreturn saturate(r + (1.0 - M.z) * 0.6 * MA - length(D.xy) * 0.1 * DA - G * 0.35);")
    for node, names in ((normal, ("D", "M", "DA", "MA", "B", "P", "BA", "PA", "Mus")), (rough, ("D", "M", "DA", "MA", "R", "G", "RS", "RSA", "L", "LR"))):
        entries = []
        for input_name in names:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", input_name)
            entries.append(entry)
        set_prop(node, "inputs", entries)
        MEL.connect_material_expressions(samples[0], "RGB", node, "D")
        MEL.connect_material_expressions(samples[1], "RGB", node, "M")
        MEL.connect_material_expressions(detail_amount, "", node, "DA")
        MEL.connect_material_expressions(micro_amount, "", node, "MA")
    MEL.connect_material_expressions(rough_base, "", rough, "R")
    MEL.connect_material_expressions(rs_sample, "RGBA", rough, "RS")
    MEL.connect_material_expressions(rs_amount, "", rough, "RSA")
    if lip_rough is not None:
        MEL.connect_material_expressions(lip_rough, "R", rough, "LR")
    else:
        MEL.connect_material_expressions(scalar("LipRoughness", 0.5, -800, 3700), "", rough, "LR")
    if lips is not None:
        MEL.connect_material_expressions(lips, "", rough, "L")
    else:
        MEL.connect_material_expressions(scalar("NoLips", 0.0, -800, 3600), "", rough, "L")
    if gloss is not None:
        MEL.connect_material_expressions(gloss, "", rough, "G")
    else:
        MEL.connect_material_expressions(scalar("NoGloss", 0.0, -800, 3400), "", rough, "G")
    MEL.connect_material_expressions(samples[2], "RGB", normal, "B")
    MEL.connect_material_expressions(samples[3], "RGB", normal, "P")
    MEL.connect_material_expressions(body_amount, "", normal, "BA")
    MEL.connect_material_expressions(physique_amount, "", normal, "PA")
    MEL.connect_material_expressions(muscle_relief, "", normal, "Mus")
    MEL.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)


EYE_VERSION = 8  # à augmenter quand le dessin de l'œil change : le matériau est alors refait
# Masque d'iris d'inZOI (celui de MetaHuman) : R = relief, V = fibres de l'iris, B = mélange des 2 couleurs, A = pupille
IRIS_MASKS = ["MetaHumans/Common/Face/Textures/IrisTextures/iris_001/T_Iris_A_M",
              "Characters/Materials/DefaultTextures/Eyes/IrisTextures/iris_001/T_Iris_A_M"]
# Rayon de l'iris dans les UV du globe inZOI (0,125) / rayon de l'iris dans la texture (0,45)
IRIS_UV_SCALE = 3.0  # 3,6 avant : iris trop petit face aux vignettes (beaucoup de blanc, regard écarquillé)
# Bord de l'iris dans T_Iris_A_M (là où son canal B, le fondu des 2 couleurs, arrive à 1)
IRIS_EDGE_REFERENCE = 0.45
IRIS_EDGE_PARAM = "IrisTexEdge"
# Motifs d'iris d'inZOI (lentilles « Custom ») et leurs vignettes
LENS_TEXTURES = "Characters/AvatarParts/Lens/Textures"
LENS_ICONS = "UI/Icon/CharacterCustomize/Face/Makeup/Lens"


def build_eye_master():
    """M_InZOI_Eye : la vraie texture d'iris d'inZOI (T_Iris_A_M) posée sur les UV du globe (iris centré en 0,5 0,5),
    teinte IrisColor (la couleur choisie ; la 2e couleur d'inZOI en est une version foncée), pupille, limbe, blanc de l'œil."""
    existing = load(EYE_MASTER)
    if existing and EAL.get_metadata_tag(existing, "InZOIVersion") == str(EYE_VERSION):
        return existing
    if existing:
        # Version précédente : on vide le graphe et on le refait au même endroit (les têtes gardent le lien)
        MEL.delete_all_material_expressions(existing)
        material = existing
    else:
        material = new_material(EYE_MASTER)
    iris_texture = None
    for rel in IRIS_MASKS:
        iris_texture = import_texture(rel, "mask")
        if iris_texture:
            break

    def scalar(name, value, x, y):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
        set_prop(node, "parameter_name", name)
        set_prop(node, "default_value", value)
        return node

    def custom_node(code, output, names, x, y):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
        set_prop(node, "output_type", output)
        set_prop(node, "code", code)
        entries = []
        for input_name in names:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", input_name)
            entries.append(entry)
        set_prop(node, "inputs", entries)
        return node

    COT = unreal.CustomMaterialOutputType
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1500, -200)
    radius = scalar("IrisUVRadius", 1.0, -1500, 0)
    pupil_scale = scalar("PupilScale", 1.25, -1500, 150)
    # Rayon du bord de l'iris dans la texture (0,45 pour T_Iris_A_M ; mesuré pour chaque motif de lentille, voir iris_edge)
    edge = scalar(IRIS_EDGE_PARAM, IRIS_EDGE_REFERENCE, -1500, 300)
    iris_uv = custom_node("return 0.5 + (UV - 0.5) * %.3f / max(Radius, 0.1) * (Edge / %.3f);" % (IRIS_UV_SCALE, IRIS_EDGE_REFERENCE),
                          COT.CMOT_FLOAT2, ("UV", "Radius", "Edge"), -1250, -100)
    MEL.connect_material_expressions(uv, "", iris_uv, "UV")
    MEL.connect_material_expressions(radius, "", iris_uv, "Radius")
    MEL.connect_material_expressions(edge, "", iris_uv, "Edge")
    pupil_uv = custom_node("return 0.5 + (IrisUV - 0.5) / max(Scale, 0.1);", COT.CMOT_FLOAT2, ("IrisUV", "Scale"), -1250, 100)
    MEL.connect_material_expressions(iris_uv, "", pupil_uv, "IrisUV")
    MEL.connect_material_expressions(pupil_scale, "", pupil_uv, "Scale")

    samples = []
    for index, uv_node in enumerate((iris_uv, pupil_uv)):
        sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1000, -150 + index * 300)
        set_prop(sample, "parameter_name", "IrisMasks")
        set_prop(sample, "texture", iris_texture or unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
        set_prop(sample, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if iris_texture
                 else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        MEL.connect_material_expressions(uv_node, "", sample, "UVs")
        samples.append(sample)

    iris = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1000, 350)
    set_prop(iris, "parameter_name", "IrisColor")
    set_prop(iris, "default_value", unreal.LinearColor(0.189, 0.102, 0.026, 1))
    balance = scalar("IrisColorBalance", 0.19, -1000, 500)
    smooth = scalar("IrisColorBalanceSmoothness", 0.79, -1000, 600)
    limbus = scalar("LimbusDarkAmount", 0.35, -1000, 700)
    combine = custom_node((
        "float d = distance(UV, float2(0.5, 0.5));\n"
        "float k = %.3f / max(Edge, 0.05);\n"
        "float dt = distance(IrisUV, float2(0.5, 0.5)) * k;\n"
        "float dp = distance(PupilUV, float2(0.5, 0.5)) * k;\n"
        "float irisMask = 1.0 - smoothstep(0.43, 0.47, dt);\n"
        "float t = smoothstep(Balance - Smooth * 0.5, Balance + Smooth * 0.5, M.b);\n"
        "float3 irisColor = lerp(Iris.rgb, Iris.rgb * 0.3, t);\n"
        "irisColor *= lerp(0.6, 1.6, M.g);\n"
        "irisColor *= lerp(0.8, 1.1, M.r);\n"
        "float collar = 1.0 - smoothstep(0.16, 0.26, dt);\n"
        "irisColor = lerp(irisColor, irisColor * float3(1.25, 1.1, 0.85), collar * 0.5);\n"
        "irisColor *= 1.0 - Limbus * smoothstep(0.33, 0.45, dt);\n"
        "float pupil = max(1.0 - P, 1.0 - smoothstep(0.11, 0.13, dp));\n"
        "float rim = smoothstep(0.18, 0.42, d);\n"
        "float3 sclera = lerp(float3(0.56, 0.53, 0.50), float3(0.52, 0.42, 0.39), rim * 0.7) * (1.0 - 0.5 * rim);\n"
        "sclera *= 1.0 - 0.25 * Limbus * (1.0 - smoothstep(0.45, 0.6, dt));\n"
        "float3 c = lerp(sclera, irisColor, irisMask);\n"
        "return lerp(c, float3(0.004, 0.004, 0.004), pupil * irisMask);") % IRIS_EDGE_REFERENCE,
        COT.CMOT_FLOAT3, ("UV", "IrisUV", "PupilUV", "M", "P", "Iris", "Balance", "Smooth", "Limbus", "Edge"), -600, 0)
    MEL.connect_material_expressions(edge, "", combine, "Edge")
    MEL.connect_material_expressions(uv, "", combine, "UV")
    MEL.connect_material_expressions(iris_uv, "", combine, "IrisUV")
    MEL.connect_material_expressions(pupil_uv, "", combine, "PupilUV")
    MEL.connect_material_expressions(samples[0], "RGBA", combine, "M")
    MEL.connect_material_expressions(samples[1], "A", combine, "P")
    MEL.connect_material_expressions(iris, "", combine, "Iris")
    MEL.connect_material_expressions(balance, "", combine, "Balance")
    MEL.connect_material_expressions(smooth, "", combine, "Smooth")
    MEL.connect_material_expressions(limbus, "", combine, "Limbus")
    MEL.connect_material_property(combine, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Œil mouillé : très lisse (reflet net de la lumière), l'iris un peu moins. Le vernis (Clear Coat) d'avant
    # restait mat chez L'amie : on passe en éclairage normal.
    rough = custom_node("float dt = distance(IrisUV, float2(0.5, 0.5)) * %.3f / max(Edge, 0.05);\n"
                        "return lerp(0.03, 0.08, 1.0 - smoothstep(0.43, 0.47, dt));" % IRIS_EDGE_REFERENCE,
                        COT.CMOT_FLOAT1, ("IrisUV", "Edge"), -600, 250)
    MEL.connect_material_expressions(iris_uv, "", rough, "IrisUV")
    MEL.connect_material_expressions(edge, "", rough, "Edge")
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(constant(material, 1.0, -600, 350), "", unreal.MaterialProperty.MP_SPECULAR)
    set_prop(material, "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    EAL.set_metadata_tag(material, "InZOIVersion", str(EYE_VERSION))
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


EYE_SHADOW_MATERIAL = DEST + "/Materials/M_InZOI_EyeShadow"


def build_eye_shadow():
    """Couche « EyeAO » d'inZOI (coque autour de l'œil) : une ombre légère sur le blanc de l'œil, sous les paupières.
    Cachée, elle laissait un blanc éclatant et un regard écarquillé."""
    existing = load(EYE_SHADOW_MATERIAL)
    if existing:
        return existing
    material = new_material(EYE_SHADOW_MATERIAL)
    set_prop(material, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    set_prop(material, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, -100)
    set_prop(color, "constant", unreal.LinearColor(0.02, 0.012, 0.01, 1))
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 100)
    set_prop(opacity, "parameter_name", "ShadowOpacity")
    set_prop(opacity, "default_value", 0.35)
    MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    return finish_material(material, EYE_SHADOW_MATERIAL)


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


def set_head_relief(instance, gender):
    """Normale de détail d'inZOI sur la peau d'une tête (le corps garde DetailNormalAmount à 0), fausse lumière"""
    if abs(MEL.get_material_instance_scalar_parameter_value(instance, "FakeLightAmount") - FAKE_LIGHT_AMOUNT) > 0.001:
        MEL.set_material_instance_scalar_parameter_value(instance, "FakeLightAmount", FAKE_LIGHT_AMOUNT)
        MEL.update_material_instance(instance)
        EAL.save_loaded_asset(instance)
    if abs(MEL.get_material_instance_scalar_parameter_value(instance, "DetailNormalAmount") - HEAD_DETAIL_AMOUNT) < 0.01:
        return
    texture = import_texture(HEAD_DETAIL_NORMAL[gender], "normal")
    if not texture:
        log("Peau : pas de normale de détail pour %s" % gender)
        return
    MEL.set_material_instance_texture_parameter_value(instance, "DetailNormal", texture)
    MEL.set_material_instance_scalar_parameter_value(instance, "DetailNormalAmount", HEAD_DETAIL_AMOUNT)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance)


def set_skin_rs(instance, rel_folder, is_head):
    """_Main_RS (corps : _RS) de la tête ou du corps : R = rugosité, A = occlusion cuite"""
    rs = None
    for sub in ("Textures", "Texture", ""):
        root = os.path.join(EXPORTS, *rel_folder.split("/"), sub) if sub else os.path.join(EXPORTS, *rel_folder.split("/"))
        if os.path.isdir(root):
            rs = next((f[:-4] for f in sorted(os.listdir(root)) if f.endswith(("_Main_RS.png", "_RS.png"))), None)
            if rs:
                rs = (rel_folder + "/" + sub + "/" + rs).replace("//", "/")
                break
    texture = import_texture(rs, "mask") if rs else None
    if not texture and is_head:
        texture = import_texture(SKIN_RS_SHARED, "mask")
    if texture:
        MEL.set_material_instance_texture_parameter_value(instance, "SkinRS", texture)
        MEL.set_material_instance_scalar_parameter_value(instance, "SkinRSAmount", 1.0)
        MEL.update_material_instance(instance)
        EAL.save_loaded_asset(instance)


def set_body_relief(instance, gender):
    """Relief du corps (fin + muscles) et couleur musclée d'inZOI sur la peau du corps ; MuscleDefinition vient du jeu"""
    main, physique, muscle = BODY_RELIEF[gender]
    fat = "Characters/AvatarParts/Body/%s/Textures/%s_Body_Fat_D" % (gender, gender)
    for param, rel, role in (("BodyRelief", main, "mask"), ("PhysiqueRelief", physique, "mask"), ("MuscleColorMap", muscle, "color"),
                             ("FatColorMap", fat, "color")):
        texture = import_texture(rel, role)
        if texture:
            if role == "color":
                force_color_texture(texture)
            MEL.set_material_instance_texture_parameter_value(instance, param, texture)
            if param == "MuscleColorMap":
                MEL.set_material_instance_scalar_parameter_value(instance, "MuscleColorAmount", BODY_MUSCLE_COLOR_AMOUNT)
            if param == "FatColorMap":
                MEL.set_material_instance_scalar_parameter_value(instance, "FatColorAmount", BODY_FAT_COLOR_AMOUNT)
        else:
            log("Corps %s : %s absente" % (gender, rel))
    MEL.set_material_instance_scalar_parameter_value(instance, "BodyReliefAmount", BODY_RELIEF_AMOUNT)
    MEL.set_material_instance_scalar_parameter_value(instance, "PhysiqueStrength", BODY_PHYSIQUE_AMOUNT)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance)


def apply_skin(mesh, rel_folder, age, only_skin_slots):
    """Peau sur le corps ou la tête, plus yeux et dents des têtes ; renvoie les emplacements teintés par la couleur de peau"""
    prefer = ("Youth_D", "Main_D", "_D") if age == "Youth" else ("Main_D", "_D")
    textures = scan_textures(rel_folder, prefer)
    mi_folder = dest(rel_folder + "/Materials")
    # Nouveau nom (MI_SkinV2_) : les anciennes instances mélangeaient d'autres textures
    instance = skin_instance(mi_folder + "/MI_SkinV2_" + mesh.get_name(), textures.get("color"))
    set_skin_rs(instance, rel_folder, only_skin_slots)
    if only_skin_slots:
        set_head_relief(instance, "Female" if "female" in rel_folder.lower() else "Male")
    else:
        set_body_relief(instance, "Female" if "female" in rel_folder.lower() else "Male")
    # Le matériau voulu pour chaque emplacement. On ne compte plus sur l'asset : Unreal ignorait les changements
    # faits depuis Python sur les emplacements du Skeletal Mesh. Le jeu les pose lui-même (Materials de la base
    # ou de la pièce, appliqués par TrainerAppearanceRender).
    chosen = []
    tinted = []
    for index, slot in enumerate(mesh.get_editor_property("materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        lower = slot_name.lower()
        material = slot.get_editor_property("material_interface")
        if "eyeball" in lower:
            material = build_eye_master()
            EYE_SLOTS.add(slot_name)
        elif "eyeao" in lower or "eye_ao" in lower:
            material = build_eye_shadow()
        elif any(key in lower for key in ("saliva", "eyetear", "eye_tear", "lash", "cartilage")):
            material = build_hidden()
        elif "teeth" in lower or "tooth" in lower or "tongue" in lower:
            material = build_teeth()
        else:
            is_skin = any(key in lower for key in ("skin", "head", "body", "face"))
            if not only_skin_slots or is_skin or (index == 0 and len(mesh.get_editor_property("materials")) == 1):
                material = instance
                tinted.append(slot_name)
        chosen.append(material)
    SKIN_MATERIALS[mesh.get_path_name()] = chosen
    texture = MEL.get_material_instance_texture_parameter_value(instance, "BaseColorMap")
    log("Matériaux de %s (posés par le jeu) : %s ; peau %s, texture %s" % (
        mesh.get_name(), ", ".join(m.get_name() if m else "aucun" for m in chosen),
        instance.get_name(), texture.get_name() if texture else "AUCUNE"))
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
        group = groups.setdefault(row["groupe"], {"pos": [], "neg": [], "muscle": None, "mass": None, "gizmo": 1.0})
        offset = unreal.TrainerBoneOffset()
        offset.set_editor_property("bone", row["os"])
        offset.set_editor_property("location", unreal.Vector(float_of(row, "loc_x"), float_of(row, "loc_y"), float_of(row, "loc_z")))
        offset.set_editor_property("rotation", unreal.Quat(float_of(row, "rot_x"), float_of(row, "rot_y"), float_of(row, "rot_z"), float_of(row, "rot_w", 1.0)))
        offset.set_editor_property("scale", unreal.Vector(float_of(row, "scale_x"), float_of(row, "scale_y"), float_of(row, "scale_z")))
        group["pos" if row.get("signe") == "+" else "neg"].append(offset)
        if group["muscle"] is None:
            group["muscle"] = float_of(row, "delta_muscle")
            group["mass"] = float_of(row, "delta_masse")
            group["gizmo"] = float_of(row, "delta_gizmo", 1.0)

    sliders = []

    def slider(slider_id, label, section, signed=False):
        definition = unreal.TrainerSliderDef()
        definition.set_editor_property("id", slider_id)
        definition.set_editor_property("label", label)
        definition.set_editor_property("section", section)
        definition.set_editor_property("group", unreal.TrainerSliderGroup.BODY)
        definition.set_editor_property("signed", signed)
        return definition

    sliders.append(slider("Muscle", "Muscles", "Silhouette", False))  # 0 à 1 dans inZOI (MuscleDefinition)
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
        definition.set_editor_property("gizmo_coef", group["gizmo"])
        sliders.append(definition)
    return sliders


# ---------------------------------------------------------------- Traits du visage

# Partie du visage (nom du groupe inZOI sans « C_ », ni côté L/R, ni axe) -> (section, nom affiché)
FACE_FEATURE_FR = OrderedDict([
    ("Head", ("Forme du visage", "Visage")),
    ("ForeHead", ("Forme du visage", "Front")),
    ("Brow", ("Sourcils", "Sourcils")),
    ("BrowIn", ("Sourcils", "Début des sourcils")),
    ("BrowMid", ("Sourcils", "Milieu des sourcils")),
    ("BrowOut", ("Sourcils", "Fin des sourcils")),
    ("Eye", ("Yeux", "Yeux")),
    ("Eyelash", ("Yeux", "Cils")),
    ("EyeUpIn", ("Yeux", "Paupière haute, intérieur")),
    ("EyeUpMid", ("Yeux", "Paupière haute, milieu")),
    ("EyeUpOut", ("Yeux", "Paupière haute, extérieur")),
    ("EyeLowIn", ("Yeux", "Paupière basse, intérieur")),
    ("EyeLowMid", ("Yeux", "Paupière basse, milieu")),
    ("EyeLowOut", ("Yeux", "Paupière basse, extérieur")),
    ("EyeCornerIn", ("Yeux", "Coin intérieur de l'œil")),
    ("EyeCornerOut", ("Yeux", "Coin extérieur de l'œil")),
    ("EyesackUpper", ("Yeux", "Pli de la paupière")),
    ("EyesackUpperIn", ("Yeux", "Pli de la paupière, intérieur")),
    ("EyesackUpperMid", ("Yeux", "Pli de la paupière, milieu")),
    ("EyesackUpperOut", ("Yeux", "Pli de la paupière, extérieur")),
    ("Nose", ("Nez", "Nez")),
    ("NoseUp", ("Nez", "Arête du nez")),
    ("NoseMid", ("Nez", "Milieu du nez")),
    ("NoseTip", ("Nez", "Bout du nez")),
    ("NoseLow", ("Nez", "Base du nez")),
    ("Nostril", ("Nez", "Narines")),
    ("Cheek", ("Joues", "Pommettes")),
    ("CheekInner", ("Joues", "Pommettes, intérieur")),
    ("CheekOuter", ("Joues", "Pommettes, extérieur")),
    ("CheekLow", ("Joues", "Joues")),
    ("CheekLowInner", ("Joues", "Joues, intérieur")),
    ("CheekLowOuter", ("Joues", "Joues, extérieur")),
    ("Mouth", ("Bouche", "Bouche")),
    ("MouthUp", ("Bouche", "Lèvre du haut")),
    ("MouthLow", ("Bouche", "Lèvre du bas")),
    ("MouthLipUpper", ("Bouche", "Épaisseur lèvre du haut")),
    ("MouthLipLower", ("Bouche", "Épaisseur lèvre du bas")),
    ("MouthCorner", ("Bouche", "Coins de la bouche")),
    ("UpperMouth", ("Bouche", "Au-dessus de la bouche")),
    ("LowerMouth", ("Bouche", "Sous la bouche")),
    ("Tongue", ("Bouche", "Langue")),
    ("Jaw", ("Mâchoire et menton", "Mâchoire")),
    ("JawBack", ("Mâchoire et menton", "Angle de la mâchoire")),
    ("JawUnder", ("Mâchoire et menton", "Dessous de la mâchoire")),
    ("JawLower", ("Mâchoire et menton", "Bas de la mâchoire")),
    ("Chin", ("Mâchoire et menton", "Menton")),
    ("Chin_C", ("Mâchoire et menton", "Pointe du menton")),
    ("UnderChin", ("Mâchoire et menton", "Sous le menton")),
    ("Ear", ("Oreilles", "Oreilles")),
    ("EarUp", ("Oreilles", "Haut des oreilles")),
    ("EarMid", ("Oreilles", "Milieu des oreilles")),
    ("EarLow", ("Oreilles", "Lobes")),
])

# Axe du groupe -> réglage (calculé d'après le squelette de la tête : X = côté, Y = avant, Z = haut)
FACE_AXIS_FR = {"": "", "X": "écart", "Y": "avancée", "Z": "hauteur", "Scale": "taille", "ScaleX": "largeur",
                "ScaleY": "hauteur", "RotateX": "inclinaison", "RotateY": "rotation", "RotateZ": "inclinaison"}
# Oreilles : leurs os sont tournés, les axes ne veulent pas dire la même chose
EAR_AXIS_FR = {"X": "hauteur", "Y": "décollement", "Z": "écart"}


def face_group_key(group):
    """C_EyeUpIn_L_X -> ('EyeUpIn', 'X') ; C_Chin_C_Y -> ('Chin_C', 'Y') ; C_Nose -> ('Nose', '')"""
    name = re.sub(r"_(L|R)(?=_|$)", "", group)
    name = name[2:] if name.startswith("C_") else name
    feature, _, axis = name.rpartition("_")
    if not feature or axis not in FACE_AXIS_FR:
        return name, ""
    return feature, axis


def face_sliders(gender, age):
    """Traits du visage d'après Morphologie_Visage_Os.csv : un curseur par partie et par axe, côtés gauche et droit ensemble"""
    if not os.path.isfile(MORPHO_FACE):
        log("Morphologie_Visage_Os.csv introuvable : pas de curseurs du visage")
        return []
    groups = OrderedDict()
    for row in read_csv(MORPHO_FACE):
        if row.get("genre") != gender or row.get("age") != age:
            continue
        feature, axis = face_group_key(row["groupe"])
        group = groups.setdefault((feature, axis), {"pos": [], "neg": []})
        offset = unreal.TrainerBoneOffset()
        offset.set_editor_property("bone", row["os"])
        offset.set_editor_property("location", unreal.Vector(float_of(row, "loc_x"), float_of(row, "loc_y"), float_of(row, "loc_z")))
        offset.set_editor_property("rotation", unreal.Quat(float_of(row, "rot_x"), float_of(row, "rot_y"), float_of(row, "rot_z"), float_of(row, "rot_w", 1.0)))
        offset.set_editor_property("scale", unreal.Vector(float_of(row, "scale_x"), float_of(row, "scale_y"), float_of(row, "scale_z")))
        group["pos" if row.get("signe") == "+" else "neg"].append(offset)

    order = list(FACE_FEATURE_FR)
    keys = sorted(groups, key=lambda k: (order.index(k[0]) if k[0] in order else len(order), k[1]))
    sliders = []
    for feature, axis in keys:
        group = groups[(feature, axis)]
        section, label = FACE_FEATURE_FR.get(feature, ("Autres", feature))
        axis_fr = (EAR_AXIS_FR if feature.startswith("Ear") else FACE_AXIS_FR).get(axis, axis)
        definition = unreal.TrainerSliderDef()
        definition.set_editor_property("id", "Visage_" + feature + ("_" + axis if axis else ""))
        definition.set_editor_property("label", label + (" : " + axis_fr if axis_fr else ""))
        definition.set_editor_property("section", section)
        definition.set_editor_property("group", unreal.TrainerSliderGroup.FACE)
        definition.set_editor_property("signed", bool(group["neg"]))
        definition.set_editor_property("bones_positive", group["pos"])
        definition.set_editor_property("bones_negative", group["neg"])
        sliders.append(definition)
    log("Traits du visage %s %s : %d curseurs" % (gender, age, len(sliders)))
    return sliders


# ---------------------------------------------------------------- Catalogue

# Longueurs (dossier sous la catégorie) : Pants/Short = shorts, Skirt/Long = jupes longues...
LENGTH_LABELS = {
    "TShirt": {"Short": "T-shirts manches courtes", "Long": "T-shirts manches longues", "Sleeveless": "Débardeurs"},
    "Shirt": {"Short": "Chemises manches courtes", "Long": "Chemises", "Mid": "Chemises"},
    "Pants": {"Short": "Shorts", "Mid": "Pantacourts", "Long": "Pantalons"},
    "Skirt": {"Short": "Jupes courtes", "Mid": "Jupes mi-longues", "Long": "Jupes longues"},
}


def category_of(mesh_rel, slot):
    parts = mesh_rel.split("/")
    # Characters/Costumes/Female/Top/TShirt/Long/TS_L001/Mesh/SKM_... (ou .../Female/Custom/Top/Tshirt/...)
    if len(parts) > 5 and parts[1] == "Costumes":
        if parts[3] == "Custom":
            parts = parts[:3] + parts[4:]
        key = "TShirt" if parts[4].lower() == "tshirt" else parts[4]
        label = CATEGORY_FR.get(key, key)
        if key in LENGTH_LABELS and len(parts) > 6:
            label = LENGTH_LABELS[key].get(parts[5], label)
        return label
    return ""


def parse_list(text):
    try:
        value = json.loads(text) if text and text.strip().startswith("[") else []
    except ValueError:
        value = []
    return [v for v in value if isinstance(v, str)]


def thumbnail_fallbacks(row, same_mesh_rows):
    """Icônes à essayer pour une ligne sans « Thumbnail » : celle d'une variante de couleur (Id_CT001...) sans le
    suffixe _CTnnn, puis celles des variantes elles-mêmes, puis n'importe quelle ligne du même modèle"""
    own_id = row.get("Id", "")
    ordered = sorted(same_mesh_rows, key=lambda other: (not other.get("Id", "").startswith(own_id + "_CT"), other.get("Id", "")))
    candidates = []
    for other in ordered:
        rel = game_rel(other.get("Thumbnail"))
        if not rel:
            continue
        plain = re.sub(r"_CT\d+$", "", rel)
        for candidate in (plain, rel):
            if candidate not in candidates and os.path.isfile(src(candidate, ".png")):
                candidates.append(candidate)
    return candidates


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
            rows_by_mesh = {mesh_rel: list(mesh_rows) for mesh_rel, mesh_rows in meshes.items()}
            mesh_items = list(meshes.items())
            if MAX_MESHES_PER_SLOT:
                # Une pièce de chaque catégorie à tour de rôle : l'essai a des T-shirts ET des chemises, pas 15 blouses
                by_category = OrderedDict()
                for item in mesh_items:
                    by_category.setdefault(category_of(item[0], slot), []).append(item)
                mixed = []
                while len(mixed) < len(mesh_items):
                    for items in by_category.values():
                        if items:
                            mixed.append(items.pop(0))
                mesh_items = mixed[:MAX_MESHES_PER_SLOT]
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
                        # Têtes : nos matériaux de peau, yeux et dents (apply_skin). Les « OverrideMaterial »
                        # d'inZOI et ceux créés par l'import donnaient des taches et des yeux en damier
                        materials = list(SKIN_MATERIALS.get(mesh.get_path_name(), []))
                    else:
                        materials = [part_material(rel) for rel in (game_rel(p) for p in parse_list(row.get("OverrideMaterial"))) if rel]
                    part.set_editor_property("materials", [m for m in materials if m])
                    thumbnail = import_texture(game_rel(row.get("Thumbnail")), "ui")
                    if not thumbnail:
                        # Ligne sans icône (souvent la version de base) : icône d'une variante de couleur du même modèle
                        for other in thumbnail_fallbacks(row, rows_by_mesh.get(mesh_rel, [])):
                            thumbnail = import_texture(other, "ui")
                            if thumbnail:
                                break
                    if thumbnail:
                        part.set_editor_property("thumbnail", thumbnail)
                    parts.append(part)
                    defaults.setdefault(base_id, {}).setdefault(slot, row["Id"])
            log("%s / %s : %d modèles" % (base_id, slot_label, len(mesh_items)))
    return parts, defaults


# ---------------------------------------------------------------- Barbes (dossier Groom)

HAIR_MASTER = DEST + "/Materials/M_InZOI_Hair"
HAIR_VERSION = 27  # à augmenter quand le graphe change : le matériau est refait au même endroit
# Emplacements sans table inZOI (catalogue fait d'après les dossiers) : emplacement, libellé, onglet, suit
# Les cheveux sont skinnés sur les os du corps (head) : ils suivent le corps ; la barbe suit la tête.
GROOM_SLOTS = [("Hair", "Cheveux", "Hair", None), ("Beard", "Barbe", "Hair", "Head"), ("Mustache", "Moustache", "Hair", "Head"),
               ("Eyebrows", "Sourcils", "Hair", "Head"), ("Eyelashes", "Cils", "Hair", "Head")]
# Sourcils et cils : tables inZOI (Data/JsonResult), meshes dans Groom/Eyebrows et Groom/Eyelashes
FACE_GROOMS = [
    ("Eyebrows", "Sourcils", "AppearanceEyebrow.json", GROOM_REL + "/Textures/Eyebrows/Eyebrows_001_DIRO", (0.03, 0.017, 0.006)),
    ("Eyelashes", "Cils", "AppearanceEyelash.json", GROOM_REL + "/Textures/Eyelashes/Eyelashes_old_DIRO", (0.010417, 0.005399, 0.001878)),
]
JSON_TABLES = os.path.join(EXPORTS, "Data", "JsonResult")
HAIR_CATEGORY_FR = {"Short": "Courts", "Shaved": "Rasés", "Medium": "Mi-longs", "Long": "Longs", "Bob": "Carrés",
                    "Bald": "Chauve"}
HAIR_CATEGORY_ORDER = ["Short", "Shaved", "Bob", "Medium", "Long", "Bald"]
# Versions de la coupe sous un chapeau (pas encore utilisées)
HAIR_SKIP_SUFFIXES = ("_Hat", "_Beanie", "_Fedora")
# Zones de couleur des coupes (nuancier) : principale, dégradé vers les pointes, mèches, mèches devant ; l'alpha règle
# le début du dégradé (0 = racines, 1 = pointes) ou la part de mèches colorées
HAIR_ZONE_LABELS = ["Principale", "Dégradé", "Mèches", "Mèches devant"]
HAIR_ZONE_AMOUNT_LABELS = ["", "Début du dégradé", "Quantité de mèches", "Quantité devant"]
HAIR_ZONE_AMOUNTS = [1.0, 0.5, 0.3, 0.6]
# Position de la 2e couleur d'inZOI -> début du dégradé (voir full_presets)
HAIR_SECOND_POSITION_SCALE = 0.35
# Rendu « Hair » : diffusion dans les mèches et courbe de couleur (réglés au banc BancPerso, planche de ressemblance)
HAIR_SCATTER = 0.3
HAIR_COLOR_GAMMA = 0.85
# Densité des cils (opacité tramée) : les cartes d'inZOI pleines donnaient des cils trop épais
EYELASH_DENSITY = 1.0
# Sourcils à 70 % (note du 9 novembre, vérifié au banc A/B) : pleins, ils faisaient des barres épaisses
EYEBROW_DENSITY = 0.7
DEFAULT_CARD = GROOM_REL + "/Textures/Hair/HairCard_001/HairCard_001_DIRO"
BEARD_CARD = GROOM_REL + "/Textures/Beard/BeardCard_002/BeardCard_002_DIRO"
# Brun foncé d'inZOI (MI_Beard_Scalp_003) quand le dossier n'a pas de fichier de matériau
BEARD_ROOT = (0.03434, 0.017642, 0.006049)
BEARD_TIP = (0.084376, 0.043735, 0.015209)


def build_hair_master():
    """M_InZOI_Hair : cheveux et barbes en cartes, texture DIRO d'inZOI (R = profondeur, G = id de mèche,
    B = dégradé racine -> pointe, A = opacité).
    Couleur : RootColor -> TipColor ; une couleur choisie dans le nuancier (Tint, blanc = couleur d'origine) la remplace."""
    existing = load(HAIR_MASTER)
    if existing and EAL.get_metadata_tag(existing, "InZOIVersion") == str(HAIR_VERSION):
        return existing
    if existing:
        MEL.delete_all_material_expressions(existing)
    material = existing or new_material(HAIR_MASTER)
    set_prop(material, "blend_mode", unreal.BlendMode.BLEND_MASKED)
    set_prop(material, "two_sided", True)
    set_prop(material, "opacity_mask_clip_value", 0.3333)
    diro = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -900, -200)
    set_prop(diro, "parameter_name", "DIROMap")
    # Texture par défaut en linéaire elle aussi : avec la texture blanche sRGB du moteur, le matériau ne compilait
    # plus (« sampler type ») et tous les poils sortaient en damier gris
    default_card = diro_texture(DEFAULT_CARD, None)
    set_prop(diro, "texture", default_card or unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    # Données (profondeur, id, dégradé, opacité), pas une couleur : lues telles quelles. En sRGB, l'opacité et le dégradé
    # étaient mis à la puissance 2,2 : poils clairsemés et dégradé repoussé au bout des mèches.
    if default_card:
        set_prop(diro, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    else:
        log("Cheveux : carte DIRO par défaut introuvable, lecture en sRGB (poils plus fins)")
    params = []
    for index, (name, color) in enumerate((("RootColor", BEARD_ROOT), ("TipColor", BEARD_TIP), ("Tint", (1.0, 1.0, 1.0)))):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, 50 + index * 150)
        set_prop(node, "parameter_name", name)
        set_prop(node, "default_value", unreal.LinearColor(color[0], color[1], color[2], 1))
        params.append((name, node))
    # Dégradé (Color1, alpha = début), mèches au hasard (Color2, alpha = quantité), mèches devant (Color3, alpha = quantité).
    # Une zone reste blanche (= pas choisie) tant que le joueur ne lui a pas donné de couleur.
    zone_nodes = []
    for index in range(3):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1200, 50 + index * 150)
        set_prop(node, "parameter_name", "Color%d" % (index + 1))
        set_prop(node, "default_value", unreal.LinearColor(1, 1, 1, HAIR_ZONE_AMOUNTS[index + 1]))
        zone_nodes.append(node)
    front = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1200, 500)
    set_prop(front, "parameter_name", "FrontAxis")  # devant du personnage dans le repère du mesh (+Y chez Unreal)
    set_prop(front, "default_value", unreal.LinearColor(0, 1, 0, 0))
    front_start = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1200, 650)
    set_prop(front_start, "parameter_name", "FrontStart")  # cm devant le centre de la tête où commencent les mèches devant
    set_prop(front_start, "default_value", 5.0)
    # Position dans le repère du mesh (cm) ; si ce nœud n'existe pas dans cette version d'Unreal : pas de mèches devant
    local_class = getattr(unreal, "MaterialExpressionPreSkinnedLocalPosition", None) or getattr(unreal, "MaterialExpressionLocalPosition", None)
    head_center = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1200, 800)
    set_prop(head_center, "parameter_name", "HeadCenter")  # centre de la tête dans le repère du mesh (posé par le jeu)
    set_prop(head_center, "default_value", unreal.LinearColor(0, 0, 0, 0))
    if local_class:
        # Position AVANT l'animation (repère du squelette de référence, comme HeadCenter et FrontAxis), lue dans le
        # vertex shader et passée au pixel par un Vertex Interpolator. Lue directement au pixel, elle suivait la pose
        # et le plateau du studio : les « mèches devant » changeaient selon l'angle (toutes de face, aucune de 3/4).
        skinned = MEL.create_material_expression(material, local_class, -1800, 800)
        interpolator_class = getattr(unreal, "MaterialExpressionVertexInterpolator", None)
        if interpolator_class:
            position = MEL.create_material_expression(material, interpolator_class, -1500, 800)
            MEL.connect_material_expressions(skinned, "", position, "")
        else:
            position = skinned
    else:
        position = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -1500, 800)
        log("Cheveux : nœud LocalPosition absent, mèches devant désactivées")
    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -500, -50)
    set_prop(custom, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    set_prop(custom, "code", (
        "float g = saturate(Grad);\n"
        "float3 natural = lerp(Root.rgb, Tip.rgb, g);\n"
        "bool picked = any(Tint.rgb < 0.999);\n"
        "float3 c = picked ? Tint.rgb * lerp(0.7, 1.0, g) : natural;\n"
        "if (any(C1.rgb < 0.999))\n"
        "{\n"
        "    float t = smoothstep(C1a - 0.2, C1a + 0.2, g);\n"
        "    c = lerp(c, C1.rgb * lerp(0.8, 1.0, g), t);\n"
        "}\n"
        "bool baked = VC.b < 0.5;\n"
        "float h = baked ? VC.g : frac(sin(ID * 91.345 + 0.123) * 43758.5453);\n"
        "if (any(C2.rgb < 0.999) && h < C2a)\n"
        "{\n"
        "    c = C2.rgb * lerp(0.75, 1.0, g);\n"
        "}\n"
        "if (any(C3.rgb < 0.999))\n"
        "{\n"
        "    float ahead = smoothstep(Start, Start + 3.0, dot(Pos - Center.xyz, normalize(Front.xyz + float3(0.0, 0.0, 0.0001))));\n"
        # Hauteur du visage seulement (Center.z = hauteur du nez) : la frange et les mèches qui encadrent le visage,
        # pas les longueurs qui tombent devant les épaules (de face, toute la coupe longue devenait de la couleur)
        "    ahead *= smoothstep(Center.z - 9.0, Center.z - 3.0, Pos.z);\n"
        "    float h2 = baked ? frac(VC.g * 13.713 + 0.37) : frac(sin(ID * 47.853 + 0.71) * 24634.6345);\n"
        "    if (baked) { ahead = VC.r; }\n"
        "    if (h2 < C3a * ahead)\n"
        "    {\n"
        "        c = C3.rgb * lerp(0.75, 1.0, g);\n"
        "    }\n"
        "}\n"
        # Comme les vignettes d'inZOI : couleurs éclaircies et adoucies (noir -> gris foncé, pêche -> platine,
        # rose vif -> rose pastel). Le modèle « Hair » d'Unreal assombrit beaucoup les couleurs claires.
        "c *= lerp(1.0 - RootDark, 1.0, smoothstep(0.0, RootArea, g));\n"
        # Variation par mèche 0,15 et creux des mèches à 0,3 (réglages vérifiés dans Blender, note du 9 novembre)
        "c *= lerp(0.925, 1.075, frac(sin(ID * 12.9898 + 4.13) * 43758.5453));\n"
        "c = pow(max(c, 0.0), Gamma);\n"
        "return c * lerp(0.7, 1.0, saturate(Depth));"))
    inputs = []
    for input_name in ("Grad", "Root", "Tip", "Tint", "Depth", "ID", "C1", "C2", "C3", "C1a", "C2a", "C3a", "Pos", "Front", "Start", "Center", "Gamma", "RootDark", "RootArea", "VC"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        inputs.append(entry)
    set_prop(custom, "inputs", inputs)
    MEL.connect_material_expressions(diro, "B", custom, "Grad")
    MEL.connect_material_expressions(diro, "R", custom, "Depth")
    MEL.connect_material_expressions(diro, "G", custom, "ID")
    for (name, node), input_name in zip(params, ("Root", "Tip", "Tint")):
        MEL.connect_material_expressions(node, "", custom, input_name)
    for index, node in enumerate(zone_nodes):
        MEL.connect_material_expressions(node, "", custom, "C%d" % (index + 1))
        # La sortie principale d'un paramètre de couleur n'a que RVB : l'alpha (réglage de la zone) passe à part
        MEL.connect_material_expressions(node, "A", custom, "C%da" % (index + 1))
    MEL.connect_material_expressions(position, "", custom, "Pos")
    MEL.connect_material_expressions(front, "", custom, "Front")
    MEL.connect_material_expressions(front_start, "", custom, "Start")
    MEL.connect_material_expressions(head_center, "", custom, "Center")
    gamma = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1200, 950)
    set_prop(gamma, "parameter_name", "ColorGamma")  # < 1 éclaircit et adoucit les couleurs (vignettes inZOI)
    set_prop(gamma, "default_value", HAIR_COLOR_GAMMA)
    MEL.connect_material_expressions(gamma, "", custom, "Gamma")
    # Couleurs de sommet des coupes préparées (bake_hair_strands) : R = devant, G = numéro de mèche, B = 0
    vertex_color = MEL.create_material_expression(material, unreal.MaterialExpressionVertexColor, -1200, 1300)
    MEL.connect_material_expressions(vertex_color, "", custom, "VC")
    # Racine assombrie comme le matériau d'inZOI (Root_Area 0,15, Root_Darkness 0,5) ; 0 pour sourcils et cils
    # (leur canal B ne dépasse pas 0,09 : tout le poil serait « racine »)
    for name, value, y, pin in (("RootDarkness", 0.5, 1100, "RootDark"), ("RootArea", 0.15, 1200, "RootArea")):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1200, y)
        set_prop(node, "parameter_name", name)
        set_prop(node, "default_value", value)
        MEL.connect_material_expressions(node, "", custom, pin)
    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Opacité tramée comme dans inZOI (paramètre « Dither ») : chaque pixel est gardé avec la probabilité de l'alpha,
    # le bruit change à chaque image et l'anti-crénelage temporel le lisse. Sourcils et cils doux au lieu de plaques.
    density = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 600)
    set_prop(density, "parameter_name", "Density")
    set_prop(density, "default_value", 1.0)
    dither = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -500, 450)
    set_prop(dither, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    set_prop(dither, "code", (
        "float2 p = Parameters.SvPosition.xy + float(View.StateFrameIndexMod8) * float2(32.665, 11.815);\n"
        "float n = frac(52.9829189 * frac(dot(p, float2(0.06711056, 0.00583715))));\n"
        "float threshold = 0.5 + (n - 0.5) * Spread;\n"
        "return saturate(Alpha * Density) - threshold + 0.3333;"))
    entries = []
    for input_name in ("Alpha", "Density", "Spread"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        entries.append(entry)
    set_prop(dither, "inputs", entries)
    MEL.connect_material_expressions(diro, "A", dither, "Alpha")
    MEL.connect_material_expressions(density, "", dither, "Density")
    # Tramage seulement entre 30 et 70 % d'opacité (avant : de 0 à 100 %, d'où le grain et les mèches grisées
    # où l'on voyait le fond à travers)
    spread = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 680)
    set_prop(spread, "parameter_name", "DitherSpread")
    set_prop(spread, "default_value", 0.15)  # 0,15 : moins de grain de loin (banc AD : 0,4 granuleux, 0 trop dur)
    MEL.connect_material_expressions(spread, "", dither, "Spread")
    MEL.connect_material_property(dither, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    roughness = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 200)
    set_prop(roughness, "parameter_name", "Roughness")
    set_prop(roughness, "default_value", 0.42)
    MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Spéculaire faible (0,25 donnait un voile gris argenté sur les cheveux et barbes foncés), plus faible au creux des mèches
    specular = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 750)
    set_prop(specular, "parameter_name", "Specular")
    set_prop(specular, "default_value", 0.45)
    spec_node = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -500, 300)
    set_prop(spec_node, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    set_prop(spec_node, "code", "return Spec * lerp(0.35, 1.0, saturate(Depth));")
    spec_inputs = []
    for input_name in ("Spec", "Depth"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        spec_inputs.append(entry)
    set_prop(spec_node, "inputs", spec_inputs)
    MEL.connect_material_expressions(specular, "", spec_node, "Spec")
    MEL.connect_material_expressions(diro, "R", spec_node, "Depth")
    MEL.connect_material_property(spec_node, "", unreal.MaterialProperty.MP_SPECULAR)
    # Modèle d'éclairage « Hair » d'Unreal (comme inZOI) : reflets clairs et larges le long des mèches. En éclairage
    # normal la couleur sortait brute et saturée (pêche au lieu de platine, roux au lieu de châtain cendré).
    # Dans ce modèle, la broche Normal donne la direction des mèches : le long du V des cartes.
    set_prop(material, "shading_model", unreal.MaterialShadingModel.MSM_HAIR)
    tangent = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, 600)
    set_prop(tangent, "constant", unreal.LinearColor(0, 1, 0, 0))
    MEL.connect_material_property(tangent, "", unreal.MaterialProperty.MP_NORMAL)
    # Modèle « Hair » : la broche Metallic devient Scatter (diffusion dans les mèches).
    # Non branchée, Scatter vaut 0 et les cheveux clairs sortaient bruns (platine, rose pastel...).
    scatter = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 700)
    set_prop(scatter, "parameter_name", "Scatter")
    set_prop(scatter, "default_value", HAIR_SCATTER)
    # Diffusion selon la clarté de la couleur : utile aux cheveux clairs (platine, rose pastel, sinon bruns), mais elle
    # éclaircissait les foncés en kaki (W_Head07_CT022 : brun 402909 rendu kaki ; sans diffusion, brun comme inZOI)
    scatter_mix = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -300, 700)
    set_prop(scatter_mix, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    set_prop(scatter_mix, "code", "return S * smoothstep(0.03, 0.25, dot(C, float3(0.2126, 0.7152, 0.0722)));")
    entries = []
    for input_name in ("S", "C"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        entries.append(entry)
    set_prop(scatter_mix, "inputs", entries)
    MEL.connect_material_expressions(scatter, "", scatter_mix, "S")
    MEL.connect_material_expressions(custom, "", scatter_mix, "C")
    MEL.connect_material_property(scatter_mix, "", unreal.MaterialProperty.MP_METALLIC)
    EAL.set_metadata_tag(material, "InZOIVersion", str(HAIR_VERSION))
    return finish_material(material, HAIR_MASTER)


def groom_json(json_rel):
    """Couleurs (linéaires) et texture DIRO d'un matériau de poils inZOI. Deux formats d'export :
    {"Textures", "Colors"} (exercice) ou [{"Properties": {"TextureParameterValues", "VectorParameterValues"}}] (FModel)"""
    colors, diro = {}, None
    filename = src(json_rel, ".json")
    if not os.path.isfile(filename):
        return colors, diro
    try:
        with open(filename, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return colors, diro
    if isinstance(data, dict):
        for name, value in (data.get("Colors") or {}).items():
            if isinstance(value, dict) and "R" in value:
                colors[name] = (value["R"], value["G"], value["B"])
        texture = (data.get("Textures") or {}).get("DIRO Map")
        diro = game_rel(texture.get("ObjectPath", "")) if isinstance(texture, dict) else None
        return colors, diro
    for obj in data if isinstance(data, list) else []:
        props = obj.get("Properties") or {}
        for entry in props.get("VectorParameterValues", []) or []:
            value = entry.get("ParameterValue") or {}
            if isinstance(value, dict) and "R" in value:
                colors[(entry.get("ParameterInfo") or {}).get("Name", "")] = (value["R"], value["G"], value["B"])
        for entry in props.get("TextureParameterValues", []) or []:
            if (entry.get("ParameterInfo") or {}).get("Name") == "DIRO Map":
                value = entry.get("ParameterValue") or {}
                diro = game_rel(value.get("ObjectPath", "")) if isinstance(value, dict) else None
        if props:
            break
    return colors, diro


HAIR_SCALARS = {"Specular": "Specular", "Roughness": "Roughness", "Scatter": "Scatter",
                "Root_Darkness": "RootDarkness", "Root_Area": "RootArea"}


def groom_scalars(json_rel):
    """Réglages numériques d'un matériau de poils inZOI repris tels quels (HAIR_SCALARS : nom inZOI -> nom du jeu) ;
    ceux absents du JSON viennent du parent MI_HairCard_Base (illisible) : on garde alors nos valeurs par défaut"""
    filename = src(json_rel, ".json")
    found = {}
    try:
        with open(filename, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return found
    if isinstance(data, dict):
        for name, value in (data.get("Scalars") or {}).items():
            if name in HAIR_SCALARS and isinstance(value, (int, float)):
                found[HAIR_SCALARS[name]] = float(value)
        return found
    for obj in data if isinstance(data, list) else []:
        props = (obj.get("Properties") or {}) if isinstance(obj, dict) else {}
        for entry in props.get("ScalarParameterValues", []) or []:
            name = (entry.get("ParameterInfo") or {}).get("Name", "")
            if name in HAIR_SCALARS and isinstance(entry.get("ParameterValue"), (int, float)):
                found[HAIR_SCALARS[name]] = float(entry["ParameterValue"])
        if props:
            break
    return found


def groom_colors(json_rel):
    """RootColor / TipColor d'un fichier de matériau inZOI (valeurs linéaires), sinon le brun par défaut"""
    colors, _diro = groom_json(json_rel)
    tip = colors.get("TipColor", BEARD_TIP)
    root = colors.get("RootColor", tuple(v * 0.5 for v in tip) if "TipColor" in colors else BEARD_ROOT)
    return root, tip


def diro_texture(diro_rel, fallback):
    """Texture DIRO. Si celle du JSON manque dans l'export, la carte par défaut : sans opacité, les cartes de poils
    deviennent des plaques pleines (sourcils et cils tout noirs). En linéaire (pas sRGB) : ce sont des données."""
    for rel in (diro_rel, fallback, DEFAULT_CARD):
        texture = import_texture(rel, "color") if rel else None
        if texture:
            if texture.get_editor_property("srgb"):
                set_prop(texture, "srgb", False)
                EAL.save_loaded_asset(texture)
            return texture
    return None


def hair_instance(asset_path, diro_rel, colors, fallback=None, zones=False, density=None, root_darkness=0.5, specular=None,
                  scalars=None):
    """Matériau de poils ; refait à chaque import (texture et couleurs), pour corriger les anciens.
    zones : expose Color1-3 (dégradé, mèches) pour que le nuancier les montre ; density : < 1 éclaircit (cils)"""
    instance = load(asset_path)
    if not instance:
        folder, name = split(asset_path)
        instance = asset_tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        stats["materials"] += 1
    MEL.set_material_instance_parent(instance, build_hair_master())
    texture = diro_texture(diro_rel, fallback)
    if texture:
        MEL.set_material_instance_texture_parameter_value(instance, "DIROMap", texture)
    root, tip = colors
    MEL.set_material_instance_vector_parameter_value(instance, "RootColor", unreal.LinearColor(root[0], root[1], root[2], 1))
    MEL.set_material_instance_vector_parameter_value(instance, "TipColor", unreal.LinearColor(tip[0], tip[1], tip[2], 1))
    if zones:
        for zone in (1, 2, 3):
            MEL.set_material_instance_vector_parameter_value(instance, "Color%d" % zone, unreal.LinearColor(1, 1, 1, HAIR_ZONE_AMOUNTS[zone]))
    MEL.set_material_instance_scalar_parameter_value(instance, "Density", density if density is not None else 1.0)
    MEL.set_material_instance_scalar_parameter_value(instance, "RootDarkness", root_darkness)
    MEL.set_material_instance_scalar_parameter_value(instance, "Specular", specular if specular is not None else 0.45)
    # Valeurs propres du matériau d'inZOI (sinon celles du maître : Roughness 0,42, Scatter 0,3, RootArea 0,15)
    defaults = {"Roughness": 0.42, "Scatter": HAIR_SCATTER, "RootArea": 0.15}
    for name, value in dict(defaults, **(scalars or {})).items():
        if name in ("Specular", "RootDarkness") and scalars and name in scalars:
            if name == "Specular" and specular is not None:
                continue  # barbe et moustache : spéculaire faible imposé
            if name == "RootDarkness" and root_darkness == 0.0:
                continue  # sourcils et cils : pas de racine
        MEL.set_material_instance_scalar_parameter_value(instance, name, value)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance)
    return instance


def head_names(parts):
    """Noms des meshes de têtes importées (SKM_Male_Head_002...)"""
    return sorted({p.get_editor_property("mesh").get_name() for p in parts
                   if str(p.get_editor_property("slot")) == "Head" and p.get_editor_property("mesh")})


def head_variants(mesh_rel, heads):
    """inZOI ajuste barbes, moustaches et sourcils à chaque tête : SKM_X_For_Male_Head_002 à côté de SKM_X"""
    variants = []
    for head in heads:
        variant_rel = "%s_For_%s" % (mesh_rel, head[4:] if head.startswith("SKM_") else head)
        if not os.path.isfile(src(variant_rel, ".glb")):
            continue
        mesh = import_mesh(variant_rel)
        if mesh:
            variant = unreal.TrainerPartVariant()
            variant.set_editor_property("head_mesh", head)
            variant.set_editor_property("mesh", mesh)
            variants.append(variant)
    return variants


def groom_materials(mesh, folder, card_fallback, default_colors, density=None):
    """Un matériau M_InZOI_Hair par emplacement du mesh, d'après le JSON du même nom dans <dossier>/Materials.
    Les JSON des cartes sont parfois vides : on prend alors la carte par défaut et les couleurs du cuir (Scalp)."""
    infos = []
    for slot_name in material_slot_names(mesh):
        json_rel = folder + "/Materials/" + slot_name
        colors, diro = groom_json(json_rel)
        infos.append((slot_name, json_rel, colors, diro, groom_scalars(json_rel)))
    shared = next((c for _n, _j, c, _d, _s in infos if "TipColor" in c), {})
    materials = []
    for slot_name, json_rel, colors, diro, scalars in infos:
        colors = colors if "TipColor" in colors else shared
        tip = colors.get("TipColor", default_colors[1])
        root = colors.get("RootColor", tuple(v * 0.6 for v in tip) if "TipColor" in colors else default_colors[0])
        if not diro and "scalp" in slot_name.lower():
            found = scan_folder_textures(folder, "Scalp_DIRO")
            diro = found or card_fallback
        # Sourcils et cils : pas de dégradé de racine (canal B quasi nul)
        no_root = any(key in folder for key in ("Eyebrow", "Eyelash"))
        # Barbe et moustache : spéculaire faible (0,12 : reflets blancs sinon)
        beard = any(key in folder for key in ("Beard", "Mustache"))
        materials.append(hair_instance(dest(json_rel + "_Jeu"), diro or card_fallback, (root, tip), card_fallback, density=density,
                                       root_darkness=0.0 if no_root else 0.5, specular=0.12 if beard else None,
                                       scalars=scalars))
    return materials


def scan_folder_textures(folder, suffix):
    for root in groom_roots():
        textures = os.path.join(root, *folder[len(GROOM_REL) + 1:].split("/"), "Textures")
        if os.path.isdir(textures):
            for filename in sorted(os.listdir(textures)):
                if filename.endswith(suffix + ".png"):
                    return folder + "/Textures/" + filename[:-4]
    return None


def find_folder_grooms(subdir):
    """Groom/<subdir>/<Nom>/Mesh/SKM_*.glb sans les versions « _For_<tête> » : [(dossier, mesh)]"""
    found = OrderedDict()
    for root in groom_roots():
        base = os.path.join(root, subdir)
        if not os.path.isdir(base):
            continue
        for name in sorted(os.listdir(base)):
            mesh_dir = os.path.join(base, name, "Mesh")
            if not os.path.isdir(mesh_dir):
                continue
            for filename in sorted(os.listdir(mesh_dir)):
                stem, ext = os.path.splitext(filename)
                if ext.lower() == ".glb" and stem.startswith("SKM_") and "_For_" not in stem:
                    folder = "%s/%s/%s" % (GROOM_REL, subdir, name)
                    found.setdefault(stem, (folder, folder + "/Mesh/" + stem))
    return list(found.values())


def build_beards(slow, bases, parts):
    """Barbes et moustaches (dossiers Groom/Beard et Groom/Mustache) : bases masculines, une variante par tête,
    elles suivent la tête (FollowSlot) donc les curseurs du visage les déforment avec elle."""
    male_bases = [str(b.get_editor_property("id")) for b in bases if b.get_editor_property("male")]
    if not male_bases:
        return []
    heads = head_names(parts)
    result = []
    for slot, label, subdir in (("Beard", "Barbe", "Beard"), ("Mustache", "Moustache", "Mustache")):
        number = 0
        for folder, mesh_rel in find_folder_grooms(subdir):
            slow.enter_progress_frame(0, "%s : %s" % (label, mesh_rel.rsplit("/", 1)[-1]))
            default = import_mesh(mesh_rel)
            if not default:
                continue
            number += 1
            variants = head_variants(mesh_rel, heads)
            materials = groom_materials(default, folder, BEARD_CARD, (BEARD_ROOT, BEARD_TIP))
            part = unreal.TrainerPartDef()
            part.set_editor_property("id", "InZOI_" + mesh_rel.rsplit("/", 1)[-1][4:])
            part.set_editor_property("label", "%s %d" % (label, number))
            part.set_editor_property("slot", slot)
            part.set_editor_property("bases", male_bases)
            part.set_editor_property("mesh", default)
            part.set_editor_property("variants", variants)
            part.set_editor_property("materials", materials)
            thumbnail = groom_thumbnail(mesh_rel)
            if thumbnail:
                part.set_editor_property("thumbnail", thumbnail)
            result.append(part)
        log("%s : %d modèles" % (label, number))
    return result


def groom_roots():
    """Dossiers Groom à fouiller : la copie à la racine de l'exercice (export des coiffures), puis l'export d'origine"""
    roots = [os.path.join(SOURCE, "Groom"), os.path.join(EXPORTS, *GROOM_REL.split("/"))]
    return [root for root in roots if os.path.isdir(root)]


def find_hairs(gender):
    """Coupes d'un genre : [(catégorie, chemin relatif du mesh)] (sans les versions sous chapeau ni les enfants)"""
    found = OrderedDict()
    for root in groom_roots():
        hair_root = os.path.join(root, "Hair", gender)
        for current, _dirs, files in os.walk(hair_root):
            for filename in sorted(files):
                name, ext = os.path.splitext(filename)
                if ext.lower() != ".glb" or not name.startswith("SKM_") or name.endswith(HAIR_SKIP_SUFFIXES) or "Child" in name:
                    continue
                rel = os.path.relpath(os.path.join(current, name), root).replace("\\", "/")
                category = rel.split("/")[2] if rel.count("/") >= 3 else ""
                found.setdefault(name, (category, GROOM_REL + "/" + rel))
    items = list(found.values())
    order = {key: index for index, key in enumerate(HAIR_CATEGORY_ORDER)}
    items.sort(key=lambda item: (order.get(item[0], len(order)), item[1]))
    if MAX_MESHES_PER_SLOT:
        kept, counts = [], {}
        for category, rel in items:
            counts[category] = counts.get(category, 0) + 1
            if counts[category] <= MAX_MESHES_PER_SLOT:
                kept.append((category, rel))
        items = kept
    return items


_GROOM_THUMBS = None


def groom_thumbnail(mesh_rel):
    """Aperçu d'une coupe, barbe ou moustache d'après les tables d'inZOI (AppearanceHairMesh, AppearanceBeard,
    AppearanceMustache) : la ligne dont le Mesh est ce modèle"""
    global _GROOM_THUMBS
    if _GROOM_THUMBS is None:
        _GROOM_THUMBS = {}
        for table in ("AppearanceHairMesh.json", "AppearanceBeard.json", "AppearanceMustache.json"):
            try:
                with open(os.path.join(JSON_TABLES, table), encoding="utf-8") as handle:
                    rows = json.load(handle)
            except (OSError, ValueError):
                log("Aperçus : table %s introuvable" % table)
                continue
            for row in rows if isinstance(rows, list) else []:
                mesh, thumb = game_rel(row.get("Mesh")), game_rel(row.get("Thumbnail"))
                if mesh and thumb:
                    _GROOM_THUMBS.setdefault(mesh.rsplit("/", 1)[-1], thumb)
    rel = _GROOM_THUMBS.get(mesh_rel.rsplit("/", 1)[-1])
    return import_texture(rel, "ui") if rel else None


# Préparation des cartes de cheveux (couleurs de sommet lues par M_InZOI_Hair) ; à augmenter si le calcul change
STRANDS_VERSION = 3


def bake_hair_strands(mesh):
    """Pour « Mèches » et « Mèches devant » : chaque carte reçoit en couleur de sommet R = devant (racine sur la frange
    ou les côtés du visage), G = numéro de mèche, B = 0. Fait une fois par coupe (étiquette InZOIStrands)."""
    library = getattr(unreal, "InZOIImportLibrary", None)
    if not library or not hasattr(library, "bake_hair_strands"):
        return
    if EAL.get_metadata_tag(mesh, "InZOIStrands") == str(STRANDS_VERSION):
        return
    result = library.bake_hair_strands(mesh, unreal.Vector(0.0, 1.0, 0.0), 70.0, -6.0, 5.0)
    if result:
        EAL.set_metadata_tag(mesh, "InZOIStrands", str(STRANDS_VERSION))
        EAL.save_loaded_asset(mesh)
        log("Mèches de %s : %s cartes devant" % (mesh.get_name(), result))


def build_hairs(slow, bases):
    """Coiffures d'inZOI (export « hairs » de L'amie, copié dans Exercice Compilation/Groom) : mèches en cartes,
    skinnées sur les os du corps. Matériaux M_InZOI_Hair d'après leurs JSON (carte DIRO, couleurs racine / pointe)."""
    parts, defaults = [], {}
    for gender, male in (("Male", True), ("Female", False)):
        fits = [str(b.get_editor_property("id")) for b in bases if bool(b.get_editor_property("male")) == male]
        if not fits:
            continue
        counters = {}
        for category, mesh_rel in find_hairs(gender):
            if slow.should_cancel():
                return parts, defaults
            name = mesh_rel.rsplit("/", 1)[-1]
            slow.enter_progress_frame(1, "Cheveux : " + name)
            mesh = import_mesh(mesh_rel)
            if not mesh:
                continue
            bake_hair_strands(mesh)
            folder = mesh_rel.rsplit("/Mesh/", 1)[0] if "/Mesh/" in mesh_rel else mesh_rel.rsplit("/", 1)[0]
            materials = []
            for slot_name in material_slot_names(mesh):
                json_rel = folder + "/Materials/" + slot_name
                colors, diro = groom_json(json_rel)
                if not diro:
                    diro = DEFAULT_CARD
                tip = colors.get("TipColor", BEARD_TIP)
                root = colors.get("RootColor", tuple(v * 0.5 for v in tip))
                materials.append(hair_instance(dest(json_rel + "_Jeu"), diro, (root, tip), DEFAULT_CARD, zones=True))
            label = HAIR_CATEGORY_FR.get(category, category or "Cheveux")
            counters[label] = counters.get(label, 0) + 1
            part = unreal.TrainerPartDef()
            part.set_editor_property("id", "InZOI_" + name)
            part.set_editor_property("label", "%s %d" % (label, counters[label]))
            part.set_editor_property("slot", "Hair")
            part.set_editor_property("category", label)
            part.set_editor_property("bases", fits)
            part.set_editor_property("mesh", mesh)
            part.set_editor_property("materials", materials)
            thumbnail = groom_thumbnail(mesh_rel)
            if thumbnail:
                part.set_editor_property("thumbnail", thumbnail)
            parts.append(part)
            for base_id in fits:
                defaults.setdefault(base_id, {}).setdefault("Hair", "InZOI_" + name)
        log("Cheveux %s : %d coupes" % (gender, sum(counters.values())))
    return parts, defaults


def build_face_grooms(slow, bases, heads):
    """Sourcils et cils d'après les tables AppearanceEyebrow / AppearanceEyelash ; ils suivent la tête (FollowSlot)"""
    parts, defaults = [], {}
    for slot, label, table, fallback_diro, fallback_tip in FACE_GROOMS:
        filename = os.path.join(JSON_TABLES, table)
        try:
            with open(filename, encoding="utf-8") as handle:
                rows = json.load(handle)
        except (OSError, ValueError):
            log("%s : table %s introuvable" % (label, filename))
            continue
        missing = 0
        for (gender, age), (base_id, _label, _male, _body) in BASES.items():
            if age not in AGES or not any(str(b.get_editor_property("id")) == base_id for b in bases):
                continue
            count = 0
            for row in rows:
                if row.get("GenderType") != gender or row.get("BodyAgeType") != age or row.get("HiddenFromUI"):
                    continue
                if MAX_MESHES_PER_SLOT and count >= MAX_MESHES_PER_SLOT:
                    break
                mesh_rel = game_rel(row.get("Mesh"))
                if not mesh_rel or not os.path.isfile(src(mesh_rel, ".glb")):
                    missing += 1
                    continue
                slow.enter_progress_frame(0, "%s : %s" % (label, mesh_rel.rsplit("/", 1)[-1]))
                mesh = import_mesh(mesh_rel)
                if not mesh:
                    continue
                folder = mesh_rel.rsplit("/Mesh/", 1)[0]
                materials = groom_materials(mesh, folder, fallback_diro, (tuple(v * 0.7 for v in fallback_tip), fallback_tip),
                                            EYELASH_DENSITY if slot == "Eyelashes" else EYEBROW_DENSITY if slot == "Eyebrows" else None)
                count += 1
                part = unreal.TrainerPartDef()
                part.set_editor_property("id", row["Id"])
                part.set_editor_property("label", "%s %d" % (label, count))
                part.set_editor_property("slot", slot)
                part.set_editor_property("bases", [base_id])
                part.set_editor_property("mesh", mesh)
                part.set_editor_property("variants", head_variants(mesh_rel, heads))
                part.set_editor_property("materials", materials)
                thumbnail = import_texture(game_rel(row.get("Thumbnail")), "ui")
                if thumbnail:
                    part.set_editor_property("thumbnail", thumbnail)
                parts.append(part)
                defaults.setdefault(base_id, {}).setdefault(slot, row["Id"])
            log("%s %s : %d modèles" % (label, base_id, count))
        if missing:
            log("%s : %d modèles absents (exporter Groom/%s avec FModel, puis copier_coiffures.bat)" % (label, missing, slot))
    return parts, defaults


# ---------------------------------------------------------------- Maquillage, motifs d'iris, animations

MAKEUP_ZONE_LABELS = ["", "Couleur 1", "Couleur 2", "Couleur 3"]
MAKEUP_AMOUNT_LABELS = ["", "Intensité", "Intensité", "Intensité"]


def build_makeup():
    """Maquillage d'inZOI (table AppearanceMakeup) : une pièce sans modèle par ligne dont le masque est dans l'export.
    Chaque canal R, V, B du masque garde la couleur et l'intensité d'inZOI (modifiables dans le nuancier)."""
    rows = read_table("AppearanceMakeup")
    by_category = {category: (slot, label) for category, _letter, slot, label, _blend in MAKEUP_PARTS}
    parts, counts, missing = [], {}, []
    for row in rows if isinstance(rows, list) else []:
        info = by_category.get(row.get("MakeupPartsId"))
        if not info or row.get("HiddenFromUI"):
            continue
        mask_rel = game_rel(row.get("MaskTexture"))
        mask = import_texture(mask_rel, "mask") if mask_rel and os.path.isfile(src(mask_rel, ".png")) else None
        if not mask:
            missing.append(row.get("Id", "?"))
            continue
        slot, label = info
        disabled = set(row.get("DisabledChannels") or [])
        values = {entry.get("MaskChannel"): entry for entry in row.get("DefaultValues") or [] if isinstance(entry, dict)}
        colors = []
        for channel in "RGB":
            entry = values.get(channel) or {}
            hexes = entry.get("ColorValues") or []
            scalars = entry.get("ScalarValues") or []
            rgb = linear_hex(hexes[0]) if hexes else None
            amount = float(scalars[0]) if scalars else 1.0
            if not rgb or channel in disabled or amount <= 0:
                colors.append(unreal.LinearColor(0, 0, 0, 0))  # canal inutilisé (souvent la brillance chez inZOI)
            else:
                colors.append(unreal.LinearColor(rgb[0], rgb[1], rgb[2], min(1.0, amount)))
        counts[slot] = counts.get(slot, 0) + 1
        part = unreal.TrainerPartDef()
        part.set_editor_property("id", "InZOI_Makeup_" + row["Id"])
        part.set_editor_property("label", "%s %d" % (label, counts[slot]))
        part.set_editor_property("slot", slot)
        part.set_editor_property("mask_texture", mask)
        part.set_editor_property("mask_colors", colors)
        thumbnail = import_texture(game_rel(row.get("Thumbnail")), "ui")
        if thumbnail:
            part.set_editor_property("thumbnail", thumbnail)
        parts.append(part)
    log("Maquillage : %d styles (%s)" % (len(parts), ", ".join("%s %d" % (k, v) for k, v in sorted(counts.items())) or "aucun"))
    if missing:
        log("Maquillage : %d masques absents (ex. %s) : dans FModel, clic droit sur %s > Save Folder's Packages "
            "Textures (.png), vers Exercice Compilation\\makeup (les .json seuls ne suffisent pas)" % (
                len(missing), ", ".join(missing[:4]), MAKEUP_MASKS))
    return parts


def mask_weights(rel):
    """Canaux d'un masque qui portent le dessin. Un dessin est clairsemé : canal surtout noir avec des pixels clairs
    (poids 1) ou surtout blanc avec des pixels sombres (poids -1 = inversé). Un canal à mi-gris (normale) ou plein
    est ignoré. {"R": 1, "G": 0, ...} ; R seul si la texture ne se lit pas"""
    image = png_rgba(src(rel, ".png"))
    weights = {"R": 1.0, "G": 0.0, "B": 0.0, "A": 0.0}
    if not image:
        return weights, "non lu"
    width, height, rows = image
    step = max(1, width // 256)
    sums, peaks, lows, count = [0.0] * 4, [0] * 4, [255] * 4, 0
    for y in range(0, height, step):
        line = rows[y]
        for x in range(0, width, step):
            count += 1
            for channel in range(4):
                value = line[x * 4 + channel]
                sums[channel] += value
                if value > peaks[channel]:
                    peaks[channel] = value
                if value < lows[channel]:
                    lows[channel] = value
    means = [total / (255.0 * max(1, count)) for total in sums]
    light = [channel for channel in range(4) if peaks[channel] > 60 and means[channel] < 0.3]
    dark = [channel for channel in range(4) if lows[channel] < 195 and means[channel] > 0.7]
    if light:
        weights = {name: (1.0 if index in light else 0.0) for index, name in enumerate("RGBA")}
    elif dark:
        weights = {name: (-1.0 if index in dark[:1] else 0.0) for index, name in enumerate("RGBA")}
    else:
        weights = {name: 0.0 for name in "RGBA"}  # rien de reconnu : mieux vaut rien afficher que teinter tout le visage
    detail = " ".join("%s moy %.2f min %d max %d" % (name, means[index], lows[index], peaks[index])
                      for index, name in enumerate("RGBA"))
    return weights, detail


def build_skin_features():
    """Détails de peau d'inZOI : taches de rousseur, grains de beauté, cicatrices, tatouages, peintures de visage.
    Pièces sans modèle comme le maquillage (masque posé sur la peau de la tête). Les lignes homme/femme et adulte/enfant
    d'inZOI partagent le même masque : une seule pièce par masque."""
    parts = []
    for slot, label, table, kind in SKIN_FEATURES:
        rows = read_table(table)
        if table == "AppearanceTattoo":
            rows = [row for row in rows if bool(row.get("bIsFacePainting")) == (slot == "SkinFacePaint")]
        seen, weights, number, missing = set(), None, 0, 0
        for row in rows:
            mask_rel = game_rel(row.get("MaskTexture"))
            if not mask_rel or mask_rel in seen or row.get("HiddenFromUI"):
                continue
            seen.add(mask_rel)
            if not os.path.isfile(src(mask_rel, ".png")):
                missing += 1
                continue
            mask = import_texture(mask_rel, "mask")
            if not mask:
                continue
            part = unreal.TrainerPartDef()
            number += 1
            part.set_editor_property("id", "InZOI_%s_%s" % (slot, mask_rel.rsplit("/", 1)[-1]))
            part.set_editor_property("label", "%s %d" % (SKIN_FEATURE_SINGULAR.get(slot, label), number))
            part.set_editor_property("slot", slot)
            part.set_editor_property("mask_texture", mask)
            if kind == "rgb":
                colors = []
                for key in ("DefaultTattooColor1", "DefaultTattooColor2", "DefaultTattooColor3"):
                    text = (row.get(key) or "").strip()
                    rgb = linear_hex(text)
                    # 000000 = couleur inutilisée, sauf la 1re (tatouage noir)
                    if rgb and (key.endswith("1") or text.upper()[:6] != "000000"):
                        colors.append(unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
                    else:
                        colors.append(unreal.LinearColor(0, 0, 0, 0))
            else:
                if weights is None:
                    # Une analyse par sorte (pixels lus en Python : quelques secondes)
                    weights, detail = mask_weights(mask_rel)
                    log("%s : canaux du masque %s (%s)" % (label, " ".join("%s%s" % ("-" if v < 0 else "", k) for k, v in weights.items() if v) or "aucun", detail))
                part.set_editor_property("mask_scalars", {"%s_W%s" % (slot, k): v for k, v in weights.items()})
                if kind == "scar":
                    rgb = (1.0, 1.0, 1.0)
                    color_rel = game_rel(row.get("ColorTexture"))
                    color = import_texture(color_rel, "color") if color_rel and os.path.isfile(src(color_rel, ".png")) else None
                    if color:
                        part.set_editor_property("mask_extra_textures", {SCAR_COLOR_PARAM: color})
                    amount = 0.9
                else:
                    rgb = linear_hex(row.get("DefaultMoleColor") or FRECKLE_COLOR) or linear_hex(FRECKLE_COLOR)
                    amount = 1.0 if slot == "SkinMoles" else 0.7
                colors = [unreal.LinearColor(rgb[0], rgb[1], rgb[2], amount)]
            part.set_editor_property("mask_colors", colors)
            thumbnail = import_texture(game_rel(row.get("Thumbnail")), "ui")
            if thumbnail:
                part.set_editor_property("thumbnail", thumbnail)
            parts.append(part)
        log("%s : %d%s" % (label, number, (" (%d masques absents de l'export)" % missing) if missing else ""))
    return parts


def png_rgba(filename):
    """Lit un PNG 8 bits non entrelacé (gris, RVB, RVBA) : (largeur, hauteur, lignes de pixels RVBA). None sinon"""
    import struct
    import zlib
    try:
        with open(filename, "rb") as handle:
            data = handle.read()
    except OSError:
        return None
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    pos, idat, header = 8, [], None
    while pos + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            idat.append(chunk)
        elif kind == b"IEND":
            break
        pos += 12 + length
    if not header:
        return None
    width, height, depth, color_type, _compression, _filter, interlace = header
    channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color_type)
    if depth != 8 or interlace or not channels:
        return None
    raw = zlib.decompress(b"".join(idat))
    stride = width * channels
    rows, previous, offset = [], bytearray(stride), 0
    for _y in range(height):
        kind = raw[offset]
        line = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += 1 + stride
        for i in range(stride):
            left = line[i - channels] if i >= channels else 0
            up = previous[i]
            if kind == 1:
                line[i] = (line[i] + left) & 255
            elif kind == 2:
                line[i] = (line[i] + up) & 255
            elif kind == 3:
                line[i] = (line[i] + ((left + up) >> 1)) & 255
            elif kind == 4:
                corner = previous[i - channels] if i >= channels else 0
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                line[i] = (line[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
        rows.append(line)
        previous = line
    expanded = []
    for line in rows:
        if channels == 4:
            expanded.append(line)
            continue
        out = bytearray(width * 4)
        for x in range(width):
            px = line[x * channels:(x + 1) * channels]
            if channels == 1:
                out[x * 4:x * 4 + 4] = bytes((px[0], px[0], px[0], 255))
            elif channels == 2:
                out[x * 4:x * 4 + 4] = bytes((px[0], px[0], px[0], px[1]))
            else:
                out[x * 4:x * 4 + 4] = bytes((px[0], px[1], px[2], 255))
        expanded.append(out)
    return width, height, expanded


def iris_edge(rel):
    """Rayon (dans la texture) du bord de l'iris d'un motif : premier anneau où le canal B (fondu des couleurs) arrive
    à 0,95 en moyenne, comme dans T_Iris_A_M (0,45). None si la texture ne se lit pas."""
    image = png_rgba(src(rel, ".png"))
    if not image:
        return None
    width, height, rows = image
    bins = 25
    sums, counts = [0.0] * bins, [0] * bins
    for y in range(height):
        line = rows[y]
        dy = (y + 0.5) / height - 0.5
        for x in range(width):
            radius = math.hypot((x + 0.5) / width - 0.5, dy)
            index = int(radius / 0.02)
            if index < bins:
                sums[index] += line[x * 4 + 2] / 255.0
                counts[index] += 1
    for index in range(3, bins):
        if counts[index] and sums[index] / counts[index] >= 0.95:
            return round(index * 0.02, 3)
    return None


def build_lenses():
    """Motifs d'iris d'inZOI (lentilles Lens_Custom_NNN_M, même format que T_Iris_A_M) : onglet Visage > Yeux.
    Le motif remplace la texture d'iris de l'œil ; la couleur reste celle choisie dans le nuancier des yeux."""
    folder = os.path.join(EXPORTS, *LENS_TEXTURES.split("/"))
    if not os.path.isdir(folder):
        log("Motifs d'iris : dossier %s absent de l'export" % LENS_TEXTURES)
        return []
    reference = None
    for rel in IRIS_MASKS:
        reference = iris_edge(rel)
        if reference:
            break
    reference = reference or IRIS_EDGE_REFERENCE
    parts = []
    for filename in sorted(os.listdir(folder)):
        match = re.match(r"Lens_Custom_(\d+)_M\.png$", filename)
        if not match or match.group(1) == "000":  # 000 = lentille vide
            continue
        rel = LENS_TEXTURES + "/" + filename[:-4]
        texture = import_texture(rel, "mask")
        if not texture:
            continue
        number = int(match.group(1))
        edge = iris_edge(rel)
        part = unreal.TrainerPartDef()
        part.set_editor_property("id", "InZOI_Lens_%03d" % number)
        part.set_editor_property("label", "Motif %d" % number)
        part.set_editor_property("slot", "Lens")
        part.set_editor_property("mask_texture", texture)
        # Bord de l'iris du motif, à l'échelle de notre référence (0,45 dans T_Iris_A_M)
        scaled = IRIS_EDGE_REFERENCE * (edge / reference) if edge else IRIS_EDGE_REFERENCE
        part.set_editor_property("mask_scalars", {IRIS_EDGE_PARAM: round(max(0.2, min(0.7, scaled)), 3)})
        thumbnail = import_texture("%s/Icon_Lens_CT%03d" % (LENS_ICONS, number), "ui")
        if thumbnail:
            part.set_editor_property("thumbnail", thumbnail)
        parts.append(part)
        log("Motif d'iris %d : bord %s" % (number, edge if edge else "non mesuré"))
    log("Motifs d'iris : %d (référence T_Iris_A_M : bord %.2f)" % (len(parts), reference))
    return parts


# Animations .ueanim importées (sans extension). Les « ANIM_F_ » d'inZOI vont aussi sur le corps d'homme (même
# squelette). La première est l'attente du studio ; les autres sont les poses du bouton « Pose » de l'éditeur.
# Pour en voir d'autres : Index_Complet/Manifeste_Assets.csv, filtrer « .ueanim ».
# Attente du créateur : un vrai idle du corps (debout, respiration, petits mouvements). Les « Portrait » d'inZOI
# n'animent que le visage : le corps restait en T. Le premier trouvé sert ; les autres de la liste sont des secours.
# NPC_Idle3 d'abord : comparée par le banc BancPerso, c'est la plus calme (debout de face, pieds fixes). Crowd_idle1,
# l'ancienne, est une attente de foule qui fait des pas et tourne sur place : le personnage sortait de l'écran.
IDLE_ANIMATIONS = [
    "Characters/Anims/Interaction/Female/ANIM_F_NPC_Idle3",
    "Characters/Anims/Interaction/Female/Emotion/Body/ANIM_F_Crowd_idle7",
    "Characters/Anims/Interaction/Female/Emotion/Body/ANIM_F_Crowd_idle1",
]
IDLE_LABEL = "Attente"
ANIMATIONS = [(rel, IDLE_LABEL) for rel in IDLE_ANIMATIONS] + [
    ("Characters/Anims/Interaction/Female/ANIM_F_Clerk_Idle_Short_01", "Mains jointes"),
    ("Characters/Anims/Interaction/Female/ANIM_F_HappyGreet", "Salut"),
    ("Characters/Anims/Interaction/Female/ANIM_F_Clap_Stand_2", "Bravo"),
    ("Characters/Anims/Interaction/Female/ANIM_F_Selfie_Pose", "Selfie"),
    ("Characters/Anims/Interaction/Female/ANIM_F_MusclePose_2", "Muscles"),
    ("Characters/Anims/Interaction/Female/ANIM_F_Pose_CatHeart", "Cœur"),
    ("Characters/Anims/Interaction/Female/ANIM_F_Craft_Proud", "Fier"),
    ("Characters/Anims/Interaction/Female/Emotion/Body/ANIM_F_Bored_Idle1", "Ennui"),
    ("Characters/Anims/Interaction/Female/ANIM_F_NPC_Idle2", "Détente"),
    # Cabine d'essayage d'inZOI (« Fit_Stand ») : on se regarde, on montre sa tenue
    ("Characters/Anims/Interaction/Female/ANIM_F_Normal_Female1_Fit_Stand", "Essayage 1"),
    ("Characters/Anims/Interaction/Female/ANIM_F_Normal_Female2_Fit_Stand", "Essayage 2"),
    ("Characters/Anims/Interaction/Female/ANIM_F_Normal_Female3_Fit_Stand", "Essayage 3"),
]
_ANIM_FILES = None


def find_anim_file(rel):
    """Le .ueanim ; s'il n'est pas au chemin prévu (sous-dossier Emotion/Body...), on le cherche par son nom"""
    global _ANIM_FILES
    direct = src(rel, ".ueanim")
    if os.path.isfile(direct):
        return direct
    if _ANIM_FILES is None:
        _ANIM_FILES = {}
        for current, _dirs, files in os.walk(os.path.join(EXPORTS, "Characters", "Anims")):
            for filename in files:
                if filename.endswith(".ueanim"):
                    _ANIM_FILES.setdefault(filename, os.path.join(current, filename))
    return _ANIM_FILES.get(rel.rsplit("/", 1)[-1] + ".ueanim")


def build_animations(bases):
    """Attente et poses du studio. Les animations manquantes sont importées sans la fenêtre du plugin UEFormat
    (UInZOIImportLibrary, C++) ; celles déjà importées (même par l'ancien script) sont reprises telles quelles."""
    library = getattr(unreal, "InZOIImportLibrary", None)
    can_import = bool(library and library.can_import_ue_anim())
    if not can_import:
        log("Animations : plugin UEFormat absent (Plugins/UEFormat) ou projet pas recompilé : seules celles déjà importées servent")
    for base in bases:
        base_id = str(base.get_editor_property("id"))
        mesh = base.get_editor_property("mesh")
        skeleton = mesh.get_editor_property("skeleton") if mesh else None
        ready, imported, failed = [], 0, []
        idle = None
        for rel, label in ANIMATIONS:
            if label == IDLE_LABEL and idle:
                continue  # attente déjà trouvée : les secours ne sont pas importés
            name = rel.rsplit("/", 1)[-1]
            asset_path = "%s/Anims/%s/%s" % (DEST, base_id, name)
            animation = load(asset_path)
            if not animation and can_import and skeleton:
                filename = find_anim_file(rel)
                animation = library.import_ue_anim(filename, skeleton, asset_path) if filename else None
                if animation:
                    EAL.save_loaded_asset(animation)
                    imported += 1
                else:
                    failed.append(name if filename else name + " (fichier absent)")
            if animation and not animation.get_editor_property("force_root_lock"):
                # Sur place : certaines attentes d'inZOI déplacent la racine (le personnage partait sur le côté)
                set_prop(animation, "force_root_lock", True)
                EAL.save_loaded_asset(animation)
            if animation and label == IDLE_LABEL:
                idle = animation
            elif animation:
                ready.append((label, animation))
        # Toujours remplacée : l'ancienne attente (portrait, corps en T) ne doit pas rester
        base.set_editor_property("idle_animation", idle)
        poses = []
        for label, animation in ready:
            entry = unreal.TrainerStudioAnimation()
            entry.set_editor_property("label", label)
            entry.set_editor_property("animation", animation)
            poses.append(entry)
        base.set_editor_property("studio_animations", poses)
        log("Animations %s : attente %s, %d poses (%d importées maintenant)%s" % (
            base_id, "oui" if base.get_editor_property("idle_animation") else "non", len(poses), imported,
            " ; échecs : " + ", ".join(failed) if failed else ""))


# Onglet Modèles : les personnages tout faits d'inZOI (table AppearanceFacePreset) : tête, coupe et sa couleur,
# couleur de peau, couleur des yeux, sourcils, cils, barbe et moustache. Les vêtements restent ceux du joueur.
MAX_PRESETS_PER_BASE = 40
# Teinte de peau d'inZOI qui correspond à la texture telle quelle (la plus claire, c101) : les autres la foncent
SKIN_REFERENCE = "C19C8EFF"


def srgb_to_linear(value):
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def linear_hex(text):
    """« A68570FF » (sRGB, comme dans les tables d'inZOI) -> (r, g, b) linéaires, ou None"""
    color = hex_color(text)
    if not color or (text or "").strip().lower() in ("none", ""):
        return None
    return tuple(srgb_to_linear(c) for c in (color.r, color.g, color.b))


SKIN_WARM = (0.87, 0.83, 0.76)


def skin_tint(tone_hex, palette_hex=None):
    """SkinTint (multiplié à la texture de peau) pour une teinte d'inZOI. La teinte vient de SkinTone (neutre), la
    luminosité de PalleteColor (celle de la vignette) : SkinTone seule assombrissait tous les visages, la palette
    seule les rendait orange (couleurs de nuancier très saturées)."""
    tone, reference, palette = linear_hex(tone_hex), linear_hex(SKIN_REFERENCE), linear_hex(palette_hex)
    if not tone:
        return None
    tint = [t / r for t, r in zip(tone, reference)]
    if palette:
        luminance = lambda c: 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
        target = luminance([p / r for p, r in zip(palette, reference)])
        current = luminance(tint)
        if current > 0.0:
            tint = [t * target / current for t in tint]
    # Un peu désaturée : les visages foncés viraient à l'orange sous la lumière du studio
    gray = 0.2126 * tint[0] + 0.7152 * tint[1] + 0.0722 * tint[2]
    tint = [gray + (t - gray) * 0.6 for t in tint]
    # 15 % plus foncée et plus chaude (note du 9 novembre ; banc A/B sur W_Head07_CT010 : peau moins grise)
    tint = [t * k for t, k in zip(tint, SKIN_WARM)]
    return tuple(max(0.05, min(1.0, t)) for t in tint)


def hair_part_id(mesh_id):
    """W_Hair_M_009_05 -> InZOI_SKM_Female_Hair_M_009 (le _05 final est une variante de couleur)"""
    pieces = (mesh_id or "").split("_")
    if len(pieces) < 4 or pieces[1] != "Hair":
        return None
    gender = {"W": "Female", "M": "Male"}.get(pieces[0])
    if not gender:
        return None
    return "InZOI_SKM_%s_Hair_%s_%s" % (gender, pieces[2], pieces[3])


BODY_SHAPE_FR = {"Average": "Moyen", "Slim": "Mince", "Overweight": "Rond", "Muscular": "Musclé", "Bodyhair": "Poilu"}


def body_presets(bases):
    """Silhouettes d'inZOI (AppearanceBodyShape.json) : Muscles (MuscleDefinition, 0 à 1), Corpulence (BodyMassRatio,
    -1 à 1) et les curseurs du corps (BodyModifierParameters, -1 à 1, rangés par index_groupe de Morphologie_Corps_Os)"""
    rows = read_table("AppearanceBodyShape")
    morpho = read_csv(MORPHO_BODY)
    presets = []
    for (gender, age), (base_id, _label, _male, _body) in BASES.items():
        if not any(str(b.get_editor_property("id")) == base_id for b in bases):
            continue
        groups = {}
        for row in morpho:
            if row.get("genre") == gender and row.get("age") == age:
                try:
                    groups.setdefault(int(row.get("index_groupe")), row["groupe"])
                except (TypeError, ValueError):
                    pass
        counts = {}
        for row in rows:
            if row.get("GenderType") != gender or row.get("BodyAgeType") != age or row.get("HiddenFromUI"):
                continue
            try:
                values = [float(v) for v in (row.get("BodyModifierParameters") or "").strip("()").split(",") if v.strip()]
            except ValueError:
                continue
            if len(values) <= max(groups or [0]):
                continue  # anciennes silhouettes à 21 valeurs, cachées dans inZOI
            sliders = []
            for slider_id, value in [("Muscle", row.get("MuscleDefinition", 0.0)), ("Masse", row.get("BodyMassRatio", 0.0))] + [
                    ("Corps_" + groups[i], v) for i, v in enumerate(values) if i in groups]:
                entry = unreal.TrainerSliderValue()
                entry.set_editor_property("slider", slider_id)
                entry.set_editor_property("value", max(-1.0, min(1.0, float(value or 0.0))))
                sliders.append(entry)
            look = unreal.TrainerAppearance()
            look.set_editor_property("base", base_id)
            look.set_editor_property("sliders", sliders)
            match = re.match(r"[MF]_([A-Za-z]+?)(\d*)$", row.get("Id", ""))
            kind = BODY_SHAPE_FR.get(match.group(1), match.group(1)) if match else row.get("Id", "")
            counts[kind] = counts.get(kind, 0) + 1
            preset = unreal.TrainerPresetDef()
            preset.set_editor_property("label", "%s %d" % (kind, counts[kind]))
            thumbnail = import_texture(game_rel(row.get("Thumbnail")), "ui")
            if thumbnail:
                preset.set_editor_property("thumbnail", thumbnail)
            preset.set_editor_property("look", look)
            presets.append(preset)
        log("Silhouettes %s : %d" % (base_id, sum(counts.values())))
    return presets


def face_shape_sliders(row, groups):
    """FaceShapeParameters d'un modèle -> valeurs des curseurs du visage (TrainerSliderValue)"""
    try:
        values = json.loads(row.get("FaceShapeParameters") or "[]")
    except ValueError:
        values = []
    sums = OrderedDict()
    for index, value in enumerate(values if isinstance(values, list) else []):
        group = groups.get(index)
        if not group:
            continue
        feature, axis = face_group_key(group)
        slider_id = "Visage_" + feature + ("_" + axis if axis else "")
        sums.setdefault(slider_id, []).append(float(value or 0.0))
    sliders = []
    for slider_id, items in sums.items():
        value = max(-1.0, min(1.0, sum(items) / len(items)))
        if abs(value) < 0.001:
            continue
        entry = unreal.TrainerSliderValue()
        entry.set_editor_property("slider", slider_id)
        entry.set_editor_property("value", value * FACE_SHAPE_GAIN)
        sliders.append(entry)
    return sliders


# Force des formes du visage des modèles (1 = valeurs d'inZOI)
FACE_SHAPE_GAIN = 1.0


def full_presets(bases, parts):
    rows = read_csv(os.path.join(TABLES, "AppearanceFacePreset.csv"))
    # Couleur de la palette (PalleteColor) : celle qu'on voit sur la vignette. SkinTone, plus foncée, est une teinte de
    # masque d'inZOI : multipliée à notre texture (déjà couleur peau), elle assombrissait tous les visages.
    skin_tones = {r.get("ID"): (r.get("SkinTone"), r.get("PalleteColor")) for r in read_csv(os.path.join(TABLES, "AppearanceSkinColor.csv"))}
    by_id = {str(p.get_editor_property("id")): p for p in parts}
    groom_ids = {"Eyebrow": ("Eyebrows", ""), "Eyelash": ("Eyelashes", ""), "Beard": ("Beard", "InZOI_"), "Mustache": ("Mustache", "InZOI_")}
    presets, per_base = [], OrderedDict()
    makeup_slots = {category.lower(): slot for category, _letter, slot, _label, _blend in MAKEUP_PARTS}
    face_groups = {}
    for morpho in read_csv(MORPHO_FACE) if os.path.isfile(MORPHO_FACE) else []:
        if morpho.get("age") == "Adult":
            try:
                face_groups.setdefault(morpho.get("genre"), {})[int(morpho.get("index_groupe"))] = morpho["groupe"]
            except (TypeError, ValueError):
                pass
    for (gender, age), (base_id, base_label, _male, _body) in BASES.items():
        if not any(str(b.get_editor_property("id")) == base_id for b in bases):
            continue
        fits = lambda part_id: part_id in by_id and base_id in [str(b) for b in by_id[part_id].get_editor_property("bases")]
        count, seen = 0, set()
        candidates = [r for r in rows if r.get("GenderType") == gender and r.get("BodyAgeType") == age
                      and r.get("HiddenFromUI") != "True" and fits(r.get("BaseHeadId"))]
        # D'abord ceux dont la coupe est importée (sinon le modèle garderait la coupe actuelle)
        candidates.sort(key=lambda r: 0 if fits(hair_part_id(r.get("Hair.MeshId")) or "") else 1)
        for row in candidates:
            if count >= MAX_PRESETS_PER_BASE:
                break
            head_id = row.get("BaseHeadId")
            if row.get("Id") in seen:
                continue
            seen.add(row.get("Id"))
            look = unreal.TrainerAppearance()
            look.set_editor_property("base", base_id)
            outfit, colors = [head_id], []

            def add_color(slot, hex_text, zone=0, amount=1.0):
                rgb = linear_hex(hex_text)
                if rgb:
                    entry = unreal.TrainerPartColor()
                    entry.set_editor_property("slot", slot)
                    entry.set_editor_property("zone", zone)
                    entry.set_editor_property("color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], amount))
                    colors.append(entry)

            hair_id = hair_part_id(row.get("Hair.MeshId"))
            if hair_id and fits(hair_id):
                outfit.append(hair_id)
                tip = row.get("Hair.VectorParameters.tipColor")
                add_color("Hair", tip)
                # Coupe bicolore d'inZOI (2ndColor à partir de 2ndColorPosition) -> zone Dégradé, alpha = début.
                # Le dégradé des cartes d'inZOI (canal B) n'est pas réparti sur toute la longueur : à position égale,
                # la 2e couleur commençait bien plus bas que sur la vignette (W_Head03_CT017 : rose dès le haut).
                # Position 0 = 2e couleur inutilisée (W_Head07_CT010 : brun sur la vignette, pas la 2e couleur claire).
                # Sans 2e couleur, bridgeColor1 + sa position donnent les pointes (M_Head04_CT011 : pointes blondes).
                second = row.get("Hair.VectorParameters.2ndColor")
                position = float_of(row, "Hair.ScalarParameters.2ndColorPosition", 0.5)
                if not (second and linear_hex(second) and second.upper() != (tip or "").upper() and position > 0.0):
                    second = row.get("Hair.VectorParameters.bridgeColor1")
                    position = float_of(row, "Hair.ScalarParameters.bridgeColor1Position", 1.0)
                if second and linear_hex(second) and second.upper() != (tip or "").upper() and 0.0 < position < 1.0:
                    add_color("Hair", second, 1, max(0.0, min(1.0, position)) * HAIR_SECOND_POSITION_SCALE)
            try:
                face_hairs = json.loads(row.get("FaceHairs") or "[]")
            except ValueError:
                face_hairs = []
            for entry in face_hairs if isinstance(face_hairs, list) else []:
                slot, prefix = groom_ids.get(entry.get("FaceHairType"), (None, ""))
                if not slot:
                    continue
                groom_id = entry.get("FaceHairId") or "None"
                if groom_id == "None":
                    outfit.append("-" + slot)
                    continue
                if fits(prefix + groom_id):
                    outfit.append(prefix + groom_id)
                    if slot != "Eyelashes":  # les cils gardent leur noir naturel
                        add_color(slot, (entry.get("VectorParameters") or {}).get("tipColor"))
            # Maquillage du modèle (lèvres, fard, blush...) : pièce + couleurs de chaque canal (alpha = intensité)
            try:
                makeups = json.loads(row.get("Makeup") or "[]")
            except ValueError:
                makeups = []
            used = set()
            for entry in makeups if isinstance(makeups, list) else []:
                slot = makeup_slots.get((entry.get("MakeupType") or "").lower())
                part_id = "InZOI_Makeup_%s" % entry.get("MakeupId")
                if not slot or part_id not in by_id:
                    continue
                outfit.append(part_id)
                used.add(slot)
                zones = {"R": 1, "G": 2, "B": 3}
                for mask in entry.get("MaskData") or []:
                    zone = zones.get(mask.get("MakeupChannel"))
                    hexes = [v for v in (mask.get("ColorValues") or {}).values() if v]
                    amounts = [v for k, v in (mask.get("ScalarValues") or {}).items() if k.lower().endswith("intensity")]
                    if zone and hexes:
                        add_color(slot, hexes[0], zone, max(0.0, min(1.0, float(amounts[0]) if amounts else 1.0)))
                    elif zone == 3 and slot == "MakeupLips":
                        scalars = mask.get("ScalarValues") or {}
                        rough = [v for k, v in scalars.items() if k.lower().endswith("spec_roughness")]
                        spec = [v for k, v in scalars.items() if k.lower().endswith("spec_intensity")]
                        if rough or spec:
                            entry = unreal.TrainerPartColor()
                            entry.set_editor_property("slot", slot)
                            entry.set_editor_property("zone", zone)
                            entry.set_editor_property("color", unreal.LinearColor(
                                float(rough[0]) if rough else 0.5, 0.0, 0.0, max(0.0, min(1.0, float(spec[0]) if spec else 1.0))))
                            colors.append(entry)
                    elif zone and slot == "MakeupHighlight":
                        # Canal sans couleur du highlight = brillance (HighlightSpec_Intensity) : alpha = intensité
                        spec = [v for k, v in (mask.get("ScalarValues") or {}).items() if k.lower().endswith("spec_intensity")]
                        if spec:
                            add_color(slot, "FFFFFFFF", zone, max(0.0, min(1.0, float(spec[0]))))
            for _category, _letter, slot, _label, _blend in MAKEUP_PARTS:
                if slot not in used:
                    outfit.append("-" + slot)  # sinon le maquillage du modèle précédent restait
            look.set_editor_property("outfit", outfit)
            look.set_editor_property("part_colors", colors)
            # Formes du visage du modèle (FaceShapeParameters : une valeur par groupe C_* de Morphologie_Visage_Os,
            # rangé par index_groupe) -> curseurs « Visage_<partie>_<axe> » du jeu (gauche et droite : moyenne).
            # Sans elles, chaque modèle gardait le visage de base de sa tête (yeux trop ouverts, air renfrogné).
            look.set_editor_property("sliders", face_shape_sliders(row, face_groups.get(gender, {})))
            tone, palette = skin_tones.get(row.get("FaceSkinColorId")) or (row.get("SkinTone"), None)
            tint = skin_tint(tone, palette)
            if tint:
                look.set_editor_property("skin_color", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
            eye = linear_hex(row.get("IrisVectorParameters.eyeColor1"))
            if eye:
                look.set_editor_property("eye_color", unreal.LinearColor(eye[0], eye[1], eye[2], 1.0))
            count += 1
            preset = unreal.TrainerPresetDef()
            preset.set_editor_property("label", "")  # pas de « Homme 3 » : la vignette suffit (demandé)
            thumbnail = import_texture(game_rel(row.get("Thumbnail")), "ui") or by_id[head_id].get_editor_property("thumbnail")
            if thumbnail:
                preset.set_editor_property("thumbnail", thumbnail)
            preset.set_editor_property("look", look)
            per_base.setdefault(base_id, []).append(preset)
        log("Modèles complets %s : %d (table AppearanceFacePreset)" % (base_id, count))
    # Hommes et femmes mélangés dans la grille (une vignette de chaque à tour de rôle)
    lists = list(per_base.values())
    while any(lists):
        for items in lists:
            if items:
                presets.append(items.pop(0))
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
    hairs = len(find_hairs("Male")) + len(find_hairs("Female"))
    return total + 12 + hairs


def main():
    if not os.path.isdir(EXPORTS):
        unreal.log_error("[InZOI] Dossier introuvable : %s (corrige SOURCE en haut du script)" % EXPORTS)
        return

    # Matériaux maîtres refaits tout de suite s'ils ont changé de version : avant, ils ne l'étaient qu'à la création
    # d'une tête ou d'une coupe, et les têtes déjà importées gardaient l'ancienne peau (maquillage sans effet)
    for builder in (build_master, build_skin_master, build_eye_master, build_hair_master):
        builder()
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
            base.set_editor_property("materials", [m for m in SKIN_MATERIALS.get(body.get_path_name(), []) if m])
            base.set_editor_property("male", male)
            base.set_editor_property("anim_class", unreal.TrainerAnimInstance.static_class())
            base.set_editor_property("sliders", body_sliders(gender, age) + face_sliders(gender, age))
            bases.append(base)

        parts, defaults = build_parts(slow)
        hair_parts, hair_defaults = build_hairs(slow, bases)
        parts += hair_parts
        for base_id, slots in hair_defaults.items():
            defaults.setdefault(base_id, {}).update(slots)
        parts += build_beards(slow, bases, parts)
        face_parts, face_defaults = build_face_grooms(slow, bases, head_names(parts))
        parts += face_parts
        for base_id, slots in face_defaults.items():
            defaults.setdefault(base_id, {}).update(slots)
        slow.enter_progress_frame(1, "Maquillage et motifs d'iris")
        parts += build_makeup()
        parts += build_skin_features()
        parts += build_lenses()
        slow.enter_progress_frame(1, "Animations")
        build_animations(bases)

        # Peau de la tête teintée aussi
        for part in parts:
            if str(part.get_editor_property("slot")) == "Head":
                mesh = part.get_editor_property("mesh")
                if mesh:
                    # Les matériaux voulus (SKIN_MATERIALS), pas ceux de l'asset : Python ne les enregistre pas sur le mesh
                    chosen = SKIN_MATERIALS.get(mesh.get_path_name(), [])
                    for slot_name, interface in zip(material_slot_names(mesh), chosen):
                        if interface and interface.get_name().startswith("MI_Skin"):
                            skin_slots.add(slot_name)

        for base in bases:
            base_id = str(base.get_editor_property("id"))
            base_defaults = defaults.setdefault(base_id, {})
            for slot, wanted in DEFAULT_CATEGORIES.items():
                fitting = [p for p in parts if str(p.get_editor_property("slot")) == slot
                           and base_id in [str(b) for b in p.get_editor_property("bases")]]
                for category in wanted:
                    match = [p for p in fitting if str(p.get_editor_property("category")).startswith(category)]
                    if match:
                        base_defaults[slot] = match[0].get_editor_property("id")
                        break
            log("Tenue de départ %s : %s" % (base_id, ", ".join("%s=%s" % (k, base_defaults[k]) for k in DEFAULT_SLOTS if k in base_defaults)))
            base.set_editor_property("default_parts", [base_defaults[s] for s in DEFAULT_SLOTS if s in base_defaults])

        slot_defs = []
        for slot, slot_label, tab, required, replaces, _table in SLOTS:
            definition = unreal.TrainerPartSlotDef()
            definition.set_editor_property("slot", slot)
            definition.set_editor_property("label", slot_label)
            definition.set_editor_property("tab", tab)
            definition.set_editor_property("required", required)
            definition.set_editor_property("replaces", replaces)
            if slot in LOOSE_SLOTS:
                definition.set_editor_property("helper_scale_limit", LOOSE_SLOTS[slot])
            slot_defs.append(definition)
        for slot, slot_label, tab, follow in GROOM_SLOTS:
            definition = unreal.TrainerPartSlotDef()
            definition.set_editor_property("slot", slot)
            definition.set_editor_property("label", slot_label)
            definition.set_editor_property("tab", tab)
            if follow:
                definition.set_editor_property("follow_slot", follow)
            if slot == "Hair":
                definition.set_editor_property("zone_labels", [unreal.Text(t) for t in HAIR_ZONE_LABELS])
                definition.set_editor_property("zone_amount_labels", [unreal.Text(t) for t in HAIR_ZONE_AMOUNT_LABELS])
                definition.set_editor_property("zone_default_amounts", HAIR_ZONE_AMOUNTS)
            slot_defs.append(definition)
        # Pièces sans modèle posées sur la tête : maquillage (onglet Visage > Maquillage), motif d'iris (Visage > Yeux)
        for _category, _letter, slot, slot_label, _blend in MAKEUP_PARTS:
            definition = unreal.TrainerPartSlotDef()
            definition.set_editor_property("slot", slot)
            definition.set_editor_property("label", slot_label)
            definition.set_editor_property("tab", "Makeup")
            definition.set_editor_property("follow_slot", "Head")
            definition.set_editor_property("mask_parameter", slot)
            definition.set_editor_property("zone_labels", [unreal.Text(t) for t in MAKEUP_ZONE_LABELS])
            definition.set_editor_property("zone_amount_labels", [unreal.Text(t) for t in MAKEUP_AMOUNT_LABELS])
            slot_defs.append(definition)
        for slot, slot_label, _table, kind in SKIN_FEATURES:
            definition = unreal.TrainerPartSlotDef()
            definition.set_editor_property("slot", slot)
            definition.set_editor_property("label", slot_label)
            definition.set_editor_property("tab", "Makeup")
            definition.set_editor_property("follow_slot", "Head")
            definition.set_editor_property("mask_parameter", slot)
            labels = MAKEUP_ZONE_LABELS if kind == "rgb" else ["", "Teinte" if kind == "scar" else "Couleur"]
            definition.set_editor_property("zone_labels", [unreal.Text(t) for t in labels])
            definition.set_editor_property("zone_amount_labels", [unreal.Text(t) for t in MAKEUP_AMOUNT_LABELS[:len(labels)]])
            slot_defs.append(definition)
        definition = unreal.TrainerPartSlotDef()
        definition.set_editor_property("slot", "Lens")
        definition.set_editor_property("label", "Motif de l'iris")
        definition.set_editor_property("tab", "Eyes")
        definition.set_editor_property("follow_slot", "Head")
        definition.set_editor_property("mask_parameter", "IrisMasks")
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
        data.set_editor_property("body_presets", body_presets(bases))
        # Muscles et Corpulence en entier (coefficients delta_muscle / delta_masse d'inZOI, voir GizmoCoef)
        data.set_editor_property("global_slider_share", 1.0)
        data.set_editor_property("parts", parts)
        log("Emplacements teintés par la couleur de peau : " + ", ".join(sorted(skin_slots)))
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
