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

import json
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
    # Le reste de Kanto : centre et cadre de chaque ville relevés sur la carte FRLG (cases),
    # ramenés à TOWN_SCALE mètres par case (voir plus bas).
    ("Azuria", 578, 116, 96, 60, 4),
    ("Céladopole", 420, 280, 145, 89, 4),
    ("Safrania", 576, 281, 102, 92, 4),
    ("Carmin sur Mer", 580, 428, 80, 61, 4),
    ("Lavanville", 790, 272, 55, 69, 4),
    ("Parmanie", 433, 634, 112, 78, 4),
    ("Cramois'Île", 148, 773, 50, 41, 4),
    ("Plateau Indigo", 25, 29, 42, 28, 4),
]

# Dans les villes, la carte FRLG est ramenée à TOWN_SCALE mètres par case (au lieu de CELL
# sur les routes) : les bâtiments, à leur vraie taille, gardent leur place relative du jeu
# sans que la ville fasse 600 m de large.
TOWN_SCALE = 2.5

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

# Types de bâtiments du reste de Kanto : (profondeur x, largeur y, hauteur, mur, toit, style).
BUILDING_TYPES = {
    "Maison": (9, 11, 6.5, "House", None, "pignon"),      # toit de couleur variée
    "Centre": (18, 22, 8, "House", "Center", "plat"),
    "Boutique": (14, 16, 7, "House", "Mart", "plat"),
    "Commerce": (12, 14, 7, "House", "RoofBrown", "plat"),
    "Arène": (26, 30, 11, "Gym", "RoofGrey", "plat"),
    "Grand Magasin": (30, 40, 28, "Lab", "RoofGrey", "plat"),
    "Résidence": (20, 24, 18, "House", "RoofBrown", "plat"),
    "Tour Sylphe": (36, 36, 60, "Lab", "RoofGrey", "plat"),
    "Tour": (18, 18, 40, "Gym", "RoofBrown", "plat"),
    "Casino": (22, 30, 10, "House", "RoofRed", "plat"),
    "Dojo": (20, 24, 9, "House", "RoofBrown", "pignon"),
    "Manoir": (24, 26, 12, "Gym", "RoofBrown", "plat"),
    "Labo": (16, 24, 8, "Lab", "RoofGrey", "plat"),
    "Ligue": (30, 50, 18, "Lab", "RoofRed", "plat"),
    "Centrale": (24, 40, 12, "Gym", "RoofGrey", "plat"),
}
HOUSE_ROOFS = ["RoofRed", "RoofBlue", "RoofGreen", "RoofBrown"]

