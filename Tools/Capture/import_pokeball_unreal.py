"""Importe les Poké Balls du lancer en 3D (SCRUM-40, SCRUM-75) dans Unreal.

Outils > Exécuter un script Python... > choisir ce fichier (n'importe quelle map ouverte).

Modèles de L'amie (SCRUM-75), une fois par Ball de Tools/Capture/Balls/<Id>/ :
  - Content/Capture/Balls/<Id>/SM_<Id>_Haut et SM_<Id>_Bas : les deux moitiés (le couvercle s'ouvre) ;
  - T_<Id>_D, T_<Id>_N, T_<Id>_R, T_<Id>_M : couleur, normale (OpenGL, vert inversé à l'import), rugosité, métal,
    lues dans Tools/Capture/Generated/Sources (copier là les dossiers Pokeball_Part1 et Pokeball_Part2 livrés,
    textures 2K, sinon 1K) ;
  - MI_<Id> : instance de Content/Capture/Balls/M_Ball (matériau commun, lueur « Eclat » pour les éclairs).
  Sans les textures d'une Ball, ses moitiés sont importées quand même (gris).

Ancien modèle provisoire, gardé en secours (utilisé seulement si Content/Capture/Balls est vide) :
  - SM_PokeBall (modèle de Tools/Capture/Generated/SM_PokeBall.fbx, fait par make_pokeball.py) ;
  - M_PokeBall : plastique brillant, couleurs lues dans les couleurs de sommets, moitié haute réglable
    par le paramètre CouleurHaut (APokeBallThrow le change selon la Ball : rouge, bleue, jaune, violette) ;
  - Sons/SW_Ball_* : sons provisoires du lancer (fabriqués ici même s'ils manquent).
Relançable : remplace le modèle et les sons, garde M_PokeBall s'il existe déjà.

Sans le FBX, seuls les sons et le matériau sont créés ; le jeu lance alors une sphère rouge.
"""

import importlib.util
import os
import re

import unreal

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
GENERATED_DIR = os.path.join(TOOLS_DIR, "Generated")
FBX_PATH = os.path.join(GENERATED_DIR, "SM_PokeBall.fbx")
CONTENT_DIR = "/Game/Capture"
SOUND_DIR = "/Game/Capture/Sons"
EFFECTS_CLASS = "/Game/Audio/SC_Effects"
BALLS_DIR = os.path.join(TOOLS_DIR, "Balls")
SOURCES_DIR = os.path.join(GENERATED_DIR, "Sources")
BALLS_CONTENT = "/Game/Capture/Balls"
MASTER_PATH = BALLS_CONTENT + "/M_Ball"
# Textures livrées (Substance Painter) : suffixe Unreal, nom dans le fichier, réglages
TEXTURE_KINDS = [
    ("D", "Base_color"),
    ("N", "Normal_OpenGL"),
    ("R", "Roughness"),
    ("M", "Metallic"),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def import_files(files, destination, names=None):
    tasks = []
    for index, path in enumerate(files):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", destination)
        if names:
            task.set_editor_property("destination_name", names[index])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        if path.lower().endswith(".fbx"):
            options = unreal.FbxImportUI()
            options.set_editor_property("import_mesh", True)
            options.set_editor_property("import_as_skeletal", False)
            options.set_editor_property("import_materials", False)
            options.set_editor_property("import_textures", False)
            options.set_editor_property("import_animations", False)
            mesh_data = options.get_editor_property("static_mesh_import_data")
            mesh_data.set_editor_property("combine_meshes", True)
            mesh_data.set_editor_property("auto_generate_collision", False)
            # Ancien modèle : couleurs dans les couleurs de sommets ; Balls de L'amie : textures
            vertex = unreal.VertexColorImportOption.REPLACE if names is None else unreal.VertexColorImportOption.IGNORE
            mesh_data.set_editor_property("vertex_color_import_option", vertex)
            task.set_editor_property("options", options)
        tasks.append(task)
    asset_tools.import_asset_tasks(tasks)


def make_material():
    """M_PokeBall : R des sommets = moitié haute (CouleurHaut), G = blanc, le reste noir."""
    path = CONTENT_DIR + "/M_PokeBall"
    if eal.does_asset_exist(path):
        # Déjà fait (à effacer à la main pour le refaire)
        return eal.load_asset(path)
    material = asset_tools.create_asset("M_PokeBall", CONTENT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    vertex = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -900, 0)
    top = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, -250)
    top.set_editor_property("parameter_name", "CouleurHaut")
    top.set_editor_property("default_value", unreal.LinearColor(0.80, 0.05, 0.04, 1.0))
    white = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -900, 200)
    white.set_editor_property("constant", unreal.LinearColor(0.85, 0.85, 0.83, 1.0))
    black = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -900, 350)
    black.set_editor_property("constant", unreal.LinearColor(0.02, 0.02, 0.02, 1.0))

    bottom = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -550, 250)
    mel.connect_material_expressions(black, "", bottom, "A")
    mel.connect_material_expressions(white, "", bottom, "B")
    mel.connect_material_expressions(vertex, "G", bottom, "Alpha")

    color = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -300, 0)
    mel.connect_material_expressions(bottom, "", color, "A")
    mel.connect_material_expressions(top, "", color, "B")
    mel.connect_material_expressions(vertex, "R", color, "Alpha")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -300, 300)
    roughness.set_editor_property("parameter_name", "Rugosite")
    roughness.set_editor_property("default_value", 0.25)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(material)
    eal.save_asset(path)
    return material


