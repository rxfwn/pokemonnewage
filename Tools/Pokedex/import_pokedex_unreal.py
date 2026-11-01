"""Importe le Pokédex 3D du menu dans Unreal (script Python d'éditeur, SCRUM-64).

À lancer dans l'éditeur, APRÈS avoir compilé le C++ (la classe PokedexModel doit exister) :
    Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> choisir Tools/Pokedex/import_pokedex_unreal.py

Le modèle de L'amie, déjà découpé, est dans le dépôt : Tools/Pokedex/Generated/Pokedex.fbx et Generated/textures/
(refait par prepare_pokedex_model.py depuis Pokedex.blend). Rien à dézipper.
Normalement inutile de le lancer à la main : Content/Python/init_unreal.py le lance tout seul à l'ouverture
de l'éditeur quand DA_PokedexModel manque ou n'a pas le modèle 3D.

Ce qu'il fait, tout seul :
  1. cherche Tools/Pokedex/Generated/Pokedex.fbx ; s'il n'y est pas, lance Blender (s'il est installé)
     avec prepare_pokedex_model.py sur Tools/Pokedex/Source/Pokedex.blend ;
  2. importe les textures du modèle (Generated/textures) dans /Game/UI/Pokedex/Textures ;
  3. crée les matériaux : M_PokedexTextured (+ une instance MI_Pokedex_Udim000N par matériau du modèle)
     et M_Pokedex (uni, pour les 10 boutons du volet sous les icônes) ;
  4. importe les pièces dans /Game/UI/Pokedex/Meshes (une Static Mesh par pièce) et leur pose les MI_Pokedex_* ;
  5. importe les 10 icônes et la carte de Kanto (Tools/Pokedex/Assets) dans /Game/UI/Pokedex/Icons et /Map ;
  6. fabrique les sons (make_pokedex_sounds.py), les importe dans /Game/UI/Pokedex/Sounds, et crée les réglages
     de volume /Game/Audio/SC_Music, SC_Effects et SM_Options (options Musique et Bruitages) ;
  7. crée ou met à jour DA_PokedexModel, que le Pokédex 3D charge à l'ouverture du menu ;
  8. lance Tools/HUD/import_hud_unreal.py (images et polices du HUD en jeu).
Relançable : les assets existants sont remplacés.
"""

import glob
import importlib.util
import json
import os
import subprocess

import unreal

CONTENT_DIR = "/Game/UI/Pokedex"
MESH_DIR = CONTENT_DIR + "/Meshes"
SOUND_DIR = CONTENT_DIR + "/Sounds"
TEXTURE_DIR = CONTENT_DIR + "/Textures"
ICON_DIR = CONTENT_DIR + "/Icons"
MAP_DIR = CONTENT_DIR + "/Map"
AUDIO_DIR = "/Game/Audio"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
TOOLS_DIR = os.path.join(PROJECT_DIR, "Tools", "Pokedex")
GENERATED_DIR = os.path.join(TOOLS_DIR, "Generated")
FBX_PATH = os.path.join(GENERATED_DIR, "Pokedex.fbx")
TEXTURE_FILES_DIR = os.path.join(GENERATED_DIR, "textures")
BLEND_PATH = os.path.join(TOOLS_DIR, "Source", "Pokedex.blend")
ASSETS_DIR = os.path.join(TOOLS_DIR, "Assets")

# Pièces attendues par APokedexDevice (découpe de prepare_pokedex_model.py)
PARTS = ["Body", "Hinge", "Lens", "LensRing", "LedRed", "LedYellow", "LedGreen", "ScreenLeds", "Screen", "MiniScreen0",
         "BackButton", "ConfirmButton", "HomeButton", "DexButton", "DPad", "Lid", "LidScreen", "MiniScreen1",
         "MiniScreen2", "LidLeds", "PrevButton", "NextButton", "LidButton"] + ["MenuButton%d" % i for i in range(10)]

