"""
Icônes des 10 boutons du volet du Pokédex (SCRUM-64), découpées dans la planche de L'amie.

Lancer : python Tools/Pokedex/make_pokedex_icons.py   (Python 3 avec Pillow et numpy)
Entrée : Tools/Pokedex/Source/planche_icones.png (2 rangées de 5 icônes sur fond noir)
Sortie : Tools/Pokedex/Assets/icons/T_PokedexIcon_<Menu>.png (256 × 256, fond transparent)
L'ordre suit les boutons du volet (de gauche à droite, puis de haut en bas) et EPokedexMenu dans le C++.
"""

import os
from collections import deque

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "Source", "planche_icones.png")
OUT_DIR = os.path.join(HERE, "Assets", "icons")

MENUS = ["Pokemon", "Pokedex", "Bag", "Radio", "Trainer", "Map", "Save", "Online", "Options", "Style"]
SIZE = 256
BLACK = 28          # en dessous : fond
EDGE_SOFTNESS = 22  # fondu du bord (niveaux)


def bands(mask, min_size=20):
    result, start = [], None
    for index, value in enumerate(list(mask) + [False]):
        if value and start is None:
            start = index
        elif not value and start is not None:
            if index - start > min_size:
                result.append((start, index))
            start = None
    return result


def remove_background(cell):
    """Fond noir relié aux bords -> transparent (les parties noires à l'intérieur des icônes restent)."""
    height, width, _ = cell.shape
    level = cell.max(axis=2).astype(float)
    background = np.zeros((height, width), bool)
    queue = deque()
    for y in range(height):
        for x in (0, width - 1):
            queue.append((y, x))
    for x in range(width):
        for y in (0, height - 1):
            queue.append((y, x))
    while queue:
        y, x = queue.popleft()
        if background[y, x] or level[y, x] > BLACK + EDGE_SOFTNESS:
            continue
        background[y, x] = True
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < height and 0 <= nx < width and not background[ny, nx]:
                queue.append((ny, nx))
    alpha = np.where(background, np.clip((level - BLACK) / EDGE_SOFTNESS, 0, 1), 1.0)
    return alpha


def main():
    image = np.asarray(Image.open(SOURCE).convert("RGB")).copy()
    # Filigrane « miro » en bas à droite : on l'efface (pixels gris seulement)
    corner = image[-70:, -100:].astype(int)
    grey = (corner.max(axis=2) - corner.min(axis=2)) < 30
    corner[grey] = 0
    image[-70:, -100:] = corner

    mask = image.max(axis=2) > 40
    rows = bands(mask.any(axis=1))
    os.makedirs(OUT_DIR, exist_ok=True)
    index = 0
    for top, bottom in rows:
        for left, right in bands(mask[top:bottom].any(axis=0)):
            sub = mask[top:bottom, left:right]
            ys = np.where(sub.any(axis=1))[0]
            y0, y1 = top + ys[0], top + ys[-1] + 1
            pad = 6
            y0, y1 = max(0, y0 - pad), min(image.shape[0], y1 + pad)
            x0, x1 = max(0, left - pad), min(image.shape[1], right + pad)
            cell = image[y0:y1, x0:x1]
            alpha = remove_background(cell)
            rgba = np.dstack([cell, (alpha * 255).astype(np.uint8)])
            icon = Image.fromarray(rgba, "RGBA")
            # Carré centré, marge de 6 %
            side = int(max(icon.size) * 1.06)
            square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
            square.paste(icon, ((side - icon.size[0]) // 2, (side - icon.size[1]) // 2))
            square = square.resize((SIZE, SIZE), Image.LANCZOS)
            name = MENUS[index] if index < len(MENUS) else "Extra%d" % index
            square.save(os.path.join(OUT_DIR, "T_PokedexIcon_%s.png" % name))
            index += 1
    print("%d icônes écrites dans %s" % (index, OUT_DIR))


if __name__ == "__main__":
    main()
