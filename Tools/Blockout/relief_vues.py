"""Relief v3 « des choses à voir au loin » : part du relief que L'amie a sculpté dans Unreal et
casse le plat, sans tout sculpter à la main.

Entrée : reference/relief_amie_2026-11-03.png, la heightmap exportée d'Unreal
(Paysage > Gestion > Importer/Exporter > Exporter, échelle Z 200 au moment de l'export).

Sortie : kanto_v1_heightmap.png (à réimporter avec l'échelle Z de kanto_v1_layout.LANDSCAPE_SCALE_Z)
et deux aperçus (vue du dessus ombrée, heightmap en gris).

Lancement : pip install numpy numba scipy pillow, puis
    python Tools/Blockout/relief_vues.py            (pleine résolution, quelques minutes)
    python Tools/Blockout/relief_vues.py --test DIR (aperçus seulement, dans DIR)

Ce que fait le script, dans l'ordre :
1. Nettoyage du fichier de L'amie : les bosses rondes du pinceau sont fondues dans le terrain,
   les trous sous le fond de la mer sont rebouchés (ses lacs restent).
2. Collines douces partout (sauf dans les villes, qui restent plates), forêts en collines qui
   grandissent loin des routes : l'horizon bouge tout le temps.
3. Grands repères visibles de loin : Plateau Indigo tout en haut, Route Victoire, Mont Sélénite,
   montagnes de la côte nord, Tunnel Roche, volcan de Cramois'Île.
4. Hors de la zone jouable : le Mont Argenté vers Johto (à l'ouest) et des îles montagneuses au
   large (« une autre région au loin »).
5. Érosion par la pluie sur les nouveaux reliefs (vallées et arêtes naturelles).
6. Cascade sur la Route 23 : une falaise, un torrent qui descend du massif, un bassin.
7. Chemins adoucis : plus de marche que le personnage ne peut pas monter.
"""

import os
import sys
import time

import numpy as np
from PIL import Image
from numba import njit
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kanto_v1_layout as layout  # noqa: E402
import make_heightmap as mh  # noqa: E402
from make_heightmap import fbm, ridged, smoothstep, sample  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "reference", "relief_amie_2026-11-03.png")
SOURCE_SCALE_Z = 200.0  # échelle Z du Landscape quand L'amie a exporté son fichier
SIZE = layout.HEIGHTMAP_SIZE
STEP = layout.METERS_PER_PIXEL
COARSE = 4  # les grandes formes sont calculées sur une grille 4 fois plus large (6 m)
SEED = 251

TEST = "--test" in sys.argv
OUT = sys.argv[sys.argv.index("--test") + 1] if TEST else HERE

# --- Réglages (mètres) -------------------------------------------------------------------
BLOB_MELT = 22.0          # les bosses du pinceau sont étalées sur cette distance
LAKE_FLOOR = -6.0         # fond le plus bas d'un lac sur la terre (le trou est rebouché)
WATER_FLOOR = 12.0        # l'eau creusée par L'amie descend au plus à -12 m
ROLLING = 16.0            # collines douces partout, routes comprises (pentes < 10 %)
ROLLING_SCALE = 650.0     # distance entre deux collines
FOREST_HILLS = 70.0       # collines boisées loin des routes
FOREST_START, FOREST_FULL = 25.0, 260.0  # elles montent depuis le bord des routes
TOWN_FALLOFF = 90.0       # autour des villes, le terrain rejoint la ville en pente douce
PATH_SMOOTH = 25.0        # lissage des chemins (pas de marches)
PATH_MAX_SLOPE = 0.15     # pente maximale dans les routes (8,5°) : le personnage est petit
PATH_EDGE = 60.0          # le bord des routes rejoint le relief sur cette distance
EMBANK_SLOPE = 0.6        # bords de route (remblai ou déblai) : 31° en moyenne (de 17 à 42°)

# Où mettre montagnes et forêts : la carte de L'amie (reference/kanto_relief_amie.png, 1009 × 1009,
# nord en haut, même cadre que la heightmap). Brun/gris/blanc = montagne, vert foncé = forêt.
ZONES = os.path.join(HERE, "reference", "kanto_relief_amie.png")
MOUNTAIN_HEIGHT = 330.0   # montagnes de la carte (les zones enneigées montent plus haut)
SNOW_EXTRA = 160.0
INDIGO_TOP = 640.0        # le point le plus haut du jeu
# Repère en plus de la carte : (nom, colonne FRLG, ligne FRLG, rayon m, hauteur m).
VOLCANO = ("Volcan de Cramois'Île", 118, 752, 330, 230)
# Plateau Indigo : table plate au sommet, falaises tout autour (on y monte par la Route Victoire,
# une grotte : le relief n'a pas besoin de chemin praticable jusqu'en haut).
INDIGO = (25, 29, 140.0)  # colonne, ligne, rayon du sommet plat (m)

# Altitude des villes (m) : plus de contraste entre elles ; le terrain suit sur TOWN_RAMP m
# autour, et les routes y montent en pente douce. Les villes absentes gardent leur hauteur.
TOWN_LEVELS = {"Argenta": 200.0, "Lavanville": 150.0, "Azuria": 70.0, "Safrania": 85.0}
TOWN_RAMP = 650.0

