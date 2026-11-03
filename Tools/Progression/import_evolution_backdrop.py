"""Fond étoilé animé de la scène d'évolution (demandé par L'amie le 2026-11-03) : à lancer dans Unreal.

Outils > Exécuter un script Python... > choisir ce fichier (n'importe quelle map ouverte). Relançable : refait
les deux matériaux à chaque fois (les réglages changés à la main dans ces matériaux sont alors perdus, les noter).

Crée dans /Game/Progression :
  - T_EvolutionSky : étoiles, bokeh, phase de scintillement, traînées (Tools/Progression/Assets/T_EvolutionSky.png,
    fabriquée par make_evolution_sky.py) ;
  - M_EvolutionSky : le ciel derrière le Pokémon. Dégradé bleu (foncé en haut, lumineux à l'horizon), grande
    lueur au centre de l'image, étoiles qui scintillent et glissent, bokeh qui dérive, traînées de lumière ;
  - M_EvolutionFloor : le sol lumineux sous le Pokémon (lueur cyan au centre, même bleu que le ciel à l'horizon).
Utilisés par UEvolutionBackdropComponent (Source/PokemonNewAge/Progression/EvolutionBackdrop.h), un composant
de AEvolutionScene. Sans ces matériaux, la scène garde l'ancien fond (le terrain).

Réglages (ouvrir le matériau, cliquer sur le paramètre, changer « Default Value », Appliquer) : voir
docs/PROGRESSION.md, section « Fond étoilé ».
"""

import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
TEXTURE_FILE = os.path.join(HERE, "Assets", "T_EvolutionSky.png")
CONTENT_DIR = "/Game/Progression"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Couleurs (linéaires) : bleu profond en haut, bleu vif à l'horizon, cyan au centre, comme la référence de L'amie
HORIZON = unreal.LinearColor(0.01, 0.1, 0.55, 1.0)
TOP = unreal.LinearColor(0.0, 0.008, 0.08, 1.0)
CENTER = unreal.LinearColor(0.2, 0.7, 2.0, 1.0)
STARS = unreal.LinearColor(0.85, 0.92, 1.0, 1.0)
BOKEH = unreal.LinearColor(0.2, 0.5, 1.0, 1.0)
STREAKS = unreal.LinearColor(1.0, 0.7, 0.95, 1.0)
FLOOR_GLOW = unreal.LinearColor(0.1, 0.6, 1.6, 1.0)


def import_texture():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", TEXTURE_FILE)
    task.set_editor_property("destination_path", CONTENT_DIR)
    task.set_editor_property("destination_name", "T_EvolutionSky")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    texture = eal.load_asset(CONTENT_DIR + "/T_EvolutionSky")
    # Données (pas des couleurs) : pas de sRGB ; toujours en pleine résolution (gros plan, apparaît d'un coup)
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("never_stream", True)
    eal.save_loaded_asset(texture)
    return texture


