"""Plan du monde ouvert de la V1 : de Bourg Palette à Argenta (SCRUM-62).

Fichier de données partagé, en Python pur (sans numpy) :
- make_heightmap.py s'en sert pour fabriquer le relief (heightmap 16 bits) ;
- blockout_kanto_v1.py s'en sert dans Unreal pour poser les bâtiments à la bonne hauteur.

Repère : x = nord, y = est, en mètres. Le coin (0, 0) est le coin du Landscape.
Échelle : vraies tailles (un personnage fait 1,80 m), pas l'échelle des cases du jeu.
Les positions suivent la carte de Rouge Feu / Vert Feuille, à vérifier avec les fiches Miro.
"""

import math

# Landscape : 1009 × 1009 points, un point tous les 1,5 m, soit environ 1,5 km de côté.
HEIGHTMAP_SIZE = 1009
METERS_PER_PIXEL = 1.5
WORLD_SIZE = (HEIGHTMAP_SIZE - 1) * METERS_PER_PIXEL

SEA_LEVEL = 0.0

# Zones où l'on marche : (nom, x_min, x_max, y_min, y_max, hauteur au début, hauteur à la fin, axe).
# La hauteur monte doucement le long de l'axe (« x » : du sud au nord, « -y » : d'est en ouest).
ZONES = [
    ("Bourg Palette", 160, 270, 690, 810, 4.0, 4.0, "x"),
    ("Route 1", 270, 500, 715, 785, 4.0, 8.0, "x"),
    ("Jadielle", 500, 640, 670, 830, 8.0, 8.0, "x"),
    ("Route 22", 540, 610, 430, 670, 8.0, 6.0, "-y"),
    ("Route 2 (sud)", 640, 740, 720, 780, 8.0, 10.0, "x"),
    ("Forêt de Jade", 740, 920, 650, 850, 10.0, 12.0, "x"),
    ("Route 2 (nord)", 920, 970, 720, 780, 12.0, 15.0, "x"),
    ("Argenta", 970, 1090, 670, 830, 15.0, 15.0, "x"),
]

# Montagnes autour d'Argenta (le Mont Sélénite est à l'est) : centre, rayon, hauteur max.
MOUNTAINS = [
    ((1200, 930), 380, 170.0),
    ((1150, 1250), 300, 120.0),
]


def coast_x(y):
    """Limite nord de la mer. Une petite crique entre dans Bourg Palette en bas à gauche."""
    x = 150 + 6 * math.sin(y / 70.0)
    if 680 < y < 745:
        x = max(x, 175 - 0.02 * (y - 712) ** 2)
    return x


# Bâtiments : (zone, nom, centre x, centre y, profondeur x, largeur y, hauteur, couleur).
# Toutes les portes sont au sud, comme dans le jeu d'origine. Tailles réelles,
# assez grandes pour y mettre plus tard un intérieur sans chargement.
BUILDINGS = [
    ("Bourg Palette", "Maison du joueur", 245, 725, 9, 11, 7, "Wall"),
    ("Bourg Palette", "Maison du rival", 245, 775, 9, 11, 7, "Wall"),
    ("Bourg Palette", "Labo du Prof. Chen", 195, 782, 14, 22, 8, "Lab"),
    ("Jadielle", "Centre Pokémon", 535, 712, 18, 22, 8, "Center"),
    ("Jadielle", "Boutique", 590, 700, 14, 16, 7, "Mart"),
    ("Jadielle", "École des dresseurs", 575, 765, 14, 18, 7, "Wall"),
    ("Jadielle", "Arène (fermée)", 612, 805, 26, 30, 11, "Gym"),
    ("Jadielle", "Maison", 530, 805, 10, 11, 7, "Wall"),
    ("Argenta", "Arène de Pierre", 1025, 705, 26, 28, 11, "Gym"),
    ("Argenta", "Musée des sciences", 1072, 715, 20, 36, 12, "Lab"),
    ("Argenta", "Centre Pokémon", 1000, 772, 18, 22, 8, "Center"),
    ("Argenta", "Boutique", 1048, 795, 14, 16, 7, "Mart"),
    ("Argenta", "Maison", 995, 815, 10, 11, 7, "Wall"),
]

# Postes de passage (on les traverse) : (nom, centre x, centre y, axe du passage).
GATES = [
    ("Poste de la Forêt (sud)", 740, 750, "x"),
    ("Poste de la Forêt (nord)", 920, 750, "x"),
    ("Poste de la Ligue (fermé)", 575, 430, "y"),
]