# Bâtiments du reste de Kanto, relevés sur la carte FRLG : (zone, nom, colonne, ligne, type).
# Dans une ville (zone = nom d'une ville des AREAS), la position est ramenée à TOWN_SCALE
# autour du centre de la ville ; ailleurs (sur une route), c'est la vraie case de la carte.
_FRLG_BUILDINGS = [
    ("Azuria", "Centre Pokémon", 571, 111, "Centre"),
    ("Azuria", "Arène d'Azuria", 590, 116, "Arène"),
    ("Azuria", "Boutique", 576, 131, "Boutique"),
    ("Azuria", "Magasin de vélos", 561, 129, "Commerce"),
    ("Azuria", "Maison", 553, 111, "Maison"),
    ("Azuria", "Maison", 553, 97, "Maison"),
    ("Azuria", "Maison", 594, 97, "Maison"),
    ("Azuria", "Maison cambriolée", 610, 96, "Maison"),
    ("Azuria", "Maison", 589, 131, "Maison"),
    ("Céladopole", "Grand Magasin", 386, 262, "Grand Magasin"),
    ("Céladopole", "Résidence Céladon", 368, 255, "Résidence"),
    ("Céladopole", "Centre Pokémon", 457, 261, "Centre"),
    ("Céladopole", "Casino", 437, 291, "Casino"),
    ("Céladopole", "Échange de lots", 455, 291, "Commerce"),
    ("Céladopole", "Arène de Céladopole", 378, 302, "Arène"),
    ("Céladopole", "Restaurant", 468, 276, "Commerce"),
    ("Céladopole", "Hôtel", 420, 291, "Commerce"),
    ("Céladopole", "Maison", 420, 255, "Maison"),
    ("Céladopole", "Maison", 440, 256, "Maison"),
    ("Safrania", "Sylphe SARL", 576, 273, "Tour Sylphe"),
    ("Safrania", "Centre Pokémon", 552, 301, "Centre"),
    ("Safrania", "Boutique", 601, 265, "Boutique"),
    ("Safrania", "Dojo", 590, 250, "Dojo"),
    ("Safrania", "Arène de Safrania", 606, 250, "Arène"),
    ("Safrania", "Maison de M. Psy", 548, 284, "Maison"),
    ("Safrania", "Maison", 548, 255, "Maison"),
    ("Safrania", "Maison", 548, 270, "Maison"),
    ("Safrania", "Maison", 605, 284, "Maison"),
    ("Safrania", "Maison", 560, 312, "Maison"),
    ("Safrania", "Maison", 595, 312, "Maison"),
    ("Carmin sur Mer", "Centre Pokémon", 561, 410, "Centre"),
    ("Carmin sur Mer", "Boutique", 586, 432, "Boutique"),
    ("Carmin sur Mer", "Fan-club Pokémon", 553, 432, "Commerce"),
    ("Carmin sur Mer", "Arène de Carmin", 600, 445, "Arène"),
    ("Carmin sur Mer", "Gare maritime", 575, 448, "Commerce"),
    ("Carmin sur Mer", "Maison", 582, 412, "Maison"),
    ("Carmin sur Mer", "Maison", 565, 440, "Maison"),
    ("Lavanville", "Tour Pokémon", 803, 255, "Tour"),
    ("Lavanville", "Centre Pokémon", 781, 268, "Centre"),
    ("Lavanville", "Boutique", 797, 289, "Boutique"),
    ("Lavanville", "Maison du Nommeur", 785, 280, "Maison"),
    ("Lavanville", "Maison", 783, 294, "Maison"),
    ("Parmanie", "Zone Safari (entrée)", 432, 612, "Commerce"),
    ("Parmanie", "Centre Pokémon", 437, 659, "Centre"),
    ("Parmanie", "Boutique", 410, 659, "Boutique"),
    ("Parmanie", "Arène de Parmanie", 395, 655, "Arène"),
    ("Parmanie", "Maison du Gardien", 430, 630, "Maison"),
    ("Parmanie", "Maison", 403, 615, "Maison"),
    ("Parmanie", "Maison", 458, 615, "Maison"),
    ("Parmanie", "Maison", 403, 637, "Maison"),
    ("Parmanie", "Maison", 462, 632, "Maison"),
    ("Parmanie", "Maison", 470, 655, "Maison"),
    ("Cramois'Île", "Manoir Pokémon", 136, 763, "Manoir"),
    ("Cramois'Île", "Labo Pokémon", 138, 778, "Labo"),
    ("Cramois'Île", "Arène de Cramois'Île", 158, 778, "Arène"),
    ("Cramois'Île", "Centre Pokémon", 160, 764, "Centre"),
    ("Plateau Indigo", "Ligue Pokémon", 25, 24, "Ligue"),
    # Sur les routes.
    ("Route 4", "Centre Pokémon du Mont Sélénite", 322, 152, "Centre"),
    ("Route 10", "Centre Pokémon du Tunnel Roche", 772, 140, "Centre"),
    ("Route 10", "Centrale", 800, 190, "Centrale"),
    ("Route 5", "Pension", 562, 190, "Maison"),
    ("Route 25", "Maison de Léo", 735, 22, "Maison"),
]


def _frlg_position(zone, column, row):
    """Case FRLG -> mètres : ramenée à TOWN_SCALE dans une ville, vraie case sur une route."""
    for name, c0, r0, *_ in AREAS:
        if name == zone:
            cx, cy = from_frlg(c0, r0)
            return cx - (row - r0) * TOWN_SCALE, cy + (column - c0) * TOWN_SCALE
    return from_frlg(column, row)


def _frlg_buildings():
    result = []
    for index, (zone, name, column, row, kind) in enumerate(_FRLG_BUILDINGS):
        depth, width, height, wall, roof, style = BUILDING_TYPES[kind]
        roof = roof or HOUSE_ROOFS[index % len(HOUSE_ROOFS)]
        result.append((zone, name, *_frlg_position(zone, column, row), depth, width, height, wall, roof, style))
    return result


BUILDINGS += _frlg_buildings()