# Montagnes en plus, demandées par L'amie (entourées en rouge sur l'essai 1) :
# (colonne, ligne sur la carte 1009 × 1009 nord en haut, rayon en pixels, hauteur m).
EXTRA_MOUNTAINS = [
    (340, 182, 45, 280),     # entre la côte nord et la Route 3
    (747, 127, 22, 150),     # au bord du lac au nord d'Azuria
    (817, 280, 70, 340),     # à l'est d'Azuria, au-dessus de la Route 9
    (117, 566, 50, 230),     # au sud de la Route 22
] + [(993, row, 30, 230) for row in range(175, 430, 40)]  # chaîne le long de la côte est

# Lacs : les zones bleues de la carte de L'amie à l'intérieur des terres (pas la baie, pas la
# piste cyclable). Fond de LAKE_DEPTH m, berges en pente douce sur LAKE_BANK m.
LAKE_DEPTH = 6.0
LAKE_BANK = 55.0
LAKE_MIN, LAKE_MAX = 25, 5000          # taille en pixels de la carte (6 m)
CYCLING_ROAD = (370, 395, 415, 725)    # colonnes et lignes à ignorer (bandes d'eau FRLG)
TALUS_SLOPE = 0.45        # rebords arrondis hors montagnes (24°) : plus de talus nets
TALUS_CUT = 14.0          # ... sur 14 m de haut au plus (au-delà, c'est une falaise)
CLIFF_SLOPE = 1.0         # falaises hors montagnes couchées à 45° au plus (plus de murs)
EDGE_SOFTEN = 4.5         # lissage final des arêtes (m)
WARP = 28.0               # les bords des couloirs ondulent de ± WARP m (fini les lignes droites)

# Hors de la zone jouable : (nom, x nord m, y est m, rayon, hauteur).
FAR_MOUNTAINS = [
    ("Mont Argenté", 5150, 120, 700, 560),
    ("Chaîne vers Johto", 4300, 60, 600, 380),
    ("Île du sud-ouest", 250, 420, 420, 260),
    ("Îlot du sud", 120, 1500, 330, 170),
    ("Île de l'est", 700, 5900, 380, 210),
]

# Cascade de la Route 23 : bassin au bord de la route, lèvre de la chute, source dans le massif.
FALL_POOL = (3, 215)        # case FRLG du bassin (bord ouest de la Route 23)
FALL_HEIGHT = 32.0          # hauteur de la chute
FALL_SOURCE = (-28, 170)    # le torrent descend de là (case FRLG, à l'ouest de la route)


def frlg_xy(column, row):
    return layout.from_frlg(column, row)


# --- 1. Lecture et nettoyage -----------------------------------------------------------------

def read_heights(path, scale_z):
    values = np.asarray(Image.open(path)).astype(np.float64)
    return (values - 32768.0) * scale_z / 12800.0


def clean(amie, before):
    """Fond les bosses rondes (dans les retouches de L'amie par rapport au relief v2) et
    rebouche les trous ; les grandes retouches restent. Renvoie aussi l'eau qu'elle a creusée
    (sous le niveau de la mer, sur la terre du relief v2) : elle est remise à la fin."""
    diff = amie - before
    raised = ndimage.gaussian_filter(np.maximum(diff, 0), BLOB_MELT / STEP)
    dug = np.maximum(np.minimum(diff, 0), np.minimum(LAKE_FLOOR - before, 0))
    dug = ndimage.gaussian_filter(dug, 2.0)
    water = ndimage.binary_opening((amie < 0.0) & (before > 0.3), iterations=2)
    water_bed = np.maximum(amie, -WATER_FLOOR)  # le trou à -512 m devient un fond de lac
    return before + raised + dug, water, water_bed


def town_levels(height):
    now = {}
    for name, column, row, half_east, half_north, _ in layout.AREAS:
        cx, cy = frlg_xy(column, row)
        r, c = int(cy / STEP), int(cx / STEP)
        part = height[r - int(half_east / STEP):r + int(half_east / STEP),
                      c - int(half_north / STEP):c + int(half_north / STEP)]
        now[name] = float(np.median(part))
    return now


# --- 2 à 4. Grandes formes (grille de 6 m) ---------------------------------------------------

def coarse_fields(cells):
    """Distances aux routes et aux villes, en mètres, sur la grille de 6 m."""
    _, ground, *_ = cells
    n = (SIZE - 1) // COARSE + 1
    step = STEP * COARSE
    x, y = mh.grid_xy(range(n), n, step)
    column, row = layout.to_frlg(x, y)
    on_route = sample(ground.astype(np.float64), column + mh.PAD, row + mh.PAD, 0.0) > 0.5
    # Dans les cartes FRLG, seules les cases ouvertes (chemins, herbe, sable, dalles) sont des
    # routes ; les rangées d'arbres deviennent des collines boisées (fini les couloirs carrés).
    frlg = ndimage.binary_dilation(on_route, iterations=3)  # toute la carte FRLG (arbres compris)
    on_route &= open_tiles()
    # Bouts de route isolés (cases mal lues) : retirés, sinon ils font des marches.
    labels, count = ndimage.label(on_route)
    sizes = ndimage.sum(on_route, labels, range(1, count + 1))
    on_route = np.isin(labels, np.nonzero(sizes >= 40)[0] + 1)
    on_route = ndimage.binary_closing(on_route, iterations=2)
    on_route |= mh.v1_path_distance(x, y) < 8
    route_d = ndimage.distance_transform_edt(~on_route) * step
    town_d = np.full(x.shape, 1e9)
    for name, column0, row0, half_east, half_north, _ in layout.AREAS:
        if name == "Forêt de Jade":
            continue
        cx, cy = frlg_xy(column0, row0)
        town_d = np.minimum(town_d, round_box(x, y, cx, cy, half_north + 15, half_east + 15))
    return x, y, route_d, town_d, frlg