# Matériaux du modèle de L'amie (Udim0003 a aussi une carte émissive : voyants, objectif)
UDIMS = ["Udim0001", "Udim0002", "Udim0003", "Udim0004", "Udim0005"]

# Icônes des 10 boutons, dans l'ordre de EPokedexMenu (C++). Camera et Steps : provisoires (Tools/HUD/make_hud_assets.py)
ICONS = ["Pokedex", "Map", "Bag", "Pokemon", "Trainer", "Radio", "Online", "Camera", "Steps", "Save"]

# Propriété de DA_PokedexModel -> son
SOUNDS = {
    "click_sound": "SW_Pokedex_Click",
    "beep_sound": "SW_Pokedex_Beep",
    "nav_sound": "SW_Pokedex_Nav",
    "open_sound": "SW_Pokedex_Open",
    "close_sound": "SW_Pokedex_Close",
    "error_sound": "SW_Pokedex_Error",
    "save_sound": "SW_Pokedex_Save",
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def find_blender():
    """Blender installé à l'emplacement habituel sous Windows (la version la plus récente)."""
    candidates = sorted(glob.glob(r"C:\Program Files\Blender Foundation\Blender*\blender.exe"), reverse=True)
    return candidates[0] if candidates else None


def make_fbx():
    if os.path.exists(FBX_PATH):
        return True
    blender = find_blender()
    if not blender or not os.path.exists(BLEND_PATH):
        unreal.log_warning(
            "Pokedex.fbx introuvable : seul le menu (icônes, carte, sons) est importé, sans le Pokédex 3D. "
            "Pour le 3D : faire git pull (Tools/Pokedex/Generated est dans le dépôt), puis relancer.")
        return False
    unreal.log("Préparation du modèle avec Blender : " + blender)
    subprocess.run([blender, "--background", "--python", os.path.join(TOOLS_DIR, "prepare_pokedex_model.py"), "--",
                    BLEND_PATH, GENERATED_DIR], check=False)
    return os.path.exists(FBX_PATH)


def import_files(files, destination):
    tasks = []
    for path in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        if path.lower().endswith(".fbx"):
            options = unreal.FbxImportUI()
            options.set_editor_property("import_mesh", True)
            options.set_editor_property("import_as_skeletal", False)
            # Les matériaux sont faits par ce script (MI_Pokedex_*), l'import FBX garde seulement les noms d'emplacements
            options.set_editor_property("import_materials", False)
            options.set_editor_property("import_textures", False)
            options.set_editor_property("import_animations", False)
            mesh_data = options.get_editor_property("static_mesh_import_data")
            mesh_data.set_editor_property("combine_meshes", False)
            mesh_data.set_editor_property("auto_generate_collision", True)
            mesh_data.set_editor_property("transform_vertex_to_absolute", True)
            mesh_data.set_editor_property("convert_scene", True)
            task.set_editor_property("options", options)
        tasks.append(task)
    asset_tools.import_asset_tasks(tasks)


def part_key(asset_name):
    """SM_Pokedex_Body, Pokedex_Body... -> Body"""
    if "Pokedex_" not in asset_name:
        return None
    key = asset_name.split("Pokedex_")[-1]
    return key if key in PARTS else None


def collect_meshes():
    meshes = {}
    for path in eal.list_assets(MESH_DIR, recursive=True, include_folder=False):
        asset = eal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            key = part_key(asset.get_name())
            if key:
                meshes[key] = asset
    return meshes


mel = unreal.MaterialEditingLibrary


def _vector(material, name, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(*value))
    return node


def _scalar(material, name, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def _texture(material, name, sampler, default, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    # Texture par défaut obligatoire (sinon le matériau ne compile pas), du même type que celles des instances
    if default:
        node.set_editor_property("texture", default)
        node.set_editor_property("sampler_type", sampler)
    else:
        node.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    return node


def _new_material(name, version=None):
    """Nouveau matériau, ou None s'il existe déjà. Avec version : un matériau d'une autre version est vidé et refait
    (sans le supprimer, pour garder ses instances MI_Pokedex_*)."""
    path = CONTENT_DIR + "/" + name
    if eal.does_asset_exist(path):
        if version is None:
            return path, None
        material = eal.load_asset(path)
        if eal.get_metadata_tag(material, "PokedexVersion") == version:
            return path, None
        mel.delete_all_material_expressions(material)
        return path, material
    return path, asset_tools.create_asset(name, CONTENT_DIR, unreal.Material, unreal.MaterialFactoryNew())


def make_material():
    """M_Pokedex : matériau uni à paramètres (boutons du volet, pièces sans texture)."""
    path, material = _new_material("M_Pokedex")
    if material is None:
        return eal.load_asset(path)

    base = _vector(material, "BaseColor", (0.5, 0.035, 0.025, 1.0), -500, -300)
    metallic = _scalar(material, "Metallic", 0.0, -500, -100)
    roughness = _scalar(material, "Roughness", 0.45, -500, 0)
    emissive = _vector(material, "EmissiveColor", (0.0, 0.0, 0.0, 1.0), -700, 200)
    strength = _scalar(material, "EmissiveStrength", 0.0, -700, 400)
    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 250)

    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_expressions(emissive, "", multiply, "A")
    mel.connect_material_expressions(strength, "", multiply, "B")
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    eal.save_asset(path)
    return material


def make_widget_material():
    """M_PokedexWidget : écrans et icônes du Pokédex (composants Widget). Comme le matériau d'Unreal
    (Widget3DPassThrough), mais avec un paramètre Brightness que le C++ règle sur l'exposition du menu :
    sans lui, les écrans paraissent noirs en plein jour."""
    path = CONTENT_DIR + "/M_PokedexWidget"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    material = asset_tools.create_asset("M_PokedexWidget", CONTENT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    # Net malgré l'anticrénelage temporel et le flou du décor (version 4)
    material.set_editor_property("enable_responsive_aa", True)
    try:
        material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    except Exception as error:
        unreal.log_warning("M_PokedexWidget : passe après le flou non réglée ({})".format(error))

    # L'écran est rendu en grand (Panel Render Scale) puis réduit à l'écran : sans lissage, le texte « grésille ».
    # 4 lectures normales de la texture (mêmes couleurs que le matériau d'Unreal) à ±1/4 de pixel affiché, moyennées.
    # (Version 2 : nœud Custom à 16 points, couleurs délavées et traits trop effacés.)
    slate_tex = mel.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -1700, 0)
    slate_tex.set_editor_property("parameter_name", "SlateUI")
    slate_tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1700, 250)
    ddx = mel.create_material_expression(material, unreal.MaterialExpressionDDX, -1550, 350)
    ddy = mel.create_material_expression(material, unreal.MaterialExpressionDDY, -1550, 450)
    mel.connect_material_expressions(uv, "", ddx, "")
    mel.connect_material_expressions(uv, "", ddy, "")
    samples = []
    for index, (a, b) in enumerate(((-0.25, -0.25), (0.25, -0.25), (-0.25, 0.25), (0.25, 0.25))):
        y = -300 + index * 220
        off_x = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -1400, y)
        off_x.set_editor_property("const_b", a)
        mel.connect_material_expressions(ddx, "", off_x, "A")
        off_y = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -1400, y + 80)
        off_y.set_editor_property("const_b", b)
        mel.connect_material_expressions(ddy, "", off_y, "A")
        offset = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -1300, y)
        mel.connect_material_expressions(off_x, "", offset, "A")
        mel.connect_material_expressions(off_y, "", offset, "B")
        coords = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -1200, y)
        mel.connect_material_expressions(uv, "", coords, "A")
        mel.connect_material_expressions(offset, "", coords, "B")
        sample = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -1050, y)
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        mel.connect_material_expressions(slate_tex, "", sample, "Tex")
        mel.connect_material_expressions(coords, "", sample, "UVs")
        samples.append(sample)
    sum_a = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -900, -200)
    mel.connect_material_expressions(samples[0], "RGBA", sum_a, "A")
    mel.connect_material_expressions(samples[1], "RGBA", sum_a, "B")
    sum_b = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -900, 200)
    mel.connect_material_expressions(samples[2], "RGBA", sum_b, "A")
    mel.connect_material_expressions(samples[3], "RGBA", sum_b, "B")
    total = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -820, 0)
    mel.connect_material_expressions(sum_a, "", total, "A")
    mel.connect_material_expressions(sum_b, "", total, "B")
    slate = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -760, 0)
    slate.set_editor_property("const_b", 0.25)
    mel.connect_material_expressions(total, "", slate, "A")
    slate_rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -700, 0)
    for channel in ("r", "g", "b"):
        slate_rgb.set_editor_property(channel, True)
    slate_a = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -700, 150)
    slate_a.set_editor_property("a", True)
    mel.connect_material_expressions(slate, "", slate_rgb, "")
    mel.connect_material_expressions(slate, "", slate_a, "")
    tint = _vector(material, "TintColorAndOpacity", (1.0, 1.0, 1.0, 1.0), -900, 300)
    brightness = _scalar(material, "Brightness", 1.0, -900, 500)
    _scalar(material, "OpacityFromTexture", 1.0, -900, 650)  # attendu par UWidgetComponent
    color = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 100)
    tint_rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -700, 300)
    tint_rgb.set_editor_property("r", True)
    tint_rgb.set_editor_property("g", True)
    tint_rgb.set_editor_property("b", True)
    lit = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 150)
    opacity = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 400)

    mel.connect_material_expressions(tint, "", tint_rgb, "")
    mel.connect_material_expressions(slate_rgb, "", color, "A")
    mel.connect_material_expressions(tint_rgb, "", color, "B")
    mel.connect_material_expressions(color, "", lit, "A")
    mel.connect_material_expressions(brightness, "", lit, "B")
    mel.connect_material_expressions(slate_a, "", opacity, "A")
    # Sortie « A » du paramètre directement (sa sortie par défaut n'a que RGB)
    mel.connect_material_expressions(tint, "A", opacity, "B")
    mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    # Version lue par Content/Python/init_unreal.py : une version plus ancienne relance l'import à l'ouverture d'Unreal
    eal.set_metadata_tag(material, "PokedexWidgetVersion", "4")
    eal.save_asset(path)
    return material