# Intérieurs (SCRUM-25) : on entre sans chargement. Chaque bâtiment est creux, avec une
# porte au sud, un sol, un plafond à CEILINGS mètres et ces meubles de blockout :
# (nom, centre x, centre y, profondeur x, largeur y, hauteur, couleur), en mètres depuis le
# centre du bâtiment (x = nord, la porte est au sud). Le nom du bâtiment sert de clé.
INTERIORS = {
    "Maison du joueur": [
        ("Table", 0.0, -1.0, 1.4, 2.0, 0.75, "Wood"),
        ("Télé", 3.7, -3.2, 0.6, 1.4, 1.1, "Stone"),
        ("Cuisine", 3.8, -0.2, 0.7, 2.4, 0.95, "Fence"),
        ("Escalier", 2.4, 4.4, 3.2, 1.2, 2.9, "Wood"),
    ],
    "Maison du rival": [
        ("Table", 0.0, 0.0, 1.4, 2.0, 0.75, "Wood"),
        ("Bibliothèque", 3.9, -3.0, 0.5, 2.0, 2.0, "Wood"),
        ("Plante", 3.6, 4.4, 0.6, 0.6, 1.2, "Leaves"),
    ],
    "Labo du Prof. Chen": [
        ("Table des Poké Balls", 2.5, 0.0, 1.0, 2.4, 0.95, "Mart"),
        ("Bibliothèque ouest", 4.9, -6.0, 0.5, 5.0, 2.4, "Wood"),
        ("Bibliothèque est", 4.9, 6.0, 0.5, 5.0, 2.4, "Wood"),
        ("Machine", 0.5, -8.4, 1.5, 1.0, 1.8, "Stone"),
        ("Bureau", -2.0, 6.0, 1.2, 2.4, 0.8, "Wood"),
    ],
    "Centre Pokémon": [
        ("Comptoir", 4.0, 0.0, 1.0, 6.0, 1.1, "Center"),
        ("Banquette", -5.0, -8.0, 1.0, 3.0, 0.5, "Wood"),
    ],
    "Boutique": [
        ("Comptoir", -3.0, -4.0, 3.0, 1.0, 1.1, "Mart"),
        ("Rayons", 3.0, 3.0, 1.0, 6.0, 1.8, "Wood"),
    ],
}
# Hauteur sous plafond par style de toit (une maison a un étage au-dessus, pas encore fait).
CEILINGS = {"pignon": 3.0, "plat": 4.5}
# Bâtiments dont la porte reste fermée à clé (pas encore prévus dans la V1).
LOCKED = {"Arène (fermée)"}
DOOR_WIDTH = 1.4
DOOR_HEIGHT = 2.4
WALL_THICKNESS = 0.3

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
DECOR += [("Céladopole", "Water", *_frlg_position("Céladopole", 399, 285), 20, 15)]
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
    ("Poste de la Ligue", *from_frlg(36, 404), "y"),
    # Les quatre postes autour de Safrania.
    ("Poste de la Route 5", *from_frlg(577, 236), "x"),
    ("Poste de la Route 6", *from_frlg(576, 326), "x"),
    ("Poste de la Route 7", *from_frlg(526, 281), "y"),
    ("Poste de la Route 8", *from_frlg(626, 281), "y"),
]

