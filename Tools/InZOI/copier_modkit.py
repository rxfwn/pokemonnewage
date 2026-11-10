# -*- coding: utf-8 -*-
"""Matériaux maîtres des personnages d'inZOI, depuis le ModKit officiel (Epic Games Store, Unreal 5.6).

Le ModKit contient les matériaux en version projet (graphe de noeuds ouvrable), pas seulement compilés :
M_HairCard_V03 (cheveux), M_HeadSkin_SkinTone_Makeup_V02 (peau de tête + maquillage), M_BodySkin_SkinTone_V02,
leurs instances de base, leurs fonctions et leurs textures par défaut. Notre Unreal 5.8 les ouvre tels quels.

Ces fichiers appartiennent à inZOI : ils finissent dans Content/ImportedAssets/InZOIModKit/ (gitignoré), jamais sur Git.

Deux étapes, Unreal fermé :
1) python copier_modkit.py [dossier du ModKit]  (défaut D:\\Dev\\inZOIModKit) : copie les fichiers à leur chemin
   d'origine dans Content/ (Content/Characters/Materials...), car leurs liens internes pointent là ;
2) UnrealEditor-Cmd PokemonNewAge.uproject -run=pythonscript -script=Tools/InZOI/copier_modkit.py : dans Unreal,
   les déplace vers /Game/ImportedAssets/InZOIModKit/ ; Unreal met lui-même les liens à jour.
"""
import os
import shutil
import sys

PROJECT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CONTENT = os.path.join(PROJECT, "Content")
DEST_GAME = "/Game/ImportedAssets/InZOIModKit"

# Dossiers entiers, puis fichiers isolés dont dépendent les matériaux (relevés dans leurs graphes)
FOLDERS = ["Characters/Materials"]
FILES = [
    "BG/Material/MPC/BG_NightWindow_MaterialParameterCollection",
    "Characters/AvatarParts/Body/Male/Textures/Male_BodyHair_Body_001_M",
    "MetaHumans/Common/Face/Textures/IrisTextures/iris_001/T_Iris_A_M",
    "MetaHumans/Common/Materials/Textures/Clothing/upresTest/MF_NormalStrength",
]


def copy_from_modkit(modkit):
    src = os.path.join(modkit, "inZOIModKit", "Content")
    if not os.path.isdir(src):
        print("ModKit introuvable : " + src)
        return 1
    if os.path.isdir(os.path.join(CONTENT, "ImportedAssets", "InZOIModKit", "Characters", "Materials")):
        print("Déjà en place dans Content/ImportedAssets/InZOIModKit (supprimer le dossier pour recopier)")
        return 0
    rels = []
    for folder in FOLDERS:
        for root, _dirs, files in os.walk(os.path.join(src, *folder.split("/"))):
            rels += [os.path.relpath(os.path.join(root, n), src) for n in files if n.endswith((".uasset", ".ubulk", ".uexp"))]
    for rel in FILES:
        for ext in (".uasset", ".ubulk", ".uexp"):
            if os.path.isfile(os.path.join(src, *rel.split("/")) + ext):
                rels.append(rel.replace("/", os.sep) + ext)
    for rel in rels:
        target = os.path.join(CONTENT, rel)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(os.path.join(src, rel), target)
    print("ModKit : %d fichiers copiés dans Content (étape 2 : les déplacer avec Unreal)" % len(rels))
    return 0


def move_in_unreal():
    import unreal
    eal = unreal.EditorAssetLibrary
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    reg.scan_paths_synchronous(["/Game/Characters/Materials", "/Game/BG", "/Game/MetaHumans", "/Game/Characters/AvatarParts"], True)
    moved = 0
    # Fichiers isolés d'abord : les dossiers déplacés ensuite pointent ainsi vers leur nouveau chemin
    for rel in FILES:
        source = "/Game/" + rel
        if eal.does_asset_exist(source):
            if eal.rename_asset(source, DEST_GAME + "/" + rel):
                moved += 1
    for folder in FOLDERS:
        source = "/Game/" + folder
        if eal.does_directory_exist(source) and eal.list_assets(source, True, False):
            # Chargés d'abord, tous ensemble : leurs liens entre eux sont résolus, puis renommés avec mise à jour
            for path in eal.list_assets(source, True, False):
                eal.load_asset(path)
            if eal.rename_directory(source, DEST_GAME + "/" + folder):
                moved += 1
            else:
                unreal.log_error("[ModKit] échec du déplacement de " + source)
    # Redirections laissées par les déplacements : résolues puis supprimées
    redirectors = []
    for root in ("/Game/Characters", "/Game/BG", "/Game/MetaHumans", DEST_GAME):
        if eal.does_directory_exist(root):
            for path in eal.list_assets(root, True, False):
                data = eal.find_asset_data(path)
                if data and str(data.asset_class_path.asset_name) == "ObjectRedirector":
                    redirectors.append(eal.load_asset(path))
    # Tous les fichiers ont été déplacés ensemble : aucun autre asset ne pointe vers les redirections, on les supprime
    for redirector in redirectors:
        if redirector:
            eal.delete_asset(redirector.get_path_name())
    for root in ("/Game/Characters/Materials", "/Game/Characters/AvatarParts", "/Game/BG", "/Game/MetaHumans"):
        if eal.does_directory_exist(root) and not eal.list_assets(root, True, False):
            eal.delete_directory(root)
    # Tout réenregistré : les liens pointent vers les nouveaux chemins (voir aussi [CoreRedirects] de DefaultEngine.ini)
    for path in eal.list_assets(DEST_GAME, True, False):
        if eal.load_asset(path):
            eal.save_asset(path, False)
    unreal.log("[ModKit] %d déplacements vers %s, %d redirections corrigées" % (moved, DEST_GAME, len(redirectors)))


if __name__ == "__main__":
    try:
        import unreal  # noqa: F401  (lancé par Unreal : étape 2)
        move_in_unreal()
    except ImportError:
        sys.exit(copy_from_modkit(sys.argv[1] if len(sys.argv) > 1 else r"D:\Dev\inZOIModKit"))