def open_tiles():
    """Cases FRLG où l'on marche, lues sur la carte de L'amie ([ligne = est, colonne = nord])."""
    k = np.asarray(Image.open(ZONES).convert("RGB")).astype(np.float64)
    r, g, b = k[..., 0], k[..., 1], k[..., 2]
    mean = np.dstack([ndimage.uniform_filter(k[..., i], 3) for i in range(3)])
    std = np.sqrt(np.maximum(ndimage.uniform_filter(g ** 2, 3) - ndimage.uniform_filter(g, 3) ** 2, 0))
    tree = ((mean[..., 1] > 140) & (mean[..., 1] < 215) & (mean[..., 0] < 150)
            & (mean[..., 2] < 130) & (std > 22))
    tree = ndimage.binary_closing(tree, iterations=1)
    teal = (g > 170) & (b > 120) & (r < 190)
    sand = (r > 190) & (g > 170) & (b < 170)
    paving = (np.abs(r - g) < 20) & (np.abs(g - b) < 20) & (r > 150)
    tiles = (teal | sand | paving | (std > 15)) & ~tree
    tiles = ndimage.binary_closing(tiles, iterations=2)
    # Petits trous (une case mal lue) bouchés, sinon une bosse pousserait au milieu du chemin.
    holes = ndimage.binary_fill_holes(tiles) & ~tiles
    labels, count = ndimage.label(holes)
    sizes = ndimage.sum(holes, labels, range(1, count + 1))
    tiles |= np.isin(labels, np.nonzero(sizes < 60)[0] + 1)
    return np.transpose(tiles[::-1])


def round_box(x, y, cx, cy, half_x, half_y, corner=45.0):
    """Distance (m) à un rectangle aux coins très arrondis et au bord un peu irrégulier."""
    corner = min(corner, half_x, half_y)
    qx = np.abs(x - cx) - (half_x - corner)
    qy = np.abs(y - cy) - (half_y - corner)
    d = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - corner
    return np.maximum(d + fbm(x, y, 120, 2, SEED + 5) * 12, 0)


def town_shift(x, y, now):
    """Hauteur à ajouter pour amener les villes à TOWN_LEVELS (rampe douce autour)."""
    shift = np.zeros_like(x)
    for name, column, row, half_east, half_north, _ in layout.AREAS:
        if name not in TOWN_LEVELS:
            continue
        cx, cy = frlg_xy(column, row)
        d = round_box(x, y, cx, cy, half_north + 15, half_east + 15)
        shift += (TOWN_LEVELS[name] - now[name]) * smoothstep(TOWN_RAMP, 0, d)
    return shift


def blob(x, y, cx, cy, radius, warp_seed):
    """Distance normalisée (0 au centre, 1 au bord), au contour irrégulier."""
    wx = x + fbm(x, y, radius * 0.8, 3, warp_seed) * radius * 0.35
    wy = y + fbm(x, y, radius * 0.8, 3, warp_seed + 5) * radius * 0.35
    return np.hypot(wx - cx, wy - cy) / radius


def mountain(x, y, cx, cy, radius, top, seed):
    """Montagne en arêtes et pics pointus (comme les Alpes), pas un dôme."""
    d = blob(x, y, cx, cy, radius, seed)
    body = smoothstep(1.0, 0.0, d)
    crest = ridged(x + fbm(x, y, 300, 3, seed + 7) * 120, y, radius * 0.6, 7, seed + 3)
    return top * body ** 1.1 * (0.4 + 0.6 * crest ** 1.4)


def map_zones(route_d, frlg):
    """Champs 0 à 1 (montagne, neige, forêt) lus sur la carte de L'amie, adoucis : pas de bords
    carrés, et rien sur les routes (les tuiles FRLG de la carte ne comptent pas)."""
    k = np.asarray(Image.open(ZONES).convert("RGB")).astype(np.float64)
    k = np.transpose(k[::-1], (1, 0, 2))  # nord en haut -> [ligne = est, colonne = nord]
    r, g, b = k[..., 0], k[..., 1], k[..., 2]
    high, low = k.max(2), k.min(2)
    sat = (high - low) / np.maximum(high, 1)
    sea = (b > g + 10) & (b > r + 30)
    snow = low > 200
    rock = ~sea & (sat < 0.35) & (r >= g - 15) & (high < 200)
    forest = ~sea & (g > r + 15) & (high < 120)
    off_route = route_d > 20
    fields = []
    step = STEP * COARSE
    for i, (mask, reach) in enumerate(((rock | snow, 380.0), (snow, 260.0), (forest | rock | snow, 160.0))):
        mask = mask & off_route
        if i < 2:
            mask &= ~frlg  # les rochers dessinés dans les cartes FRLG ne sont pas des montagnes
        mask = ndimage.binary_opening(mask, iterations=2)
        mask = ndimage.binary_closing(mask, iterations=3)
        # Profondeur dans la zone : les pentes montent depuis le bord (larges zones = hauts sommets).
        depth = ndimage.distance_transform_edt(mask) * step
        fields.append(ndimage.gaussian_filter(smoothstep(0, reach, depth), 3))
    return fields