def import_sounds():
    spec = importlib.util.spec_from_file_location("make_ball_sounds", os.path.join(TOOLS_DIR, "make_ball_sounds.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    files = [os.path.join(GENERATED_DIR, name + ".wav") for name in module.SOUNDS]
    if not all(os.path.exists(path) for path in files):
        files = module.main(GENERATED_DIR)
    import_files(files, SOUND_DIR)

    # Bruitages : suivent le réglage « Bruitages » des Options si le menu Pokédex est installé
    effects = eal.load_asset(EFFECTS_CLASS) if eal.does_asset_exist(EFFECTS_CLASS) else None
    count = 0
    for name in module.SOUNDS:
        path = SOUND_DIR + "/" + name
        if not eal.does_asset_exist(path):
            continue
        count += 1
        if effects:
            sound = eal.load_asset(path)
            sound.set_editor_property("sound_class_object", effects)
            eal.save_loaded_asset(sound)
    return count


# ---------------------------------------------------------------- Balls de L'amie (SCRUM-75)

def ball_id(folder_name):
    """Pokeball -> PokeBall (même règle que prepare_pokeballs.py)."""
    base = folder_name[:-4] if folder_name.lower().endswith("ball") else folder_name
    return base[:1].upper() + base[1:].lower() + "Ball"


def find_texture_folders():
    """{Id: dossier des textures} : <Nom>_textures/2K (sinon 1K) n'importe où sous Generated/Sources."""
    found = {}
    if not os.path.isdir(SOURCES_DIR):
        return found
    for current, dirs, _files in os.walk(SOURCES_DIR):
        match = re.match(r"(?i)^(.+)_textures$", os.path.basename(current))
        if not match:
            continue
        for size in ("2K", "1K"):
            if size in dirs:
                found[ball_id(match.group(1))] = (os.path.join(current, size), match.group(1))
                break
    return found


def texture_file(folder, stem, kind):
    """<Nom>_<kind>.png ; tolère <Nom>_Base_<kind>.png (Ultraball)."""
    for name in (f"{stem}_{kind}.png", f"{stem}_Base_{kind}.png"):
        path = os.path.join(folder, name)
        if os.path.exists(path):
            return path
    # 2K incomplet : on prend la 1K à côté
    other = os.path.join(os.path.dirname(folder), "1K")
    if os.path.basename(folder) != "1K" and os.path.isdir(other):
        return texture_file(other, stem, kind)
    return None


def setup_texture(path, suffix):
    texture = eal.load_asset(path)
    if not texture:
        return None
    if suffix == "N":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
        # Export « OpenGL » de Substance Painter : Unreal attend le vert inversé (DirectX)
        texture.set_editor_property("flip_green_channel", True)
    elif suffix in ("R", "M"):
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
    eal.save_loaded_asset(texture)
    return texture


def import_ball_textures(identifier, sources):
    """Importe les 4 textures d'une Ball. Renvoie {suffixe: texture}."""
    if identifier not in sources:
        return {}
    folder, stem = sources[identifier]
    destination = f"{BALLS_CONTENT}/{identifier}"
    files, names, suffixes = [], [], []
    for suffix, kind in TEXTURE_KINDS:
        path = texture_file(folder, stem, kind)
        if path:
            files.append(path)
            names.append(f"T_{identifier}_{suffix}")
            suffixes.append(suffix)
    if files:
        import_files(files, destination, names)
    textures = {}
    for suffix, name in zip(suffixes, names):
        texture = setup_texture(f"{destination}/{name}", suffix)
        if texture:
            textures[suffix] = texture
    return textures


def make_ball_master(defaults):
    """M_Ball : couleur, normale, rugosité, métal en paramètres de texture + lueur émissive (Eclat, CouleurEclat).
    defaults : textures d'une Ball (valeurs par défaut des paramètres, il en faut pour compiler)."""
    if eal.does_asset_exist(MASTER_PATH):
        # Déjà fait (à effacer à la main pour le refaire)
        return eal.load_asset(MASTER_PATH)
    if len(defaults) < 4:
        return None
    material = asset_tools.create_asset("M_Ball", BALLS_CONTENT, unreal.Material, unreal.MaterialFactoryNew())

    def sampler(name, suffix, sampler_type, y):
        node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("texture", defaults[suffix])
        node.set_editor_property("sampler_type", sampler_type)
        return node

    color = sampler("Couleur", "D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -300)
    normal = sampler("Normale", "N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 0)
    roughness = sampler("Rugosite", "R", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 300)
    metallic = sampler("Metal", "M", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 600)
    mel.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(roughness, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metallic, "R", unreal.MaterialProperty.MP_METALLIC)

    # Lueur des éclairs (APokeBallThrow, séquence d'évolution) : CouleurEclat × Eclat, éteinte par défaut
    glow = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, 900)
    glow.set_editor_property("parameter_name", "Eclat")
    glow.set_editor_property("default_value", 0.0)
    glow_color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 1050)
    glow_color.set_editor_property("parameter_name", "CouleurEclat")
    glow_color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.2, 0.15, 1.0))
    emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 950)
    mel.connect_material_expressions(glow_color, "", emissive, "A")
    mel.connect_material_expressions(glow, "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    eal.save_asset(MASTER_PATH)
    return material


def make_ball_instance(identifier, master, textures):
    path = f"{BALLS_CONTENT}/{identifier}/MI_{identifier}"
    if eal.does_asset_exist(path):
        instance = eal.load_asset(path)
    else:
        instance = asset_tools.create_asset(f"MI_{identifier}", f"{BALLS_CONTENT}/{identifier}",
                                            unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, master)
    for suffix, parameter in (("D", "Couleur"), ("N", "Normale"), ("R", "Rugosite"), ("M", "Metal")):
        if suffix in textures:
            mel.set_material_instance_texture_parameter_value(instance, parameter, textures[suffix])
    eal.save_loaded_asset(instance)
    return instance


def import_balls():
    """Toutes les Balls de Tools/Capture/Balls. Renvoie (Balls importées, Balls avec textures)."""
    if not os.path.isdir(BALLS_DIR):
        return 0, 0
    identifiers = sorted(name for name in os.listdir(BALLS_DIR) if os.path.isdir(os.path.join(BALLS_DIR, name)))
    sources = find_texture_folders()

    # Textures d'abord : M_Ball a besoin de textures par défaut (celles de la Poké Ball si possible)
    all_textures = {}
    for identifier in identifiers:
        all_textures[identifier] = import_ball_textures(identifier, sources)
    defaults = all_textures.get("PokeBall") or next((t for t in all_textures.values() if len(t) == 4), {})
    master = make_ball_master(defaults)

    count = 0
    textured = 0
    for identifier in identifiers:
        folder = os.path.join(BALLS_DIR, identifier)
        destination = f"{BALLS_CONTENT}/{identifier}"
        parts = [f"SM_{identifier}_Haut", f"SM_{identifier}_Bas"]
        files = [os.path.join(folder, part + ".fbx") for part in parts]
        if not all(os.path.exists(path) for path in files):
            unreal.log_warning(f"{identifier} : moitiés manquantes dans {folder}")
            continue
        import_files(files, destination, parts)
        textures = all_textures[identifier]
        instance = make_ball_instance(identifier, master, textures) if master and textures else None
        for part in parts:
            mesh_path = f"{destination}/{part}"
            if not eal.does_asset_exist(mesh_path):
                continue
            if instance:
                mesh = eal.load_asset(mesh_path)
                mesh.set_material(0, instance)
                eal.save_loaded_asset(mesh)
        count += 1
        if instance:
            textured += 1
        else:
            unreal.log_warning(f"{identifier} : textures introuvables dans {SOURCES_DIR} (moitiés importées sans).")
    return count, textured


def main():
    material = make_material()
    sounds = import_sounds()

    balls, textured = import_balls()
    unreal.log(f"Balls de L'amie : {balls} importées dans {BALLS_CONTENT}, {textured} avec leurs textures.")

    mesh_path = CONTENT_DIR + "/SM_PokeBall"
    if os.path.exists(FBX_PATH):
        import_files([FBX_PATH], CONTENT_DIR)
    if eal.does_asset_exist(mesh_path):
        mesh = eal.load_asset(mesh_path)
        mesh.set_material(0, material)
        eal.save_loaded_asset(mesh)
        unreal.log(f"Poké Ball importée : {mesh_path}, M_PokeBall, {sounds} sons.")
    elif balls:
        unreal.log(f"Sons importés : {sounds}.")
    else:
        unreal.log_warning("Ni Balls de L'amie ni SM_PokeBall.fbx : le jeu lancera une sphère rouge. "
                           f"Sons importés : {sounds}. Voir docs/CAPTURE.md.")


main()
