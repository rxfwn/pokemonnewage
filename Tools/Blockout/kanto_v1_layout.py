"""Plan du monde ouvert : Kanto entier pour le relief, la V1 (Bourg Palette à Argenta) en détail.

Fichier de données partagé, en Python pur (sans numpy) :
- make_heightmap.py s'en sert pour fabriquer le relief (heightmap 16 bits) ;
- blockout_kanto_v1.py s'en sert dans Unreal pour poser les objets à la bonne hauteur.

Le contour des terres (kanto_terres.bin) est décalqué de la carte officielle de Kanto
de Let's Go Pikachu / Évoli. Les lieux sont repérés en pixels sur cette carte (1185 × 843),
puis convertis en mètres : 1 pixel de la carte = 4,6 m, soit environ 1,7 km de Bourg Palette
à Argenta et un Kanto de 5,5 × 3,9 km. Les villes, elles, ont leur taille réelle en mètres :
agrandir la carte allonge les routes, pas les villes.

Repère du monde : x = nord, y = est, en mètres. Le coin (0, 0) est le coin du Landscape.
Vraies tailles : un personnage fait 1,80 m.
"""

import math
import os
import struct
import zlib

# Landscape : 4033 × 4033 points, un point tous les 1,5 m, environ 6 km de côté.
HEIGHTMAP_SIZE = 4033
METERS_PER_PIXEL = 1.5
WORLD_SIZE = (HEIGHTMAP_SIZE - 1) * METERS_PER_PIXEL

SEA_LEVEL = 0.0

# Carte de référence : 1185 × 843 pixels, 4,6 m par pixel.
MAP_SCALE = 4.6
MAP_WIDTH, MAP_HEIGHT = 1185, 843
MAP_OFFSET_X, MAP_OFFSET_Y = 1080.0, 300.0  # Kanto centré, la mer tout autour


def from_map(u, v):
    """Pixel (u vers la droite, v vers le bas) de la carte -> mètres (x nord, y est)."""
    return (MAP_HEIGHT - v) * MAP_SCALE + MAP_OFFSET_X, u * MAP_SCALE + MAP_OFFSET_Y


def to_map(x, y):
    return (y - MAP_OFFSET_Y) / MAP_SCALE, MAP_HEIGHT - (x - MAP_OFFSET_X) / MAP_SCALE


# --- Contour des terres -----------------------------------------------------------------

def _load_land():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "kanto_terres.bin")
    with open(path, "rb") as handle:
        data = zlib.decompress(handle.read())
    _, width, height = struct.unpack_from("<4sHH", data)
    size = width * height
    planes = [data[8 + i * size:8 + (i + 1) * size] for i in range(3)]
    return (width, height, *planes)


# Trois plans de la taille de la carte : terre précise, terre lissée,
# distance à la mer (en pixels de la carte, plafonnée à 255).
_LAND_W, _LAND_H, _LAND_FINE, _LAND_WIDE, _LAND_INLAND = _load_land()


def _sample(plane, u, v, scale=255.0):
    """Valeur interpolée (entre 0 et 1 par défaut). Hors de la carte : la mer."""
    if not (0 <= u < _LAND_W - 1 and 0 <= v < _LAND_H - 1):
        return 0.0
    iu, iv = int(u), int(v)
    tu, tv = u - iu, v - iv
    row, below = iv * _LAND_W, (iv + 1) * _LAND_W
    a, b = plane[row + iu], plane[row + iu + 1]
    c, d = plane[below + iu], plane[below + iu + 1]
    return (a + (b - a) * tu + (c - a) * tv + (a - b - c + d) * tu * tv) / scale


def land_at(x, y):
    """(terre précise, terre lissée, distance à la mer en mètres) au point (x, y)."""
    u, v = to_map(x, y)
    return (_sample(_LAND_FINE, u, v), _sample(_LAND_WIDE, u, v),
            _sample(_LAND_INLAND, u, v, 1.0) * MAP_SCALE)


# --- Lieux de la V1 (repérés en pixels sur la carte) ----------------------------------

# Villes et forêt : (nom, centre u, centre v en pixels de la carte,
# demi-largeur est-ouest, demi-profondeur nord-sud en mètres, forme).
# Forme 2 = ellipse (la forêt) ; 4 = rectangle aux coins arrondis (les villes, bordées
# d'arbres comme dans le jeu d'origine).
AREAS = [
    ("Bourg Palette", 283, 540, 39, 37, 4),
    ("Jadielle", 297, 380, 92, 87, 4),
    ("Forêt de Jade", 305, 258, 207, 184, 2),
    ("Argenta", 297, 172, 87, 74, 4),
]

