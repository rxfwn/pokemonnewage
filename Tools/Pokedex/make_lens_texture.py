"""Texture de l'objectif du Pokédex (T_PokedexLens.png, 1024 × 1024) : objectif d'appareil photo réaliste.

Verre sombre bleu nuit, bague noire striée sur le bord, reflets des lentilles internes en anneaux fins,
traitement antireflet bleu-violet et vert, pupille noire au centre, petit reflet doux en haut à gauche.
Lancer avec Python 3 + numpy + Pillow : python Tools/Pokedex/make_lens_texture.py
"""

import os

import numpy as np
from PIL import Image

SIZE = 1024
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Assets", "lens", "T_PokedexLens.png")


def ring(r, center, width):
    return np.exp(-((r - center) / width) ** 2)


def blob(x, y, cx, cy, sx, sy):
    return np.exp(-(((x - cx) / sx) ** 2 + ((y - cy) / sy) ** 2))


def main():
    axis = (np.arange(SIZE) + 0.5) / SIZE * 2.0 - 1.0
    x, y = np.meshgrid(axis, axis)  # y vers le bas
    r = np.sqrt(x * x + y * y)
    angle = np.arctan2(y, x)

    # Verre : bleu nuit qui s'assombrit vers le centre
    color = np.zeros((SIZE, SIZE, 3))
    glass = np.array([0.010, 0.016, 0.045])
    color += glass * (0.55 + 0.45 * np.clip(r / 0.85, 0, 1))[..., None]

    # Pupille (diaphragme) noire au centre, bord doux
    pupil = np.clip((r - 0.16) / 0.05, 0, 1)
    color *= (0.08 + 0.92 * pupil)[..., None]

    # Lentilles internes : anneaux fins qui renvoient un peu de lumière, plus visibles en haut à gauche
    side = 0.55 + 0.45 * np.clip(-(x + y) / 1.4, -1, 1)
    for center, strength, tint in ((0.30, 0.03, (0.35, 0.45, 0.9)), (0.48, 0.04, (0.55, 0.4, 0.95)),
                                   (0.64, 0.03, (0.3, 0.75, 0.8)), (0.78, 0.05, (0.6, 0.65, 0.9))):
        color += (np.array(tint) * strength) * (ring(r, center, 0.012) * side)[..., None]

    # Traitement antireflet : croissant violet en haut à gauche, vert-bleu en bas à droite
    color += np.array([0.32, 0.16, 0.55]) * (blob(x, y, -0.35, -0.38, 0.45, 0.28) * (r < 0.86))[..., None] * 0.22
    color += np.array([0.05, 0.38, 0.32]) * (blob(x, y, 0.42, 0.40, 0.32, 0.22) * (r < 0.86))[..., None] * 0.12

    # Bague noire striée (bord de l'objectif) entre 0.86 et 1
    barrel = (r >= 0.86) & (r <= 1.0)
    grooves = 0.5 + 0.5 * np.sin(r * 420.0)
    knurl = 0.5 + 0.5 * np.sin(angle * 180.0)
    barrel_color = 0.006 + 0.008 * grooves * (0.7 + 0.3 * knurl) + 0.04 * ring(r, 0.865, 0.006)
    color[barrel] = np.stack([barrel_color, barrel_color, barrel_color * 1.1], axis=-1)[barrel]

    # Petit reflet doux de fenêtre en haut à gauche (le moteur ajoute les vrais reflets des lumières)
    color += 0.45 * (blob(x, y, -0.33, -0.36, 0.09, 0.055) * (r < 0.86))[..., None]
    color += 0.05 * (blob(x, y, 0.28, 0.3, 0.05, 0.035) * (r < 0.86))[..., None]

    # Hors du disque : bague (au cas où l'UV déborde)
    color[r > 1.0] = 0.008

    srgb = np.where(color <= 0.0031308, color * 12.92, 1.055 * np.power(np.clip(color, 0, None), 1 / 2.4) - 0.055)
    image = Image.fromarray((np.clip(srgb, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    image.save(OUT)
    print("Texture de l'objectif :", OUT)


if __name__ == "__main__":
    main()
