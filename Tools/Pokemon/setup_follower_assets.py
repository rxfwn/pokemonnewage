"""Matériaux du Pokémon qui suit le joueur (docs/POKEMON_QUI_SUIT.md). Script Python d'éditeur.

À lancer dans Unreal APRÈS l'import des modèles (Tools/Pokemon/import_pokemon_models.py) :
    Outils > Exécuter un script Python (Tools > Execute Python Script)
    -> Tools/Pokemon/setup_follower_assets.py

Crée dans /Game/Pokemon/Follower/ :
- M_PokemonEyeExpression : matériau des yeux dont on peut décaler la planche d'expressions (paramètre EyeUVOffset).
  Les modèles Let's Go ont une planche de 2 × 4 cases par œil : ouvert, fâché / mi-clos, plissé / fermé, surpris /
  plissé. Décaler de (0 ; 0,5) montre la 3e ligne : les yeux fermés.
- M_FollowerBubble : bulle transparente (sommeil).
- M_FollowerSparkle : éclat lumineux doré (apparition et disparition dans les étoiles).
Puis, à côté de chaque matériau d'yeux des modèles (reconnu à sa texture pm0025_00_Eye_col..., car les matériaux
s'appellent M_Pikachu_1, _2...) : <matériau>_Closed, une instance de M_PokemonEyeExpression avec la même texture et
les yeux fermés. Le jeu la met pendant le sommeil et les clignements.

Relançable : les matériaux sont refaits, les instances mises à jour.
"""

import unreal

FOLLOWER_DIR = "/Game/Pokemon/Follower"
MODELS_DIR = "/Game/Pokemon/Models"
CLOSED_EYE_OFFSET = (0.0, 0.5)

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def new_material(name):
    """Matériau vide (créé, ou vidé s'il existe déjà : ses instances restent branchées)."""
    path = f"{FOLLOWER_DIR}/{name}"
    if eal.does_asset_exist(path):
        material = eal.load_asset(path)
        mel.delete_all_material_expressions(material)
        return material
    return asset_tools.create_asset(name, FOLLOWER_DIR, unreal.Material, unreal.MaterialFactoryNew())


def constant3(material, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(value[0], value[1], value[2], 1.0))
    return node


def constant(material, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def make_eye_material():
    material = new_material("M_PokemonEyeExpression")
    coords = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    offset = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, 150)
    offset.set_editor_property("parameter_name", "EyeUVOffset")
    offset.set_editor_property("default_value", unreal.LinearColor(CLOSED_EYE_OFFSET[0], CLOSED_EYE_OFFSET[1], 0.0, 0.0))
    mask = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -650, 150)
    for channel in "rgba":
        mask.set_editor_property(channel, channel in "rg")
    add = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -450, 50)
    texture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -250, 0)
    texture.set_editor_property("parameter_name", "BaseTexture")
    texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    roughness = constant(material, 0.7, -250, 300)
    specular = constant(material, 0.3, -250, 400)

    mel.connect_material_expressions(offset, "", mask, "")
    mel.connect_material_expressions(coords, "", add, "A")
    mel.connect_material_expressions(mask, "", add, "B")
    mel.connect_material_expressions(add, "", texture, "UVs")
    mel.connect_material_property(texture, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    eal.save_loaded_asset(material)
    return material


def make_bubble_material():
    material = new_material("M_FollowerBubble")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", False)
    color = constant3(material, (0.78, 0.92, 1.0), -400, 0)
    glow = constant3(material, (0.08, 0.1, 0.12), -400, 120)
    # Plus opaque sur les bords qu'au centre, comme une bulle de savon
    fresnel = mel.create_material_expression(material, unreal.MaterialExpressionFresnel, -600, 300)
    fresnel.set_editor_property("exponent", 2.5)
    lerp = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -300, 300)
    low = constant(material, 0.18, -500, 260)
    high = constant(material, 0.8, -500, 380)
    roughness = constant(material, 0.05, -300, 480)
    specular = constant(material, 1.0, -300, 560)

    mel.connect_material_expressions(low, "", lerp, "A")
    mel.connect_material_expressions(high, "", lerp, "B")
    mel.connect_material_expressions(fresnel, "", lerp, "Alpha")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    eal.save_loaded_asset(material)


def make_sparkle_material():
    material = new_material("M_FollowerSparkle")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.85, 0.35, 1.0))
    glow = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 200)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 8.0)
    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, 80)
    mel.connect_material_expressions(color, "", multiply, "A")
    mel.connect_material_expressions(glow, "", multiply, "B")
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    eal.save_loaded_asset(material)


def used_textures(material):
    """Textures d'un matériau importé (matériau ou instance)."""
    if isinstance(material, unreal.MaterialInstance):
        found = [v.parameter_value for v in material.get_editor_property("texture_parameter_values") if v.parameter_value]
        parent = material.get_editor_property("parent")
        return found + (used_textures(parent) if parent else [])
    return [t for t in mel.get_used_textures(material) if isinstance(t, unreal.Texture2D)]


def is_eye_texture(texture):
    name = texture.get_name()
    return "Eye" in name and "Iris" not in name


def eye_texture(material, by_name):
    """Texture de couleur des yeux de ce matériau, ou None si ce n'est pas un matériau d'yeux.

    Les matériaux des modèles s'appellent M_Pikachu_1, _2... : on les reconnaît à leur texture (pm0025_00_Eye_col).
    """
    textures = used_textures(material)
    eyes = [t for t in textures if is_eye_texture(t)]
    if not eyes and by_name:
        eyes = textures
    # La couleur avant les autres (normales, masques)
    eyes.sort(key=lambda t: (0 if "col" in t.get_name().lower() else 1,
                             1 if any(k in t.get_name().lower() for k in ("nrm", "normal", "lym", "msk", "mask")) else 0,
                             t.get_name()))
    return eyes[0] if eyes else None


def make_closed_eyes(master):
    made = 0
    for path in eal.list_assets(MODELS_DIR, recursive=True, include_folder=False):
        name = path.split(".")[-1]
        if name.endswith("_Closed") or "Iris" in name:
            continue
        material = eal.load_asset(path)
        if not isinstance(material, unreal.MaterialInterface):
            continue
        texture = eye_texture(material, by_name="Eye" in name)
        if texture is None:
            continue
        folder = path.rsplit("/", 1)[0]
        closed_path = f"{folder}/{name}_Closed"
        if eal.does_asset_exist(closed_path):
            instance = eal.load_asset(closed_path)
        else:
            instance = asset_tools.create_asset(f"{name}_Closed", folder, unreal.MaterialInstanceConstant,
                                                unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(instance, master)
        mel.set_material_instance_texture_parameter_value(instance, "BaseTexture", texture)
        mel.set_material_instance_vector_parameter_value(
            instance, "EyeUVOffset", unreal.LinearColor(CLOSED_EYE_OFFSET[0], CLOSED_EYE_OFFSET[1], 0.0, 0.0))
        eal.save_loaded_asset(instance)
        made += 1
        unreal.log(f"{closed_path} : yeux fermés ({texture.get_name()})")
    if made == 0:
        unreal.log_warning("Aucun matériau d'yeux trouvé (texture dont le nom contient « Eye ») dans " + MODELS_DIR)
    return made


def main():
    if not eal.does_directory_exist(FOLLOWER_DIR):
        eal.make_directory(FOLLOWER_DIR)
    master = make_eye_material()
    make_bubble_material()
    make_sparkle_material()
    made = make_closed_eyes(master)
    unreal.log(f"Pokémon qui suit : matériaux prêts ({made} matériau(x) d'yeux fermés)")


main()