# Routes : couloirs qui suivent une ligne brisée (nom, demi-largeur en mètres, points
# en pixels de la carte). Elles partent du centre des villes pour bien s'y raccorder.
ROUTES = [
    ("Route 1", 30, [(283, 540), (286, 515), (280, 490), (295, 465), (285, 440), (292, 418),
                     (297, 380)]),
    ("Route 22", 25, [(297, 382), (257, 385), (220, 392), (180, 380), (140, 382)]),
    ("Route 2 (sud)", 22, [(297, 380), (292, 342), (290, 300)]),
    ("Route 2 (nord)", 20, [(298, 216), (297, 204), (297, 172)]),
]

# Sol des zones : monte doucement du sud (Bourg Palette, 1,2 m) au nord (Argenta, 15 m).
FLOOR_SOUTH, FLOOR_NORTH = 1.2, 15.0
_FLOOR_X0 = (MAP_HEIGHT - 540) * MAP_SCALE + MAP_OFFSET_X
_FLOOR_X1 = (MAP_HEIGHT - 172) * MAP_SCALE + MAP_OFFSET_X

# Massifs : ellipses (centre u, centre v, demi-largeur u, demi-hauteur v en pixels, poids).
# Le poids règle la part de montagne (1 = grandes montagnes, 0,5 = moyennes).
# Leurs bords sont déformés par du bruit : pas de cercles parfaits.
MOUNTAINS = [
    (495, 90, 120, 85, 1.0),    # Mont Sélénite
    (140, 220, 85, 190, 1.0),   # Route Victoire et Plateau Indigo
    (330, 80, 110, 70, 0.75),   # derrière Argenta
    (930, 230, 90, 140, 0.85),  # Tunnel Roche
    (720, 110, 160, 70, 0.6),   # crêtes du nord
    (1090, 380, 60, 110, 0.5),  # collines de l'est
]

# La mer, là où la carte de Let's Go dessine de la terre mais où le jeu d'origine met de l'eau :
# (nom, ville de référence, décalage x, y du centre en mètres, demi-profondeur x, demi-largeur y).
SEA_AREAS = [
    ("Route 21 (mer au sud de Bourg Palette)", "Bourg Palette", -190, -10, 125, 230),
]

# Eau creusée dans le sol (sous le niveau de la mer, donc remplie par la mer) :
# (nom, ville de référence, demi-largeur en m, points relatifs en mètres).
# L'étang de Bourg Palette se jette dans la mer, comme dans Rouge Feu / Vert Feuille.
_CHANNELS = [
    ("Étang de Bourg Palette", "Bourg Palette", 5, [(-13, -22), (-37, -22), (-55, -24), (-80, -26)]),
]

def area_center(name):
    for area_name, u, v, _, _, _ in AREAS:
        if area_name == name:
            return from_map(u, v)
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
# Types : Water (étang), Flowers (parterre), Fence (barrière blanche), Sign (panneau),
# Mailbox (boîte aux lettres), Sand (sable). Relatif au centre de la ville, en mètres.
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
    # Étang en bas à gauche, qui file vers la mer (l'eau est creusée dans le relief, voir
    # CHANNELS) : ici seulement son muret de pierre.
    ("Bourg Palette", "Water", -25, -22, 24, 10),
    # Barrière et panneau en bas, puis le chemin de sable et les fleurs en bas à droite.
    ("Bourg Palette", "Fence", -17, 2, 0.15, 16),
    ("Bourg Palette", "Sign", -17, 11, 0.2, 1.2),
    ("Bourg Palette", "Sand", -25, 5, 5, 20),
    ("Bourg Palette", "Flowers", -28, 19, 3, 8),
]
DECOR = [(zone, kind, *_offset(zone, dx, dy), depth, width) for zone, kind, dx, dy, depth, width in _DECOR]

# Murs d'arbres serrés autour des villes : (zone, demi-profondeur x, demi-largeur y,
# ouvertures). Une ouverture = (côté, début, fin) en mètres le long du côté :
# côtés « N » et « S » mesurés vers l'est (y), « O » et « E » vers le nord (x).
TREE_WALLS = [
    ("Bourg Palette", 34, 36, [("N", -2, 16), ("S", -28, -16)]),
]

# Postes de passage (on les traverse) : (nom, x, y, axe du passage).
GATES = [
    ("Poste de la Forêt (sud)", *from_map(290, 300), "x"),
    ("Poste de la Forêt (nord)", *from_map(298, 216), "x"),
    ("Poste de la Ligue (fermé)", *from_map(140, 382), "y"),
]

