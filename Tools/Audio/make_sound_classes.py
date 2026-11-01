"""Classes de son des réglages de volume (script Python d'éditeur).

À lancer dans l'éditeur, Outils > Exécuter un script Python -> Tools/Audio/make_sound_classes.py.
Content/Python/init_unreal.py le lance tout seul à l'ouverture de l'éditeur s'il manque une classe.

Crée dans /Game/Audio (sans toucher à celles qui existent) :
  - SC_Music : musique (réglage « Musique ») ;
  - SC_Footsteps : bruits de pas (réglage « Bruits de pas ») ;
  - SC_Weather : pluie, vent, orage... (réglage « Météo ») ;
  - SC_Effects : tous les autres bruitages, attaques, interface (réglage « Vfx ») ;
  - SM_Options : le mélange qui applique ces volumes.
Pour qu'un son suive un réglage : ouvrir le son (ou sa Sound Cue / MetaSound) > Détails > Classe de son (Sound Class)
-> la classe voulue. Un son sans classe ne suit que le volume général.
"""

import unreal

AUDIO_DIR = "/Game/Audio"
CLASSES = ["SC_Music", "SC_Footsteps", "SC_Weather", "SC_Effects"]
MIX = "SM_Options"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def missing():
    return [name for name in CLASSES + [MIX] if not eal.does_asset_exist(AUDIO_DIR + "/" + name)]


def make_sound_classes():
    created = []
    for name in missing():
        if name == MIX:
            asset = asset_tools.create_asset(name, AUDIO_DIR, unreal.SoundMix, unreal.SoundMixFactory())
        else:
            asset = asset_tools.create_asset(name, AUDIO_DIR, unreal.SoundClass, unreal.SoundClassFactory())
        if asset:
            eal.save_loaded_asset(asset)
            created.append(name)
    unreal.log("Classes de son prêtes dans %s (créées : %s)." % (AUDIO_DIR, ", ".join(created) or "aucune"))


if __name__ != "make_sound_classes":
    make_sound_classes()
