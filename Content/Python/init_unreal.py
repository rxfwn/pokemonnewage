"""Lancé tout seul par Unreal à chaque ouverture de l'éditeur (plugin Python, fichier Content/Python/init_unreal.py).

Installe le Pokédex 3D (SCRUM-64) s'il manque : quand /Game/UI/Pokedex/DA_PokedexModel n'existe pas ou n'a pas
les pièces du modèle de L'amie, lance Tools/Pokedex/import_pokedex_unreal.py (modèle, textures, icônes, carte,
sons, HUD). Sinon ne fait rien. Pour réimporter de force : Outils > Exécuter un script Python > ce même script d'import.
"""

import importlib.util
import os

import unreal

MODEL_PATH = "/Game/UI/Pokedex/DA_PokedexModel"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
IMPORT_SCRIPT = os.path.join(PROJECT_DIR, "Tools", "Pokedex", "import_pokedex_unreal.py")
FBX_PATH = os.path.join(PROJECT_DIR, "Tools", "Pokedex", "Generated", "Pokedex.fbx")


def pokedex_installed():
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(MODEL_PATH):
        return False
    model = eal.load_asset(MODEL_PATH)
    try:
        parts = model.get_editor_property("parts")
    except Exception:
        return False
    return any(str(key) == "Body" and value for key, value in parts.items())


def install_pokedex():
    if pokedex_installed() or not os.path.exists(FBX_PATH) or not os.path.exists(IMPORT_SCRIPT):
        return
    if unreal.load_class(None, "/Script/PokemonNewAge.PokedexModel") is None:
        unreal.log_warning("Pokédex 3D pas installé : compiler le C++ d'abord.")
        return
    unreal.log("Pokédex 3D absent : installation automatique (Tools/Pokedex/import_pokedex_unreal.py)...")
    spec = importlib.util.spec_from_file_location("import_pokedex_unreal", IMPORT_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.main()


# Une seule fois, quand l'éditeur a fini de s'ouvrir (le registre des assets est alors chargé)
_handle = None


def _on_tick(delta_seconds):
    global _handle
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    if registry.is_loading_assets():
        return
    unreal.unregister_slate_post_tick_callback(_handle)
    try:
        install_pokedex()
    except Exception as error:
        unreal.log_error("Installation du Pokédex 3D ratée : %s (lancer Tools/Pokedex/import_pokedex_unreal.py à la main)" % error)


if not unreal.SystemLibrary.is_unattended():
    _handle = unreal.register_slate_post_tick_callback(_on_tick)
