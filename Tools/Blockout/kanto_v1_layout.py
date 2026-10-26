"""Plan du monde ouvert : Kanto entier pour le relief, la V1 (Bourg Palette à Argenta) en détail.

Fichier de données partagé, en Python pur (sans numpy) :
- make_heightmap.py s'en sert pour fabriquer le relief (heightmap 16 bits) ;
- blockout_kanto_v1.py s'en sert dans Unreal pour poser les objets à la bonne hauteur.

Le réseau des routes et des villes est décalqué de la carte de Rouge Feu / Vert Feuille
(kanto_frlg.bin, fait par make_frlg_mask.py) : 815 × 800 cases d'une demi-tuile, 1 case =
6,5 m, soit environ 1 km de Bourg Palette à Jadielle et un Kanto de 5,3 × 5,2 km.
Tout ce qui est hors des routes devient du relief infranchissable (falaises, montagnes) ou
la mer. La silhouette des côtes vient de la carte de Let's Go (kanto_terres.bin).
Les villes de la V1 ont leur taille réelle en mètres (bâtiments, décor).

Repère du monde : x = nord, y = est, en mètres. Le coin (0, 0) est le coin du Landscape.
Vraies tailles : un personnage fait 1,80 m.
"""

import math
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

# Landscape : 4033 × 4033 points, un point tous les 1,5 m, environ 6 km de côté.
# Échelle à l'import : 150, 150, 200 (l'échelle Z de 200 permet des sommets à 500 m).
HEIGHTMAP_SIZE = 4033
METERS_PER_PIXEL = 1.5
WORLD_SIZE = (HEIGHTMAP_SIZE - 1) * METERS_PER_PIXEL
LANDSCAPE_SCALE_Z = 200.0
HEIGHT_UNITS_PER_METER = 128.0 * 100.0 / LANDSCAPE_SCALE_Z  # 64 : 32768 = altitude 0

SEA_LEVEL = 0.0

# --- Carte de Rouge Feu / Vert Feuille ---------------------------------------------------

FRLG_COLS, FRLG_ROWS = 815, 800
CELL = 6.5  # mètres par case
FRLG_OFFSET_X, FRLG_OFFSET_Y = 424.0, 375.0  # Kanto centré dans le Landscape


def from_frlg(column, row):
    """Case (colonne vers l'est, ligne vers le sud) de la carte FRLG -> mètres (x nord, y est)."""
    return (FRLG_ROWS - row) * CELL + FRLG_OFFSET_X, column * CELL + FRLG_OFFSET_Y


def to_frlg(x, y):
    return (y - FRLG_OFFSET_Y) / CELL, FRLG_ROWS - (x - FRLG_OFFSET_X) / CELL


# Carte de Let's Go (silhouette des côtes) : 1185 × 843 pixels. Passage d'une case FRLG
# à un pixel de Let's Go, calé sur la position des onze villes (écart moyen ~15 pixels).
LETSGO_FROM_FRLG = ((1.14382, -0.025161, 123.288), (0.040931, 0.970313, 6.052))


def _load_land():
    with open(os.path.join(HERE, "kanto_terres.bin"), "rb") as handle:
        data = zlib.decompress(handle.read())
    _, width, height = struct.unpack_from("<4sHH", data)
    size = width * height
    planes = [data[8 + i * size:8 + (i + 1) * size] for i in range(3)]
    return (width, height, *planes)


# Trois plans de la carte de Let's Go : terre précise, terre lissée, distance à la mer.
_LAND_W, _LAND_H, _LAND_FINE, _LAND_WIDE, _LAND_INLAND = _load_land()

