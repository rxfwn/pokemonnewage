"""Décalque le réseau des routes et des villes depuis la carte de Rouge Feu / Vert Feuille.

Entrée : l'image de toute la carte de Kanto (FRLG, tuiles assemblées, fond en damier), par
exemple celle de yaNaNco (https://imgur.com/gallery/FQzbkZM), passée en argument.
Sortie : kanto_frlg.bin, une grille de cases (1 case = 8 pixels de l'image, une demi-tuile)
avec 0 = vide (hors des routes : deviendra montagne, falaise ou mer), 1 = sol où l'on marche,
2 = mer, 3 = lac ou rivière.

Lancement : pip install numpy pillow, puis
python Tools/Blockout/make_frlg_mask.py chemin/vers/carte_frlg.png
"""

import json
import os
import struct
import sys
import zlib
from collections import deque

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
CELL = 8


def components(mask):
    """Composantes connexes (4 voisins) : liste de listes de (ligne, colonne)."""
    seen = np.zeros(mask.shape, bool)
    rows, cols = mask.shape
    result = []
    for r0 in range(rows):
        for c0 in range(cols):
            if mask[r0, c0] and not seen[r0, c0]:
                seen[r0, c0] = True
                queue, cells = deque([(r0, c0)]), []
                while queue:
                    r, c = queue.popleft()
                    cells.append((r, c))
                    for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        rr, cc = r + dr, c + dc
                        if 0 <= rr < rows and 0 <= cc < cols and mask[rr, cc] and not seen[rr, cc]:
                            seen[rr, cc] = True
                            queue.append((rr, cc))
                result.append(cells)
    return result


def main(path):
    image = np.asarray(Image.open(path).convert("RGB")).astype(int)
    r, g, b = image[..., 0], image[..., 1], image[..., 2]
    spread = image.max(axis=2) - image.min(axis=2)
    # Le fond en damier : gris très foncé et gris moyen, sans couleur.
    checker = (spread < 8) & ((np.abs(r - 18) < 6) | (np.abs(r - 78) < 8))
    water = (b > r + 40) & (b > g + 5)
    rows, cols = image.shape[0] // CELL, image.shape[1] // CELL

    def blocks(mask):
        return mask[:rows * CELL, :cols * CELL].reshape(rows, CELL, cols, CELL).mean(axis=(1, 3))

    void = blocks(checker) > 0.6
    grid = np.where(void, 0, 1).astype(np.uint8)
    grid[(~void) & (blocks(water) > 0.35)] = 2

    # Petites taches isolées (la légende de l'image) : vide. On garde Cramois'Île.
    for cells in components(grid > 0):
        if len(cells) < 4000 and (770, 145) not in cells:
            for rr, cc in cells:
                grid[rr, cc] = 0
    # Petites taches bleues dans les villes (toits) : du sol. Les autres eaux fermées
    # (sans contact avec le vide, donc entourées de terre) : lacs et rivières.
    for cells in components(grid == 2):
        if len(cells) < 25:
            for rr, cc in cells:
                grid[rr, cc] = 1
            continue
        touches_void = any(
            0 <= rr + dr < rows and 0 <= cc + dc < cols and grid[rr + dr, cc + dc] == 0
            for rr, cc in cells[::3] for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        if not touches_void and len(cells) < 1500:
            for rr, cc in cells:
                grid[rr, cc] = 3
    # Pas de mer collée à Bourg Palette : la Route 21 commence plus au sud.
    grid[560:650, 115:175][grid[560:650, 115:175] == 2] = 1

    export_tall_grass(image, rows, cols)

    header = struct.pack("<4sHH", b"FRLG", cols, rows)
    with open(os.path.join(HERE, "kanto_frlg.bin"), "wb") as handle:
        handle.write(zlib.compress(header + grid.tobytes(), 9))
    colors = np.array([(0, 0, 0), (120, 190, 90), (40, 90, 200), (90, 160, 230)], np.uint8)
    Image.fromarray(colors[grid]).save(os.path.join(HERE, "kanto_frlg_apercu.png"))
    print(f"{cols} × {rows} cases ; sol {np.mean(grid == 1):.0%}, mer {np.mean(grid == 2):.0%}, "
          f"lacs {np.mean(grid == 3):.1%}")


def export_tall_grass(image, rows, cols):
    """Hautes herbes : tuiles de 16 px qui mêlent l'herbe claire et les brins vert foncé
    (sans le reflet clair des arbres). Certaines zones de l'image sont voilées de gris :
    on cherche aussi les couleurs voilées. Sortie : kanto_herbes.json, des rectangles en
    cases (colonne, ligne, largeur, hauteur)."""
    def share(tile, colors):
        hit = np.zeros(tile.shape[:2], bool)
        for color in colors:
            hit |= np.all(np.abs(tile - color) <= 3, axis=2)
        return hit.mean()

    blade = [(56, 88, 16), (95, 109, 77)]
    lawn = [(112, 200, 160), (120, 160, 142)]
    leaf = [(184, 248, 136), (158, 190, 134)]
    tiles_r, tiles_c = rows // 2, cols // 2
    grass = np.zeros((tiles_r, tiles_c), bool)
    for tr in range(tiles_r):
        for tc in range(tiles_c):
            tile = image[tr * 16:(tr + 1) * 16, tc * 16:(tc + 1) * 16]
            grass[tr, tc] = share(tile, blade) > 0.12 and share(tile, lawn) > 0.12 and share(tile, leaf) < 0.04
    # Rectangles : morceaux de lignes, fusionnés avec la ligne du dessous quand ils sont identiques.
    runs, open_runs = [], {}
    for tr in range(tiles_r + 1):
        current = {}
        if tr < tiles_r:
            tc = 0
            while tc < tiles_c:
                if grass[tr, tc]:
                    start = tc
                    while tc < tiles_c and grass[tr, tc]:
                        tc += 1
                    current[(start, tc)] = None
                else:
                    tc += 1
        for key, top in list(open_runs.items()):
            if key in current:
                current[key] = top
            else:
                runs.append((key[0] * 2, top * 2, (key[1] - key[0]) * 2, (tr - top) * 2))
        open_runs = {key: (tr if top is None else top) for key, top in current.items()}
    runs = [r for r in runs if r[2] * r[3] >= 8]  # au moins 2 tuiles
    with open(os.path.join(HERE, "kanto_herbes.json"), "w", encoding="utf-8") as handle:
        json.dump(runs, handle)
    print(f"{len(runs)} rectangles de hautes herbes")


if __name__ == "__main__":
    main(sys.argv[1])
