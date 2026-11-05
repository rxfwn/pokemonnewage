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
Puis, à côté de chaque matériau d'yeux des modèles (liste dans Tools/Pokemon/eye_materials.json, car les matériaux
s'appellent M_Pikachu_1, _2... et les textures T_Pikachu_1...) : <matériau>_Closed, une instance de M_PokemonEyeExpression avec la même texture et
les yeux fermés. Le jeu la met pendant le sommeil et les clignements.

Relançable : les matériaux sont refaits, les instances mises à jour.
"""

import json
import os

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


def sampler_type_for(texture):
    """Type d'échantillonneur qu'Unreal exige pour cette texture (sinon il la refuse et met le damier gris)."""
    st = unreal.MaterialSamplerType
    srgb = texture.get_editor_property("srgb")
    if texture.get_editor_property("virtual_texture_streaming"):
        return st.SAMPLERTYPE_VIRTUAL_COLOR if srgb else st.SAMPLERTYPE_VIRTUAL_LINEAR_COLOR
    compression = texture.get_editor_property("compression_settings")
    tc = unreal.TextureCompressionSettings
    if compression == tc.TC_NORMALMAP:
        return st.SAMPLERTYPE_NORMAL
    if compression == tc.TC_MASKS:
        return st.SAMPLERTYPE_MASKS
    if compression == tc.TC_GRAYSCALE:
        return st.SAMPLERTYPE_GRAYSCALE if srgb else st.SAMPLERTYPE_LINEAR_GRAYSCALE
    return st.SAMPLERTYPE_COLOR if srgb else st.SAMPLERTYPE_LINEAR_COLOR


MASTERS = {}


def eye_master_for(texture):
    """Matériau des yeux qui accepte cette texture (un par type : couleur, texture virtuelle...)."""
    sampler = sampler_type_for(texture)
    if sampler not in MASTERS:
        suffix = "" if sampler == unreal.MaterialSamplerType.SAMPLERTYPE_COLOR else "_" + str(sampler.name).replace("SAMPLERTYPE_", "").title().replace("_", "")
        MASTERS[sampler] = make_eye_material("M_PokemonEyeExpression" + suffix, sampler, texture)
    return MASTERS[sampler]


def make_eye_material(name="M_PokemonEyeExpression", sampler=None, example=None):
    material = new_material(name)
    # Les surfaces des yeux peuvent être retournées (vues de dos) : sans « deux faces », on voit le trou derrière
    material.set_editor_property("two_sided", True)
    coords = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    offset = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, 150)
    offset.set_editor_property("parameter_name", "EyeUVOffset")
    offset.set_editor_property("default_value", unreal.LinearColor(CLOSED_EYE_OFFSET[0], CLOSED_EYE_OFFSET[1], 0.0, 0.0))
    mask = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -650, 150)
    for channel in "rgba":
        mask.set_editor_property(channel, channel in "rg")
    # Les UV des yeux sortent de 0..1 (ex. V de -0,98 à -0,77) : on les ramène dans la planche avant de décaler,
    # sinon la texture (bord ou répétition selon l'import) montre des carrés de peau au lieu des yeux fermés
    frac = mel.create_material_expression(material, unreal.MaterialExpressionFrac, -650, 0)
    add = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -450, 50)
    texture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -250, 0)
    texture.set_editor_property("parameter_name", "BaseTexture")
    if sampler is not None and sampler != unreal.MaterialSamplerType.SAMPLERTYPE_COLOR:
        # Texture d'exemple du même type (les instances mettent la leur)
        texture.set_editor_property("texture", example)
        texture.set_editor_property("sampler_type", sampler)
    else:
        texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    roughness = constant(material, 0.7, -250, 300)
    specular = constant(material, 0.3, -250, 400)

    mel.connect_material_expressions(offset, "", mask, "")
    mel.connect_material_expressions(coords, "", frac, "")
    mel.connect_material_expressions(frac, "", add, "A")
    mel.connect_material_expressions(mask, "", add, "B")
    mel.connect_material_expressions(add, "", texture, "UVs")
    mel.connect_material_property(texture, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    # Posé sur un mesh à squelette : sans ce réglage, Unreal affiche à la place son damier gris (WorldGridMaterial)
    material.set_editor_property("used_with_skeletal_mesh", True)
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
    getter = getattr(mel, "get_material_used_textures", None) or mel.get_used_textures
    return [t for t in getter(material) if isinstance(t, unreal.Texture2D)]


def load_eye_list():
    """Matériaux d'yeux de chaque espèce et leur texture (eye_materials.json, fait par find_eye_materials.py).

    Les matériaux s'appellent M_Pikachu_1, _2... et les textures T_Pikachu_1... : rien dans Unreal ne dit
    lesquels sont des yeux. Le nom d'origine des images (pm0025_00_Eye_col) n'existe que dans les FBX.
    """
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "eye_materials.json")
    if not os.path.exists(path):
        unreal.log_warning(f"{path} introuvable : lancer Tools/Pokemon/find_eye_materials.py")
        return {}
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def eye_texture(folder, material, texture_name):
    """Texture de la planche d'expressions : celle notée dans le JSON, sinon celle du matériau."""
    for candidate in (f"{folder}/Materials/{texture_name}", f"{folder}/{texture_name}"):
        if texture_name and eal.does_asset_exist(candidate):
            texture = eal.load_asset(candidate)
            if isinstance(texture, unreal.Texture2D):
                return texture
    textures = used_textures(material)
    return textures[0] if textures else None


