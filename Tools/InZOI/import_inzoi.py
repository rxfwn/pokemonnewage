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


def src(rel, ext):
    path = os.path.join(EXPORTS, *rel.split("/")) + ext
    # Poils (barbes) : aussi cherchés dans la copie Groom/ à la racine de l'exercice
    if not os.path.isfile(path) and rel.startswith(GROOM_REL + "/"):
        copy = os.path.join(SOURCE, "Groom", *rel[len(GROOM_REL) + 1:].split("/")) + ext
        if os.path.isfile(copy):
            return copy
    return path


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

PART_VERSION = 2  # à augmenter quand le graphe de M_InZOI_Part change : refait au même endroit


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
    EAL.set_metadata_tag(material, "InZOIVersion", str(PART_VERSION))

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


EYE_VERSION = 3  # à augmenter quand le dessin de l'œil change : le matériau est alors refait
# Masque d'iris d'inZOI (celui de MetaHuman) : R = relief, V = fibres de l'iris, B = mélange des 2 couleurs, A = pupille
IRIS_MASKS = ["MetaHumans/Common/Face/Textures/IrisTextures/iris_001/T_Iris_A_M",
              "Characters/Materials/DefaultTextures/Eyes/IrisTextures/iris_001/T_Iris_A_M"]
# Rayon de l'iris dans les UV du globe inZOI (0,125) / rayon de l'iris dans la texture (0,45)
IRIS_UV_SCALE = 3.6


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
    EAL.set_metadata_tag(material, "InZOIVersion", str(EYE_VERSION))
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
    pupil_scale = scalar("PupilScale", 1.1, -1500, 150)
    iris_uv = custom_node("return 0.5 + (UV - 0.5) * %.3f / max(Radius, 0.1);" % IRIS_UV_SCALE, COT.CMOT_FLOAT2,
                          ("UV", "Radius"), -1250, -100)
    MEL.connect_material_expressions(uv, "", iris_uv, "UV")
    MEL.connect_material_expressions(radius, "", iris_uv, "Radius")
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
    balance = scalar("IrisColorBalance", 0.35, -1000, 500)
    smooth = scalar("IrisColorBalanceSmoothness", 0.8, -1000, 600)
    combine = custom_node((
        "float d = distance(UV, float2(0.5, 0.5));\n"
        "float dt = distance(IrisUV, float2(0.5, 0.5));\n"
        "float dp = distance(PupilUV, float2(0.5, 0.5));\n"
        "float irisMask = 1.0 - smoothstep(0.43, 0.47, dt);\n"
        "float t = smoothstep(Balance - Smooth * 0.5, Balance + Smooth * 0.5, M.b);\n"
        "float3 irisColor = lerp(Iris.rgb, Iris.rgb * 0.3, t);\n"
        "irisColor *= lerp(0.45, 1.35, M.g);\n"
        "irisColor *= 1.0 - 0.6 * smoothstep(0.34, 0.46, dt);\n"
        "float pupil = max(1.0 - P, 1.0 - smoothstep(0.11, 0.13, dp));\n"
        "float3 sclera = float3(0.80, 0.78, 0.76) * (1.0 - 0.3 * smoothstep(0.2, 0.45, d));\n"
        "float3 c = lerp(sclera, irisColor, irisMask);\n"
        "return lerp(c, float3(0.004, 0.004, 0.004), pupil * irisMask);"),
        COT.CMOT_FLOAT3, ("UV", "IrisUV", "PupilUV", "M", "P", "Iris", "Balance", "Smooth"), -600, 0)
    MEL.connect_material_expressions(uv, "", combine, "UV")
    MEL.connect_material_expressions(iris_uv, "", combine, "IrisUV")
    MEL.connect_material_expressions(pupil_uv, "", combine, "PupilUV")
    MEL.connect_material_expressions(samples[0], "RGBA", combine, "M")
    MEL.connect_material_expressions(samples[1], "A", combine, "P")
    MEL.connect_material_expressions(iris, "", combine, "Iris")
    MEL.connect_material_expressions(balance, "", combine, "Balance")
    MEL.connect_material_expressions(smooth, "", combine, "Smooth")
    MEL.connect_material_property(combine, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(constant(material, 0.12, -600, 250), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(constant(material, 0.5, -600, 350), "", unreal.MaterialProperty.MP_SPECULAR)
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
        elif any(key in lower for key in ("saliva", "eyeao", "eyetear", "eye_ao", "eye_tear", "lash", "cartilage")):
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
                    if thumbnail:
                        part.set_editor_property("thumbnail", thumbnail)
                    parts.append(part)
                    defaults.setdefault(base_id, {}).setdefault(slot, row["Id"])
            log("%s / %s : %d modèles" % (base_id, slot_label, len(mesh_items)))
    return parts, defaults


# ---------------------------------------------------------------- Barbes (dossier Groom)

HAIR_MASTER = DEST + "/Materials/M_InZOI_Hair"
HAIR_VERSION = 3  # à augmenter quand le graphe change : le matériau est refait au même endroit
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
    EAL.set_metadata_tag(material, "InZOIVersion", str(HAIR_VERSION))
    set_prop(material, "blend_mode", unreal.BlendMode.BLEND_MASKED)
    set_prop(material, "two_sided", True)
    set_prop(material, "opacity_mask_clip_value", 0.3333)
    diro = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -900, -200)
    set_prop(diro, "parameter_name", "DIROMap")
    set_prop(diro, "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    params = []
    for index, (name, color) in enumerate((("RootColor", BEARD_ROOT), ("TipColor", BEARD_TIP), ("Tint", (1.0, 1.0, 1.0)))):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, 50 + index * 150)
        set_prop(node, "parameter_name", name)
        set_prop(node, "default_value", unreal.LinearColor(color[0], color[1], color[2], 1))
        params.append((name, node))
    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -500, -50)
    set_prop(custom, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    set_prop(custom, "code", (
        "float3 natural = lerp(Root.rgb, Tip.rgb, saturate(Grad));\n"
        "bool picked = any(Tint.rgb < 0.999);\n"
        "float3 c = picked ? Tint.rgb * lerp(0.7, 1.0, saturate(Grad)) : natural;\n"
        "return c * lerp(0.65, 1.0, saturate(Depth));"))
    inputs = []
    for input_name in ("Grad", "Root", "Tip", "Tint", "Depth"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        inputs.append(entry)
    set_prop(custom, "inputs", inputs)
    MEL.connect_material_expressions(diro, "B", custom, "Grad")
    MEL.connect_material_expressions(diro, "R", custom, "Depth")
    for (name, node), input_name in zip(params, ("Root", "Tip", "Tint")):
        MEL.connect_material_expressions(node, "", custom, input_name)
    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Bords des mèches en tramage temporel (adouci par l'anti-crénelage) au lieu d'une coupure nette
    dither = MEL.create_material_expression(material, unreal.MaterialExpressionDitherTemporalAA, -500, 450)
    if (MEL.connect_material_expressions(diro, "A", dither, "AlphaThreshold")
            or MEL.connect_material_expressions(diro, "A", dither, "Alpha Threshold")):
        MEL.connect_material_property(dither, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    else:
        MEL.delete_material_expression(material, dither)
        MEL.connect_material_property(diro, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    roughness = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 200)
    set_prop(roughness, "parameter_name", "Roughness")
    set_prop(roughness, "default_value", 0.6)
    MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(constant(material, 0.25, -500, 300), "", unreal.MaterialProperty.MP_SPECULAR)
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


def groom_colors(json_rel):
    """RootColor / TipColor d'un fichier de matériau inZOI (valeurs linéaires), sinon le brun par défaut"""
    colors, _diro = groom_json(json_rel)
    tip = colors.get("TipColor", BEARD_TIP)
    root = colors.get("RootColor", tuple(v * 0.5 for v in tip) if "TipColor" in colors else BEARD_ROOT)
    return root, tip


def diro_texture(diro_rel, fallback):
    """Texture DIRO (données, pas une couleur : sans sRGB). Si celle du JSON manque dans l'export, la carte par défaut :
    sans opacité, les cartes de poils deviennent des plaques pleines (sourcils et cils tout noirs)."""
    for rel in (diro_rel, fallback, DEFAULT_CARD):
        texture = import_texture(rel, "color") if rel else None
        if texture:
            if texture.get_editor_property("srgb"):
                set_prop(texture, "srgb", False)
                EAL.save_loaded_asset(texture)
            return texture
    return None


def hair_instance(asset_path, diro_rel, colors, fallback=None):
    """Matériau de poils ; refait à chaque import (texture et couleurs), pour corriger les anciens"""
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


def groom_materials(mesh, folder, card_fallback, default_colors):
    """Un matériau M_InZOI_Hair par emplacement du mesh, d'après le JSON du même nom dans <dossier>/Materials.
    Les JSON des cartes sont parfois vides : on prend alors la carte par défaut et les couleurs du cuir (Scalp)."""
    infos = []
    for slot_name in material_slot_names(mesh):
        json_rel = folder + "/Materials/" + slot_name
        colors, diro = groom_json(json_rel)
        infos.append((slot_name, json_rel, colors, diro))
    shared = next((c for _n, _j, c, _d in infos if "TipColor" in c), {})
    materials = []
    for slot_name, json_rel, colors, diro in infos:
        colors = colors if "TipColor" in colors else shared
        tip = colors.get("TipColor", default_colors[1])
        root = colors.get("RootColor", tuple(v * 0.6 for v in tip) if "TipColor" in colors else default_colors[0])
        if not diro and "scalp" in slot_name.lower():
            found = scan_folder_textures(folder, "Scalp_DIRO")
            diro = found or card_fallback
        materials.append(hair_instance(dest(json_rel + "_Jeu"), diro or card_fallback, (root, tip), card_fallback))
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
            folder = mesh_rel.rsplit("/Mesh/", 1)[0] if "/Mesh/" in mesh_rel else mesh_rel.rsplit("/", 1)[0]
            materials = []
            for slot_name in material_slot_names(mesh):
                json_rel = folder + "/Materials/" + slot_name
                colors, diro = groom_json(json_rel)
                if not diro:
                    diro = DEFAULT_CARD
                tip = colors.get("TipColor", BEARD_TIP)
                root = colors.get("RootColor", tuple(v * 0.5 for v in tip))
                materials.append(hair_instance(dest(json_rel + "_Jeu"), diro, (root, tip), DEFAULT_CARD))
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
                materials = groom_materials(mesh, folder, fallback_diro, (tuple(v * 0.7 for v in fallback_tip), fallback_tip))
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


def skin_tint(tone_hex):
    """SkinTint (multiplié à la texture de peau) pour une teinte d'inZOI"""
    tone, reference = linear_hex(tone_hex), linear_hex(SKIN_REFERENCE)
    if not tone:
        return None
    return tuple(max(0.05, min(1.0, t / r)) for t, r in zip(tone, reference))


def hair_part_id(mesh_id):
    """W_Hair_M_009_05 -> InZOI_SKM_Female_Hair_M_009 (le _05 final est une variante de couleur)"""
    pieces = (mesh_id or "").split("_")
    if len(pieces) < 4 or pieces[1] != "Hair":
        return None
    gender = {"W": "Female", "M": "Male"}.get(pieces[0])
    if not gender:
        return None
    return "InZOI_SKM_%s_Hair_%s_%s" % (gender, pieces[2], pieces[3])


def full_presets(bases, parts):
    rows = read_csv(os.path.join(TABLES, "AppearanceFacePreset.csv"))
    skin_tones = {r.get("ID"): r.get("SkinTone") for r in read_csv(os.path.join(TABLES, "AppearanceSkinColor.csv"))}
    by_id = {str(p.get_editor_property("id")): p for p in parts}
    groom_ids = {"Eyebrow": ("Eyebrows", ""), "Eyelash": ("Eyelashes", ""), "Beard": ("Beard", "InZOI_"), "Mustache": ("Mustache", "InZOI_")}
    presets = []
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

            def add_color(slot, hex_text):
                rgb = linear_hex(hex_text)
                if rgb:
                    entry = unreal.TrainerPartColor()
                    entry.set_editor_property("slot", slot)
                    entry.set_editor_property("zone", 0)
                    entry.set_editor_property("color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
                    colors.append(entry)

            hair_id = hair_part_id(row.get("Hair.MeshId"))
            if hair_id and fits(hair_id):
                outfit.append(hair_id)
                add_color("Hair", row.get("Hair.VectorParameters.tipColor"))
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
                    add_color(slot, (entry.get("VectorParameters") or {}).get("tipColor"))
            look.set_editor_property("outfit", outfit)
            look.set_editor_property("part_colors", colors)
            tone = skin_tones.get(row.get("FaceSkinColorId")) or row.get("SkinTone")
            tint = skin_tint(tone)
            if tint:
                look.set_editor_property("skin_color", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
            eye = linear_hex(row.get("IrisVectorParameters.eyeColor1"))
            if eye:
                look.set_editor_property("eye_color", unreal.LinearColor(eye[0], eye[1], eye[2], 1.0))
            count += 1
            preset = unreal.TrainerPresetDef()
            preset.set_editor_property("label", "%s %d" % (base_label, count))
            thumbnail = import_texture(game_rel(row.get("Thumbnail")), "ui") or by_id[head_id].get_editor_property("thumbnail")
            if thumbnail:
                preset.set_editor_property("thumbnail", thumbnail)
            preset.set_editor_property("look", look)
            presets.append(preset)
        log("Modèles complets %s : %d (table AppearanceFacePreset)" % (base_id, count))
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
    return total + 10 + hairs


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
            slot_defs.append(definition)
        for slot, slot_label, tab, follow in GROOM_SLOTS:
            definition = unreal.TrainerPartSlotDef()
            definition.set_editor_property("slot", slot)
            definition.set_editor_property("label", slot_label)
            definition.set_editor_property("tab", tab)
            if follow:
                definition.set_editor_property("follow_slot", follow)
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
