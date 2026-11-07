# -*- coding: utf-8 -*-
"""
Import des animations .ueanim de l'exercice inZOI (attentes, émotions, gestes, danses) sur les corps importés
par import_inzoi.py, et choix de l'animation d'attente du studio de l'éditeur.

Il faut le plugin UEFormat dans Plugins/UEFormat (voir docs/INTEGRATION_INZOI.md, étape « Animations »).
À lancer DANS Unreal : Outils > Exécuter un script Python > ce fichier, APRÈS import_inzoi.py.

Unreal ouvre une fenêtre « Animation Import Options » une fois par base : choisir le squelette indiqué dans le
Journal de sortie (et dans le titre de la barre de progression), puis « Import All ».
Relançable : les animations déjà importées sont sautées.
"""
import os

import unreal

# ---------------------------------------------------------------- Réglages

SOURCE = r"D:\Dev\SourceArt\CCMenu\Exercice Compilation"

# Animations importées (noms des fichiers .ueanim sans extension). Les « ANIM_F_ » d'inZOI vont aussi sur le corps
# d'homme (même squelette). Pour en voir d'autres : Index_Complet/Manifeste_Assets.csv, filtrer « .ueanim ».
ANIMATIONS = [
    # Attentes (la première sert au studio)
    "Characters/Anims/Interaction/Female/ANIM_F_NPC_Idle1",
    "Characters/Anims/Interaction/Female/ANIM_F_NPC_Idle2",
    "Characters/Anims/Interaction/Female/ANIM_F_NPC_Idle3",
    "Characters/Anims/Interaction/Female/ANIM_F_Clerk_Idle_Short_01",
    # Gestes et poses (carte dresseur, photo, émotes)
    "Characters/Anims/Interaction/Female/ANIM_F_HappyGreet",
    "Characters/Anims/Interaction/Female/ANIM_F_Clap_Stand_2",
    "Characters/Anims/Interaction/Female/ANIM_F_Selfie_Pose",
    "Characters/Anims/Interaction/Female/ANIM_F_MusclePose_2",
    "Characters/Anims/Interaction/Female/ANIM_F_Pose_CatHeart",
    "Characters/Anims/Interaction/Female/ANIM_F_Craft_Proud",
    # Émotions
    "Characters/Anims/Interaction/Female/Emotion/ANIM_F_Angry",
    "Characters/Anims/Interaction/Female/Emotion/ANIM_F_Afraid_1",
    "Characters/Anims/Interaction/Female/Emotion/ANIM_F_apologize",
    # Course sur place (tapis de course) : base possible pour un premier jogging en jeu
    "Characters/Anims/Interaction/Female/ANIM_F_Runningmachine_Jog_StartLoopEnd_Interaction_Montage",
]

# Animation d'attente du studio (doit être dans la liste)
STUDIO_IDLE = "ANIM_F_NPC_Idle1"

DEST = "/Game/ImportedAssets/InZOI"
EXPORTS = os.path.join(SOURCE, "Output", "Exports", "BlueClient", "Content")
DATA_ASSET = DEST + "/DA_TrainerAppearance_InZOI"
EAL = unreal.EditorAssetLibrary

# Corps des bases (mêmes chemins que import_inzoi.py), au cas où le catalogue ne renvoie pas le mesh chargé
BODIES = {
    "Homme": "Characters/AvatarParts/Body/Male/Mesh/SKM_Male_Base",
    "Femme": "Characters/AvatarParts/Body/Female/Mesh/SKM_Female_Base",
    "Garcon": "Characters/AvatarParts/Body/Child/Mesh/SKM_Child_Body",
    "Fille": "Characters/AvatarParts/Body/Child/Mesh/SKM_Child_Body",
}


def log(message):
    unreal.log("[InZOI] " + message)


def find_file(rel):
    """Le chemin exact peut avoir un sous-dossier en plus (Emotion/...) : on cherche le nom dans le dossier des animations"""
    direct = os.path.join(EXPORTS, *rel.split("/")) + ".ueanim"
    if os.path.isfile(direct):
        return direct
    name = rel.rsplit("/", 1)[-1] + ".ueanim"
    root = os.path.join(EXPORTS, "Characters", "Anims")
    for current, _dirs, files in os.walk(root):
        if name in files:
            return os.path.join(current, name)
    return None


def anim_factory():
    for class_name in ("EFAnimFactory", "UEFAnimFactory"):
        factory_class = getattr(unreal, class_name, None)
        if factory_class:
            return factory_class()
    return None


def main():
    data = unreal.load_asset(DATA_ASSET) if EAL.does_asset_exist(DATA_ASSET) else None
    if not data:
        unreal.log_error("[InZOI] Lance d'abord Tools/InZOI/import_inzoi.py (catalogue %s absent)" % DATA_ASSET)
        return
    if not anim_factory():
        unreal.log_error("[InZOI] Plugin UEFormat absent : copie-le dans Plugins/UEFormat puis recompile (docs/INTEGRATION_INZOI.md)")
        return

    files = []
    for rel in ANIMATIONS:
        path = find_file(rel)
        if path:
            files.append(path)
        else:
            log("Animation introuvable : " + rel)

    bases = data.get_editor_property("bases")
    for base in bases:
        base_id = str(base.get_editor_property("id"))
        mesh = base.get_editor_property("mesh")
        if not isinstance(mesh, unreal.SkeletalMesh) and base_id in BODIES:
            body_path = DEST + "/" + BODIES[base_id]
            folder, name = body_path.rsplit("/", 1)
            for candidate in (body_path, "%s/%s/SkeletalMeshes/%s" % (folder, name, name)):
                if EAL.does_asset_exist(candidate):
                    mesh = unreal.load_asset(candidate)
                    break
        skeleton = mesh.get_editor_property("skeleton") if mesh else None
        if not skeleton:
            log("Pas de squelette pour la base " + base_id)
            continue
        folder = "%s/Anims/%s" % (DEST, base_id)
        tasks = []
        factory = anim_factory()  # une fabrique par base = une seule fenêtre de choix du squelette
        for path in files:
            name = os.path.splitext(os.path.basename(path))[0]
            if EAL.does_asset_exist(folder + "/" + name):
                continue
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", path)
            task.set_editor_property("destination_path", folder)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("factory", factory)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("save", True)
            tasks.append(task)
        if tasks:
            message = "Base %s : dans la fenêtre, choisis le squelette « %s » puis Import All" % (base_id, skeleton.get_name())
            log(message)
            unreal.EditorDialog.show_message("Animations inZOI", message, unreal.AppMsgType.OK)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

        idle_path = folder + "/" + STUDIO_IDLE
        if EAL.does_asset_exist(idle_path):
            base.set_editor_property("idle_animation", unreal.load_asset(idle_path))
            log("Attente du studio pour %s : %s" % (base_id, STUDIO_IDLE))

    data.set_editor_property("bases", bases)
    EAL.save_loaded_asset(data)
    log("Animations terminées : " + DEST + "/Anims")


main()