def make_textured_material(textures):
    """M_PokedexTextured : textures du modèle (couleur, ORM, normales, émissif) + lumière ajoutée par le C++ (voyants)."""
    # Version 2 : Desaturation et RoughnessBoost (plastique moins rouge vif et moins brillant, réglés par le C++)
    path, material = _new_material("M_PokedexTextured", "2")
    if material is None:
        return eal.load_asset(path)

    def default(kind):
        for udim in UDIMS:
            if kind in textures.get(udim, {}):
                return textures[udim][kind]
        return None

    color_map = _texture(material, "BaseColorMap", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, default("BaseColor"), -900, -400)
    tint = _vector(material, "Tint", (1.0, 1.0, 1.0, 1.0), -900, -150)
    desaturation = _scalar(material, "Desaturation", 0.0, -900, -250)
    desaturated = mel.create_material_expression(material, unreal.MaterialExpressionDesaturation, -700, -350)
    mel.connect_material_expressions(color_map, "RGB", desaturated, "")
    mel.connect_material_expressions(desaturation, "", desaturated, "Fraction")
    tinted = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, -300)
    mel.connect_material_expressions(desaturated, "", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # ORM (glTF) : R occlusion, G rugosité, B métal ; RoughnessBoost rapproche la rugosité de 1 (plus mat)
    orm = _texture(material, "ORMMap", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, default("ORM"), -900, 50)
    roughness_boost = _scalar(material, "RoughnessBoost", 0.0, -900, 200)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -600, 100)
    roughness.set_editor_property("const_b", 1.0)
    mel.connect_material_expressions(orm, "G", roughness, "A")
    mel.connect_material_expressions(roughness_boost, "", roughness, "Alpha")
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)

    normal = _texture(material, "NormalMap", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, default("Normal"), -900, 350)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    # Émissif = carte émissive × force + couleur × force (les voyants s'allument depuis le C++)
    emissive_map = _texture(material, "EmissiveMap", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, default("Emissive"), -900, 650)
    map_strength = _scalar(material, "EmissiveMapStrength", 1.0, -900, 900)
    map_emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 700)
    mel.connect_material_expressions(emissive_map, "RGB", map_emissive, "A")
    mel.connect_material_expressions(map_strength, "", map_emissive, "B")
    color = _vector(material, "EmissiveColor", (0.0, 0.0, 0.0, 1.0), -900, 1050)
    strength = _scalar(material, "EmissiveStrength", 0.0, -900, 1250)
    color_emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 1100)
    mel.connect_material_expressions(color, "", color_emissive, "A")
    mel.connect_material_expressions(strength, "", color_emissive, "B")
    total = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -350, 850)
    mel.connect_material_expressions(map_emissive, "", total, "A")
    mel.connect_material_expressions(color_emissive, "", total, "B")
    mel.connect_material_property(total, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    eal.set_metadata_tag(material, "PokedexVersion", "2")
    eal.save_asset(path)
    return material


def import_textures():
    """Textures du modèle -> {"Udim0001": {"BaseColor": Texture2D, ...}}"""
    files = sorted(glob.glob(os.path.join(TEXTURE_FILES_DIR, "T_Pokedex_*.png")))
    if not files:
        unreal.log_warning("Pas de textures dans Tools/Pokedex/Generated/textures : le Pokédex sera uni.")
        return {}
    import_files(files, TEXTURE_DIR)
    textures = {}
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]      # T_Pokedex_Udim0001_BaseColor
        udim, kind = name.split("_")[2], name.split("_")[3]
        texture = eal.load_asset(TEXTURE_DIR + "/" + name)
        if not texture:
            continue
        if kind == "Normal":
            # Cartes de normales OpenGL (Blender) : on retourne le vert pour Unreal
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
            texture.set_editor_property("flip_green_channel", True)
        elif kind == "ORM":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            texture.set_editor_property("srgb", False)
        eal.save_asset(TEXTURE_DIR + "/" + name)
        textures.setdefault(udim, {})[kind] = texture
    return textures