# Chemins de terre, en pixels de la carte.
_PATHS = [
    ("Chemin Bourg Palette", [(283, 534), (286, 515)]),
    ("Chemin Route 1", [(286, 515), (280, 490), (295, 465), (285, 440), (292, 418)]),
    ("Chemin Jadielle", [(292, 418), (292, 342)]),
    ("Chemin Jadielle ouest", [(292, 385), (257, 385)]),
    ("Chemin Route 22", [(257, 385), (220, 392), (180, 380), (144, 382)]),
    ("Chemin Route 2", [(292, 342), (290, 300)]),
    ("Chemin Route 2 nord", [(298, 216), (297, 204)]),
    ("Chemin Argenta", [(297, 204), (297, 160)]),
]
PATHS = [(name, [from_map(u, v) for u, v in points]) for name, points in _PATHS]


def _forest_path():
    """Le chemin de la Forêt de Jade serpente entre les deux postes."""
    (x0, y0), (x1, y1) = from_map(290, 300), from_map(298, 216)
    points = []
    for step in range(21):
        t = step / 20
        wave = 110 * math.sin(t * math.pi * 2) * math.sin(t * math.pi)
        points.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + wave))
    return points


FOREST_PATH = _forest_path()

# Hautes herbes : (zone, centre u, centre v, profondeur x en m, largeur y en m).
_TALL_GRASS = [
    ("Route 1", 281, 497, 37, 33),
    ("Route 1", 296, 470, 45, 27),
    ("Route 1", 283, 446, 33, 37),
    ("Route 22", 228, 389, 33, 45),
    ("Route 22", 178, 383, 37, 37),
    ("Route 2 (sud)", 293, 322, 33, 22),
    ("Forêt de Jade", 283, 272, 45, 45),
    ("Forêt de Jade", 327, 245, 45, 45),
    ("Forêt de Jade", 300, 232, 33, 45),
]
TALL_GRASS = [(zone, *from_map(u, v), depth, width) for zone, u, v, depth, width in _TALL_GRASS]

# Panneaux à l'entrée de chaque lieu : (nom, ville de référence, décalage x, y en mètres)
# ou (nom, None, u, v en pixels de la carte).
_SIGNS = [
    ("Bourg Palette", "Bourg Palette", 30, 8),
    ("Route 1", "Bourg Palette", 55, 8),
    ("Jadielle", "Jadielle", -80, 8),
    ("Route 22", "Jadielle", 4, -105),
    ("Route 2", "Jadielle", 100, 8),
    ("Forêt de Jade", None, 291, 304),
    ("Argenta", "Argenta", -68, 8),
]
SIGNS = [(name, *(_offset(town, a, b) if town else from_map(a, b))) for name, town, a, b in _SIGNS]

CHANNELS = [(name, half, [_offset(town, dx, dy) for dx, dy in points])
            for name, town, half, points in _CHANNELS]
SEA = [(name, *_offset(town, dx, dy), half_x, half_y) for name, town, dx, dy, half_x, half_y in SEA_AREAS]

# Point de départ du joueur : entre les deux maisons, face au nord (la sortie vers la Route 1).
PLAYER_START = _offset("Bourg Palette", 6, 0)


# --- Relief ---------------------------------------------------------------------------

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


def _segment_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + dx * t), py - (ay + dy * t))


def zone_distance(x, y):
    """(distance en mètres au lieu de la V1 le plus proche, nom). Négative à l'intérieur.
    Les bords ondulent pour suivre le terrain comme de vraies vallées."""
    u, v = to_map(x, y)
    wobble = (fractal_noise(x, y, 60, 3, 21) - 0.5) * 2  # entre -1 et 1
    best, best_name = 1e9, None
    for name, cu, cv, half_east, half_north, shape in AREAS:
        cx, cy = from_map(cu, cv)
        a, b = abs(y - cy) / half_east, abs(x - cx) / half_north
        norm = (a ** shape + b ** shape) ** (1.0 / shape)
        distance = (norm - 1 - wobble * 0.12) * min(half_east, half_north)
        if distance < best:
            best, best_name = distance, name
    for name, half_width, points in ROUTES:
        segment = min(_segment_distance(u, v, *a, *b) for a, b in zip(points, points[1:]))
        distance = segment * MAP_SCALE - half_width * (1 + wobble * 0.3)
        if distance < best:
            best, best_name = distance, name
    return best, best_name


def find_zone(x, y):
    distance, name = zone_distance(x, y)
    return name if distance <= 0 else None


# --- Hauteur du sol : lue dans la heightmap ------------------------------------------
# Le relief est fabriqué une fois par make_heightmap.py (bruit, érosion, vallées) ;
# Unreal et ce module lisent ensuite le même PNG, donc les objets tombent pile sur le sol.

HEIGHTMAP_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "kanto_v1_heightmap.png")
_HEIGHTS = None


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
    return (value - 32768) / 128.0
