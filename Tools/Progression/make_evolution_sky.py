"""Fabrique la texture du fond étoilé de la scène d'évolution (demandé par L'amie le 2026-11-03).

    python Tools/Progression/make_evolution_sky.py      (numpy + pillow)

Inspiré de l'évolution de Pokémon Soleil et Lune (captures de L'amie) : salle bleue, piliers de lumière, gros
bokeh, éclats à quatre branches, lignes de lumière qui partent du centre.

Sorties (2048 × 2048, se répètent sans raccord), une donnée par canal, lues par M_EvolutionSky et
M_EvolutionFloor (import_evolution_backdrop.py).

T_EvolutionSky.png :
  - R : petites étoiles nettes, quelques-unes avec une croix de lumière ;
  - G : bokeh (grosses taches rondes et douces, comme des lumières floues) ;
  - B : phase de scintillement de chaque étoile ou tache (0-1, le matériau la décale dans le temps) ;
  - A : traînées de lumière fines (étoiles filantes), toutes dans la même direction.

T_EvolutionFx.png :
  - R : piliers de lumière verticaux (cœur fin et halo large), de hauteurs variées ;
  - G : éclats à quatre branches (étoiles « scintillement » de Soleil et Lune) ;
  - B : phase d'apparition de chaque éclat ;
  - A : lignes de vitesse, rangées en (angle, distance au centre) : le matériau les fait partir du centre.
Le tirage est fixe (graine) : relancer donne les mêmes images.
"""

import math
import os

import numpy as np
from PIL import Image

SIZE = 2048
SEED = 20261103
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Assets", "T_EvolutionSky.png")
OUT_FX = os.path.join(HERE, "Assets", "T_EvolutionFx.png")

STARS = 350            # petites étoiles
FLARED_STARS = 30      # étoiles avec une croix de lumière
BOKEH = 110            # taches rondes floues (nombreuses dans Soleil et Lune)
STREAKS = 36           # traînées
STREAK_ANGLE = math.radians(-28.0)   # vers le bas à droite


def splat(layer, phase, cx, cy, radius, value_at, phase_value):
    """Pose une forme centrée en (cx, cy) en se répétant sur les bords. value_at(dx, dy) -> intensité 0-1."""
    r = int(math.ceil(radius))
    xs = np.arange(-r, r + 1)
    dx, dy = np.meshgrid(xs.astype(np.float32), xs.astype(np.float32))
    dx = dx - (cx - math.floor(cx))
    dy = dy - (cy - math.floor(cy))
    value = value_at(dx, dy)
    ix = (np.arange(-r, r + 1) + int(math.floor(cx))) % SIZE
    iy = (np.arange(-r, r + 1) + int(math.floor(cy))) % SIZE
    region = np.ix_(iy, ix)
    current = layer[region]
    stronger = value > current
    layer[region] = np.maximum(current, value)
    if phase is not None:
        patch = phase[region]
        patch[stronger & (value > 0.02)] = phase_value
        phase[region] = patch