# Chemins de terre : listes de points (x, y).
PATHS = [
    ("Chemin Bourg Palette", [(205, 750), (250, 750), (270, 750)]),
    ("Chemin Route 1", [(270, 750), (310, 738), (350, 762), (400, 742), (450, 765), (500, 750)]),
    ("Chemin Jadielle", [(500, 750), (640, 750)]),
    ("Chemin Jadielle ouest", [(575, 750), (575, 670)]),
    ("Chemin Route 22", [(575, 670), (580, 600), (565, 520), (575, 430)]),
    ("Chemin Route 2", [(640, 750), (690, 745), (740, 750)]),
    ("Chemin Route 2 nord", [(920, 750), (970, 750)]),
    ("Chemin Argenta", [(970, 750), (1060, 750)]),
]


def forest_path_y(x):
    """Le chemin de la Forêt de Jade serpente entre les deux postes."""
    t = (x - 740) / 180.0
    return 750 + 60 * math.sin(t * math.pi * 2) * math.sin(t * math.pi)


FOREST_PATH = [(x, forest_path_y(x)) for x in range(740, 921, 10)]

# Hautes herbes : (zone, centre x, centre y, profondeur x, largeur y).
TALL_GRASS = [
    ("Route 1", 320, 735, 25, 20),
    ("Route 1", 380, 770, 30, 15),
    ("Route 1", 445, 730, 20, 25),
    ("Route 22", 575, 600, 22, 30),
    ("Route 22", 560, 500, 25, 25),
    ("Route 2 (sud)", 690, 768, 20, 15),
    ("Forêt de Jade", 790, 700, 30, 30),
    ("Forêt de Jade", 850, 800, 30, 30),
    ("Forêt de Jade", 890, 720, 20, 30),
]

# Point de départ du joueur : devant sa maison, face au nord (la sortie vers la Route 1).
PLAYER_START = (232, 740)


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


def _zone_floor(zone, x, y):
    _, x0, x1, y0, y1, h0, h1, axis = zone
    if axis == "x":
        t = (x - x0) / (x1 - x0)
    else:
        t = (y1 - y) / (y1 - y0)
    t = max(0.0, min(1.0, t))
    return h0 + (h1 - h0) * t


def _distance_to_zone(zone, x, y):
    _, x0, x1, y0, y1 = zone[:5]
    dx = max(x0 - x, 0.0, x - x1)
    dy = max(y0 - y, 0.0, y - y1)
    return math.hypot(dx, dy)


def find_zone(x, y):
    for zone in ZONES:
        if _distance_to_zone(zone, x, y) == 0.0:
            return zone[0]
    return None


def height_at(x, y):
    """Hauteur du sol en mètres au point (x, y)."""
    # Sol des zones : moyenne pondérée des zones proches, pour éviter les marches
    # là où la zone la plus proche change.
    distances = [(_distance_to_zone(zone, x, y), zone) for zone in ZONES]
    distance = min(d for d, _ in distances)
    weights = [(math.exp(-(d - distance) / 25.0), zone) for d, zone in distances]
    total = sum(w for w, _ in weights)
    floor = sum(w * _zone_floor(zone, x, y) for w, zone in weights) / total

    # Sol presque plat dans les zones, avec de légères ondulations.
    ground = floor + (fractal_noise(x, y, 60, 3, 1) - 0.5) * 0.8

    # Hors des zones : talus raide de 8 m (impossible à gravir, même en sautant),
    # puis collines boisées qui ondulent entre 10 et 35 m.
    bank = 8.0 * (1 - math.exp(-distance / 2.5))
    hills = fractal_noise(x, y, 180, 5, 7) * 28.0 * smoothstep(0, 90, distance)
    land = ground + bank + hills

    for (mx, my), radius, peak in MOUNTAINS:
        mask = max(0.0, 1 - math.hypot(x - mx, y - my) / radius)
        ridges = 0.6 + 0.4 * fractal_noise(x, y, 120, 4, 11)
        land += peak * mask ** 1.5 * ridges * smoothstep(5, 45, distance)

    # Mer au sud : la plage descend doucement sous l'eau.
    sea = coast_x(y)
    sea_profile = SEA_LEVEL - 1.0 - min(20.0, max(0.0, sea - x) * 0.12)
    sea_factor = smoothstep(sea + 22, sea - 6, x)
    height = land + (sea_profile - land) * sea_factor
    return max(-40.0, min(240.0, height))
