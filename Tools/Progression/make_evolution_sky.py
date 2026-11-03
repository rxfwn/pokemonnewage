"""Fabrique la texture du fond étoilé de la scène d'évolution (demandé par L'amie le 2026-11-03).

    python Tools/Progression/make_evolution_sky.py      (numpy + pillow)

Sortie : Tools/Progression/Assets/T_EvolutionSky.png (2048 × 2048, se répète sans raccord). Une donnée par canal,
lue par M_EvolutionSky et M_EvolutionFloor (import_evolution_backdrop.py) :
  - R : petites étoiles nettes, quelques-unes avec une croix de lumière ;
  - G : bokeh (grosses taches rondes et douces, comme des lumières floues) ;
  - B : phase de scintillement de chaque étoile ou tache (0-1, le matériau la décale dans le temps) ;
  - A : traînées de lumière fines (étoiles filantes), toutes dans la même direction.
Le tirage est fixe (graine) : relancer donne la même image.
"""

import math
import os

import numpy as np
from PIL import Image

SIZE = 2048
SEED = 20261103
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Assets", "T_EvolutionSky.png")

STARS = 700            # petites étoiles
FLARED_STARS = 45      # étoiles avec une croix de lumière
BOKEH = 70             # taches rondes floues
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
        radius = rng.uniform(14, 60)
        peak = rng.uniform(0.25, 0.8)

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


if __name__ == "__main__":
    make()
