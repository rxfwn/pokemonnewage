"""Lancé tout seul par Unreal à chaque ouverture de l'éditeur (plugin Python, fichier Content/Python/init_unreal.py).

Installe le Pokédex 3D (SCRUM-64) s'il manque : quand /Game/UI/Pokedex/DA_PokedexModel n'existe pas ou n'a pas
les pièces du modèle de L'amie, lance Tools/Pokedex/import_pokedex_unreal.py (modèle, textures, icônes, carte,
sons, HUD). Fait aussi le matériau de verre des menus (Tools/UI/make_glass_material.py). Importe aussi les polices FF_*.ttf de Tools/HUD/Assets/fonts nouvelles ou modifiées. Sinon ne fait rien. Pour réimporter de force : Outils > Exécuter un script Python > ce même script d'import.
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
    # M_PokedexWidget : ajouté le 2026-10-31 (écrans noirs sans lui) ; son absence relance l'import
    if not eal.does_asset_exist(MODEL_PATH) or not eal.does_asset_exist("/Game/UI/Pokedex/M_PokedexWidget"):
        return False
    # Icône appareil photo et écrans lissés (version 8 du matériau) : ajoutés le 2026-11-01
    if not eal.does_asset_exist("/Game/UI/Pokedex/Icons/T_PokedexButton_Camera"):
        return False
    widget_material = eal.load_asset("/Game/UI/Pokedex/M_PokedexWidget")
    if not widget_material or eal.get_metadata_tag(widget_material, "PokedexWidgetVersion") != "8":
        return False
    # Objectif d'appareil photo (ajouté le 2026-11-01)
    if not eal.does_asset_exist("/Game/UI/Pokedex/M_PokedexLens"):
        return False
    lens = eal.load_asset("/Game/UI/Pokedex/M_PokedexLens")
    if not lens or eal.get_metadata_tag(lens, "PokedexVersion") != "2":
        return False
    textured = eal.load_asset("/Game/UI/Pokedex/M_PokedexTextured")
    if not textured or eal.get_metadata_tag(textured, "PokedexVersion") != "2":
        return False
    model = eal.load_asset(MODEL_PATH)
    try:
        parts = model.get_editor_property("parts")
    except Exception:
        return False
    return any(str(key) == "Body" and value for key, value in parts.items())


def missing_fonts():
    """Polices de Tools/HUD/Assets/fonts pas encore importées, ou dont le .ttf est plus récent que l'asset
    (ex. accents ajoutés à FF_Futura_Bold.ttf par Tools/HUD/add_futura_accents.py)."""
    import glob
    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    stale = []
    for path in glob.glob(os.path.join(PROJECT_DIR, "Tools", "HUD", "Assets", "fonts", "FF_*.ttf")):
        name = os.path.splitext(os.path.basename(path))[0]
        if "." in name:  # FF_Futura_Bold.orig.ttf : copie de l'original, pas une police du jeu
            continue
        asset_file = os.path.join(content_dir, "UI", "HUD", "Fonts", name + ".uasset")
        if not os.path.exists(asset_file) or os.path.getmtime(path) > os.path.getmtime(asset_file):
            stale.append(name)
    return stale


def stale_images(folder, content_subdir, only_existing=False):
    """Images PNG de Tools/<folder>/Assets pas encore importées (sauf only_existing), ou plus récentes que leur asset."""
    import glob
    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    stale = []
    for path in glob.glob(os.path.join(PROJECT_DIR, "Tools", folder, "Assets", "*.png")):
        name = os.path.splitext(os.path.basename(path))[0]
        asset_file = os.path.join(content_dir, content_subdir, name + ".uasset")
        if not os.path.exists(asset_file):
            if not only_existing:
                stale.append(name)
        elif os.path.getmtime(path) > os.path.getmtime(asset_file):
            stale.append(name)
    return stale


def install_title_images():
    """Écran de titre : il lit les PNG de Tools/Title/Assets tant qu'ils ne sont pas importés ; une fois importés,
    un PNG changé (ex. nouveau logo du 2026-11-01) doit être réimporté, sinon l'ancien asset reste affiché."""
    script = os.path.join(PROJECT_DIR, "Tools", "Title", "import_title_unreal.py")
    if not stale_images("Title", os.path.join("UI", "Title"), only_existing=True) or not os.path.exists(script):
        return
    unreal.log("Images de l'écran de titre modifiées : import automatique (Tools/Title/import_title_unreal.py)...")
    spec = importlib.util.spec_from_file_location("import_title_unreal_auto", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)  # le script importe tout en se chargeant


def install_fonts():
    script = os.path.join(PROJECT_DIR, "Tools", "HUD", "import_hud_unreal.py")
    # Polices ou images du HUD nouvelles ou modifiées (ex. boussole de la maquette v2, 2026-11-01)
    if not (missing_fonts() or stale_images("HUD", os.path.join("UI", "HUD"))) or not os.path.exists(script):
        return
    unreal.log("Polices ou images du HUD nouvelles ou modifiées : import automatique (Tools/HUD/import_hud_unreal.py)...")
    spec = importlib.util.spec_from_file_location("import_hud_unreal", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.import_hud()


def install_glass():
    """Matériau de verre des menus (M_UI_Glass, 2026-11-01) : fait ou refait s'il manque ou a changé de version."""
    script = os.path.join(PROJECT_DIR, "Tools", "UI", "make_glass_material.py")
    if not os.path.exists(script):
        return
    spec = importlib.util.spec_from_file_location("make_glass_material", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if not module.glass_material_up_to_date():
        unreal.log("Matériau de verre absent ou ancien : création automatique (Tools/UI/make_glass_material.py)...")
        module.make_glass_material()


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
        install_fonts()
        install_title_images()
        install_glass()
    except Exception as error:
        unreal.log_error("Installation du Pokédex 3D ratée : %s (lancer Tools/Pokedex/import_pokedex_unreal.py à la main)" % error)


if not unreal.SystemLibrary.is_unattended():
    _handle = unreal.register_slate_post_tick_callback(_on_tick)