def make_closed_eyes():
    made = 0
    for species, materials in load_eye_list().items():
        folder = f"{MODELS_DIR}/{species}"
        for name, texture_name in materials.items():
            path = f"{folder}/Materials/{name}"
            if not eal.does_asset_exist(path):
                continue  # modèle pas importé (ou chromatique absent)
            material = eal.load_asset(path)
            texture = eye_texture(folder, material, texture_name)
            if texture is None:
                unreal.log_warning(f"{path} : pas de texture trouvée, yeux fermés impossibles")
                continue
            closed_path = f"{path}_Closed"
            if eal.does_asset_exist(closed_path):
                instance = eal.load_asset(closed_path)
            else:
                instance = asset_tools.create_asset(f"{name}_Closed", f"{folder}/Materials", unreal.MaterialInstanceConstant,
                                                    unreal.MaterialInstanceConstantFactoryNew())
            # Matériau posé en cours de jeu : le streaming des textures ne le connaît pas et ne charge que la
            # plus petite version de la texture (yeux en gros carrés flous). On garde la texture entière en mémoire.
            if not texture.get_editor_property("never_stream"):
                texture.set_editor_property("never_stream", True)
                eal.save_loaded_asset(texture)
            mel.set_material_instance_parent(instance, eye_master_for(texture))
            mel.set_material_instance_texture_parameter_value(instance, "BaseTexture", texture)
            if mel.get_material_instance_texture_parameter_value(instance, "BaseTexture") != texture:
                unreal.log_warning(f"{closed_path} : texture {texture.get_name()} refusée par le matériau des yeux")
            mel.set_material_instance_vector_parameter_value(
                instance, "EyeUVOffset", unreal.LinearColor(CLOSED_EYE_OFFSET[0], CLOSED_EYE_OFFSET[1], 0.0, 0.0))
            eal.save_loaded_asset(instance)
            made += 1
            unreal.log(f"{closed_path} : yeux fermés ({texture.get_name()}, {sampler_type_for(texture).name})")
    if made == 0:
        unreal.log_warning("Aucun matériau d'yeux trouvé dans " + MODELS_DIR + " (voir eye_materials.json)")
    return made


def main():
    if not eal.does_directory_exist(FOLLOWER_DIR):
        eal.make_directory(FOLLOWER_DIR)
    make_eye_material()
    make_bubble_material()
    make_sparkle_material()
    made = make_closed_eyes()
    unreal.log(f"Pokémon qui suit : matériaux prêts ({made} matériau(x) d'yeux fermés)")


main()