def big_shapes(x, y, route_d, town_d, land, frlg):
    """Hauteur à ajouter : collines partout, forêts en collines, montagnes de la carte,
    volcan, montagnes lointaines."""
    rock, snow, wood = map_zones(route_d, frlg)
    # Contours irréguliers : on déforme un peu la lecture des champs.
    calm = smoothstep(0, TOWN_FALLOFF, town_d)            # 0 dans les villes
    shore = smoothstep(10, 220, land)                      # 0 sur les plages
    rolling = ROLLING * fbm(x, y, ROLLING_SCALE, 4, SEED) * calm * shore
    # Bords des routes irréguliers : les collines commencent plus ou moins loin (pas de couloir droit).
    route_d = np.maximum(route_d + fbm(x, y, 260, 4, SEED + 2) * 60 - 10, 0)
    away = smoothstep(FOREST_START, FOREST_FULL, route_d)
    texture = np.clip(0.3 + 0.7 * (0.5 + 0.5 * fbm(x, y, 420, 4, SEED + 1)), 0, 1.3)
    forest = FOREST_HILLS * wood * away * shore * calm * texture
    keep_route = smoothstep(20, 160, route_d) * calm
    crest = ridged(x, y, 520, 7, SEED + 3)
    body = rock ** 0.9
    mountains = ((MOUNTAIN_HEIGHT + SNOW_EXTRA * snow)
                 * body * (0.45 + 0.55 * crest ** 1.2) * keep_route * smoothstep(0, 120, land))
    # Hauteur absolue (pas ajoutée au relief déjà sculpté), toujours sous le Plateau Indigo.
    cap = INDIGO_TOP - 90
    mountains = np.where(mountains > cap - 80, cap - 80 + 80 * np.tanh((mountains - cap + 80) / 80), mountains)
    add = rolling + forest
    for i, (col_px, row_px, radius_px, top) in enumerate(EXTRA_MOUNTAINS):
        cx, cy = (1008 - row_px) * STEP * COARSE, col_px * STEP * COARSE
        extra = mountain(x, y, cx, cy, radius_px * STEP * COARSE * 1.3, top, SEED + 100 + i)
        # Ajoutées au relief existant (elles doivent se voir même là où il y a déjà des collines).
        add = add + extra * smoothstep(10, 120, route_d) * calm * smoothstep(0, 80, land)
    name, column, row, radius, top = VOLCANO
    cx, cy = frlg_xy(column, row)
    d = blob(x, y, cx, cy, radius, SEED + 20)
    cone = top * (smoothstep(1.0, 0.12, d) ** 1.3) * (1 + 0.18 * ridged(x, y, 90, 4, SEED + 30))
    cone -= 45.0 * smoothstep(0.16, 0.05, d)  # cratère
    peaks = np.maximum(mountains, cone * smoothstep(15, 90, town_d) * smoothstep(10, 90, route_d))
    far = np.zeros_like(x)
    for i, (name, cx, cy, radius, top) in enumerate(FAR_MOUNTAINS):
        far = np.maximum(far, mountain(x, y, cx, cy, radius, top, SEED + 60 + i))
    return add, peaks, far, rock + snow


def indigo_plateau(height, x, y):
    """Le plus haut sommet de Kanto : un massif en arêtes (pas un dôme) dont le sommet est une
    table plate à INDIGO_TOP, bordée de falaises (la Ligue est posée dessus)."""
    column, row, radius = INDIGO
    cx, cy = frlg_xy(column, row)
    d = blob(x, y, cx, cy, radius, SEED + 80) * radius
    massif = mountain(x, y, cx, cy, radius + 650, INDIGO_TOP - 70, SEED + 81)
    massif = np.maximum(massif, (INDIGO_TOP - 45) * smoothstep(radius + 420, radius + 20, d) ** 0.8
                        * (0.7 + 0.3 * ridged(x, y, 140, 5, SEED + 82)))
    top = INDIGO_TOP + fbm(x, y, 40, 2, SEED + 83) * 0.3
    cliff = np.where(d < radius + 35, INDIGO_TOP - 55 * smoothstep(radius, radius + 30, d), -1e9)
    plateau = np.where(d < radius, top, np.maximum(cliff, np.where(massif > 1, massif, -1e9)))
    return np.maximum(height, plateau)


# --- 6. Cascade -------------------------------------------------------------------------------

def waterfall(height):
    """Cascade de la Route 23, posée à la fin (après le lissage des chemins) : un bassin au bord
    de la route, un ruisseau jusqu'au pied d'une falaise de FALL_HEIGHT m, et au-dessus le
    torrent qui descend du massif. On ne fait que creuser, sauf la falaise elle-même."""
    px, py = frlg_xy(*FALL_POOL)
    sx, sy = frlg_xy(*FALL_SOURCE)
    length = float(np.hypot(sx - px, sy - py))
    reach = length + 80
    r0, r1 = max(0, int((py - reach) / STEP)), min(SIZE, int((py + reach) / STEP) + 2)
    c0, c1 = max(0, int((px - reach) / STEP)), min(SIZE, int((px + reach) / STEP) + 2)
    part = (slice(r0, r1), slice(c0, c1))
    y, x = np.mgrid[r0:r1, c0:c1] * STEP
    h = height[part]
    ux, uy = (sx - px) / length, (sy - py) / length
    along = (x - px) * ux + (y - py) * uy
    side = (x - px) * -uy + (y - py) * ux
    side = side + fbm(x, y, 140, 2, SEED + 90) * 12 * smoothstep(40, 140, along)  # torrent sinueux
    ground = float(np.median(h[np.hypot(x - px, y - py) < 12]))
    lip = ground + FALL_HEIGHT
    foot, top = 26.0, 31.0  # la chute : entre 26 et 31 m du bassin
    bed = np.where(along < foot, ground - 1.2,
                   np.where(along < top, ground - 1.2 + (lip - ground + 1.2) * smoothstep(foot, top, along),
                            lip + (along - top) * 0.12))
    within = (along > -20) & (along < length)
    # La falaise : le relief derrière la chute monte au moins jusqu'à la lèvre (+ 6 m sur les côtés).
    wall = smoothstep(foot - 2, top + 2, along) * smoothstep(60, 25, np.abs(side)) * (along < 120)
    h = np.maximum(h, h + (lip + 6 * smoothstep(4, 20, np.abs(side)) - h) * wall)
    # Le lit du torrent (8 m de large) et ses berges en pente.
    channel = smoothstep(9, 4, np.abs(side)) * within
    banks = smoothstep(35, 8, np.abs(side)) * within * (along > top)
    h = np.where(banks > 0, h + (np.minimum(h, np.maximum(bed + 3, h - 12)) - h) * banks, h)
    h = h + (np.minimum(h, bed) - h) * channel
    # Le bassin au pied (rayon 13 m, 2,5 m de fond).
    d = np.hypot(x - px, y - py)
    h = h + (np.minimum(h, ground - 2.5) - h) * smoothstep(15, 10, d)
    height[part] = h
    return ground, lip


