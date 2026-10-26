"""Plan du monde ouvert : Kanto entier pour le relief, la V1 (Bourg Palette à Argenta) en détail.

Fichier de données partagé, en Python pur (sans numpy) :
- make_heightmap.py s'en sert pour fabriquer le relief (heightmap 16 bits) ;
- blockout_kanto_v1.py s'en sert dans Unreal pour poser les objets à la bonne hauteur.

Le contour des terres (kanto_terres.bin) est décalqué de la carte officielle de Kanto
de Let's Go Pikachu / Évoli. Les lieux sont repérés en pixels sur cette carte (1185 × 843),
puis convertis en mètres : 1 pixel de la carte = 2,3 m, soit environ 850 m de Bourg Palette
à Argenta et un Kanto de 2,7 × 1,9 km.

Repère du monde : x = nord, y = est, en mètres. Le coin (0, 0) est le coin du Landscape.
Vraies tailles : un personnage fait 1,80 m.
"""

import math
import os
import struct
import zlib

# Landscape : 2017 × 2017 points, un point tous les 1,5 m, environ 3 km de côté.
HEIGHTMAP_SIZE = 2017
METERS_PER_PIXEL = 1.5
WORLD_SIZE = (HEIGHTMAP_SIZE - 1) * METERS_PER_PIXEL

SEA_LEVEL = 0.0

# Carte de référence : 1185 × 843 pixels, 2,3 m par pixel.
MAP_SCALE = 2.3
MAP_WIDTH, MAP_HEIGHT = 1185, 843
MAP_OFFSET_X, MAP_OFFSET_Y = 300.0, 100.0  # marge de mer au sud et à l'ouest


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
    return width, height, data[8:8 + size], data[8 + size:8 + 2 * size]


_LAND_W, _LAND_H, _LAND_FINE, _LAND_WIDE = _load_land()


def _sample(plane, u, v):
    """Valeur entre 0 (mer) et 1 (terre), interpolée. Hors de la carte : la mer."""
    if not (0 <= u < _LAND_W - 1 and 0 <= v < _LAND_H - 1):
        return 0.0
    iu, iv = int(u), int(v)
    tu, tv = u - iu, v - iv
    row, below = iv * _LAND_W, (iv + 1) * _LAND_W
    a, b = plane[row + iu], plane[row + iu + 1]
    c, d = plane[below + iu], plane[below + iu + 1]
    return (a + (b - a) * tu + (c - a) * tv + (a - b - c + d) * tu * tv) / 255.0


def land_at(x, y):
    """(terre précise, terre lissée) : la première dessine la côte, la seconde la pente."""
    u, v = to_map(x, y)
    return _sample(_LAND_FINE, u, v), _sample(_LAND_WIDE, u, v)


# --- Lieux de la V1 (repérés en pixels sur la carte) ----------------------------------

# Villes et forêt : ellipses (nom, centre u, centre v, demi-largeur u, demi-hauteur v).
AREAS = [
    ("Bourg Palette", 283, 540, 30, 26),
    ("Jadielle", 297, 380, 40, 38),
    ("Forêt de Jade", 305, 258, 45, 40),
    ("Argenta", 297, 172, 38, 32),
]

# Routes : couloirs qui suivent une ligne brisée (nom, demi-largeur en pixels, points).
ROUTES = [
    ("Route 1", 11, [(286, 515), (280, 490), (295, 465), (285, 440), (292, 418)]),
    ("Route 22", 9, [(257, 385), (220, 392), (180, 380), (140, 382)]),
    ("Route 2 (sud)", 8, [(292, 342), (290, 300)]),
    ("Route 2 (nord)", 7, [(298, 216), (297, 204)]),
]

# Sol des zones : monte doucement du sud (Bourg Palette, 3 m) au nord (Argenta, 15 m).
FLOOR_SOUTH, FLOOR_NORTH = 3.0, 15.0
_FLOOR_X0 = (MAP_HEIGHT - 540) * MAP_SCALE + MAP_OFFSET_X
_FLOOR_X1 = (MAP_HEIGHT - 172) * MAP_SCALE + MAP_OFFSET_X

