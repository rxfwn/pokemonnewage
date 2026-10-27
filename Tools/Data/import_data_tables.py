"""Importe Data/Pokemon/DT_*.json dans Unreal (script Python d'éditeur).

À lancer dans l'éditeur, APRÈS avoir compilé le C++ (les structs FSpeciesRow... doivent exister) :
    Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> choisir Tools/Data/import_data_tables.py

Crée ou met à jour les Data Tables dans /Game/Pokemon/Data/ et l'asset DA_PokemonDatabase qui
les regroupe. Relançable : les tables existantes sont vidées puis re-remplies depuis le JSON.
"""

import os

import unreal

CONTENT_DIR = "/Game/Pokemon/Data"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
JSON_DIR = os.path.join(PROJECT_DIR, "Data", "Pokemon")

# (asset, struct C++, propriété de DA_PokemonDatabase)
TABLES = [
    ("DT_Species", "SpeciesRow", "species"),
    ("DT_Moves", "MoveRow", "moves"),
    ("DT_TypeChart", "TypeChartRow", "type_chart"),
    ("DT_Learnsets", "LearnsetRow", "learnsets"),
    ("DT_Machines", "MachineRow", "machines"),
    ("DT_Abilities", "AbilityRow", "abilities"),
    ("DT_Natures", "NatureRow", "natures"),
    ("DT_Experience", "ExperienceRow", "experience"),
    ("DT_Encounters", "EncounterTableRow", "encounters"),
    ("DT_Trainers", "TrainerRow", "trainers"),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def get_or_create_table(name, struct):
    path = f"{CONTENT_DIR}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", struct)
    return asset_tools.create_asset(name, CONTENT_DIR, unreal.DataTable, factory)


def main():
    tables = {}
    for name, struct_name, prop in TABLES:
        struct = unreal.load_object(None, f"/Script/PokemonNewAge.{struct_name}")
        if struct is None:
            unreal.log_error(f"Struct {struct_name} introuvable : compiler le C++ d'abord.")
            return
        json_path = os.path.join(JSON_DIR, f"{name}.json")
        table = get_or_create_table(name, struct)
        ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(table, json_path, struct)
        rows = len(unreal.DataTableFunctionLibrary.get_data_table_row_names(table))
        if ok:
            unreal.log(f"{name} : {rows} lignes importées")
        else:
            unreal.log_error(f"{name} : échec de l'import de {json_path} (voir le journal)")
        eal.save_asset(table.get_path_name())
        tables[prop] = table

    db_path = f"{CONTENT_DIR}/DA_PokemonDatabase"
    if eal.does_asset_exist(db_path):
        database = eal.load_asset(db_path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.PokemonDatabase)
        database = asset_tools.create_asset("DA_PokemonDatabase", CONTENT_DIR, unreal.PokemonDatabase, factory)
    for prop, table in tables.items():
        database.set_editor_property(prop, table)
    eal.save_asset(database.get_path_name())
    unreal.log("DA_PokemonDatabase à jour.")


main()