# --- Lacs et eau --------------------------------------------------------------------------------

def lake_mask(route_d):
    """Lacs (grille de 6 m, [ligne = est, colonne = nord]) : le bleu de la carte de L'amie qui
    ne touche pas la mer du bord de la carte."""
    k = np.asarray(Image.open(ZONES).convert("RGB")).astype(np.float64)
    r, g, b = k[..., 0], k[..., 1], k[..., 2]
    water = (b > g + 10) & (b > r + 30)
    labels, _ = ndimage.label(water)
    border = np.unique(np.r_[labels[0], labels[-1], labels[:, 0], labels[:, -1]])
    lakes = water & ~np.isin(labels, border)
    c0, r0, c1, r1 = CYCLING_ROAD
    lakes[r0:r1, c0:c1] = False
    sea = water & np.isin(labels, border[border > 0])
    lakes &= ndimage.distance_transform_edt(~sea) > 15  # pas les bouts de mer des routes maritimes
    lakes = ndimage.binary_opening(lakes, iterations=1)
    labels, count = ndimage.label(lakes)
    sizes = ndimage.sum(lakes, labels, range(1, count + 1))
    lakes = np.isin(labels, np.nonzero((sizes >= LAKE_MIN) & (sizes <= LAKE_MAX))[0] + 1)
    lakes = ndimage.binary_closing(lakes, iterations=2)
    lakes = np.transpose(lakes[::-1])  # nord en haut -> grille du Landscape
    # Bords ronds et irréguliers (les cases FRLG sont carrées) : flou + seuil bruité.
    n = lakes.shape[0]
    xs, ys = mh.grid_xy(range(n), n, STEP * COARSE)
    soft = ndimage.gaussian_filter(lakes.astype(np.float64), 3.5)
    return soft > 0.42 + 0.12 * fbm(xs, ys, 90, 3, SEED + 130)  # nord en haut -> grille du Landscape


def dig_lakes(height, lakes):
    """Creuse chaque lac : surface un peu sous le bord le plus bas, fond de LAKE_DEPTH m,
    berges en pente douce. Renvoie la liste (x, y, surface) des lacs."""
    fine = upsample(lakes.astype(np.float64)) > 0.5
    labels, count = ndimage.label(fine)
    found = []
    for index, part in enumerate(ndimage.find_objects(labels)):
        pad = int(LAKE_BANK / STEP) + 4
        box = tuple(slice(max(0, sl.start - pad), sl.stop + pad) for sl in part)
        inside = labels[box] == index + 1
        h = height[box]
        ring = ndimage.binary_dilation(inside, iterations=3) & ~inside
        level = float(np.percentile(h[ring], 25)) - 0.6
        d_in = ndimage.distance_transform_edt(inside) * STEP
        d_out = ndimage.distance_transform_edt(~inside) * STEP
        bed = level - 0.5 - (LAKE_DEPTH - 0.5) * smoothstep(0, 40, d_in)
        bank = level + 0.15 * d_out
        target = np.where(inside, bed, bank)
        weight = np.where(inside, 1.0, smoothstep(LAKE_BANK, 0, d_out))  # sans marche au bord
        dug = np.maximum(np.minimum(h, target), h - LAKE_DEPTH - 2.0)  # jamais de fosse profonde
        height[box] = h + (dug - h) * weight
        rows, cols = np.nonzero(inside)
        found.append(((box[1].start + cols.mean()) * STEP, (box[0].start + rows.mean()) * STEP, level))
    return found


def warp_edges(height, weight):
    """Fait onduler les lignes droites (bords des couloirs, côtes) : on relit le relief à
    côté, décalé de ± WARP m, là où `weight` vaut 1 (jamais au cœur des routes ni en ville)."""
    out = np.empty_like(height)
    cols = np.arange(SIZE, dtype=np.float64)
    for row0 in range(0, SIZE, 256):
        rows = np.arange(row0, min(SIZE, row0 + 256), dtype=np.float64)
        rr, cc = np.meshgrid(rows, cols, indexing="ij")
        x, y = cc * STEP, rr * STEP
        w = weight[row0:row0 + len(rows)] * WARP / STEP
        dr = fbm(x, y, 230, 3, SEED + 120) * w
        dc = fbm(x, y, 230, 3, SEED + 121) * w
        out[row0:row0 + len(rows)] = ndimage.map_coordinates(height, [rr + dr, cc + dc], order=1, mode="nearest")
    return out


