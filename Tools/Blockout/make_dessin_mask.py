"""Lit le dessin de L'amie (reference/dessin_kanto.png, retouche Photoshop de la vue du dessus)
et en tire les zones du relief, sans rien inventer :

- 0 = mer, 1 = plaine (routes, villes), 2 = forêt, 3 = montagne ou rochers.

Sortie : kanto_dessin.png (1009 × 1009, 8 bits), dans le même sens que la heightmap
(ligne = est, colonne = nord). make_heightmap.py s'en sert pour la côte, les forêts et les
montagnes. Lancement : python Tools/Blockout/make_dessin_mask.py (quelques secondes).
"""

import os
import sys

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kanto_v1_layout as layout  # noqa: E402

SIZE = (layout.HEIGHTMAP_SIZE - 1) // 4 + 1   # 1009 : un pixel = 6 m
FRAME = (6, 7, 808, 809)                       # la carte dans l'image (sans le cadre noir)
MIN_PIECE = 200                                # morceaux isolés plus petits : retirés (≈ 0,7 ha)


def classify(rgb):
    rgb = rgb.astype(np.float64)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    brightness = rgb.max(axis=2)
    spread = brightness - rgb.min(axis=2)
    ratio = r / np.maximum(g, 1)  # rouge / vert : ne dépend pas de l'ombrage du relief
    sea = (b > r + 45) & (b > 120)
    grey = (ratio > 0.88) & (spread < 70)  # roche et neige : gris, beige ou blanc, même à l'ombre
    sand = (ratio > 1.0) & (r - b > 35) & (r > 150)
    forest = ratio < 0.64
    out = np.ones(r.shape, np.uint8)        # plaine par défaut
    out[forest] = 2
    out[grey] = 3
    out[sand] = 1                           # plage : terre basse
    out[sea] = 0
    # Rochers dessinés (tampons cerclés de blanc) : tout le disque devient de la roche.
    ring = Image.fromarray(((brightness > 200) & (spread < 40) & ~sea).astype(np.uint8) * 255)
    stamps = np.asarray(ring.filter(ImageFilter.MaxFilter(13))) > 0
    out[stamps & (out != 0)] = 3
    return out


def majority(classes, radius):
    """Lissage : chaque pixel prend la classe la plus présente autour de lui."""
    votes = []
    for k in range(4):
        img = Image.fromarray(((classes == k) * 255).astype(np.uint8))
        votes.append(np.asarray(img.filter(ImageFilter.BoxBlur(radius))))
    return np.argmax(np.stack(votes), axis=0).astype(np.uint8)


def drop_small(classes):
    """Retire les îlots et les flaques isolés trop petits (la mer ou la terre autour gagne)."""
    from collections import deque
    land = classes > 0
    for target in (True, False):
        mask = land == target
        seen = np.zeros(mask.shape, bool)
        for r0, c0 in zip(*np.nonzero(mask)):
            if seen[r0, c0]:
                continue
            seen[r0, c0] = True
            queue, cells = deque([(r0, c0)]), []
            while queue:
                r, c = queue.popleft()
                cells.append((r, c))
                for rr, cc in ((r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)):
                    if 0 <= rr < mask.shape[0] and 0 <= cc < mask.shape[1] and mask[rr, cc] and not seen[rr, cc]:
                        seen[rr, cc] = True
                        queue.append((rr, cc))
            if len(cells) < MIN_PIECE:
                rs, cs = np.array(cells).T
                classes[rs, cs] = 2 if not target else 0
    return classes


def drop_small_mountains(classes, smallest=300):
    """Les petites taches grises isolées (talus ombrés) redeviennent de la forêt ; les vrais
    massifs et les rochers dessinés sont plus grands."""
    from collections import deque
    mask = classes == 3
    seen = np.zeros(mask.shape, bool)
    for r0, c0 in zip(*np.nonzero(mask)):
        if seen[r0, c0]:
            continue
        seen[r0, c0] = True
        queue, cells = deque([(r0, c0)]), []
        while queue:
            r, c = queue.popleft()
            cells.append((r, c))
            for rr, cc in ((r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)):
                if 0 <= rr < mask.shape[0] and 0 <= cc < mask.shape[1] and mask[rr, cc] and not seen[rr, cc]:
                    seen[rr, cc] = True
                    queue.append((rr, cc))
        if len(cells) < smallest:
            rs, cs = np.array(cells).T
            classes[rs, cs] = 2
    return classes


def drop_small_plains(classes, smallest=250):
    """Les petites taches claires au milieu des forêts (reflets du relief) redeviennent forêt."""
    from collections import deque
    mask = classes == 1
    seen = np.zeros(mask.shape, bool)
    for r0, c0 in zip(*np.nonzero(mask)):
        if seen[r0, c0]:
            continue
        seen[r0, c0] = True
        queue, cells = deque([(r0, c0)]), []
        while queue:
            r, c = queue.popleft()
            cells.append((r, c))
            for rr, cc in ((r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)):
                if 0 <= rr < mask.shape[0] and 0 <= cc < mask.shape[1] and mask[rr, cc] and not seen[rr, cc]:
                    seen[rr, cc] = True
                    queue.append((rr, cc))
        if len(cells) < smallest:
            rs, cs = np.array(cells).T
            classes[rs, cs] = 2
    return classes


def main():
    image = Image.open(os.path.join(HERE, "reference", "dessin_kanto.png")).convert("RGB")
    image = image.crop(FRAME).resize((SIZE, SIZE), Image.LANCZOS)
    classes = majority(classify(np.asarray(image)), 3)
    classes = drop_small(classes)
    # Ouverture : les fines lignes grises (ombres le long des côtes) disparaissent.
    rock = Image.fromarray((classes == 3).astype(np.uint8) * 255)
    opened = np.asarray(rock.filter(ImageFilter.MinFilter(5)).filter(ImageFilter.MaxFilter(5))) > 0
    classes[(classes == 3) & ~opened] = 2
    classes = drop_small_mountains(classes, 450)
    classes = drop_small_plains(classes)
    # Image nord en haut -> sens de la heightmap (ligne = est, colonne = nord).
    stored = np.ascontiguousarray(classes[::-1].T)
    Image.fromarray(stored).save(layout.DRAWING_PATH)
    preview = np.array([(40, 110, 170), (150, 180, 90), (45, 90, 45), (150, 140, 130)], np.uint8)[classes]
    Image.fromarray(preview).save(os.path.join(HERE, "reference", "dessin_zones_apercu.png"))
    print("Zones :", {name: int((classes == k).sum()) for k, name in enumerate(("mer", "plaine", "forêt", "montagne"))})


if __name__ == "__main__":
    main()