# --- Altitudes ---------------------------------------------------------------------------
# Chaque ville a sa hauteur ; les routes montent et descendent entre elles en passant par
# des points de passage (cols, collines, descentes vers la mer) : (nom, colonne, ligne, m).
TOWNS = [
    ("Bourg Palette", 144, 540, 22),     # plaine du sud, au pied des collines
    ("Jadielle", 144, 400, 48),          # bassin entouré de collines
    ("Argenta", 144, 160, 140),          # ville de pierre sur un plateau de montagne
    ("Plateau Indigo", 24, 35, 330),     # tout en haut, au bout de la Route Victoire
    ("Azuria", 576, 110, 40),            # vallée de la rivière, au nord
    ("Safrania", 576, 280, 62),          # grande plaine du centre
    ("Céladopole", 420, 280, 45),
    ("Carmin sur Mer", 576, 440, 8),     # le port, au niveau de la mer
    ("Lavanville", 790, 275, 115),       # village perché dans les montagnes de l'est
    ("Parmanie", 432, 640, 12),          # côte sud
    ("Cramois'Île", 145, 775, 18),       # île volcanique
]
ROUTE_POINTS = [
    ("Route 1 (colline)", 144, 470, 72),        # on voit Jadielle depuis le haut
    ("Route 1 (bas)", 144, 515, 30),
    ("Route 2 (forêt)", 144, 280, 95),
    ("Route 2 (col)", 144, 205, 125),
    ("Route 22", 60, 404, 68),
    ("Route 23 (bas)", 24, 300, 160),
    ("Route 23 (haut)", 24, 130, 285),
    ("Route 3", 260, 160, 165),
    ("Col du Mont Sélénite", 336, 125, 215),
    ("Route 4", 450, 120, 105),
    ("Route 24", 576, 45, 70),
    ("Route 25", 700, 20, 48),
    ("Route 9", 700, 120, 135),
    ("Route 10 (Tunnel Roche)", 790, 185, 175),
    ("Route 10 (sud)", 790, 235, 140),
    ("Route 5", 576, 190, 55),
    ("Route 6", 576, 370, 32),
    ("Route 7", 500, 280, 58),
    ("Route 8", 690, 280, 92),
    ("Route 11", 700, 440, 22),
    ("Route 12 (nord)", 790, 400, 62),
    ("Route 12 (sud)", 790, 520, 28),
    ("Route 13", 700, 560, 24),
    ("Route 14", 640, 620, 20),
    ("Route 15", 560, 640, 15),
    ("Piste cyclable (haut)", 290, 300, 62),
    ("Piste cyclable (milieu)", 290, 450, 40),
    ("Piste cyclable (bas)", 290, 600, 16),
    ("Route 18", 340, 640, 12),
    ("Route 21 (rivage)", 144, 610, 6),
]

# Massifs : (nom, colonne, ligne, rayon en cases, poids). Le poids règle la hauteur
# (1 = grandes montagnes). Les montagnes forment des chaînes en crêtes, pas des bosses.
MASSIFS = [
    ("Route Victoire", 70, 170, 120, 1.0),
    ("Montagnes d'Argenta", 200, 70, 110, 0.9),
    ("Mont Sélénite", 340, 50, 110, 1.0),
    ("Crêtes du nord", 460, 40, 120, 0.8),
    ("Tunnel Roche", 720, 200, 85, 0.95),
    ("Monts de Lavanville", 700, 350, 70, 0.6),
    ("Collines de Jadielle", 230, 420, 90, 0.55),
    ("Collines du sud", 210, 560, 90, 0.4),
    ("Volcan de Cramois'Île", 150, 772, 22, 0.9),
]

# --- Lieux de la V1 ------------------------------------------------------------------------

# Villes et forêt : (nom, colonne, ligne, demi-largeur est-ouest, demi-profondeur nord-sud
# en mètres, forme). Forme 2 = ellipse (la forêt) ; 4 = rectangle aux coins arrondis.
AREAS = [
    ("Bourg Palette", 144, 540, 39, 37, 4),
    ("Jadielle", 144, 400, 92, 87, 4),
    ("Forêt de Jade", 144, 280, 140, 380, 2),
    ("Argenta", 144, 160, 87, 74, 4),
]

# Terrasses des villes : (limite x en mètres depuis le centre, hauteur de la marche en m).
# Le niveau le plus bas accueille la route d'arrivée, le plus haut l'arène ou le monument.
# Une rampe de 16 m traverse chaque marche le long de l'axe nord-sud de la ville (y = 0).
TERRACES = {
    "Bourg Palette": [(3, 2.5)],
    "Jadielle": [(-15, 4.0), (28, 4.0)],
    "Argenta": [(-19.5, 5.0), (29, 5.0)],
}
RAMP_HALF_WIDTH = 5.0


def area_center(name):
    for area_name, column, row, *_ in AREAS:
        if area_name == name:
            return from_frlg(column, row)
    raise KeyError(name)


def _offset(town, dx, dy):
    cx, cy = area_center(town)
    return cx + dx, cy + dy


