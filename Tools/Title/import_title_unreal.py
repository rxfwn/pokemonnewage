"""Importe les images de l'écran de titre dans Unreal (script Python d'éditeur).

À lancer dans l'éditeur, Outils > Exécuter un script Python (Tools > Execute Python Script)
-> Tools/Title/import_title_unreal.py.

Ce qu'il fait : importe Tools/Title/Assets/*.png dans /Game/UI/Title, réglés en textures d'interface :
  - T_Title_Background (image de fond de la maquette de L'amie), T_Title_Background_2, _3... pour d'autres fonds ;
  - T_Title_Logo (logo à fond transparent, en haut à gauche), s'il est là.
L'écran de titre (UTitleScreenWidget) les trouve tout seul. Pour plusieurs fonds : Modifier > Paramètres du projet >
Jeu > Écran de titre > Backgrounds. Relançable : les assets existants sont remplacés.
"""

import glob
import os

import unreal

TITLE_DIR = "/Game/UI/Title"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ASSETS_DIR = os.path.join(PROJECT_DIR, "Tools", "Title", "Assets")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def import_title():
    images = sorted(glob.glob(os.path.join(ASSETS_DIR, "*.png")))
    tasks = []
    for path in images:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", TITLE_DIR)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    asset_tools.import_asset_tasks(tasks)

    count = 0
    for path in images:
        name = os.path.splitext(os.path.basename(path))[0]
        asset_path = TITLE_DIR + "/" + name
        texture = eal.load_asset(asset_path) if eal.does_asset_exist(asset_path) else None
        if not isinstance(texture, unreal.Texture2D):
            unreal.log_warning("Image pas importée : %s" % path)
            continue
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        if "Logo" in name:
            # Bords nets et transparence propre pour le logo
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        eal.save_asset(asset_path)
        count += 1

    unreal.log("Écran de titre prêt : %d images dans %s." % (count, TITLE_DIR))


import_title()