def make():
    rng = np.random.default_rng(SEED)
    stars = np.zeros((SIZE, SIZE), np.float32)
    bokeh = np.zeros((SIZE, SIZE), np.float32)
    phase = np.zeros((SIZE, SIZE), np.float32)
    streaks = np.zeros((SIZE, SIZE), np.float32)

    # Étoiles : gaussiennes de 1 à 2,6 px (le flou de profondeur de la scène les adoucit encore), plus de faibles que de fortes
    for _ in range(STARS):
        cx, cy = rng.uniform(0, SIZE, 2)
        sigma = rng.uniform(1.0, 2.6)
        peak = rng.uniform(0.25, 1.0) ** 1.5
        splat(stars, phase, cx, cy, sigma * 3.5,
              lambda dx, dy, s=sigma, p=peak: p * np.exp(-(dx * dx + dy * dy) / (2 * s * s)),
              rng.uniform())

    # Étoiles avec une croix (comme les éclats de la référence)
    for _ in range(FLARED_STARS):
        cx, cy = rng.uniform(0, SIZE, 2)
        length = rng.uniform(30, 90)
        peak = rng.uniform(0.7, 1.0)

        def flare(dx, dy, length=length, peak=peak):
            core = np.exp(-(dx * dx + dy * dy) / (2 * 3.0 ** 2))
            arm_x = np.exp(-(dy * dy) / (2 * 1.3 ** 2)) * np.clip(1 - np.abs(dx) / length, 0, 1) ** 2
            arm_y = np.exp(-(dx * dx) / (2 * 1.3 ** 2)) * np.clip(1 - np.abs(dy) / length, 0, 1) ** 2
            return peak * np.clip(core + 0.8 * np.maximum(arm_x, arm_y), 0, 1)

        splat(stars, phase, cx, cy, length + 2, flare, rng.uniform())

    # Bokeh : disques doux avec un bord un peu plus clair (lentille)
    for _ in range(BOKEH):
        cx, cy = rng.uniform(0, SIZE, 2)
        radius = rng.uniform(18, 95)
        peak = rng.uniform(0.25, 0.7)

        def disc(dx, dy, radius=radius, peak=peak):
            d = np.sqrt(dx * dx + dy * dy) / radius
            body = np.clip((1.0 - d) / 0.18, 0, 1)
            rim = np.exp(-((d - 0.88) ** 2) / (2 * 0.06 ** 2)) * 0.35
            return peak * np.clip(body * 0.75 + rim * (d < 1.05), 0, 1)

        splat(bokeh, phase, cx, cy, radius + 2, disc, rng.uniform())

    # Traînées : ligne fine, tête brillante, queue qui s'efface
    direction = np.array([math.cos(STREAK_ANGLE), math.sin(STREAK_ANGLE)])
    for _ in range(STREAKS):
        cx, cy = rng.uniform(0, SIZE, 2)
        length = rng.uniform(120, 360)
        peak = rng.uniform(0.4, 1.0)

        def streak(dx, dy, length=length, peak=peak):
            along = dx * direction[0] + dy * direction[1]          # 0 = tête, négatif = queue
            across = -dx * direction[1] + dy * direction[0]
            tail = np.clip(1 + along / length, 0, 1) * (along <= 1.5)
            return peak * np.exp(-(across * across) / (2 * 1.4 ** 2)) * tail ** 1.6

        splat(streaks, None, cx, cy, length + 3, streak, 0.0)

    image = np.stack([stars, bokeh, phase, streaks], axis=-1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    Image.fromarray((np.clip(image, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA").save(OUT, optimize=True)
    print(f"Texture du fond : {OUT}")


PILLARS = 12
SPARKLES = 28
SPEED_LINES = 70


def make_fx():
    rng = np.random.default_rng(SEED + 1)
    pillars = np.zeros((SIZE, SIZE), np.float32)
    sparkles = np.zeros((SIZE, SIZE), np.float32)
    phase = np.zeros((SIZE, SIZE), np.float32)
    lines = np.zeros((SIZE, SIZE), np.float32)
    ys = np.arange(SIZE, dtype=np.float32)
    xs = np.arange(SIZE, dtype=np.float32)

    def wrapped(delta):
        return (delta + SIZE / 2) % SIZE - SIZE / 2

    # Piliers : colonnes verticales, cœur net + halo, un segment de hauteur variable aux bouts adoucis
    for _ in range(PILLARS):
        cx = rng.uniform(0, SIZE)
        core = rng.uniform(1.5, 4.5)
        halo = rng.uniform(20, 70)
        peak = rng.uniform(0.5, 1.0)
        top = rng.uniform(0, SIZE)
        height = rng.uniform(0.35, 0.9) * SIZE
        dx = wrapped(xs - cx)
        across = np.exp(-(dx * dx) / (2 * core * core)) + 0.25 * np.exp(-(dx * dx) / (2 * halo * halo))
        along = (ys - top) % SIZE
        ends = np.clip(np.minimum(along, height - along) / (0.25 * height), 0, 1) * (along < height)
        pillars = np.maximum(pillars, peak * np.clip(np.outer(ends, across), 0, 1))

    # Éclats à quatre branches : astroïde pleine, bords doux, cœur brillant
    for _ in range(SPARKLES):
        cx, cy = rng.uniform(0, SIZE, 2)
        size = rng.uniform(25, 110)
        peak = rng.uniform(0.7, 1.0)

        def sparkle(dx, dy, size=size, peak=peak):
            shape = (np.abs(dx) / size) ** 0.5 + (np.abs(dy) / size) ** 0.5
            body = np.clip((1.0 - shape) / 0.25, 0, 1)
            core = np.exp(-(dx * dx + dy * dy) / (2 * (size * 0.12) ** 2))
            return peak * np.clip(body * 0.8 + core, 0, 1)

        splat(sparkles, phase, cx, cy, size + 2, sparkle, rng.uniform())

    # Lignes de vitesse : u = angle autour du centre, v = distance ; tirets fins dans le sens de v
    for _ in range(SPEED_LINES):
        cx = rng.uniform(0, SIZE)
        width = rng.uniform(1.0, 3.0)
        start = rng.uniform(0, SIZE)
        length = rng.uniform(120, 600)
        peak = rng.uniform(0.4, 1.0)
        dx = wrapped(xs - cx)
        across = np.exp(-(dx * dx) / (2 * width * width))
        along = (ys - start) % SIZE
        dash = np.clip(along / (0.3 * length), 0, 1) * np.clip((length - along) / (0.15 * length), 0, 1) * (along < length)
        lines = np.maximum(lines, peak * np.outer(dash, across))

    image = np.stack([pillars, sparkles, phase, lines], axis=-1)
    Image.fromarray((np.clip(image, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA").save(OUT_FX, optimize=True)
    print(f"Texture des effets : {OUT_FX}")


if __name__ == "__main__":
    make()
    make_fx()
