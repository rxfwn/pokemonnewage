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
    /Game/Pokemon/Models/DT_MorphPoints                  points de surface pour l'évolution (C++ compilé requis)

Le menu Pokédex lit ces chemins tels quels : ne pas renommer les assets à la main.
Relançable : une espèce déjà importée est réimportée par-dessus (même chemin).
Seulement la table des points (modèles déjà importés) : lancer Tools/Pokemon/import_morph_points.py.
Pokémon gris à damier (matériaux décrochés) : lancer Tools/Pokemon/repair_pokemon_materials.py.
Pour n'importer que quelques espèces, remplir ONLY ci-dessous, ex. ONLY = {"Pikachu"}.
"""

import json
import os

import unreal

ONLY = set()
# Mode donné par les petits scripts voisins (sans réimporter les modèles) :
#   "points"    -> import_morph_points.py : seulement DT_MorphPoints
#   "materials" -> repair_pokemon_materials.py : rebranche les matériaux des modèles déjà importés
MODE = globals().get("MODE", "all")

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
    relink_materials(mesh, folder)
    unreal.log(f"{tag} : importé ({len(info['animations'])} animation(s))")
    return True


def relink_materials(mesh, folder):
    """Rebranche chaque emplacement de matériau sur Materials/<nom de l'emplacement>.

    Les matériaux sont déplacés depuis le dossier temporaire d'import : selon l'ordre des déplacements,
    le mesh pouvait garder un lien vers l'ancien emplacement (supprimé), d'où un Pokémon gris à damier.
    """
    prop = "materials" if isinstance(mesh, unreal.SkeletalMesh) else "static_materials"
    slots = list(mesh.get_editor_property(prop))
    fixed = 0
    for slot in slots:
        name = str(slot.get_editor_property("material_slot_name"))
        current = slot.get_editor_property("material_interface")
        wanted_path = f"{folder}/Materials/{name}"
        if current is not None and current.get_path_name().split(".")[0] == wanted_path:
            continue
        if not eal.does_asset_exist(wanted_path):
            unreal.log_warning(f"{mesh.get_name()} : matériau {wanted_path} introuvable pour l'emplacement {name}")
            continue
        slot.set_editor_property("material_interface", eal.load_asset(wanted_path))
        fixed += 1
    if fixed:
        mesh.set_editor_property(prop, slots)
        eal.save_loaded_asset(mesh, only_if_is_dirty=False)
        unreal.log(f"{mesh.get_name()} : {fixed} matériau(x) rebranché(s)")
    return fixed


def repair_all_materials(summary):
    total = 0
    for species, variants in summary.items():
        folder = f"{CONTENT_ROOT}/{species}"
        for info in variants.values():
            path = f"{folder}/{info['asset']}"
            if eal.does_asset_exist(path):
                total += relink_materials(eal.load_asset(path), folder)
    unreal.log(f"Matériaux des Pokémon : {total} emplacement(s) rebranché(s)")


def import_morph_points(species_list):
    """DT_MorphPoints : points de surface de chaque espèce pour la transformation de l'évolution.

    Toujours rempli avec toutes les espèces présentes dans Generated/ (même avec ONLY). Demande le C++ compilé.
    """
    struct = unreal.load_object(None, "/Script/PokemonNewAge.PokemonMorphPointsRow")
    if struct is None:
        unreal.log_warning("PokemonMorphPointsRow introuvable : compiler le C++ puis relancer (l'évolution garde l'ancien effet).")
        return
    rows = []
    for species in species_list:
        path = os.path.join(GENERATED_DIR, species, "morph_points.json")
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                points = json.load(f)
            rows.append({"Name": species, "Points": [{"X": x, "Y": y, "Z": z} for x, y, z in points]})
    if not rows:
        unreal.log_warning("Aucun morph_points.json dans Generated/ : reprendre le dernier zip des modèles.")
        return
    json_path = os.path.join(GENERATED_DIR, "DT_MorphPoints.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(rows, f)
    path = f"{CONTENT_ROOT}/DT_MorphPoints"
    if eal.does_asset_exist(path):
        table = eal.load_asset(path)
    else:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", struct)
        table = asset_tools.create_asset("DT_MorphPoints", CONTENT_ROOT, unreal.DataTable, factory)
    if unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(table, json_path, struct):
        unreal.log(f"DT_MorphPoints : {len(rows)} espèces")
    else:
        unreal.log_error(f"DT_MorphPoints : échec de l'import de {json_path}")
    eal.save_asset(path)


def main():
    summary_path = os.path.join(GENERATED_DIR, "models.json")
    if not os.path.exists(summary_path):
        unreal.log_error(f"{summary_path} introuvable : dézipper Pokemon_Models_V1.zip dans Tools/Pokemon/Generated/")
        return
    with open(summary_path, encoding="utf-8") as f:
        summary = json.load(f)

    if MODE == "points":
        import_morph_points(list(summary))
        return
    if MODE == "materials":
        repair_all_materials(summary)
        return

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
    import_morph_points(list(summary))
    unreal.log(f"Modèles Pokémon : {ok} importés dans {CONTENT_ROOT}")
    if failed:
        unreal.log_error("Échecs : " + ", ".join(failed))


main()
