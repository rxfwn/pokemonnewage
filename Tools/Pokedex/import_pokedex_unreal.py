"""Importe le Pokédex 3D du menu dans Unreal (script Python d'éditeur, SCRUM-64).

À lancer dans l'éditeur, APRÈS avoir compilé le C++ (la classe PokedexModel doit exister) :
    Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> choisir Tools/Pokedex/import_pokedex_unreal.py

Ce qu'il fait, tout seul :
  1. cherche Tools/Pokedex/Generated/Pokedex.fbx ; s'il n'y est pas, lance Blender (s'il est installé)
     avec make_pokedex_model.py pour le créer ;
  2. importe les pièces dans /Game/UI/Pokedex/Meshes (une Static Mesh par pièce) ;
  3. crée le matériau M_Pokedex (paramètres BaseColor, Metallic, Roughness, EmissiveColor, EmissiveStrength) ;
  4. fabrique les sons (make_pokedex_sounds.py) et les importe dans /Game/UI/Pokedex/Sounds ;
  5. crée ou met à jour DA_PokedexModel, que le Pokédex 3D charge à l'ouverture du menu.
Relançable : les assets existants sont remplacés.
"""

import glob
import importlib.util
import os
import subprocess

import unreal

CONTENT_DIR = "/Game/UI/Pokedex"
MESH_DIR = CONTENT_DIR + "/Meshes"
SOUND_DIR = CONTENT_DIR + "/Sounds"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
TOOLS_DIR = os.path.join(PROJECT_DIR, "Tools", "Pokedex")
GENERATED_DIR = os.path.join(TOOLS_DIR, "Generated")
FBX_PATH = os.path.join(GENERATED_DIR, "Pokedex.fbx")

# Pièces attendues par APokedexDevice
PARTS = ["Body", "TopBand", "Bezel", "LensRing", "Lens", "LedRed", "LedYellow", "LedGreen", "ButtonRound",
         "PillRed", "PillBlue", "DPad", "Panel", "Hinge", "Lid", "LidPanel"] + ["MenuButton%d" % i for i in range(6)]

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
    if not blender:
        unreal.log_error(
            "Pokedex.fbx introuvable. Soit le copier dans Tools/Pokedex/Generated/ (fichiers du projet : pokedex/Pokedex.fbx), "
            "soit installer Blender puis relancer ce script.")
        return False
    unreal.log("Création du modèle avec Blender : " + blender)
    subprocess.run([blender, "--background", "--python", os.path.join(TOOLS_DIR, "make_pokedex_model.py"), "--", GENERATED_DIR],
                   check=False)
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
            options.set_editor_property("import_materials", True)
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


def make_material():
    path = CONTENT_DIR + "/M_Pokedex"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)

    mel = unreal.MaterialEditingLibrary
    material = asset_tools.create_asset("M_Pokedex", CONTENT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def vector(name, value, x, y):
        node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(*value))
        return node

    def scalar(name, value, x, y):
        node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        return node

    base = vector("BaseColor", (0.5, 0.035, 0.025, 1.0), -500, -300)
    metallic = scalar("Metallic", 0.0, -500, -100)
    roughness = scalar("Roughness", 0.45, -500, 0)
    emissive = vector("EmissiveColor", (0.0, 0.0, 0.0, 1.0), -700, 200)
    strength = scalar("EmissiveStrength", 0.0, -700, 400)
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


def make_data_asset(meshes, material, sounds):
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

    parts = unreal.Map(unreal.Name, unreal.StaticMesh)
    for key, mesh in meshes.items():
        parts[unreal.Name(key)] = mesh
    model.set_editor_property("parts", parts)
    model.set_editor_property("material", material)
    for prop, sound in sounds.items():
        model.set_editor_property(prop, sound)
    eal.save_asset(path)
    return model


def main():
    os.makedirs(GENERATED_DIR, exist_ok=True)
    if not make_fbx():
        return

    with unreal.ScopedSlowTask(4, "Import du Pokédex 3D") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "Pièces 3D")
        import_files([FBX_PATH], MESH_DIR)
        meshes = collect_meshes()
        task.enter_progress_frame(1, "Matériau")
        material = make_material()
        task.enter_progress_frame(1, "Sons")
        sounds = import_sounds()
        task.enter_progress_frame(1, "DA_PokedexModel")
        model = make_data_asset(meshes, material, sounds)

    missing = [key for key in PARTS if key not in meshes]
    if missing:
        unreal.log_warning("Pièces manquantes : " + ", ".join(missing))
    if model:
        unreal.log("Pokédex 3D prêt : %d pièces sur %d, %d sons. Lancer le jeu et appuyer sur Tab." % (len(meshes), len(PARTS), len(sounds)))


main()