def make_material_instances(parent, textures):
    """MI_Pokedex_Udim000N : une instance de M_PokedexTextured par matériau du modèle."""
    instances = {}
    for udim in UDIMS:
        name = "MI_Pokedex_" + udim
        path = CONTENT_DIR + "/" + name
        if eal.does_asset_exist(path):
            instance = eal.load_asset(path)
        else:
            instance = asset_tools.create_asset(name, CONTENT_DIR, unreal.MaterialInstanceConstant,
                                                unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(instance, parent)
        maps = textures.get(udim, {})
        for kind, parameter in (("BaseColor", "BaseColorMap"), ("ORM", "ORMMap"), ("Normal", "NormalMap"), ("Emissive", "EmissiveMap")):
            if kind in maps:
                mel.set_material_instance_texture_parameter_value(instance, parameter, maps[kind])
        mel.set_material_instance_scalar_parameter_value(instance, "EmissiveMapStrength", 1.0 if "Emissive" in maps else 0.0)
        eal.save_asset(path)
        instances[udim] = instance
    return instances


def assign_materials(meshes, instances):
    """Pose MI_Pokedex_Udim000N sur chaque emplacement « MAT_Udim000N » des pièces."""
    for mesh in meshes.values():
        static_materials = mesh.get_editor_property("static_materials")
        changed = False
        for index, entry in enumerate(static_materials):
            slot = str(entry.get_editor_property("material_slot_name"))
            for udim, instance in instances.items():
                if udim in slot:
                    mesh.set_material(index, instance)
                    changed = True
        if changed:
            eal.save_loaded_asset(mesh)


def import_ui_textures():
    """Icônes des boutons et carte de Kanto : textures d'interface (pas de mipmaps, pas de compression)."""
    icon_files = [os.path.join(ASSETS_DIR, "icons", "T_PokedexIcon_%s.png" % name) for name in ICONS]
    map_file = os.path.join(ASSETS_DIR, "map", "T_PokedexMap.png")
    # + icône blanche du bouton rond noir (appareil photo, image de L'amie)
    icon_files.append(os.path.join(ASSETS_DIR, "icons", "T_PokedexButton_Camera.png"))
    import_files([f for f in icon_files if os.path.exists(f)], ICON_DIR)
    import_files([map_file], MAP_DIR)

    def as_ui(path):
        texture = eal.load_asset(path) if eal.does_asset_exist(path) else None
        if texture:
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            eal.save_asset(path)
        return texture

    icons = [as_ui(ICON_DIR + "/T_PokedexIcon_" + name) for name in ICONS]
    as_ui(ICON_DIR + "/T_PokedexButton_Camera")
    map_texture = as_ui(MAP_DIR + "/T_PokedexMap")
    with open(os.path.join(ASSETS_DIR, "map", "pokedex_map.json"), encoding="utf-8") as handle:
        map_data = json.load(handle)
    return icons, map_texture, map_data


def make_audio_mix(sounds):
    """SC_Music, SC_Effects (enfants de la classe Master) et SM_Options : lus par les options Musique et Bruitages."""
    def load_or_create(name, asset_class, factory):
        path = AUDIO_DIR + "/" + name
        if eal.does_asset_exist(path):
            return eal.load_asset(path)
        return asset_tools.create_asset(name, AUDIO_DIR, asset_class, factory)

    music = load_or_create("SC_Music", unreal.SoundClass, unreal.SoundClassFactory())
    effects = load_or_create("SC_Effects", unreal.SoundClass, unreal.SoundClassFactory())
    mix = load_or_create("SM_Options", unreal.SoundMix, unreal.SoundMixFactory())
    for asset in (music, effects, mix):
        eal.save_loaded_asset(asset)
    # Les sons du Pokédex sont des bruitages
    for sound in sounds.values():
        if isinstance(sound, unreal.SoundWave):
            sound.set_editor_property("sound_class_object", effects)
            eal.save_loaded_asset(sound)
    return music is not None and effects is not None and mix is not None


def import_sounds():
    sound_dir = os.path.join(GENERATED_DIR, "sons")
    spec = importlib.util.spec_from_file_location("make_pokedex_sounds", os.path.join(TOOLS_DIR, "make_pokedex_sounds.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    os.makedirs(sound_dir, exist_ok=True)
    files = []
    for name, make in module.SOUNDS.items():
        path = os.path.join(sound_dir, name + ".wav")
        module.write_wav(path, make())
        files.append(path)
    import_files(files, SOUND_DIR)
    sounds = {}
    for prop, name in SOUNDS.items():
        asset_path = SOUND_DIR + "/" + name
        if eal.does_asset_exist(asset_path):
            sounds[prop] = eal.load_asset(asset_path)
    return sounds


def make_data_asset(meshes, material, sounds, icons, map_texture, map_data):
    model_class = unreal.load_class(None, "/Script/PokemonNewAge.PokedexModel")
    if model_class is None:
        unreal.log_error("Classe PokedexModel introuvable : compiler le C++ d'abord.")
        return None
    path = CONTENT_DIR + "/DA_PokedexModel"
    if eal.does_asset_exist(path):
        model = eal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", model_class)
        model = asset_tools.create_asset("DA_PokedexModel", CONTENT_DIR, model_class, factory)

    # Sans modèle 3D, on garde les pièces d'un import précédent
    if meshes:
        parts = unreal.Map(unreal.Name, unreal.StaticMesh)
        for key, mesh in meshes.items():
            parts[unreal.Name(key)] = mesh
        model.set_editor_property("parts", parts)
        model.set_editor_property("material", material)
    model.set_editor_property("menu_icons", icons)
    model.set_editor_property("map_texture", map_texture)
    kinds = {"town": unreal.PokedexPlaceKind.TOWN, "route": unreal.PokedexPlaceKind.ROUTE, "place": unreal.PokedexPlaceKind.PLACE}
    places = []
    for entry in map_data.get("places", []):
        place = unreal.PokedexMapPlace()
        place.set_editor_property("name", entry["name"])
        place.set_editor_property("kind", kinds.get(entry["kind"], unreal.PokedexPlaceKind.PLACE))
        place.set_editor_property("uv", unreal.Vector2D(entry["u"], entry["v"]))
        place.set_editor_property("radius", entry["radius"])
        places.append(place)
    model.set_editor_property("map_places", places)
    model.set_editor_property("map_heightmap_size", int(map_data.get("heightmap_size", 4033)))
    for prop, sound in sounds.items():
        model.set_editor_property(prop, sound)
    eal.save_asset(path)
    return model


def main():
    os.makedirs(GENERATED_DIR, exist_ok=True)
    # Sans le modèle 3D, on importe quand même ce que le menu PC classique utilise (icônes, carte, sons)
    with_model = make_fbx()

    with unreal.ScopedSlowTask(6, "Import du menu") as task:
        task.make_dialog(True)
        textures, material, meshes = {}, None, {}
        if with_model:
            task.enter_progress_frame(1, "Textures")
            textures = import_textures()
            task.enter_progress_frame(1, "Matériaux")
            material = make_material()
            instances = make_material_instances(make_textured_material(textures), textures)
            make_widget_material()
            task.enter_progress_frame(1, "Pièces 3D")
            import_files([FBX_PATH], MESH_DIR)
            meshes = collect_meshes()
            assign_materials(meshes, instances)
        task.enter_progress_frame(1, "Icônes et carte")
        icons, map_texture, map_data = import_ui_textures()
        task.enter_progress_frame(1, "Sons")
        sounds = import_sounds()
        audio_ok = make_audio_mix(sounds)
        task.enter_progress_frame(1, "DA_PokedexModel")
        model = make_data_asset(meshes, material, sounds, icons, map_texture, map_data)

    missing = [key for key in PARTS if key not in meshes]
    if with_model and missing:
        unreal.log_warning("Pièces manquantes : " + ", ".join(missing))
    if not audio_ok:
        unreal.log_warning("Réglages de volume (Content/Audio) pas créés : options Musique et Bruitages sans effet.")
    # Le HUD en jeu (icône du Pokédex, boussole, polices) va avec
    hud_script = os.path.join(PROJECT_DIR, "Tools", "HUD", "import_hud_unreal.py")
    if os.path.exists(hud_script):
        spec = importlib.util.spec_from_file_location("import_hud_unreal", hud_script)
        hud = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(hud)
        hud.import_hud()

    if model:
        unreal.log("Menu prêt : %d pièces 3D sur %d, %d textures, %d icônes, %d lieux sur la carte, %d sons. "
                   "Lancer le jeu et appuyer sur Tab." % (len(meshes), len(PARTS), sum(len(v) for v in textures.values()),
                                                           len([i for i in icons if i]), len(map_data.get("places", [])),
                                                           len(sounds)))

# Lancé seul depuis l'éditeur (pas quand Content/Python/init_unreal.py le charge sous le nom import_pokedex_unreal)
if __name__ != "import_pokedex_unreal":
    main()
