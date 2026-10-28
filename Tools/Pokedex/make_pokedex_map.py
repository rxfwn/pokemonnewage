"""
Carte de Kanto du menu Carte du Pokédex (SCRUM-64), dessinée depuis le vrai relief du jeu.

Lancer : python Tools/Pokedex/make_pokedex_map.py   (Python 3 avec Pillow et numpy)
Entrées : Tools/Blockout/kanto_v1_heightmap.png et kanto_v1_layout.py (villes, routes, grottes)
Sorties : Tools/Pokedex/Assets/map/T_PokedexMap.png (1024 × 1024, nord en haut)
          Tools/Pokedex/Assets/map/pokedex_map.json (lieux : nom, type, position u v de 0 à 1, rayon)
À relancer quand la heightmap change. Le C++ place le joueur avec la position du Landscape :
u = (Y - Y0) / taille, v = 1 - (X - X0) / taille (X = nord, Y = est, comme dans kanto_v1_layout.py).
"""

import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "Blockout"))
import kanto_v1_layout as layout  # noqa: E402

OUT_DIR = os.path.join(HERE, "Assets", "map")
SIZE = 1024


def load_heights():
    image = Image.open(layout.HEIGHTMAP_PATH)
    values = np.asarray(image, dtype=np.float32)
    if values.shape != (layout.HEIGHTMAP_SIZE, layout.HEIGHTMAP_SIZE):
        raise SystemExit("Heightmap inattendue : " + str(values.shape))
    return (values - 32768.0) / layout.HEIGHT_UNITS_PER_METER


def to_uv(x, y):
    """Mètres (x nord, y est) -> position sur la carte (u vers l'est, v vers le sud)."""
    return y / layout.WORLD_SIZE, 1.0 - x / layout.WORLD_SIZE


def colorize(height):
    land = np.array([
        (0.0, (0.36, 0.58, 0.30)),
        (40.0, (0.45, 0.64, 0.33)),
        (110.0, (0.62, 0.66, 0.38)),
        (180.0, (0.62, 0.52, 0.36)),
        (280.0, (0.55, 0.45, 0.36)),
        (400.0, (0.80, 0.79, 0.76)),
    ], dtype=object)
    levels = np.array([l for l, _ in land], dtype=np.float32)
    colors = np.array([c for _, c in land], dtype=np.float32)
    rgb = np.zeros(height.shape + (3,), np.float32)
    for channel in range(3):
        rgb[..., channel] = np.interp(height, levels, colors[:, channel])
    sea = height < layout.SEA_LEVEL + 0.5
    depth = np.clip(-height / 30.0, 0.0, 1.0)[..., None]
    shallow, deep = np.array([0.36, 0.66, 0.86]), np.array([0.12, 0.32, 0.62])
    rgb[sea] = (shallow * (1 - depth) + deep * depth)[sea]
    return rgb, sea


def main():
    heights = load_heights()
    # Nord en haut : colonne de la heightmap = x (nord), ligne = y (est)
    display = heights.T[::-1, :]
    small = np.asarray(Image.fromarray(display).resize((SIZE, SIZE), Image.BILINEAR), dtype=np.float32)

    rgb, sea = colorize(small)
    # Ombrage du relief (lumière du nord-ouest)
    meters_per_pixel = layout.WORLD_SIZE / SIZE
    gy, gx = np.gradient(small, meters_per_pixel)
    slope = np.arctan(np.hypot(gx, gy) * 1.5)
    aspect = np.arctan2(-gy, gx)
    sun_azimuth, sun_altitude = math.radians(315), math.radians(45)
    shade = (math.sin(sun_altitude) * np.cos(slope)
             + math.cos(sun_altitude) * np.sin(slope) * np.cos(sun_azimuth - aspect))
    shade = np.clip(shade, 0, 1)[..., None]
    rgb = np.where(sea[..., None], rgb, rgb * (0.55 + 0.6 * shade))
    # Côtes plus claires
    coast = np.asarray(Image.fromarray((sea * 255).astype(np.uint8)).filter(ImageFilter.FIND_EDGES)) > 0
    rgb[coast] = (0.93, 0.89, 0.72)

    image = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8), "RGB")
    draw = ImageDraw.Draw(image)

    def pixel(x, y):
        u, v = to_uv(x, y)
        return u * SIZE, v * SIZE

    places = []
    for name, points in layout.PATHS:
        draw.line([pixel(x, y) for x, y in points], fill=(236, 214, 160), width=3, joint="curve")
        if name.startswith("Chemin Route"):
            middle = points[len(points) // 2]
            u, v = to_uv(*middle)
            length = sum(math.dist(a, b) for a, b in zip(points, points[1:]))
            places.append({"name": name.replace("Chemin ", ""), "kind": "route", "u": u, "v": v,
                           "radius": max(60.0, length / 2) / layout.WORLD_SIZE})
    for name, column, row, half_east, half_north, _ in layout.AREAS:
        x, y = layout.from_frlg(column, row)
        u, v = to_uv(x, y)
        if name == "Forêt de Jade":
            places.append({"name": name, "kind": "place", "u": u, "v": v,
                           "radius": max(half_east, half_north) / layout.WORLD_SIZE})
            continue
        half = (max(half_east, half_north) + 25) / layout.WORLD_SIZE * SIZE
        px, py = u * SIZE, v * SIZE
        draw.rounded_rectangle((px - half, py - half, px + half, py + half), radius=half * 0.35,
                               fill=(236, 92, 76), outline=(255, 246, 230), width=2)
        places.append({"name": name, "kind": "town", "u": u, "v": v,
                       "radius": (max(half_east, half_north) + 40) / layout.WORLD_SIZE})
    for name, column, row, _ in layout.CAVES:
        x, y = layout.from_frlg(column, row)
        px, py = pixel(x, y)
        draw.ellipse((px - 4, py - 4, px + 4, py + 4), fill=(70, 52, 40), outline=(240, 230, 210))
        base = name.split(" (")[0]
        if not any(p["name"] == base for p in places):
            u, v = to_uv(x, y)
            places.append({"name": base, "kind": "place", "u": u, "v": v, "radius": 80.0 / layout.WORLD_SIZE})

    os.makedirs(OUT_DIR, exist_ok=True)
    image.save(os.path.join(OUT_DIR, "T_PokedexMap.png"))
    with open(os.path.join(OUT_DIR, "pokedex_map.json"), "w", encoding="utf-8") as handle:
        json.dump({"heightmap_size": layout.HEIGHTMAP_SIZE, "places": places}, handle, ensure_ascii=False, indent=1)
    print("Carte écrite :", len(places), "lieux")


if __name__ == "__main__":
    main()
