"""Lancé tout seul par Unreal à chaque ouverture de l'éditeur (plugin Python, fichier Content/Python/init_unreal.py).

Installe le Pokédex 3D (SCRUM-64) s'il manque ou si son FBX ou son script d'import a changé (empreintes dans Saved/StartupImports.json, sans charger d'asset),
et lance Tools/Pokedex/import_pokedex_unreal.py (modèle, textures, icônes, carte,
sons, HUD). Fait aussi le matériau de verre des menus (Tools/UI/make_glass_material.py). Importe aussi les polices FF_*.ttf de Tools/HUD/Assets/fonts nouvelles ou modifiées. Sinon ne fait rien. Pour réimporter de force : Outils > Exécuter un script Python > ce même script d'import.
"""

import importlib.util
import os

import unreal

MODEL_PATH = "/Game/UI/Pokedex/DA_PokedexModel"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
IMPORT_SCRIPT = os.path.join(PROJECT_DIR, "Tools", "Pokedex", "import_pokedex_unreal.py")
FBX_PATH = os.path.join(PROJECT_DIR, "Tools", "Pokedex", "Generated", "Pokedex.fbx")

# Empreintes des scripts d'import déjà passés sur CE PC (Saved/ n'est pas dans Git).
# Charger des assets au démarrage (load_asset + get_metadata_tag) faisait planter l'éditeur chez L'amie
# (EXCEPTION_ACCESS_VIOLATION dans EditorScriptingUtilities, 2026-11-07) : quand rien n'a changé depuis la dernière
# fois, on ne charge plus rien, on compare juste l'empreinte des fichiers sources.
STAMP_FILE = os.path.join(PROJECT_DIR, "Saved", "StartupImports.json")


def _load_stamps():
    import json
    try:
        with open(STAMP_FILE, encoding="utf-8") as handle:
            return json.load(handle)
    except Exception:
        return {}


def _fingerprint(paths):
    import hashlib
    digest = hashlib.sha1()
    for path in paths:
        try:
            with open(path, "rb") as handle:
                digest.update(handle.read())
        except OSError:
            digest.update(b"absent")
    return digest.hexdigest()


def up_to_date(step, paths, assets):
    """Vrai si les fichiers sources n'ont pas changé depuis le dernier passage de cette étape.
    Premier passage avec les assets déjà là (does_asset_exist ne charge rien) : on note l'empreinte et on ne fait rien."""
    stamps = _load_stamps()
    fingerprint = _fingerprint(paths)
    if stamps.get(step) == fingerprint:
        return True
    if step not in stamps and all(unreal.EditorAssetLibrary.does_asset_exist(a) for a in assets):
        mark_done(step, paths)
        return True
    return False


def mark_done(step, paths):
    import json
    stamps = _load_stamps()
    stamps[step] = _fingerprint(paths)
    try:
        os.makedirs(os.path.dirname(STAMP_FILE), exist_ok=True)
        with open(STAMP_FILE, "w", encoding="utf-8") as handle:
            json.dump(stamps, handle, indent=2)
    except OSError as error:
        unreal.log_warning("Empreinte de démarrage non écrite : %s" % error)


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


