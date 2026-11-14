"""
Découpe le jeu packagé en plusieurs fichiers .pak, un par grand dossier de Content (docs/MISES_A_JOUR.md).

Sans découpage, tout le jeu tient dans un seul gros fichier : changer un détail obligerait le
launcher à retélécharger plusieurs Go. Avec lui, seul le .pak du dossier modifié change
(pakchunk3 pour /Game/Pokemon, etc.).

À lancer dans l'éditeur : Outils > Exécuter un script Python... (Tools > Execute Python Script),
puis Fichier > Tout enregistrer. Relançable : un dossier ajouté plus tard reçoit un nouveau numéro,
et les numéros déjà donnés ne changent jamais (sinon tout serait retéléchargé une fois).

Pour chaque dossier /Game/<Dossier>, le script crée l'étiquette /Game/<Dossier>/PAL_Decoupage_<Dossier>
(Primary Asset Label) avec « Label Assets in My Directory » coché. Tout ce qui n'est rangé dans aucun
dossier étiqueté (moteur, fichiers à la racine de Content...) reste dans pakchunk0.
"""

import os

import unreal

# Dossiers sans étiquette : ceux d'Unreal (acteurs du monde ouvert, rangés par la map elle-même),
# les scripts Python de l'éditeur, les dossiers perso de l'éditeur
SKIPPED = {"Python", "Collections", "Developers"}
LABEL_PREFIX = "PAL_Decoupage_"


def content_folders():
    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    names = []
    for name in sorted(os.listdir(content_dir)):
        if os.path.isdir(os.path.join(content_dir, name)) and not name.startswith("__") and name not in SKIPPED:
            names.append(name)
    return names


def label_path(folder):
    return f"/Game/{folder}/{LABEL_PREFIX}{folder}"


def existing_chunk(folder):
    path = label_path(folder)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    label = unreal.EditorAssetLibrary.load_asset(path)
    return label.get_editor_property("rules").get_editor_property("chunk_id")


def set_label(label, chunk_id):
    rules = label.get_editor_property("rules")
    rules.set_editor_property("chunk_id", chunk_id)
    rules.set_editor_property("priority", 1)
    rules.set_editor_property("apply_recursively", True)
    label.set_editor_property("rules", rules)
    # La règle de cuisson reste « Unknown » : on ne cuisine que ce que le jeu utilise, comme avant
    label.set_editor_property("label_assets_in_my_directory", True)


def main():
    folders = content_folders()
    known = {folder: existing_chunk(folder) for folder in folders}
    next_id = max([c for c in known.values() if c] + [0]) + 1

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = []
    for folder in folders:
        if known[folder]:
            unreal.log(f"[Découpage] /Game/{folder} : déjà dans pakchunk{known[folder]}")
            continue
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.PrimaryAssetLabel)
        label = tools.create_asset(f"{LABEL_PREFIX}{folder}", f"/Game/{folder}", unreal.PrimaryAssetLabel, factory)
        if not label:
            unreal.log_error(f"[Découpage] impossible de créer l'étiquette de /Game/{folder}")
            continue
        set_label(label, next_id)
        unreal.EditorAssetLibrary.save_loaded_asset(label)
        created.append(f"/Game/{folder} -> pakchunk{next_id}")
        next_id += 1

    unreal.log("[Découpage] terminé. " + (("Nouvelles étiquettes : " + ", ".join(created)) if created else "Rien de nouveau."))
    unreal.log("[Découpage] Vérifie que « Générer des segments » (Generate Chunks) est coché dans "
               "Paramètres du projet > Packaging (c'est déjà réglé dans Config/DefaultGame.ini).")


main()