# Bâtiments : (zone, nom, centre x, centre y, profondeur x, largeur y, hauteur, mur, toit, style).
# Positions relatives au centre de la ville, en mètres (x = nord, y = est). Toutes les portes
# sont au sud, comme dans le jeu d'origine. Style « pignon » = toit à deux pans, « plat » sinon.
# Bourg Palette suit le plan de Rouge Feu / Vert Feuille : deux maisons en haut, le labo
# en bas à droite, les fleurs à gauche, l'étang en bas à gauche.
_BUILDINGS = [
    ("Bourg Palette", "Maison du joueur", 17, -19, 9, 11, 6.5, "House", "RoofRed", "pignon"),
    ("Bourg Palette", "Maison du rival", 17, 18, 9, 11, 6.5, "House", "RoofBlue", "pignon"),
    ("Bourg Palette", "Labo du Prof. Chen", -4, 17, 11, 19, 7.5, "Lab", "RoofGrey", "plat"),
    ("Jadielle", "Centre Pokémon", -35, -38, 18, 22, 8, "House", "Center", "plat"),
    ("Jadielle", "Boutique", 20, -50, 14, 16, 7, "House", "Mart", "plat"),
    ("Jadielle", "École des dresseurs", 5, 18, 14, 18, 7, "House", "RoofGreen", "pignon"),
    ("Jadielle", "Arène (fermée)", 42, 55, 26, 30, 11, "Gym", "RoofGrey", "plat"),
    ("Jadielle", "Maison", -40, 55, 10, 11, 7, "House", "RoofRed", "pignon"),
    ("Argenta", "Arène de Pierre", -5, -45, 26, 28, 11, "Gym", "RoofGrey", "plat"),
    ("Argenta", "Musée des sciences", 42, -35, 20, 36, 12, "Lab", "RoofBrown", "plat"),
    ("Argenta", "Centre Pokémon", -30, 22, 18, 22, 8, "House", "Center", "plat"),
    ("Argenta", "Boutique", 18, 45, 14, 16, 7, "House", "Mart", "plat"),
    ("Argenta", "Maison", -35, 65, 10, 11, 7, "House", "RoofBrown", "pignon"),
]
BUILDINGS = [(zone, name, *_offset(zone, dx, dy), *rest) for zone, name, dx, dy, *rest in _BUILDINGS]

# Décor des villes : (zone, type, centre x, centre y, profondeur x, largeur y).
# Types : Water (étang : creusé dans le relief, muret et eau posés par Unreal), Flowers
# (parterre), Fence (barrière blanche), Sign (panneau), Mailbox (boîte aux lettres), Sand.
_DECOR = [
    # Boîtes aux lettres à côté des portes.
    ("Bourg Palette", "Mailbox", 11, -24, 0.5, 0.5),
    ("Bourg Palette", "Mailbox", 11, 13, 0.5, 0.5),
    # Parterre de fleurs à gauche, avec sa barrière au nord et son panneau.
    ("Bourg Palette", "Flowers", -4, -18, 5, 12),
    ("Bourg Palette", "Fence", -1, -18, 0.15, 13),
    ("Bourg Palette", "Sign", -1, -10, 0.2, 1.2),
    # Fleurs devant les maisons.
    ("Bourg Palette", "Flowers", 9, -14, 2, 4),
    ("Bourg Palette", "Flowers", 9, 23, 2, 4),
    # Étang en bas à gauche.
    ("Bourg Palette", "Water", -25, -22, 20, 10),
    # Barrière et panneau en bas, puis le chemin de sable et les fleurs en bas à droite.
    ("Bourg Palette", "Fence", -17, 2, 0.15, 16),
    ("Bourg Palette", "Sign", -17, 11, 0.2, 1.2),
    ("Bourg Palette", "Sand", -25, 5, 5, 20),
    ("Bourg Palette", "Flowers", -28, 19, 3, 8),
]
DECOR = [(zone, kind, *_offset(zone, dx, dy), depth, width) for zone, kind, dx, dy, depth, width in _DECOR]
PONDS = [(x, y, depth, width) for _, kind, x, y, depth, width in DECOR if kind == "Water"]

# Murs d'arbres serrés autour des villes : (zone, demi-profondeur x, demi-largeur y,
# ouvertures). Une ouverture = (côté, début, fin) en mètres le long du côté :
# côtés « N » et « S » mesurés vers l'est (y), « O » et « E » vers le nord (x).
TREE_WALLS = [
    ("Bourg Palette", 34, 36, [("N", -8, 18), ("S", -6, 6)]),
]

# Postes de passage (on les traverse) : (nom, x, y, axe du passage).
GATES = [
    ("Poste de la Forêt (sud)", *from_frlg(144, 338), "x"),
    ("Poste de la Forêt (nord)", *from_frlg(144, 222), "x"),
    ("Poste de la Ligue (fermé)", *from_frlg(36, 404), "y"),
]