def stale_images(folder, content_subdir, only_existing=False, assets="Assets"):
    """Images PNG de Tools/<folder>/<assets> pas encore importées (sauf only_existing), ou plus récentes que leur asset."""
    import glob
    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    stale = []
    for path in glob.glob(os.path.join(PROJECT_DIR, "Tools", folder, assets, "*.png")):
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
    # (et icônes des objets du Sac de combat, Tools/HUD/Assets/Items, 2026-11-01)
    if not (missing_fonts() or stale_images("HUD", os.path.join("UI", "HUD"))
            or stale_images("HUD", os.path.join("UI", "HUD", "Items"), assets=os.path.join("Assets", "Items"))) \
            or not os.path.exists(script):
        return
    unreal.log("Polices ou images du HUD nouvelles ou modifiées : import automatique (Tools/HUD/import_hud_unreal.py)...")
    spec = importlib.util.spec_from_file_location("import_hud_unreal", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.import_hud()


def install_glass():
    """Matériau de verre des menus (M_UI_Glass, 2026-11-01) : fait ou refait s'il manque ou a changé de version."""
    script = os.path.join(PROJECT_DIR, "Tools", "UI", "make_glass_material.py")
    if not os.path.exists(script) or up_to_date("verre", [script], ["/Game/UI/Materials/M_UI_Glass"]):
        return
    spec = importlib.util.spec_from_file_location("make_glass_material", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    unreal.log("Matériau de verre absent ou modifié : création automatique (Tools/UI/make_glass_material.py)...")
    module.make_glass_material()
    mark_done("verre", [script])


def install_sound_classes():
    """Classes de son des volumes (Musique, Bruits de pas, Météo, Vfx) et mélange SM_Options, s'il en manque une."""
    script = os.path.join(PROJECT_DIR, "Tools", "Audio", "make_sound_classes.py")
    if not os.path.exists(script):
        return
    spec = importlib.util.spec_from_file_location("make_sound_classes", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if module.missing():
        unreal.log("Classes de son manquantes : création automatique (Tools/Audio/make_sound_classes.py)...")
        module.make_sound_classes()


POKEDEX_SOURCES = [FBX_PATH, IMPORT_SCRIPT]
POKEDEX_ASSETS = [MODEL_PATH, "/Game/UI/Pokedex/M_PokedexWidget", "/Game/UI/Pokedex/Icons/T_PokedexButton_Camera",
                  "/Game/UI/Pokedex/M_PokedexLens", "/Game/UI/Pokedex/M_PokedexTextured"]


def install_pokedex():
    if not os.path.exists(FBX_PATH) or not os.path.exists(IMPORT_SCRIPT):
        return
    if up_to_date("pokedex", POKEDEX_SOURCES, POKEDEX_ASSETS):
        return
    if unreal.load_class(None, "/Script/PokemonNewAge.PokedexModel") is None:
        unreal.log_warning("Pokédex 3D pas installé : compiler le C++ d'abord.")
        return
    unreal.log("Pokédex 3D absent : installation automatique (Tools/Pokedex/import_pokedex_unreal.py)...")
    spec = importlib.util.spec_from_file_location("import_pokedex_unreal", IMPORT_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.main()
    mark_done("pokedex", POKEDEX_SOURCES)


# Une seule fois, quand l'éditeur a fini de s'ouvrir (le registre des assets est alors chargé).
# On attend en plus quelques secondes, puis on fait UNE étape par image : charger des assets dès la première
# image faisait planter l'éditeur chez L'amie (EXCEPTION_ACCESS_VIOLATION, 2026-11-07). Chaque étape écrit son nom
# dans le Journal de sortie avant de commencer : en cas de crash, la dernière ligne « Démarrage : ... » dit laquelle.
START_DELAY_SECONDS = 5.0
STEPS = [
    ("Pokédex 3D", install_pokedex),
    ("polices et images du HUD", install_fonts),
    ("images de l'écran de titre", install_title_images),
    ("matériau de verre", install_glass),
    ("classes de son", install_sound_classes),
]
_handle = None
_waited = 0.0
_next_step = 0


def _game_running():
    """Vrai pendant une partie lancée avec Jouer : importer ou enregistrer des assets à ce moment-là plante l'éditeur
    (EXCEPTION_ACCESS_VIOLATION dans EditorScriptingUtilities, 2026-11-07). On attend la fin de la partie."""
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None
    except Exception:
        return False


def _on_tick(delta_seconds):
    global _handle, _waited, _next_step
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    if registry.is_loading_assets() or _game_running():
        return
    if _waited < START_DELAY_SECONDS:
        _waited += delta_seconds
        return
    if _next_step >= len(STEPS):
        unreal.unregister_slate_post_tick_callback(_handle)
        return
    name, step = STEPS[_next_step]
    _next_step += 1
    unreal.log("Démarrage : %s..." % name)
    try:
        step()
    except Exception as error:
        unreal.log_error("Démarrage : %s raté : %s" % (name, error))


if not unreal.SystemLibrary.is_unattended():
    _handle = unreal.register_slate_post_tick_callback(_on_tick)