# --- 7. Chemins praticables ------------------------------------------------------------------

@njit(cache=True)
def _cone(h, mask, rise, upper):
    """Enveloppe à pente limitée dans `mask` : upper -> min(h(q) + rise × distance),
    sinon max(h(q) - rise × distance). `rise` : montée permise par case, case par case
    (une valeur qui varie donne des bords qui ondulent). Balayages en chanfrein répétés."""
    out = h.copy()
    rows, cols = h.shape
    for _ in range(12):
        changed = False
        for r in range(rows):
            for c in range(cols):
                if not mask[r, c]:
                    continue
                for dr, dc, k in ((-1, -1, 1.41421356), (-1, 0, 1.0), (-1, 1, 1.41421356), (0, -1, 1.0)):
                    rr, cc = r + dr, c + dc
                    if 0 <= rr < rows and 0 <= cc < cols and mask[rr, cc]:
                        w = rise[r, c] * k
                        if upper and out[rr, cc] + w < out[r, c]:
                            out[r, c] = out[rr, cc] + w
                            changed = True
                        elif not upper and out[rr, cc] - w > out[r, c]:
                            out[r, c] = out[rr, cc] - w
                            changed = True
        for r in range(rows - 1, -1, -1):
            for c in range(cols - 1, -1, -1):
                if not mask[r, c]:
                    continue
                for dr, dc, k in ((1, 1, 1.41421356), (1, 0, 1.0), (1, -1, 1.41421356), (0, 1, 1.0)):
                    rr, cc = r + dr, c + dc
                    if 0 <= rr < rows and 0 <= cc < cols and mask[rr, cc]:
                        w = rise[r, c] * k
                        if upper and out[rr, cc] + w < out[r, c]:
                            out[r, c] = out[rr, cc] + w
                            changed = True
                        elif not upper and out[rr, cc] - w > out[r, c]:
                            out[r, c] = out[rr, cc] - w
                            changed = True
        if not changed:
            break
    return out


def soften_talus(height, open_land, wobble):
    """Efface les arêtes nettes hors des montagnes, en deux temps (le bas du talus, la route ou
    la plage, ne bouge pas ; on rogne le haut) :
    1. les falaises sont couchées à CLIFF_SLOPE au plus : elles barrent toujours le passage,
       mais en pente rocheuse, plus en mur ;
    2. le rebord est arrondi à TALUS_SLOPE sur TALUS_CUT m de haut au plus : un petit talus
       disparaît, une grande falaise garde son pied raide mais prend une épaule douce.
    `wobble` (0 à 1) fait varier la pente d'un endroit à l'autre : le rebord recule plus ou
    moins et ondule, au lieu de rester une ligne droite."""
    zone = (open_land > 0.5) & (height > 0.5)  # jamais la mer
    vary = 0.55 + 0.9 * wobble
    cliff = _cone(height, zone, CLIFF_SLOPE * STEP * vary, True)
    shoulder = _cone(cliff, zone, TALUS_SLOPE * STEP * vary, True)
    change = (cliff - height) + np.maximum(shoulder - cliff, -TALUS_CUT)
    del cliff, shoulder
    change = ndimage.gaussian_filter(change, 4.0 / STEP)
    weight = np.clip(ndimage.gaussian_filter(open_land, 20.0 / STEP), 0, 1)
    out = height + change * weight
    # Les plis laissés par les rampes : léger lissage là où on a changé quelque chose.
    touched = ndimage.gaussian_filter((np.abs(change) > 0.3).astype(np.float32), 6.0 / STEP)
    return out + (ndimage.gaussian_filter(out, 5.0 / STEP) - out) * np.clip(touched * 1.5, 0, 1) * weight


def smooth_paths(height, inside, keep, wobble):
    """Routes douces pour un petit personnage : sur les routes (et 20 m autour), le relief est
    remplacé par sa version très lissée (PATH_SMOOTH m), dont la pente est ensuite limitée à
    PATH_MAX_SLOPE. Le passage vers le relief d'origine suit un masque flouté sur PATH_EDGE m :
    bords arrondis, pas de couloir carré. `keep` : zones à ne pas toucher (Plateau Indigo et
    massifs : la Route 23 n'y monte pas, on passe par la Route Victoire).
    Au bord des routes, le terrain voisin ne monte ni ne descend plus vite que EMBANK_SLOPE
    (remblai ou déblai) : jamais de marche ni de mur net au bord d'une route."""
    soft = ndimage.gaussian_filter(height, PATH_SMOOTH / STEP)
    zone = inside + (wobble - 0.5) * 40 > -20  # bord qui ondule (± 20 m) : pas de couloir droit
    # Les bandes étroites entre deux chemins (moins de ~45 m) en font partie : sinon elles
    # restent en arête fine entre deux chemins abaissés.
    zone = ndimage.binary_closing(zone, iterations=int(22 / STEP), border_value=1) & (keep < 0.5)
    rise = np.full(soft.shape, PATH_MAX_SLOPE * STEP)
    limited = (_cone(soft, zone, rise, True) + _cone(soft, zone, rise, False)) / 2
    del soft
    limited = ndimage.gaussian_filter(limited, 3.0 / STEP)
    weight = np.clip(ndimage.gaussian_filter(zone.astype(np.float32), PATH_EDGE / 2.5 / STEP) * 2.0, 0, 1)
    weight = np.maximum(weight, zone * 1.0) * (1 - keep)
    out = height + (limited - height) * weight
    del limited
    # Remblai et déblai : depuis la route, le terrain voisin suit au plus EMBANK_SLOPE.
    core = zone
    land = out > 0.5
    rise = EMBANK_SLOPE * STEP * (0.5 + wobble)  # plus ou moins raide : le pied du talus ondule
    floor = _cone(np.where(core, out, -1e9), land | core, rise, False)
    ceiling = _cone(np.where(core, out, 1e9), land | core, rise, True)
    fill = np.where(core | ~land, 0.0, np.maximum(floor - out, 0.0) + np.minimum(ceiling - out, 0.0))
    del floor, ceiling
    fill = ndimage.gaussian_filter(fill, 3.0 / STEP)
    return out + fill * (1 - keep)