# Chemins de terre, en cases de la carte FRLG.
_PATHS = [
    ("Chemin Route 1", [(144, 535), (146, 515), (139, 495), (149, 470), (140, 445), (146, 425),
                        (144, 413)]),
    ("Chemin Jadielle", [(144, 413), (144, 387)]),
    ("Chemin Jadielle ouest", [(144, 404), (128, 404)]),
    ("Chemin Route 22", [(128, 404), (105, 409), (80, 400), (55, 406), (38, 404)]),
    ("Chemin Route 2", [(144, 387), (146, 365), (144, 338)]),
    ("Chemin Route 2 nord", [(144, 222), (142, 200), (144, 171)]),
    ("Chemin Argenta", [(144, 171), (144, 150)]),
]
PATHS = [(name, [from_frlg(c, r) for c, r in points]) for name, points in _PATHS]


def _forest_path():
    """Le chemin de la Forêt de Jade serpente entre les deux postes."""
    (x0, y0), (x1, y1) = from_frlg(144, 338), from_frlg(144, 222)
    points = []
    for step in range(25):
        t = step / 24
        wave = 85 * math.sin(t * math.pi * 3) * math.sin(t * math.pi)
        points.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + wave))
    return points


FOREST_PATH = _forest_path()

# Hautes herbes : (zone, colonne, ligne, profondeur x en m, largeur y en m).
_TALL_GRASS = [
    ("Route 1", 135, 505, 37, 33),
    ("Route 1", 154, 478, 45, 27),
    ("Route 1", 136, 448, 33, 37),
    ("Route 22", 95, 398, 33, 45),
    ("Route 22", 62, 410, 37, 37),
    ("Route 2 (sud)", 152, 362, 33, 22),
    ("Forêt de Jade", 128, 305, 45, 45),
    ("Forêt de Jade", 162, 262, 45, 45),
    ("Forêt de Jade", 136, 238, 33, 45),
]
TALL_GRASS = [(zone, *from_frlg(c, r), depth, width) for zone, c, r, depth, width in _TALL_GRASS]

# Panneaux à l'entrée de chaque lieu : (nom, ville de référence, décalage x, y en mètres)
# ou (nom, None, colonne, ligne de la carte FRLG).
_SIGNS = [
    ("Bourg Palette", "Bourg Palette", 30, 8),
    ("Route 1", "Bourg Palette", 60, 8),
    ("Jadielle", "Jadielle", -95, 8),
    ("Route 22", "Jadielle", 4, -105),
    ("Route 2", "Jadielle", 100, 8),
    ("Forêt de Jade", None, 146, 342),
    ("Argenta", "Argenta", -82, 8),
]
SIGNS = [(name, *(_offset(town, a, b) if town else from_frlg(a, b))) for name, town, a, b in _SIGNS]

# Point de départ du joueur : entre les deux maisons, face au nord (la sortie vers la Route 1).
PLAYER_START = _offset("Bourg Palette", 6, 0)


# --- Bruit et distances (identiques dans make_heightmap.py, version numpy) -------------

def _hash(ix, iy, seed):
    n = ix * 374761393 + iy * 668265263 + seed * 2147483647
    n = (n ^ (n >> 13)) * 1274126177
    n ^= n >> 16
    return (n & 0xFFFFFF) / float(0xFFFFFF)


def _smooth(t):
    return t * t * (3 - 2 * t)


def value_noise(x, y, scale, seed=0):
    """Bruit doux entre 0 et 1, déterministe (même résultat sur tous les PC)."""
    fx, fy = x / scale, y / scale
    ix, iy = math.floor(fx), math.floor(fy)
    tx, ty = _smooth(fx - ix), _smooth(fy - iy)
    a = _hash(ix, iy, seed)
    b = _hash(ix + 1, iy, seed)
    c = _hash(ix, iy + 1, seed)
    d = _hash(ix + 1, iy + 1, seed)
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty


def fractal_noise(x, y, scale, octaves=4, seed=0):
    total, amplitude, norm = 0.0, 1.0, 0.0
    for octave in range(octaves):
        total += value_noise(x, y, scale, seed + octave) * amplitude
        norm += amplitude
        scale /= 2.0
        amplitude /= 2.0
    return total / norm


def smoothstep(edge0, edge1, value):
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return _smooth(t)


def zone_distance(x, y):
    """(distance en mètres à la ville ou forêt de la V1 la plus proche, nom).
    Négative à l'intérieur ; les bords ondulent un peu."""
    wobble = (fractal_noise(x, y, 60, 3, 21) - 0.5) * 2  # entre -1 et 1
    best, best_name = 1e9, None
    for name, column, row, half_east, half_north, shape in AREAS:
        cx, cy = from_frlg(column, row)
        a, b = abs(y - cy) / half_east, abs(x - cx) / half_north
        norm = (a ** shape + b ** shape) ** (1.0 / shape)
        distance = (norm - 1 - wobble * 0.12) * min(half_east, half_north)
        if distance < best:
            best, best_name = distance, name
    return best, best_name