class Graph:
    """Petit assistant pour écrire un graphe de matériau en Python."""

    def __init__(self, name):
        path = f"{CONTENT_DIR}/{name}"
        if eal.does_asset_exist(path):
            self.material = eal.load_asset(path)
            mel.delete_all_material_expressions(self.material)
        else:
            self.material = asset_tools.create_asset(name, CONTENT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        self.path = path
        self.x = -2400
        self.y = 0
        self.material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        self.material.set_editor_property("two_sided", True)

    def node(self, kind, **properties):
        expression = mel.create_material_expression(self.material, kind, self.x, self.y)
        self.y += 120
        if self.y > 2400:
            self.y = 0
            self.x += 300
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def scalar(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

    def color(self, name, value):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=value)

    def link(self, source, output, target, pin):
        mel.connect_material_expressions(source, output, target, pin)

    def op(self, kind, a, b=None, a_out="", b_out="", **properties):
        expression = self.node(kind, **properties)
        self.link(a, a_out, expression, "A")
        if b is not None:
            self.link(b, b_out, expression, "B")
        return expression

    def mul(self, a, b=None, a_out="", b_out="", **properties):
        return self.op(unreal.MaterialExpressionMultiply, a, b, a_out, b_out, **properties)

    def add(self, a, b=None, a_out="", b_out="", **properties):
        return self.op(unreal.MaterialExpressionAdd, a, b, a_out, b_out, **properties)

    def lerp(self, a, b, alpha, a_out="", b_out="", alpha_out="", **properties):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, **properties)
        if a is not None:
            self.link(a, a_out, expression, "A")
        if b is not None:
            self.link(b, b_out, expression, "B")
        self.link(alpha, alpha_out, expression, "Alpha")
        return expression

    def single(self, kind, source, output="", **properties):
        expression = self.node(kind, **properties)
        self.link(source, output, expression, "")
        return expression

    def sample(self, texture, uv, speed_x, speed_y):
        """Texture lue en uv (déjà répétée), qui glisse à (speed_x, speed_y) par seconde."""
        panner = self.node(unreal.MaterialExpressionPanner, speed_x=speed_x, speed_y=speed_y)
        self.link(uv, "", panner, "Coordinate")
        sample = self.node(unreal.MaterialExpressionTextureSample, texture=texture,
                           sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        self.link(panner, "", sample, "UVs")
        return sample

    def twinkle(self, sample, speed, low):
        """low..1 selon sin(temps × speed + phase de la texture (B)) : chaque point scintille à son rythme."""
        time = self.node(unreal.MaterialExpressionTime)
        timed = self.mul(time, speed)
        phase = self.add(timed, sample, b_out="B")
        wave = self.single(unreal.MaterialExpressionSine, phase, period=1.0)
        unit = self.node(unreal.MaterialExpressionMultiply, const_b=0.5)
        self.link(wave, "", unit, "A")
        unit = self.add(unit, const_b=0.5)
        return self.lerp(None, None, unit, const_a=low, const_b=1.0)

    def power(self, source, exponent):
        expression = self.node(unreal.MaterialExpressionPower, const_exponent=exponent)
        self.link(source, "", expression, "Base")
        return expression

    def uv(self, tiling):
        coords = self.node(unreal.MaterialExpressionTextureCoordinate)
        return self.mul(coords, tiling)

    def finish(self, emissive):
        mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(self.material)
        eal.save_asset(self.path)
        return self.material


def floor_height(graph):
    """Hauteur du point au-dessus du sol (SolZ, posé par le C++ à chaque image), en cm."""
    position = graph.node(unreal.MaterialExpressionWorldPosition)
    z = graph.single(unreal.MaterialExpressionComponentMask, position, r=False, g=False, b=True, a=False)
    floor_z = graph.scalar("SolZ", 0.0)
    return graph.op(unreal.MaterialExpressionSubtract, z, floor_z), position


def center_glow(graph):
    """Lueur ronde au centre de l'écran (position à l'écran, pas sur l'objet : ciel et sol la partagent)."""
    screen = graph.node(unreal.MaterialExpressionScreenPosition)
    middle = graph.node(unreal.MaterialExpressionConstant2Vector, r=0.5, g=0.5)
    distance = graph.op(unreal.MaterialExpressionDistance, screen, middle, a_out="ViewportUV")
    falloff = graph.single(unreal.MaterialExpressionOneMinus, graph.mul(distance, const_b=1.6))
    falloff = graph.power(graph.single(unreal.MaterialExpressionSaturate, falloff), 3.0)
    return graph.mul(graph.mul(falloff, graph.scalar("LueurCentre", 1.0)), graph.color("CouleurCentre", CENTER))


def make_sky(texture):
    g = Graph("M_EvolutionSky")
    brightness = g.scalar("Luminosite", 1.0)

    # Dégradé : CouleurHorizon au ras du sol -> CouleurHaut à HauteurDegrade au-dessus
    height, _position = floor_height(g)
    gradient_height = g.scalar("HauteurDegrade", 900.0)
    t = g.single(unreal.MaterialExpressionSaturate, g.op(unreal.MaterialExpressionDivide, height, gradient_height))
    t = g.power(t, 0.7)
    sky = g.lerp(g.color("CouleurHorizon", HORIZON), g.color("CouleurHaut", TOP), t)

    # Lueur au centre de l'image : l'éclat de l'évolution (la même sur le sol, pas de cassure à l'horizon)
    sky = g.add(sky, center_glow(g))

    # Étoiles (R) : glissent doucement, scintillent chacune à son rythme
    tiling = g.scalar("Repetition", 1.2)
    stars = g.sample(texture, g.uv(tiling), 0.004, 0.0)
    twinkle = g.twinkle(stars, g.scalar("VitesseScintillement", 0.5), 0.2)
    star_light = g.mul(g.mul(stars, twinkle, a_out="R"), g.scalar("IntensiteEtoiles", 4.0))
    sky = g.add(sky, g.mul(star_light, g.color("CouleurEtoiles", STARS)))

    # Bokeh (G) : plus gros, dérive lente, pulse doucement
    bokeh_uv = g.mul(g.node(unreal.MaterialExpressionTextureCoordinate), const_b=0.8)
    bokeh = g.sample(texture, bokeh_uv, 0.0, -0.006)
    pulse = g.twinkle(bokeh, g.scalar("VitesseBokeh", 0.2), 0.55)
    bokeh_light = g.mul(g.mul(bokeh, pulse, a_out="G"), g.scalar("IntensiteBokeh", 1.0))
    sky = g.add(sky, g.mul(bokeh_light, g.color("CouleurBokeh", BOKEH)))

    # Traînées (A) : filent le long de leur propre direction (même angle que dans la texture)
    streak_uv = g.node(unreal.MaterialExpressionTextureCoordinate)
    speed = 0.04
    streaks = g.sample(texture, streak_uv, -0.883 * speed, 0.469 * speed)
    streak_light = g.mul(g.mul(streaks, g.scalar("IntensiteTrainees", 1.5), a_out="A"),
                         g.color("CouleurTrainees", STREAKS))
    sky = g.add(sky, streak_light)

    return g.finish(g.mul(sky, brightness))


def make_floor(texture):
    g = Graph("M_EvolutionFloor")
    brightness = g.scalar("Luminosite", 1.0)
    _height, position = floor_height(g)

    # Lueur centrée sous le Pokémon (CentreSol, RayonSol posés par le C++), CouleurHorizon au loin
    xy = g.single(unreal.MaterialExpressionComponentMask, position, r=True, g=True, b=False, a=False)
    center = g.single(unreal.MaterialExpressionComponentMask, g.color("CentreSol", unreal.LinearColor(0, 0, 0, 0)),
                      r=True, g=True, b=False, a=False)
    distance = g.op(unreal.MaterialExpressionDistance, xy, center)
    ratio = g.op(unreal.MaterialExpressionDivide, distance, g.scalar("RayonSol", 600.0))
    glow = g.single(unreal.MaterialExpressionOneMinus, g.single(unreal.MaterialExpressionSaturate, ratio))
    glow = g.power(glow, 2.2)
    glow = g.mul(glow, g.scalar("IntensiteSol", 0.9))
    floor = g.lerp(g.color("CouleurHorizon", HORIZON), g.color("CouleurSol", FLOOR_GLOW),
                   g.single(unreal.MaterialExpressionSaturate, glow))

    # Reflets de bokeh au sol, en coordonnées du monde (un motif tous les 4 m), très doux
    world_uv = g.mul(xy, const_b=1.0 / 400.0)
    bokeh = g.sample(texture, world_uv, 0.0, 0.01)
    reflection = g.mul(g.mul(bokeh, g.scalar("BokehSol", 0.3), a_out="G"), g.color("CouleurBokeh", BOKEH))
    floor = g.add(floor, reflection)
    floor = g.add(floor, center_glow(g))

    return g.finish(g.mul(floor, brightness))


def main():
    if not os.path.exists(TEXTURE_FILE):
        unreal.log_error(f"Texture introuvable : {TEXTURE_FILE} (lancer make_evolution_sky.py)")
        return
    texture = import_texture()
    make_sky(texture)
    make_floor(texture)
    unreal.log(f"Fond de l'évolution : T_EvolutionSky, M_EvolutionSky, M_EvolutionFloor dans {CONTENT_DIR}.")


main()