# Montagnes : (centre u, centre v, rayon en pixels, hauteur max en mètres).
MOUNTAINS = [
    (495, 80, 75, 170.0),   # Mont Sélénite
    (150, 190, 110, 160.0),  # Route Victoire et Plateau Indigo
    (300, 70, 70, 110.0),   # derrière Argenta
    (930, 220, 70, 120.0),  # Grotte (Tunnel Roche)
]


def area_center(name):
    for area_name, u, v, _, _ in AREAS:
        if area_name == name:
            return from_map(u, v)
    raise KeyError(name)


def _offset(town, dx, dy):
    cx, cy = area_center(town)
    return cx + dx, cy + dy


# Bâtiments : (zone, nom, centre x, centre y, profondeur x, largeur y, hauteur, couleur).
# Positions relatives au centre de la ville, en mètres. Toutes les portes sont au sud,
# comme dans le jeu d'origine. Tailles réelles, assez grandes pour un intérieur plus tard.
_BUILDINGS = [
    ("Bourg Palette", "Maison du joueur", 30, -25, 9, 11, 7, "Wall"),
    ("Bourg Palette", "Maison du rival", 30, 25, 9, 11, 7, "Wall"),
    ("Bourg Palette", "Labo du Prof. Chen", -22, 34, 14, 22, 8, "Lab"),
    ("Jadielle", "Centre Pokémon", -35, -38, 18, 22, 8, "Center"),
    ("Jadielle", "Boutique", 20, -50, 14, 16, 7, "Mart"),
    ("Jadielle", "École des dresseurs", 5, 18, 14, 18, 7, "Wall"),
    ("Jadielle", "Arène (fermée)", 42, 55, 26, 30, 11, "Gym"),
    ("Jadielle", "Maison", -40, 55, 10, 11, 7, "Wall"),
    ("Argenta", "Arène de Pierre", -5, -45, 26, 28, 11, "Gym"),
    ("Argenta", "Musée des sciences", 42, -35, 20, 36, 12, "Lab"),
    ("Argenta", "Centre Pokémon", -30, 22, 18, 22, 8, "Center"),
    ("Argenta", "Boutique", 18, 45, 14, 16, 7, "Mart"),
    ("Argenta", "Maison", -35, 65, 10, 11, 7, "Wall"),
]
BUILDINGS = [(zone, name, *_offset(zone, dx, dy), depth, width, height, color)
             for zone, name, dx, dy, depth, width, height, color in _BUILDINGS]

# Postes de passage (on les traverse) : (nom, x, y, axe du passage).
GATES = [
    ("Poste de la Forêt (sud)", *from_map(290, 300), "x"),
    ("Poste de la Forêt (nord)", *from_map(298, 216), "x"),
    ("Poste de la Ligue (fermé)", *from_map(140, 382), "y"),
]

# Chemins de terre, en pixels de la carte.
_PATHS = [
    ("Chemin Bourg Palette", [(283, 548), (286, 515)]),
    ("Chemin Route 1", [(286, 515), (280, 490), (295, 465), (285, 440), (292, 418)]),
    ("Chemin Jadielle", [(292, 418), (292, 342)]),
    ("Chemin Jadielle ouest", [(292, 385), (257, 385)]),
    ("Chemin Route 22", [(257, 385), (220, 392), (180, 380), (144, 382)]),
    ("Chemin Route 2", [(292, 342), (290, 300)]),
    ("Chemin Route 2 nord", [(298, 216), (297, 204)]),
    ("Chemin Argenta", [(297, 204), (297, 145)]),
]
PATHS = [(name, [from_map(u, v) for u, v in points]) for name, points in _PATHS]


def _forest_path():
    """Le chemin de la Forêt de Jade serpente entre les deux postes."""
    (x0, y0), (x1, y1) = from_map(290, 300), from_map(298, 216)
    points = []
    for step in range(21):
        t = step / 20
        wave = 55 * math.sin(t * math.pi * 2) * math.sin(t * math.pi)
        points.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + wave))
    return points