def find_zone(x, y):
    distance, name = zone_distance(x, y)
    return name if distance <= 0 else None


# --- Données produites par make_heightmap.py -------------------------------------------
# Le relief est fabriqué une fois par make_heightmap.py ; Unreal et ce module relisent
# ensuite les mêmes fichiers, donc les objets tombent pile sur le sol.

HEIGHTMAP_PATH = os.path.join(HERE, "kanto_v1_heightmap.png")
CORRIDORS_PATH = os.path.join(HERE, "kanto_couloirs.bin")
LAKES_PATH = os.path.join(HERE, "kanto_lacs.json")
CORRIDOR_STEP = 3.0  # mètres entre deux points de kanto_couloirs.bin
_HEIGHTS = None
_CORRIDORS = None


def _load_heights():
    """Lit kanto_v1_heightmap.png (16 bits, sans filtre de ligne, écrit par make_heightmap.py)."""
    import array
    import sys
    with open(HEIGHTMAP_PATH, "rb") as handle:
        data = handle.read()
    position, chunks = 8, []
    while position < len(data):
        length, kind = struct.unpack_from(">I4s", data, position)
        if kind == b"IDAT":
            chunks.append(data[position + 8:position + 8 + length])
        position += 12 + length
    raw = zlib.decompress(b"".join(chunks))
    stride = 1 + 2 * HEIGHTMAP_SIZE
    if len(raw) != stride * HEIGHTMAP_SIZE:
        raise RuntimeError("kanto_v1_heightmap.png n'a pas la taille attendue : relancer make_heightmap.py")
    rows = []
    for row in range(HEIGHTMAP_SIZE):
        if raw[row * stride] != 0:
            raise RuntimeError("kanto_v1_heightmap.png : format inattendu, relancer make_heightmap.py")
        rows.append(raw[row * stride + 1:(row + 1) * stride])
    values = array.array("H", b"".join(rows))
    if sys.byteorder == "little":
        values.byteswap()  # le PNG range les nombres poids fort d'abord
    return values


def height_at(x, y):
    """Hauteur du sol en mètres au point (x, y), lue dans la heightmap (interpolée)."""
    global _HEIGHTS
    if _HEIGHTS is None:
        _HEIGHTS = _load_heights()
    size = HEIGHTMAP_SIZE
    # Ligne de l'image = axe y (est), colonne = axe x (nord).
    column = max(0.0, min(size - 1.001, x / METERS_PER_PIXEL))
    row = max(0.0, min(size - 1.001, y / METERS_PER_PIXEL))
    c, r = int(column), int(row)
    tc, tr = column - c, row - r
    index = r * size + c
    a, b = _HEIGHTS[index], _HEIGHTS[index + 1]
    d, e = _HEIGHTS[index + size], _HEIGHTS[index + size + 1]
    value = (a * (1 - tc) + b * tc) * (1 - tr) + (d * (1 - tc) + e * tc) * tr
    return (value - 32768) / HEIGHT_UNITS_PER_METER


def v1_distance(x, y):
    """Distance en mètres au plus proche chemin ou lieu de la V1 (villes : 40 m de marge)."""
    best = 1e9
    for _, points in PATHS + [("forêt", FOREST_PATH)]:
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
            best = min(best, math.hypot(x - (ax + dx * t), y - (ay + dy * t)))
    for name, column, row, half_east, half_north, _ in AREAS:
        if name == "Forêt de Jade":
            continue
        cx, cy = from_frlg(column, row)
        box = max(abs(x - cx) - half_north, abs(y - cy) - half_east)
        best = min(best, max(box, 0) - 40)
    return best


def corridor_depth(x, y):
    """Distance en mètres au bord des routes : positive sur les routes et dans les villes,
    négative dans le relief infranchissable (lue dans kanto_couloirs.bin)."""
    global _CORRIDORS
    if _CORRIDORS is None:
        with open(CORRIDORS_PATH, "rb") as handle:
            data = zlib.decompress(handle.read())
        _, count = struct.unpack_from("<4sH", data)
        _CORRIDORS = (count, data[6:])
    count, values = _CORRIDORS
    c = int(round(x / CORRIDOR_STEP))
    r = int(round(y / CORRIDOR_STEP))
    if not (0 <= c < count and 0 <= r < count):
        return -127.0
    value = values[r * count + c]
    return float(value - 256 if value > 127 else value)
