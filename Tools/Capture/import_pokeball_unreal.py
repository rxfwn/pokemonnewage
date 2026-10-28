"""Importe la Poké Ball du lancer en 3D (SCRUM-40) dans Unreal.

Outils > Exécuter un script Python... > choisir ce fichier (n'importe quelle map ouverte).

Crée dans Content/Capture :
  - SM_PokeBall (modèle de Tools/Capture/Generated/SM_PokeBall.fbx, fait par make_pokeball.py) ;
  - M_PokeBall : plastique brillant, couleurs lues dans les couleurs de sommets, moitié haute réglable
    par le paramètre CouleurHaut (APokeBallThrow le change selon la Ball : rouge, bleue, jaune, violette) ;
  - Sons/SW_Ball_* : sons provisoires du lancer (fabriqués ici même s'ils manquent).
Relançable : remplace le modèle et les sons, garde M_PokeBall s'il existe déjà.

Sans le FBX, seuls les sons et le matériau sont créés ; le jeu lance alors une sphère rouge.
"""

import importlib.util
import os

import unreal

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
GENERATED_DIR = os.path.join(TOOLS_DIR, "Generated")
FBX_PATH = os.path.join(GENERATED_DIR, "SM_PokeBall.fbx")
CONTENT_DIR = "/Game/Capture"
SOUND_DIR = "/Game/Capture/Sons"
EFFECTS_CLASS = "/Game/Audio/SC_Effects"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


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
            options.set_editor_property("import_materials", False)
            options.set_editor_property("import_textures", False)
            options.set_editor_property("import_animations", False)
            mesh_data = options.get_editor_property("static_mesh_import_data")
            mesh_data.set_editor_property("combine_meshes", True)
            mesh_data.set_editor_property("auto_generate_collision", False)
            # Les couleurs de la Ball sont dans les couleurs de sommets
            mesh_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
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


def main():
    material = make_material()
    sounds = import_sounds()

    mesh_path = CONTENT_DIR + "/SM_PokeBall"
    if os.path.exists(FBX_PATH):
        import_files([FBX_PATH], CONTENT_DIR)
    if eal.does_asset_exist(mesh_path):
        mesh = eal.load_asset(mesh_path)
        mesh.set_material(0, material)
        eal.save_loaded_asset(mesh)
        unreal.log(f"Poké Ball importée : {mesh_path}, M_PokeBall, {sounds} sons.")
    else:
        unreal.log_warning("SM_PokeBall.fbx absent de Tools/Capture/Generated : le jeu lancera une sphère rouge. "
                           f"Sons importés : {sounds}. Voir docs/CAPTURE.md.")


main()