# --- Aperçus -----------------------------------------------------------------------------------

def shaded(height, step):
    gy, gx = np.gradient(height, step)
    light = np.array([-1.0, -1.0, 1.6])
    light /= np.linalg.norm(light)
    n = np.dstack([-gx, -gy, np.ones_like(height)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    shade = np.clip((n * light).sum(2), 0, 1)
    t = np.clip(height / INDIGO_TOP, 0, 1)[..., None]
    low, mid, high, snow = (np.array(c, float) for c in ((84, 132, 64), (110, 120, 70), (150, 140, 125), (240, 240, 240)))
    land = np.where(t < 0.3, low + (mid - low) * (t / 0.3), np.where(t < 0.75, mid + (high - mid) * ((t - 0.3) / 0.45), high + (snow - high) * ((t - 0.75) / 0.25)))
    depth = np.clip(-height / 15, 0, 1)[..., None]
    sea = np.array([70, 150, 170]) * (1 - depth) + np.array([25, 60, 120]) * depth
    color = np.where((height < 0)[..., None], sea, land)
    image = np.clip(color * (0.35 + 0.75 * shade[..., None]), 0, 255).astype(np.uint8)
    return mh.north_up(image)


def save_previews(height, folder, tag=""):
    small = height[::4, ::4]
    Image.fromarray(shaded(small, STEP * 4)).save(os.path.join(folder, f"relief_vues_dessus{tag}.png"))
    grey = np.clip((small + 20) / (INDIGO_TOP + 40) * 255, 0, 255).astype(np.uint8)
    Image.fromarray(mh.north_up(grey)).save(os.path.join(folder, f"relief_vues_gris{tag}.png"))


def protected(x, y, margin=320.0):
    """1 là où le lissage ne doit pas passer : le Plateau Indigo et son massif (sur `margin` m)."""
    column, row, radius = INDIGO
    cx, cy = frlg_xy(column, row)
    keep = smoothstep(radius + margin, radius + margin - 60, np.hypot(x - cx, y - cy))
    return keep


def upsample(field):
    """Grille de 6 m -> grille de 1,5 m (interpolation bilinéaire, par bandes)."""
    out = np.empty((SIZE, SIZE), np.float64)
    cols = np.arange(SIZE) / COARSE
    for row0 in range(0, SIZE, 512):
        rows = np.arange(row0, min(SIZE, row0 + 512)) / COARSE
        rr, cc = np.meshgrid(rows, cols, indexing="ij")
        out[row0:row0 + len(rows)] = ndimage.map_coordinates(field, [rr, cc], order=1, mode="nearest")
    return out


# --- Programme ---------------------------------------------------------------------------------

def main():
    start = time.time()
    amie = read_heights(SOURCE, SOURCE_SCALE_Z)
    before = read_heights(os.path.join(HERE, "reference", "relief_v2.png"), 200.0)
    base, water, water_bed = clean(amie, before)
    del amie, before
    print(f"Nettoyage : {time.time() - start:.0f} s")

    cells = mh.cell_fields()
    x, y, route_d, town_d, frlg = coarse_fields(cells)
    inside_d = ndimage.distance_transform_edt(route_d == 0) * STEP * COARSE
    signed = inside_d - route_d  # + dans les routes
    # Lignes droites cassées : tout ondule sauf le cœur des routes, les villes et l'eau creusée.
    calm = smoothstep(0, 60, town_d)
    still = ndimage.binary_dilation(water[::COARSE, ::COARSE], iterations=6)
    base = warp_edges(base, upsample(smoothstep(12, -8, signed) * calm * ~still))
    print(f"Bords ondulés : {time.time() - start:.0f} s")
    now = town_levels(base)
    coarse = base[::COARSE, ::COARSE]
    sea = coarse < 0.2
    land = ndimage.distance_transform_edt(~sea) * STEP * COARSE
    shifted = coarse + town_shift(x, y, now) * smoothstep(0, 40, land)
    add, peaks, far, rocky = big_shapes(x, y, route_d, town_d, land, frlg)
    grown = shifted + add
    lifted = np.maximum(peaks * smoothstep(0, 60, land), far)
    grown = np.where(lifted > 0.5, np.maximum(grown, lifted), grown)
    # Les montagnes lointaines sortent de la mer : là, elles remplacent le fond.
    grown = np.where(sea & (far > 0.5), np.maximum(coarse, far - 8), grown)
    cap = INDIGO_TOP - 60  # rien ne dépasse le Plateau Indigo
    grown = np.where(grown > cap - 60, cap - 60 + 60 * np.tanh((grown - cap + 60) / 60), grown)
    grown = indigo_plateau(grown, x, y)
    print(f"Grandes formes : {time.time() - start:.0f} s")

    # Érosion sur les nouveaux reliefs seulement (les routes et les villes ne bougent pas).
    new = np.clip((grown - shifted) / 40.0, 0, 1) * smoothstep(30, 120, route_d) * smoothstep(20, 120, town_d)
    scaled = grown / (STEP * COARSE)
    mh.erode(scaled, 1_200_000 if not TEST else 600_000, SEED, 80, 3, 0.05, 4.0, 0.01, 0.3,
             0.25, 0.012, 4.0, 0.6)
    eroded = grown + (scaled * STEP * COARSE - grown) * new
    eroded = np.maximum(eroded, np.minimum(grown, 0.3))
    delta = eroded - coarse
    print(f"Érosion : {time.time() - start:.0f} s")

    # Retour à 1,5 m : on ajoute la différence (lisse) au relief de L'amie, détails gardés.
    height = base + upsample(delta)
    del delta
    # Arêtes nettes hors des montagnes : falaises couchées, rebords arrondis, bords qui ondulent.
    mountains = (rocky > 0.02) | (peaks > 3) | (far > 3) | (protected(x, y) > 0.01)
    mountains = ndimage.binary_dilation(mountains, iterations=4)
    wobble = np.clip(0.5 + 0.8 * fbm(x, y, 140, 3, SEED + 140), 0, 1)
    height = soften_talus(height, upsample(1.0 - mountains.astype(np.float64)), upsample(wobble))
    height = smooth_paths(height, upsample(signed), upsample(np.maximum(protected(x, y, 650), smoothstep(10, 40, peaks))),
                          upsample(wobble))
    del wobble
    if not TEST:
        mh.carve_paths(height)  # chemins de la V1 : profil à 15 % au plus
    # Arêtes et coins adoucis partout (sauf au cœur des routes, déjà lisse).
    soft = ndimage.gaussian_filter(height, EDGE_SOFTEN / STEP)
    height = height + (soft - height) * 0.7 * upsample(smoothstep(10, -5, signed))
    del soft
    lakes = dig_lakes(height, lake_mask(route_d))
    # L'eau creusée par L'amie : même tracé, mais en lit peu profond (3 à 5 m sous le terrain
    # autour) avec des berges douces, au lieu d'une tranchée de plusieurs dizaines de mètres.
    dry = (~ndimage.binary_dilation(water, iterations=3)).astype(np.float32)
    sigma = 15.0 / STEP
    ground = ndimage.gaussian_filter(height * dry, sigma) / np.maximum(ndimage.gaussian_filter(dry, sigma), 1e-3)
    depth_in = ndimage.distance_transform_edt(water) * STEP
    bed = ground - 3.0 - 2.0 * smoothstep(0, 15, depth_in)
    d_out = ndimage.distance_transform_edt(~water) * STEP
    target = np.where(water, bed, ground - 3.0 + 0.15 * d_out)
    height = np.where(d_out < 25, np.minimum(height, target), height)
    height = height + (ndimage.gaussian_filter(height, 2.0) - height) * (d_out < 25)
    del dry, ground, depth_in, bed, d_out, target
    ground, lip = waterfall(height)
    print(f"{len(lakes)} lacs ; cascade : bassin à {ground:.0f} m, lèvre à {lip:.0f} m")
    print("Villes : " + ", ".join(f"{n} {now[n]:.0f} m" + (f" -> {TOWN_LEVELS[n]:.0f} m" if n in TOWN_LEVELS else "")
                                  for n in now if n != "Forêt de Jade"))
    print(f"Chemins : {time.time() - start:.0f} s")

    gy, gx = np.gradient(base, STEP)
    slope = np.hypot(gx, gy)
    core = upsample(inside_d) > 15
    print(f"(avant : 99 % sous {np.percentile(slope[core], 99) * 100:.0f} %, au-dessus de 45° : "
          f"{(slope[core] > 1).mean() * 100:.2f} %)")
    del core
    gy, gx = np.gradient(height, STEP)
    slope = np.hypot(gx, gy)
    # Cœur des routes (à plus de 15 m de leur bord) : là où l'on marche vraiment.
    on_route = upsample(inside_d) > 15
    print(f"Altitude min {height.min():.1f} m, max {height.max():.1f} m ; pente sur les routes : "
          f"médiane {np.median(slope[on_route]) * 100:.0f} %, 99 % sous "
          f"{np.percentile(slope[on_route], 99) * 100:.0f} %, au-dessus de 45° : "
          f"{(slope[on_route] > 1).mean() * 100:.2f} %")
    limit = min(32767 / layout.HEIGHT_UNITS_PER_METER, INDIGO_TOP + 0.5)  # le Plateau Indigo reste le plus haut
    height = np.clip(height, -40.0, limit)
    if TEST:
        os.makedirs(OUT, exist_ok=True)
        np.save(os.path.join(OUT, "relief.npy"), height[::2, ::2].astype(np.float32))
        steep = (slope > 0.6) & on_route
        np.save(os.path.join(OUT, "raide.npy"), steep[::4, ::4] | steep[2::4, 2::4][:steep[::4, ::4].shape[0], :steep[::4, ::4].shape[1]] if False else steep[::4, ::4])
        save_previews(height, OUT)
        return
    values = np.clip(np.round(32768 + height * layout.HEIGHT_UNITS_PER_METER), 0, 65535).astype(">u2")
    mh.write_png(layout.HEIGHTMAP_PATH, SIZE, SIZE, [r.tobytes() for r in values], 16, 0)
    save_previews(height, HERE)
    print(f"Écrit {layout.HEIGHTMAP_PATH} ({time.time() - start:.0f} s)")


if __name__ == "__main__":
    main()
