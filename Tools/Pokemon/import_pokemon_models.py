"""Importe les modèles des Pokémon convertis (FBX) dans Unreal (script Python d'éditeur).

Avant : dézipper Pokemon_Models_V1.zip dans Tools/Pokemon/Generated/ (on doit avoir
Tools/Pokemon/Generated/models.json et un dossier par espèce).

Dans l'éditeur : Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> choisir Tools/Pokemon/import_pokemon_models.py

Résultat, pour chaque espèce (nom de ligne de DT_Species, ex. Bulbasaur) :
    /Game/Pokemon/Models/<Espèce>/SK_<Espèce>            Skeletal Mesh
    /Game/Pokemon/Models/<Espèce>/SKEL_<Espèce>          Squelette
    /Game/Pokemon/Models/<Espèce>/PHYS_<Espèce>          Physics Asset
    /Game/Pokemon/Models/<Espèce>/A_<Espèce>_Idle        animations, quand le modèle en a
    /Game/Pokemon/Models/<Espèce>/SK_<Espèce>_Shiny      version chromatique (même règle)
    (SM_<Espèce>... à la place de SK_ quand le modèle n'a pas de squelette)
    /Game/Pokemon/Models/<Espèce>/Materials/             matériaux et textures

Le menu Pokédex lit ces chemins tels quels : ne pas renommer les assets à la main.
Relançable : une espèce déjà importée est réimportée par-dessus (même chemin).
Pour n'importer que quelques espèces, remplir ONLY ci-dessous, ex. ONLY = {"Pikachu"}.
"""

import json
import os

import unreal

ONLY = set()

CONTENT_ROOT = "/Game/Pokemon/Models"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
GENERATED_DIR = os.path.join(PROJECT_DIR, "Tools", "Pokemon", "Generated")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def fbx_options(skeletal):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", skeletal)
    ui.set_editor_property("mesh_type_to_import",
                           unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_animations", skeletal)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("create_physics_asset", skeletal)
    ui.set_editor_property("automated_import_should_detect_type", False)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    sk.set_editor_property("import_morph_targets", True)
    sk.set_editor_property("convert_scene", True)
    sk.set_editor_property("use_t0_as_ref_pose", False)
    anim = ui.get_editor_property("anim_sequence_import_data")
    anim.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    anim.set_editor_property("import_bone_tracks", True)
    return ui


def import_fbx(fbx_path, dest, skeletal):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx_path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", fbx_options(skeletal))
    asset_tools.import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths") or [])


def assets_in(folder):
    data = registry.get_assets_by_path(folder, recursive=True)
    return [(str(d.package_name), str(d.asset_class_path.asset_name)) for d in data]


def move(src_package, dest_folder, new_name):
    dest = f"{dest_folder}/{new_name}"
    if src_package == dest:
        return
    if eal.does_asset_exist(dest):
        eal.delete_asset(dest)
    eal.rename_asset(src_package, dest)


def anim_short_name(asset_name, known):
    low = asset_name.lower()
    for name in sorted(known, key=len, reverse=True):
        if low.endswith(name.lower()):
            return name
    return asset_name.split("_")[-1]


def import_species(species, info, base):
    folder = f"{CONTENT_ROOT}/{species}"
    asset = info["asset"]  # SK_<Espèce>, SK_<Espèce>_Shiny ou SM_... sans squelette
    tag = asset[3:]  # <Espèce> ou <Espèce>_Shiny
    temp = f"{folder}/_Import_{tag}"
    if eal.does_directory_exist(temp):
        eal.delete_directory(temp)

    skeletal = info.get("skeletal", True)
    import_fbx(os.path.join(base, info["fbx"]), temp, skeletal)

    for package, cls in assets_in(temp):
        short = package.rsplit("/", 1)[-1]
        if cls == "SkeletalMesh":
            move(package, folder, f"SK_{tag}")
        elif cls == "StaticMesh":
            move(package, folder, f"SM_{tag}")
        elif cls == "Skeleton":
            move(package, folder, f"SKEL_{tag}")
        elif cls == "PhysicsAsset":
            move(package, folder, f"PHYS_{tag}")
        elif cls == "AnimSequence":
            move(package, folder, f"A_{tag}_{anim_short_name(short, info['animations'])}")
        else:  # matériaux, instances, textures
            move(package, f"{folder}/Materials", short)
    eal.delete_directory(temp)
    eal.save_directory(folder, only_if_is_dirty=True, recursive=True)

    mesh = eal.load_asset(f"{folder}/{asset}")
    if mesh is None:
        unreal.log_error(f"{tag} : pas de Skeletal Mesh après l'import (voir le journal)")
        return False
    unreal.log(f"{tag} : importé ({len(info['animations'])} animation(s))")
    return True


def main():
    summary_path = os.path.join(GENERATED_DIR, "models.json")
    if not os.path.exists(summary_path):
        unreal.log_error(f"{summary_path} introuvable : dézipper Pokemon_Models_V1.zip dans Tools/Pokemon/Generated/")
        return
    with open(summary_path, encoding="utf-8") as f:
        summary = json.load(f)

    ok, failed = 0, []
    with unreal.ScopedSlowTask(len(summary), "Import des modèles Pokémon") as slow:
        slow.make_dialog(True)
        for species, variants in summary.items():
            if slow.should_cancel():
                break
            slow.enter_progress_frame(1, species)
            if ONLY and species not in ONLY:
                continue
            for category in ("regular", "shiny"):
                if category in variants:
                    if import_species(species, variants[category], GENERATED_DIR):
                        ok += 1
                    else:
                        failed.append(variants[category]["asset"])
    unreal.log(f"Modèles Pokémon : {ok} importés dans {CONTENT_ROOT}")
    if failed:
        unreal.log_error("Échecs : " + ", ".join(failed))


main()
