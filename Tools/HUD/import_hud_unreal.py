"""Importe les images et les polices du HUD en jeu dans Unreal (script Python d'éditeur).

À lancer dans l'éditeur, Outils > Exécuter un script Python (Tools > Execute Python Script)
-> Tools/HUD/import_hud_unreal.py. Le script du Pokédex (Tools/Pokedex/import_pokedex_unreal.py) le lance aussi à la fin.

Ce qu'il fait :
  1. importe Tools/HUD/Assets/*.png dans /Game/UI/HUD (icône du Pokédex, cône de la boussole, pictos météo),
     réglés en textures d'interface (pas de mipmaps, pas de compression) ;
  2. importe les polices de la maquette (Tools/HUD/Assets/fonts/FF_*.ttf : Inter, Futura) dans /Game/UI/HUD/Fonts
     (une Font Face par fichier, lue par UGameHUDWidget).
Relançable : les assets existants sont remplacés. Les images se refont avec Tools/HUD/make_hud_assets.py.
"""

import glob
import os

import unreal

HUD_DIR = "/Game/UI/HUD"
FONT_DIR = HUD_DIR + "/Fonts"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ASSETS_DIR = os.path.join(PROJECT_DIR, "Tools", "HUD", "Assets")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def import_files(files, destination):
    tasks = []
    for path in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    asset_tools.import_asset_tasks(tasks)
    return tasks


def as_ui(path):
    texture = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if isinstance(texture, unreal.Texture2D):
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        eal.save_asset(path)
    return texture


def import_hud():
    images = sorted(glob.glob(os.path.join(ASSETS_DIR, "*.png")))
    import_files(images, HUD_DIR)
    textures = [as_ui(HUD_DIR + "/" + os.path.splitext(os.path.basename(path))[0]) for path in images]

    fonts = sorted(glob.glob(os.path.join(ASSETS_DIR, "fonts", "FF_*.ttf")))
    import_files(fonts, FONT_DIR)
    faces = []
    for path in fonts:
        asset_path = FONT_DIR + "/" + os.path.splitext(os.path.basename(path))[0]
        asset = eal.load_asset(asset_path) if eal.does_asset_exist(asset_path) else None
        if isinstance(asset, unreal.FontFace):
            faces.append(asset)
        else:
            unreal.log_warning("Police pas importée en Font Face : %s (le HUD prendra Roboto)" % asset_path)

    unreal.log("HUD prêt : %d images, %d polices dans %s." % (len([t for t in textures if t]), len(faces), HUD_DIR))


# Lancé seul depuis l'éditeur (pas quand le script du Pokédex le charge sous le nom import_hud_unreal)
if __name__ != "import_hud_unreal":
    import_hud()
