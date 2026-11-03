"""
Prépare dans Unreal ce dont la progression a besoin (docs/PROGRESSION.md). À lancer une fois dans l'éditeur :
    Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> choisir Tools/Progression/import_progression_unreal.py

Il :
  1. fabrique les sons (make_progression_sounds.py, rien à télécharger) dans Tools/Progression/Generated/Sounds,
     les importe dans /Game/Progression/Sounds et les range dans la classe de son SC_Effects (volume « Bruitages »),
     sauf la musique de l'évolution, rangée dans SC_Music ;
  2. crée /Game/Progression/M_EvolutionGlow : la lueur de la séquence d'évolution (matériau de surcouche,
     paramètres Glow 0 à 1 et Color) ;
  3. crée /Game/Progression/M_EvolutionLight : la lumière pleine de l'entrée par la Poké Ball (rayon, boule de
     lumière, étincelles), plus forte au centre qu'aux bords, mêmes paramètres.

Relançable : les sons sont remplacés, les matériaux ne sont créés que s'ils n'existent pas (on peut les retoucher à la main).
"""

import importlib.util
import os

import unreal

CONTENT_DIR = "/Game/Progression"
SOUND_DIR = CONTENT_DIR + "/Sounds"
AUDIO_DIR = "/Game/Audio"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
TOOLS_DIR = os.path.join(PROJECT_DIR, "Tools", "Progression")
GENERATED_DIR = os.path.join(TOOLS_DIR, "Generated", "Sounds")
MUSIC = {"SW_Evolution_Music"}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def make_sounds():
    spec = importlib.util.spec_from_file_location("make_progression_sounds", os.path.join(TOOLS_DIR, "make_progression_sounds.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    os.makedirs(GENERATED_DIR, exist_ok=True)
    tasks = []
    for name, make in module.SOUNDS.items():
        path = os.path.join(GENERATED_DIR, name + ".wav")
        module.write_wav(path, make())
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", SOUND_DIR)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    asset_tools.import_asset_tasks(tasks)

    # Classes de son des Options (créées par le script du Pokédex ; sinon les sons gardent la classe Master)
    effects = eal.load_asset(AUDIO_DIR + "/SC_Effects") if eal.does_asset_exist(AUDIO_DIR + "/SC_Effects") else None
    music = eal.load_asset(AUDIO_DIR + "/SC_Music") if eal.does_asset_exist(AUDIO_DIR + "/SC_Music") else None
    count = 0
    for name in module.SOUNDS:
        path = SOUND_DIR + "/" + name
        if not eal.does_asset_exist(path):
            unreal.log_warning("Son non importé : " + name)
            continue
        sound = eal.load_asset(path)
        sound_class = music if name in MUSIC else effects
        if sound_class:
            sound.set_editor_property("sound_class_object", sound_class)
            eal.save_loaded_asset(sound)
        count += 1
    return count


def make_glow_material():
    """M_EvolutionGlow : non éclairé, additif, plus fort sur les bords (Fresnel) pour une silhouette de lumière."""
    path = CONTENT_DIR + "/M_EvolutionGlow"
    if eal.does_asset_exist(path):
        return False
    material = asset_tools.create_asset("M_EvolutionGlow", CONTENT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("used_with_skeletal_mesh", True)
    material.set_editor_property("used_with_static_lighting", False)

    glow = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 0)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 0.0)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, -250)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.75, 0.9, 1.0, 1.0))
    fresnel = mel.create_material_expression(material, unreal.MaterialExpressionFresnel, -900, 250)
    fresnel.set_editor_property("exponent", 2.5)

    # Intensité = 6 au centre, 20 sur les bords
    rim = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -650, 250)
    rim.set_editor_property("const_b", 14.0)
    mel.connect_material_expressions(fresnel, "", rim, "A")
    base = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -450, 250)
    base.set_editor_property("const_b", 6.0)
    mel.connect_material_expressions(rim, "", base, "A")

    tinted = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, -150)
    mel.connect_material_expressions(color, "", tinted, "A")
    mel.connect_material_expressions(glow, "", tinted, "B")
    emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, 0)
    mel.connect_material_expressions(tinted, "", emissive, "A")
    mel.connect_material_expressions(base, "", emissive, "B")

    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    eal.save_asset(path)
    return True


def make_light_material():
    """M_EvolutionLight : non éclairé, additif, plein au centre et doux sur les bords (inverse du Fresnel)."""
    path = CONTENT_DIR + "/M_EvolutionLight"
    if eal.does_asset_exist(path):
        return False
    material = asset_tools.create_asset("M_EvolutionLight", CONTENT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    material.set_editor_property("used_with_static_lighting", False)

    glow = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 0)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 1.0)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, -250)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.7, 0.85, 1.0, 1.0))
    strength = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 400)
    strength.set_editor_property("parameter_name", "Intensite")
    strength.set_editor_property("default_value", 25.0)
    fresnel = mel.create_material_expression(material, unreal.MaterialExpressionFresnel, -900, 200)
    fresnel.set_editor_property("exponent", 1.5)

    # Centre = 1, bords = 0 (adouci par une puissance) : un trait de lumière sans arête
    center = mel.create_material_expression(material, unreal.MaterialExpressionOneMinus, -700, 200)
    mel.connect_material_expressions(fresnel, "", center, "")
    soft = mel.create_material_expression(material, unreal.MaterialExpressionPower, -550, 200)
    soft.set_editor_property("const_exponent", 2.0)
    mel.connect_material_expressions(center, "", soft, "Base")

    tinted = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, -150)
    mel.connect_material_expressions(color, "", tinted, "A")
    mel.connect_material_expressions(glow, "", tinted, "B")
    shaped = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 200)
    mel.connect_material_expressions(soft, "", shaped, "A")
    mel.connect_material_expressions(strength, "", shaped, "B")
    emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 0)
    mel.connect_material_expressions(tinted, "", emissive, "A")
    mel.connect_material_expressions(shaped, "", emissive, "B")

    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    eal.save_asset(path)
    return True


def main():
    with unreal.ScopedSlowTask(3, "Progression : sons et lumières de l'évolution") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "Sons")
        sounds = make_sounds()
        task.enter_progress_frame(1, "Matériau M_EvolutionGlow")
        created = make_glow_material()
        task.enter_progress_frame(1, "Matériau M_EvolutionLight")
        light = make_light_material()
    unreal.log("Progression : {} sons dans {}, M_EvolutionGlow {}, M_EvolutionLight {}".format(
        sounds, SOUND_DIR, "créé" if created else "déjà là (pas touché)", "créé" if light else "déjà là (pas touché)"))


main()