FOREST_PATH = _forest_path()

# Hautes herbes : (zone, centre u, centre v, profondeur x en m, largeur y en m).
_TALL_GRASS = [
    ("Route 1", 281, 497, 25, 22),
    ("Route 1", 296, 470, 30, 18),
    ("Route 1", 283, 446, 22, 25),
    ("Route 22", 228, 389, 22, 30),
    ("Route 22", 178, 383, 25, 25),
    ("Route 2 (sud)", 293, 322, 22, 15),
    ("Forêt de Jade", 283, 272, 30, 30),
    ("Forêt de Jade", 327, 245, 30, 30),
    ("Forêt de Jade", 300, 232, 22, 30),
]
TALL_GRASS = [(zone, *from_map(u, v), depth, width) for zone, u, v, depth, width in _TALL_GRASS]

# Panneaux à l'entrée de chaque lieu, en pixels.
_SIGNS = [
    ("Bourg Palette", 290, 528),
    ("Route 1", 292, 508),
    ("Jadielle", 299, 414),
    ("Route 22", 252, 390),
    ("Route 2", 297, 338),
    ("Forêt de Jade", 296, 296),
    ("Argenta", 303, 200),
]
SIGNS = [(name, *from_map(u, v)) for name, u, v in _SIGNS]

# Point de départ du joueur : entre les deux maisons, face au nord (la sortie vers la Route 1).
PLAYER_START = _offset("Bourg Palette", 15, 0)


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
    wobble = (fractal_noise(x, y, 45, 3, 21) - 0.5) * 2  # entre -1 et 1
    best, best_name = 1e9, None
    for name, cu, cv, ru, rv in AREAS:
        norm = math.hypot((u - cu) / ru, (v - cv) / rv)
        distance = (norm - 1 - wobble * 0.15) * min(ru, rv) * MAP_SCALE
        if distance < best:
            best, best_name = distance, name
    for name, half_width, points in ROUTES:
        segment = min(_segment_distance(u, v, *a, *b) for a, b in zip(points, points[1:]))
        distance = (segment - half_width * (1 + wobble * 0.3)) * MAP_SCALE
        if distance < best:
            best, best_name = distance, name
    return best, best_name


def find_zone(x, y):
    distance, name = zone_distance(x, y)
    return name if distance <= 0 else None


def height_at(x, y):
    """Hauteur du sol en mètres au point (x, y)."""
    fine, wide = land_at(x, y)
    if wide <= 0.0 and fine <= 0.0:
        return -25.0

    distance, _ = zone_distance(x, y)
    t = max(0.0, min(1.0, (x - _FLOOR_X0) / (_FLOOR_X1 - _FLOOR_X0)))
    floor = FLOOR_SOUTH + (FLOOR_NORTH - FLOOR_SOUTH) * t
    ground = floor + (fractal_noise(x, y, 60, 3, 1) - 0.5) * 0.8

    # Hors des lieux : talus de 8 m trop raide pour être gravi, puis collines boisées.
    outside = max(0.0, distance)
    bank = 8.0 * (1 - math.exp(-outside / 2.5))
    hills = fractal_noise(x, y, 180, 5, 7) * 30.0 * smoothstep(0, 90, outside)
    land = ground + bank + hills

    u, v = to_map(x, y)
    for cu, cv, radius, peak in MOUNTAINS:
        mask = max(0.0, 1 - math.hypot(u - cu, v - cv) / radius)
        if mask > 0:
            ridges = 0.6 + 0.4 * fractal_noise(x, y, 120, 4, 11)
            land += peak * mask ** 1.5 * ridges * smoothstep(5, 45, outside)

    # Côtes : la terre descend vers la mer, plage douce ou petite falaise.
    land *= 0.25 + 0.75 * smoothstep(0.35, 0.8, wide)
    sea = SEA_LEVEL - 2.0 - 18.0 * (1 - wide)
    coast = smoothstep(0.35, 0.65, fine)
    height = sea + (land - sea) * coast
    return max(-40.0, min(240.0, height))