# Entrées de grottes : (nom, colonne, ligne, orientation de l'ouverture en degrés,
# 0 = vers le nord, 90 = vers l'est). Un porche sombre posé contre la pente.
CAVES = [
    ("Mont Sélénite (entrée)", 352, 148, 270),
    ("Mont Sélénite (sortie)", 330, 112, 90),
    ("Tunnel Roche (nord)", 776, 152, 0),
    ("Tunnel Roche (sud)", 782, 228, 180),
    ("Route Victoire (bas)", 24, 122, 180),
    ("Route Victoire (haut)", 24, 92, 0),
    ("Grotte Taupiqueur (ouest)", 160, 196, 90),
    ("Grotte Taupiqueur (est)", 622, 436, 270),
    ("Caverne Azurée", 548, 94, 90),
    ("Îles Écume", 300, 778, 0),
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
    # Le reste de Kanto (cases FRLG, au milieu des routes).
    ("Chemin Route 3", [(190, 160), (230, 166), (270, 156), (300, 160), (330, 152), (352, 148)]),
    ("Chemin Route 4", [(330, 112), (380, 116), (430, 112), (480, 116), (540, 116)]),
    ("Chemin Route 24", [(578, 92), (570, 70), (566, 45), (570, 18)]),
    ("Chemin Route 25", [(570, 18), (620, 15), (680, 16), (735, 20)]),
    ("Chemin Route 5", [(578, 140), (578, 190), (577, 244)]),
    ("Chemin Route 6", [(576, 318), (576, 360), (578, 403)]),
    ("Chemin Route 7", [(535, 281), (505, 283), (478, 281)]),
    ("Chemin Route 8", [(617, 281), (680, 281), (740, 283), (768, 276)]),
    ("Chemin Route 9", [(617, 110), (660, 112), (720, 110), (775, 112)]),
    ("Chemin Route 10", [(775, 112), (776, 152), (782, 228), (790, 245)]),
    ("Chemin Route 11", [(612, 428), (650, 438), (700, 438), (760, 440)]),
    ("Chemin Route 12", [(790, 300), (792, 360), (790, 440), (788, 540)]),
    ("Chemin Route 13", [(788, 545), (740, 555), (680, 552), (640, 556)]),
    ("Chemin Route 14", [(640, 556), (636, 600), (632, 635)]),
    ("Chemin Route 15", [(632, 638), (560, 642), (478, 640)]),
    ("Chemin Route 16", [(362, 278), (320, 276), (290, 282)]),
    ("Piste cyclable", [(290, 282), (290, 400), (290, 520), (290, 640)]),
    ("Chemin Route 18", [(290, 640), (340, 642), (388, 640)]),
    ("Chemin Route 19", [(433, 665), (432, 700)]),
    ("Chemin Route 23", [(36, 404), (24, 380), (24, 300), (22, 200), (24, 122)]),
    ("Chemin du Plateau Indigo", [(24, 92), (24, 60), (25, 40)]),
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

# Hautes herbes : relevées automatiquement sur la carte FRLG par make_frlg_mask.py
# (kanto_herbes.json, rectangles en cases : colonne, ligne, largeur, hauteur).
# On garde celles des routes, pas les taches trouvées par erreur dans les villes.
# Chaque carré : (table de DT_Encounters, x, y, profondeur, largeur, taux de rencontre).
def _tall_grass():
    with open(os.path.join(HERE, "kanto_herbes.json"), encoding="utf-8") as handle:
        rectangles = json.load(handle)
    result = []
    for column, row, wide, high in rectangles:
        x, y = from_frlg(column + wide / 2, row + high / 2)
        if any(abs(x - from_frlg(c, r)[0]) < (high * CELL / 2 + hn + 20)
               and abs(y - from_frlg(c, r)[1]) < (wide * CELL / 2 + he + 20)
               for name, c, r, he, hn, _ in AREAS if name != "Forêt de Jade"):
            continue
        table, rate = _encounter_table(x, y)
        result.append((table, x, y, high * CELL, wide * CELL, rate))
    return result


def _encounter_table(x, y):
    """Table de DT_Encounters et taux de rencontre (3G) des herbes au point (x, y) : la Forêt de Jade
    si on est dedans, sinon la route dont le chemin passe le plus près."""
    fx, fy = area_center("Forêt de Jade")
    _, _, _, half_ew, half_ns, _ = next(a for a in AREAS if a[0] == "Forêt de Jade")
    if ((x - fx) / half_ns) ** 2 + ((y - fy) / half_ew) ** 2 <= 1.0:
        return "ViridianForest_Walk", 14
    best, best_distance = "Route1_Walk", float("inf")
    for name, points in PATHS:
        table = ROUTE_TABLES.get(name)
        if table is None:
            continue
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            dx, dy = x1 - x0, y1 - y0
            t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / max(dx * dx + dy * dy, 1e-6)))
            distance = math.hypot(x - (x0 + dx * t), y - (y0 + dy * t))
            if distance < best_distance:
                best, best_distance = table, distance
    return best, 21


# Chemin -> table des hautes herbes de DT_Encounters (Rouge Feu). Les chemins des villes n'en ont pas ;
# les routes de mer (19, 20, 21) n'ont pas de chemin à terre.
ROUTE_TABLES = {f"Chemin Route {n}": f"Route{n}_Walk"
                for n in (1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 22, 23, 24, 25)}
ROUTE_TABLES.update({
    "Chemin Route 2": "Route2_SouthTowardsViridianCity_Walk",
    "Chemin Route 2 nord": "Route2_SouthTowardsViridianCity_Walk",
    "Piste cyclable": "Route17_Walk",
    "Chemin du Plateau Indigo": "Route23_Walk",
})


TALL_GRASS = _tall_grass()

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
# Le reste de Kanto : un panneau au sud de chaque ville.
_SIGNS += [(name, name, -hn - 6, 8) for name, _, _, _, hn, _ in AREAS
           if name not in ("Bourg Palette", "Jadielle", "Argenta", "Forêt de Jade")]
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
