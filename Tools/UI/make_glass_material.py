"""Fait le matériau de verre de l'interface, M_UI_Glass (script Python d'éditeur).

À lancer dans l'éditeur, Outils > Exécuter un script Python (Tools > Execute Python Script)
-> Tools/UI/make_glass_material.py. Content/Python/init_unreal.py le lance tout seul à l'ouverture de l'éditeur
quand le matériau manque ou date d'une version plus ancienne.

Reprend l'effet « Glass » de Figma des maquettes de L'amie (2026-11-01) :
  - remplissage en dégradé linéaire : #D9D9D9 à 39 % -> #FFFFFF à 13 % ;
  - contour intérieur de 2 px en dégradé : blanc à 1 % -> blanc à 17 % ;
  - opacité du calque 71 % ;
  - coins arrondis (16 px) et reflet de lumière sur les bords haut et bas (lumière de Figma) ;
  - le flou du décor (Frost) n'est pas dans le matériau : c'est le widget Background Blur
    (Source/PokemonNewAge/UI/GlassPanel.cpp le met sous chaque panneau).
Tout est en paramètres : une instance de matériau (clic droit > Créer une instance de matériau) permet de régler
à l'œil sans toucher au script. Le C++ règle lui-même CornerRadius et DpiScale sur chaque panneau.
"""

import unreal

MATERIAL_DIR = "/Game/UI/Materials"
MATERIAL_NAME = "M_UI_Glass"
VERSION = "2"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Coins arrondis (SDF de rectangle arrondi), dégradés et contour, en unités UMG (pixels de la maquette 1920 x 1080).
# La taille du panneau vient des dérivées des UV (pixels de la fenêtre), ramenée en unités UMG par DpiScale.
GLASS_HLSL = """
float2 PerPixel = float2(max(abs(ddx(UV.x)), abs(ddy(UV.x))), max(abs(ddx(UV.y)), abs(ddy(UV.y))));
float Scale = max(DpiScale, 0.01);
float2 Size = 1.0 / max(PerPixel, 1e-6) / Scale;
float Pixel = 1.0 / Scale;

float2 P = (UV - 0.5) * Size;
float R = min(CornerRadius, 0.5 * min(Size.x, Size.y));
float2 Q = abs(P) - (0.5 * Size - R);
float Dist = length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - R;
float Inside = saturate(0.5 - Dist / Pixel);
float Rim = Inside * saturate(0.5 + (Dist + StrokeWidth) / Pixel);

// Dégradés comme le CSS de Figma : angle en degrés (90 = de gauche à droite, 180 = de haut en bas),
// arrêts GradientStart / GradientEnd en fraction de la ligne du dégradé (0,4 % et 64,91 % -> 0,004 et 0,6491)
float A = radians(GradientAngle);
float2 Dir = float2(sin(A), -cos(A));
float HalfLine = max(0.5 * (abs(Size.x * Dir.x) + abs(Size.y * Dir.y)), 1e-4);
float T = dot(P, Dir) / HalfLine * 0.5 + 0.5;
T = saturate((T - GradientStart) / max(GradientEnd - GradientStart, 1e-4));

// Reflet : plus fort là où le bord fait face à la lumière (et à l'opposé, comme dans Figma)
float2 Normal = (Q.x > 0.0 && Q.y > 0.0) ? normalize(Q) * sign(P) : (Q.x > Q.y ? float2(sign(P.x), 0.0) : float2(0.0, sign(P.y)));
float L = radians(LightAngle);
float Facing = abs(dot(Normal, float2(sin(L), -cos(L))));
float Glint = LightIntensity * Facing * Facing;

float FillA = lerp(FillOpacityStart, FillOpacityEnd, T) * Inside;
float3 FillC = lerp(FillColorStart, FillColorEnd, T);
float StrokeA = saturate(lerp(StrokeOpacityStart, StrokeOpacityEnd, T) + Glint) * Rim;
float OutA = StrokeA + FillA * (1.0 - StrokeA);
float3 OutC = (StrokeColor * StrokeA + FillC * FillA * (1.0 - StrokeA)) / max(OutA, 1e-4);
return float4(OutC, OutA * LayerOpacity);
"""

# (nom, valeur par défaut) : scalaires
SCALARS = [
    ("DpiScale", 1.0),
    ("CornerRadius", 16.0),
    ("StrokeWidth", 2.0),
    ("GradientAngle", 180.0),
    ("GradientStart", 0.0),
    ("GradientEnd", 1.0),
    ("FillOpacityStart", 0.39),
    ("FillOpacityEnd", 0.13),
    ("StrokeOpacityStart", 0.01),
    ("StrokeOpacityEnd", 0.17),
    ("LayerOpacity", 0.71),
    ("LightAngle", 0.0),
    ("LightIntensity", 0.45),
]

# Couleurs en linéaire (#D9D9D9 = 0,693 ; #FFFFFF = 1)
VECTORS = [
    ("FillColorStart", (0.693, 0.693, 0.693, 1.0)),
    ("FillColorEnd", (1.0, 1.0, 1.0, 1.0)),
    ("StrokeColor", (1.0, 1.0, 1.0, 1.0)),
]


def _mask(material, channels, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, x, y)
    for channel in "rgba":
        node.set_editor_property(channel, channel in channels)
    return node


def make_glass_material():
    path = MATERIAL_DIR + "/" + MATERIAL_NAME
    if eal.does_asset_exist(path):
        material = eal.load_asset(path)
        mel.delete_all_material_expressions(material)
    else:
        material = asset_tools.create_asset(MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property("code", GLASS_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description", "Verre")

    names = ["UV"] + [name for name, _ in SCALARS] + [name for name, _ in VECTORS]
    inputs = []
    for name in names:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    custom.set_editor_property("inputs", inputs)

    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -800, -300)
    mel.connect_material_expressions(uv, "", custom, "UV")
    y = -200
    for name, value in SCALARS:
        node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        mel.connect_material_expressions(node, "", custom, name)
        y += 90
    for name, value in VECTORS:
        node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -800, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(*value))
        # Sortie par défaut = RGB (float3)
        mel.connect_material_expressions(node, "", custom, name)
        y += 200

    color = _mask(material, "rgb", -150, -60)
    opacity = _mask(material, "a", -150, 80)
    mel.connect_material_expressions(custom, "", color, "")
    mel.connect_material_expressions(custom, "", opacity, "")
    # Domaine Interface : « Couleur finale » = Émissive
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(material)
    # Lue par Content/Python/init_unreal.py : une version plus ancienne refait le matériau à l'ouverture d'Unreal
    eal.set_metadata_tag(material, "GlassVersion", VERSION)
    eal.save_asset(path)
    unreal.log("Matériau de verre prêt : %s (version %s)." % (path, VERSION))
    return material


def glass_material_up_to_date():
    path = MATERIAL_DIR + "/" + MATERIAL_NAME
    if not eal.does_asset_exist(path):
        return False
    return eal.get_metadata_tag(eal.load_asset(path), "GlassVersion") == VERSION


# Lancé seul depuis l'éditeur (pas quand init_unreal.py le charge sous le nom make_glass_material)
if __name__ != "make_glass_material":
    make_glass_material()
