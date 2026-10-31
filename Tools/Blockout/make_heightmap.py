"""Fabrique le relief de Kanto (SCRUM-62) : un vrai terrain 3D organisé autour des routes.

Sortie :
- kanto_v1_heightmap.png : heightmap 16 bits en niveaux de gris, à importer dans le Landscape
  (échelle 150, 150, 200). Unreal (blockout_kanto_v1.py) relit ce fichier pour poser les objets ;
- kanto_couloirs.bin : distance au bord des routes (pour les arbres de lisière dans Unreal) ;
- kanto_lacs.json : lacs et rivières en hauteur (Unreal y pose une surface d'eau) ;
- kanto_v1_preview.png : aperçu en couleurs et ombré ; kanto_v1_heightmap_apercu.png : la
  heightmap en gris contrastés, plus facile à lire. Nord en haut.

Lancement : pip install numpy numba, puis python Tools/Blockout/make_heightmap.py (2 à 3 minutes).

Méthode, du plus grand au plus petit :
1. Le réseau des routes et des villes vient de Rouge Feu / Vert Feuille (kanto_frlg.bin) ;
   tout le reste est soit la mer (silhouette de Let's Go), soit du relief qu'on ne peut pas
   traverser : on suit les routes, les cols et les tunnels.
2. Chaque ville a son altitude (port au niveau de la mer, ville de pierre sur un plateau,
   Plateau Indigo tout en haut) ; les routes montent et descendent entre elles en passant
   par des points de passage (collines, cols), en suivant le chemin, pas à vol d'oiseau.
3. Hors des routes : une falaise de 25 à 40 m, puis des chaînes de montagnes en crêtes qui
   grandissent avec la distance (les grands massifs de Kanto sont les plus hauts).
4. Côtes : falaises par endroits, plages ailleurs ; lacs et rivières à leur altitude.
5. Érosion par la pluie, en deux passes (vallées puis ravines), seulement dans les montagnes :
   les routes et les falaises qui les bordent restent intactes.
6. Les villes de la V1 sont posées en terrasses, avec des rampes sur l'axe principal.
"""

import json
import os
import struct
import sys
import time
import zlib

import numpy as np
from numba import njit

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kanto_v1_layout as layout  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SIZE = layout.HEIGHTMAP_SIZE
STEP = layout.METERS_PER_PIXEL
OUT = HERE
TEST = "--test" in sys.argv
if TEST:
    # Essai rapide : grille 4 fois plus grossière (6 m par pixel), fichiers écrits dans le
    # dossier donné après --test (la heightmap du dépôt n'est pas touchée).
    SIZE = (layout.HEIGHTMAP_SIZE - 1) // 4 + 1
    STEP = layout.WORLD_SIZE / (SIZE - 1)
    OUT = sys.argv[sys.argv.index("--test") + 1]
DROPS = (SIZE / layout.HEIGHTMAP_SIZE) ** 2  # moins de gouttes de pluie sur une petite grille
CELL = layout.CELL
COLS, ROWS = layout.FRLG_COLS, layout.FRLG_ROWS
SEED = 151

# Réglages du relief (mètres).
RIM_LOW, RIM_HIGH = 5.0, 18.0   # talus au bord des routes (caché par les arbres de lisière)
RIM_WIDTH = 9.0          # largeur du talus (pente d'environ 40 à 60° : on ne le monte pas)
FOREST_HILLS = 45.0      # collines boisées derrière le talus
MOUNTAIN_BASE = 90.0     # hauteur des montagnes (là où la carte en montre)
MOUNTAIN_PEAK = 300.0    # en plus, au cœur des massifs (pics fins)
WARP = 260.0             # déformation des bords des routes et des côtes (pas de rectangles)
V1_CLEARANCE = 75.0      # largeur garantie autour des chemins de la V1
ROUTE_HILLS = 22.0       # collines au fond des routes larges (pentes douces)
SHELF_DEPTH = 2.0        # eau claire : 0 à 2 m de fond près des côtes
SHELF_WIDTH = 150.0      # largeur du haut-fond (± 50 m selon l'endroit)
SEA_DEPTH = 15.0         # fond de la mer au large
BEACH_SLOPE = 0.05       # pente des plages (5 %)
TOWN_FALLOFF = 70.0      # les villes rejoignent le terrain sur cette distance (sans marche)
GROUND_NOISE_LARGE = 0.8  # bosses d'environ 30 m (± 40 cm)
GROUND_NOISE_SMALL = 0.4  # petites bosses d'environ 9 m (± 20 cm)
PATH_MAX_SLOPE = 0.15     # pente maximale d'un chemin (15 %)
PATH_MIN_ALTITUDE = 1.5   # un chemin ne passe jamais sous l'eau (ponts et jetées en blockout)
PATH_FLAT = 4.0           # demi-largeur à plat autour du chemin (m)
PATH_FALLOFF = 22.0       # le terrain rejoint le chemin en pente douce jusqu'à cette distance
PAD_FALLOFF = 18.0        # bord des plateformes sous les bâtiments hors des villes
PAD = 60                # cases ajoutées autour de la carte FRLG (la côte y suit Let's Go)


# --- Bruits (numpy) ------------------------------------------------------------------

def _hash(ix, iy, seed):
    """Même hachage que kanto_v1_layout._hash, en tableaux numpy (entiers non signés 64 bits)."""
    n = (ix.astype(np.uint64) * np.uint64(374761393) + iy.astype(np.uint64) * np.uint64(668265263)
         + np.uint64(seed) * np.uint64(2147483647))
    n = (n ^ (n >> np.uint64(13))) * np.uint64(1274126177)
    n ^= n >> np.uint64(16)
    return (n & np.uint64(0xFFFFFF)).astype(np.float64) / float(0xFFFFFF)


def value_noise(x, y, scale, seed=0):
    """Copie numpy de kanto_v1_layout.value_noise (mêmes valeurs)."""
    fx, fy = x / scale, y / scale
    ix, iy = np.floor(fx), np.floor(fy)
    tx, ty = fx - ix, fy - iy
    tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
    ix, iy = ix.astype(np.int64), iy.astype(np.int64)
    a = _hash(ix, iy, seed)
    b = _hash(ix + 1, iy, seed)
    c = _hash(ix, iy + 1, seed)
    d = _hash(ix + 1, iy + 1, seed)
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty


def fractal_noise(x, y, scale, octaves=4, seed=0):
    total, amplitude, norm = 0.0, 1.0, 0.0
    for octave in range(octaves):
        total = total + value_noise(x, y, scale, seed + octave) * amplitude
        norm += amplitude
        scale /= 2.0
        amplitude /= 2.0
    return total / norm


def perlin(x, y, scale, seed):
    """Bruit de Perlin (entre -1 et 1 environ) : plus naturel que le bruit de valeurs."""
    fx, fy = x / scale, y / scale
    ix, iy = np.floor(fx), np.floor(fy)
    tx, ty = fx - ix, fy - iy
    ix, iy = ix.astype(np.int64), iy.astype(np.int64)

    def corner(cx, cy, dx, dy):
        angle = _hash(cx, cy, seed) * 2 * np.pi
        return np.cos(angle) * dx + np.sin(angle) * dy

    n00 = corner(ix, iy, tx, ty)
    n10 = corner(ix + 1, iy, tx - 1, ty)
    n01 = corner(ix, iy + 1, tx, ty - 1)
    n11 = corner(ix + 1, iy + 1, tx - 1, ty - 1)
    ux = tx * tx * tx * (tx * (tx * 6 - 15) + 10)
    uy = ty * ty * ty * (ty * (ty * 6 - 15) + 10)
    top = n00 + (n10 - n00) * ux
    bottom = n01 + (n11 - n01) * ux
    return (top + (bottom - top) * uy) * 1.4


def fbm(x, y, scale, octaves, seed, gain=0.5):
    total, amplitude, norm = 0.0, 1.0, 0.0
    for octave in range(octaves):
        total = total + perlin(x, y, scale, seed + octave * 7) * amplitude
        norm += amplitude
        scale /= 2.0
        amplitude *= gain
    return total / norm


def ridged(x, y, scale, octaves, seed):
    """Bruit en crêtes (0 à 1) : arêtes vives et vallées, comme de vraies chaînes de montagnes."""
    total, amplitude, norm, weight = 0.0, 1.0, 0.0, 1.0
    for octave in range(octaves):
        ridge = 1.0 - np.abs(perlin(x, y, scale, seed + octave * 13))
        ridge = ridge * ridge * weight
        weight = np.clip(ridge * 1.8, 0.0, 1.0)
        total = total + ridge * amplitude
        norm += amplitude
        scale /= 2.0
        amplitude *= 0.5
    return total / norm


def smoothstep(edge0, edge1, value):
    t = np.clip((value - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# --- Grille de la carte FRLG (numba) ------------------------------------------------------

@njit(cache=True)
def geodesic(passable, seeds_r, seeds_c, values):
    """Distance (en cases) en restant dans `passable`, depuis des points de départ, et la
    valeur du départ le plus proche. Balayages en chanfrein répétés jusqu'à stabilité."""
    rows, cols = passable.shape
    dist = np.full((rows, cols), 1e9)
    value = np.zeros((rows, cols))
    for i in range(len(seeds_r)):
        dist[seeds_r[i], seeds_c[i]] = 0.0
        value[seeds_r[i], seeds_c[i]] = values[i]
    diag = 1.41421356
    changed = True
    while changed:
        changed = False
        for r in range(rows):
            for c in range(cols):
                if not passable[r, c]:
                    continue
                for dr, dc, w in ((-1, -1, diag), (-1, 0, 1.0), (-1, 1, diag), (0, -1, 1.0)):
                    rr, cc = r + dr, c + dc
                    if 0 <= rr < rows and 0 <= cc < cols and passable[rr, cc] and dist[rr, cc] + w < dist[r, c]:
                        dist[r, c] = dist[rr, cc] + w
                        value[r, c] = value[rr, cc]
                        changed = True
        for r in range(rows - 1, -1, -1):
            for c in range(cols - 1, -1, -1):
                if not passable[r, c]:
                    continue
                for dr, dc, w in ((1, 1, diag), (1, 0, 1.0), (1, -1, diag), (0, 1, 1.0)):
                    rr, cc = r + dr, c + dc
                    if 0 <= rr < rows and 0 <= cc < cols and passable[rr, cc] and dist[rr, cc] + w < dist[r, c]:
                        dist[r, c] = dist[rr, cc] + w
                        value[r, c] = value[rr, cc]
                        changed = True
    return dist, value


def distance_from(mask):
    """Distance (en cases) de chaque case à la plus proche case de `mask`, et sa position."""
    rr, cc = np.nonzero(mask)
    dist, _ = geodesic(np.ones(mask.shape, np.bool_), rr.astype(np.int64), cc.astype(np.int64),
                       np.zeros(len(rr)))
    return dist


def spread_values(mask, values):
    """Pour chaque case, la valeur de la case de `mask` la plus proche."""
    rr, cc = np.nonzero(mask)
    _, value = geodesic(np.ones(mask.shape, np.bool_), rr.astype(np.int64), cc.astype(np.int64),
                        values[rr, cc].astype(np.float64))
    return value


def masked_blur(field, mask, passes):
    """Moyenne 3 × 3 répétée, seulement entre cases de `mask` : adoucit les pentes des routes."""
    field = field.copy()
    weight = mask.astype(np.float64)
    for _ in range(passes):
        total = np.zeros_like(field)
        count = np.zeros_like(field)
        for dr in (-1, 0, 1):
            for dc in (-1, 0, 1):
                total += np.roll(np.roll(field * weight, dr, 0), dc, 1)
                count += np.roll(np.roll(weight, dr, 0), dc, 1)
        field = np.where(mask, total / np.maximum(count, 1e-9), field)
    return field


@njit(cache=True)
def erode(h, drops, seed, max_steps, radius, inertia, capacity, min_slope,
          erode_rate, deposit_rate, evaporate, gravity, max_change):
    """h : hauteurs exprimées en tailles de case (la pente vaut alors dh). Modifié sur place."""
    np.random.seed(seed)
    rows, cols = h.shape
    size = 2 * radius + 1
    brush = np.zeros((size, size))
    total = 0.0
    for i in range(size):
        for j in range(size):
            d = np.sqrt((i - radius) ** 2 + (j - radius) ** 2)
            if d < radius:
                brush[i, j] = radius - d
                total += brush[i, j]
    brush /= total
    for _ in range(drops):
        px = 1 + np.random.random() * (cols - 3)
        py = 1 + np.random.random() * (rows - 3)
        if h[int(py), int(px)] <= 0.0:
            continue  # pas de pluie sur la mer
        dx = dy = 0.0
        speed, water, sediment = 1.0, 1.0, 0.0
        for _step in range(max_steps):
            nx, ny = int(px), int(py)
            fx, fy = px - nx, py - ny
            h00, h10 = h[ny, nx], h[ny, nx + 1]
            h01, h11 = h[ny + 1, nx], h[ny + 1, nx + 1]
            gx = (h10 - h00) * (1 - fy) + (h11 - h01) * fy
            gy = (h01 - h00) * (1 - fx) + (h11 - h10) * fx
            height = h00 * (1 - fx) * (1 - fy) + h10 * fx * (1 - fy) + h01 * (1 - fx) * fy + h11 * fx * fy
            dx = dx * inertia - gx * (1 - inertia)
            dy = dy * inertia - gy * (1 - inertia)
            length = np.sqrt(dx * dx + dy * dy)
            if length < 1e-9:
                break
            dx /= length
            dy /= length
            px += dx
            py += dy
            if px < 1 or px >= cols - 2 or py < 1 or py >= rows - 2:
                break
            mx, my = int(px), int(py)
            gx2, gy2 = px - mx, py - my
            new_height = (h[my, mx] * (1 - gx2) * (1 - gy2) + h[my, mx + 1] * gx2 * (1 - gy2)
                          + h[my + 1, mx] * (1 - gx2) * gy2 + h[my + 1, mx + 1] * gx2 * gy2)
            dh = new_height - height
            cap = max(-dh, min_slope) * speed * water * capacity
            if sediment > cap or dh > 0:
                amount = min(dh, sediment) if dh > 0 else (sediment - cap) * deposit_rate
                sediment -= amount
                h[ny, nx] += amount * (1 - fx) * (1 - fy)
                h[ny, nx + 1] += amount * fx * (1 - fy)
                h[ny + 1, nx] += amount * (1 - fx) * fy
                h[ny + 1, nx + 1] += amount * fx * fy
            else:
                amount = min(min((cap - sediment) * erode_rate, -dh), max_change)
                for i in range(size):
                    for j in range(size):
                        w = brush[i, j]
                        if w == 0.0:
                            continue
                        r, c = ny + i - radius, nx + j - radius
                        if 0 <= r < rows and 0 <= c < cols:
                            h[r, c] -= amount * w
                sediment += amount
            speed = np.sqrt(max(0.0, speed * speed - dh * gravity))
            water *= 1 - evaporate
            if new_height <= 0.0:
                break  # la goutte arrive à la mer
    return h


# --- Étape 1 : la carte, case par case ---------------------------------------------------

def load_frlg():
    """La grille FRLG, entourée de PAD cases vides (indices décalés de PAD)."""
    with open(os.path.join(HERE, "kanto_frlg.bin"), "rb") as handle:
        data = zlib.decompress(handle.read())
    _, cols, rows = struct.unpack_from("<4sHH", data)
    grid = np.frombuffer(data[8:], np.uint8).reshape(rows, cols)
    return np.pad(grid, PAD)


def letsgo_land(columns, rows):
    """Terre (0 à 1) d'après la carte de Let's Go, pour des cases de la carte FRLG."""
    (a, b, c), (d, e, f) = layout.LETSGO_FROM_FRLG
    u = a * columns + b * rows + c
    v = d * columns + e * rows + f
    plane = np.frombuffer(layout._LAND_WIDE, np.uint8).reshape(layout._LAND_H, layout._LAND_W) / 255.0
    return sample(plane, u, v)


def sample(plane, u, v, outside=0.0):
    """Lecture interpolée d'une grille (u = colonne, v = ligne) ; `outside` hors de la grille."""
    h, w = plane.shape
    inside = (u >= 0) & (u <= w - 1) & (v >= 0) & (v <= h - 1)
    uc, vc = np.clip(u, 0, w - 1.001), np.clip(v, 0, h - 1.001)
    iu, iv = uc.astype(np.int64), vc.astype(np.int64)
    tu, tv = uc - iu, vc - iv
    a, b = plane[iv, iu], plane[iv, iu + 1]
    c, d = plane[iv + 1, iu], plane[iv + 1, iu + 1]
    value = (a * (1 - tu) + b * tu) * (1 - tv) + (c * (1 - tu) + d * tu) * tv
    return np.where(inside, value, outside)


def cell_fields():
    """Champs calculés sur la grille FRLG : altitude des routes, distances, mer, lacs."""
    grid = load_frlg().copy()
    # Piste cyclable : les bandes d'eau qui la longent sur la carte deviennent du sol.
    strip = grid[PAD + 250:PAD + 575, PAD + 255:PAD + 320]
    strip[strip >= 2] = 1
    walk = grid == 1
    lake = grid == 3
    # Fines bandes de terre le long des routes maritimes (bords de tuiles) : la mer.
    thin = walk & (distance_from(~walk) <= 3) & (distance_from(grid == 2) <= 4)
    grid[thin] = 2
    walk &= ~thin
    rows_i, cols_i = np.mgrid[0:grid.shape[0], 0:grid.shape[1]].astype(np.float64) - PAD
    land_lg = letsgo_land(cols_i, rows_i)

    # Lacs que Let's Go met dans la mer : la mer.
    lake_sea = lake & (land_lg < 0.5)
    lake &= ~lake_sea
    ground = walk | lake  # là où l'on marche (les lacs prennent l'altitude de la route)

    # Hors des routes : la mer si Let's Go en met, sauf tout près des routes (une falaise).
    near_ground = distance_from(ground) <= 8
    sea = (grid == 2) | lake_sea | ((grid == 0) & (land_lg < 0.5) & ~near_ground)

    # Altitude des routes : interpolation le long des chemins entre villes et points de passage.
    controls = layout.TOWNS + layout.ROUTE_POINTS
    gr, gc = np.nonzero(ground)
    total = np.zeros(grid.shape)
    weights = np.zeros(grid.shape)
    for name, column, row, altitude in controls:
        column, row = column + PAD, row + PAD
        nearest = np.argmin((gc - column) ** 2 + (gr - row) ** 2)  # point sur une route
        dist, _ = geodesic(ground, np.array([gr[nearest]]), np.array([gc[nearest]]), np.zeros(1))
        w = np.where(ground, 1.0 / (dist + 3.0) ** 3, 0.0)
        total += w * altitude
        weights += w
    altitude = np.where(ground, total / np.maximum(weights, 1e-12), 0.0)
    altitude = masked_blur(altitude, ground, 12)
    # Hors des routes : l'altitude des routes voisines, lissée (sans marche entre deux routes).
    altitude = spread_values(ground, altitude)
    altitude = masked_blur(altitude, ~ground, 60)

    # Distances signées en cases : bord des routes (+ sur la route) et côte (+ sur la terre).
    corridor = np.where(ground, distance_from(~ground), -distance_from(ground))
    corridor = masked_blur(corridor, np.ones(grid.shape, bool), 12)  # coins arrondis
    coast = np.where(sea, -distance_from(~sea), distance_from(sea))
    lake_sd = np.where(lake, distance_from(~lake), -distance_from(lake))
    return grid, ground, lake, altitude, corridor, coast, lake_sd


# --- Étape 2 : le relief à pleine résolution -------------------------------------------

def v1_path_distance(x, y):
    """Distance en mètres au plus proche chemin ou lieu de la V1."""
    best = np.full(x.shape, 1e9)
    for _, points in layout.PATHS + [("forêt", layout.FOREST_PATH)]:
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            dx, dy = bx - ax, by - ay
            t = np.clip(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0, 1)
            best = np.minimum(best, np.hypot(x - (ax + dx * t), y - (ay + dy * t)))
    for name, column, row, half_east, half_north, _ in layout.AREAS:
        cx, cy = layout.from_frlg(column, row)
        if name == "Forêt de Jade":
            continue
        box = np.maximum(np.abs(x - cx) - half_north, np.abs(y - cy) - half_east)
        best = np.minimum(best, np.maximum(box, 0) - 40)
    return best


def massif_field(x, y):
    """Part de montagne (0 à 1) : seulement les massifs de la carte (layout.MASSIFS)."""
    wx = x + fbm(x, y, 900, 3, SEED + 1) * 200
    wy = y + fbm(x, y, 900, 3, SEED + 2) * 200
    column, row = layout.to_frlg(wx, wy)
    mountain = np.zeros_like(x)
    for _, mc, mr, radius, weight in layout.MASSIFS:
        d = np.hypot(column - mc, row - mr) / radius
        mountain = 1 - (1 - mountain) * (1 - weight * smoothstep(1.0, 0.1, d))
    return mountain


def relief(x, y, fields):
    """Hauteur avant érosion, et deux aides : poids d'érosion et hauteur minimale (falaises)."""
    _, _, _, altitude_c, corridor_c, coast_c, lake_c = fields
    # Coordonnées déformées : bords des routes et côtes sinueux, pas les marches de la carte.
    wx = x + fbm(x, y, 900, 3, SEED + 4) * WARP + fbm(x, y, 180, 3, SEED + 5) * WARP * 0.3
    wy = y + fbm(x, y, 900, 3, SEED + 6) * WARP + fbm(x, y, 180, 3, SEED + 7) * WARP * 0.3
    column, row = layout.to_frlg(wx, wy)
    column, row = column + PAD, row + PAD
    altitude = sample(altitude_c, column, row, 0.0)
    # Bords des routes irréguliers à deux échelles : pas de rectangles.
    corridor = (sample(corridor_c, column, row, -60.0) * CELL + fbm(x, y, 160, 4, SEED + 10) * 30
                + fbm(x, y, 450, 3, SEED + 9) * 45 + fbm(x, y, 1100, 2, SEED + 8) * 80)
    # La V1 reste praticable malgré la déformation : couloir garanti autour de ses chemins.
    corridor = np.maximum(corridor, V1_CLEARANCE - v1_path_distance(x, y))
    coast = sample(coast_c, column, row, -60.0) * CELL + fbm(x, y, 260, 4, SEED + 11) * 40
    # Pas de mer autour de Bourg Palette : la côte la plus proche reste à plus de 300 m.
    pallet_x, pallet_y = layout.from_frlg(144, 540)
    coast = np.maximum(coast, 380 - np.hypot(x - pallet_x, y - pallet_y))
    lake = sample(lake_c, column, row, -60.0) * CELL + fbm(x, y, 60, 3, SEED + 12) * 4

    inside = np.maximum(corridor, 0.0)
    outside = np.maximum(-corridor, 0.0)
    # Fond des routes : collines douces dans les routes larges, petites bosses partout.
    floor = (altitude + fbm(x, y, 480, 4, SEED + 13) * ROUTE_HILLS * smoothstep(15, 150, inside)
             + fbm(x, y, 70, 3, SEED + 14) * 1.0)
    # Hors des routes : un talus boisé (on ne coupe pas entre deux routes), puis des collines
    # de forêt, et des montagnes seulement dans les massifs de la carte.
    rim_height = RIM_LOW + (RIM_HIGH - RIM_LOW) * (0.5 + 0.5 * fbm(x, y, 300, 2, SEED + 15))
    rim = rim_height * smoothstep(1, 1 + RIM_WIDTH * (0.8 + 0.5 * (0.5 + 0.5 * fbm(x, y, 120, 2, SEED + 19))), outside)
    # Les collines et les montagnes descendent vers la mer au lieu de tomber en falaise.
    seaward = smoothstep(20, 320, coast)
    hills = (FOREST_HILLS * smoothstep(15, 260, outside) * seaward
             * np.clip(0.2 + 0.8 * (0.5 + 0.5 * fbm(x, y, 650, 5, SEED + 16)), 0, 1.2))
    mountain = massif_field(x, y)
    crest = ridged(x, y, 560, 7, SEED + 17)
    body = smoothstep(0.0, 0.9, mountain)  # le massif lui-même, puis les arêtes et les pics fins
    peaks = (MOUNTAIN_BASE + MOUNTAIN_PEAK * mountain) * (0.35 * body + 0.65 * crest ** 1.4)
    rise = smoothstep(40, 450, outside) * smoothstep(0.05, 0.6, mountain) * seaward
    land = np.where(corridor >= 0, floor, altitude + rim + hills + peaks * rise)

    # Côtes : plage en pente douce, puis la forêt ou la route ; jamais de falaise.
    shore_width = 45 + 110 * (0.5 + 0.5 * fbm(x, y, 700, 3, SEED + 18))
    beach = 0.3 + np.maximum(coast, 0) * BEACH_SLOPE
    shore = smoothstep(15, 15 + shore_width, coast)
    land = beach + (np.maximum(land, beach) - beach) * shore
    # Mer : haut-fond clair (0 à 2 m) puis descente douce vers le large.
    offshore = np.maximum(-coast, 0.0)
    shelf = SHELF_WIDTH + 50 * fbm(x, y, 600, 2, SEED + 20)
    seabed = (-0.3 - (SHELF_DEPTH - 0.3) * smoothstep(0, shelf, offshore)
              - (SEA_DEPTH - SHELF_DEPTH) * smoothstep(shelf, shelf + 650, offshore))
    # Rochers dans l'eau le long des côtes rocheuses (là où il y avait des falaises).
    rocky = smoothstep(0.0, 0.35, fbm(x, y, 900, 3, SEED + 21))
    blobs = smoothstep(0.7, 0.88, fractal_noise(x, y, 40, 3, SEED + 22))
    reach = smoothstep(260, 120, offshore) * smoothstep(0, 8, offshore)
    rocks = blobs * rocky * reach
    seabed = seabed + (0.5 + 3.5 * rocks - seabed) * smoothstep(0.0, 0.25, rocks)
    height = seabed + (land - seabed) * smoothstep(-3, 3, coast)

    # Lacs et rivières en hauteur : creusés de 2,5 m sous la route.
    water = smoothstep(-2, 3, lake)
    height = height + (np.minimum(height, altitude - 2.5) - height) * water

    erosion = smoothstep(25, 90, outside) * smoothstep(0, 40, coast) * (0.3 + 0.7 * smoothstep(0.05, 0.4, mountain))
    # Le talus reste après l'érosion (sauf près de la plage).
    lowest = np.where((outside > 25) & (shore > 0.99), altitude + rim_height * 0.8, -1e9)
    return height, erosion, lowest, corridor, water > 0.5


# --- Étape 3 : les villes de la V1 en terrasses -------------------------------------------

def town_terraces(height, x, y):
    altitudes = {name: altitude for name, _, _, altitude in layout.TOWNS}
    wobble = (fractal_noise(x, y, 60, 3, 21) - 0.5) * 2
    for name, column, row, half_east, half_north, shape in layout.AREAS:
        cx, cy = layout.from_frlg(column, row)
        dx, dy = x - cx, y - cy
        norm = ((np.abs(dy) / half_east) ** shape + (np.abs(dx) / half_north) ** shape) ** (1.0 / shape)
        distance = (norm - 1 - wobble * 0.12) * min(half_east, half_north)
        if name == "Forêt de Jade":
            # Sous-bois vallonné : petits vallons et ravins.
            inside = smoothstep(10, -30, distance)
            height = height + (fbm(x, y, 90, 4, SEED + 40) * 4.0 - 1.0) * inside
            continue
        stepped = np.zeros_like(x)
        ramp = np.zeros_like(x)
        for boundary, rise in layout.TERRACES.get(name, []):
            stepped += rise * smoothstep(boundary - 9, boundary + 9, dx)  # pente douce, pas de marche
            ramp += rise * np.clip((dx - (boundary - 8)) / 16, 0, 1)
        level = stepped  # les niveaux sont déjà reliés par des pentes douces
        ground = altitudes[name] + level + fbm(x, y, 50, 3, SEED + 41) * 0.15
        blend = smoothstep(-5, TOWN_FALLOFF, distance)
        height = ground + (height - ground) * blend
        if name == "Bourg Palette":
            height = pallet_basin(height, dx, dy, altitudes[name], x, y)
    for px, py, depth, width in layout.PONDS:
        inside = (smoothstep(depth / 2, depth / 2 - 0.8, np.abs(x - px))
                  * smoothstep(width / 2, width / 2 - 0.8, np.abs(y - py)))
        height = height - 2.0 * inside
    return height


def pallet_basin(height, dx, dy, floor, x, y):
    """Bourg Palette au fond d'une cuvette rectangulaire : talus doux tout autour,
    ouvert au nord (Route 1) et au sud (vers la mer)."""
    half_x, half_y, depth, slope = layout.PALLET_BASIN
    corner = 30.0
    qx, qy = np.abs(dx) - (half_x - corner), np.abs(dy) - (half_y - corner)
    box = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - corner
    box = box + fbm(x, y, 140, 3, SEED + 42) * 6  # bords un peu irréguliers
    # Passages de 45 m au nord (Route 1) et au sud : le talus s'y efface.
    gap = 1 - smoothstep(40, 22, np.abs(dy)) * smoothstep(half_x * 0.4, half_x * 0.7, np.abs(dx))
    top = floor + depth * gap
    wall = floor + (top - floor) * smoothstep(-slope, 0, box)
    height = np.where(box < 0, wall, height)
    # Autour : le terrain ne descend pas sous le haut du talus, puis redevient naturel.
    lift = np.maximum(top - height, 0) * smoothstep(90, 0, box)
    return np.where(box >= 0, height + lift, height)


def lookouts(height, x, y):
    """Petites buttes au bord des routes (belvédères) : 5 m de haut, sommet plat de 8 m."""
    for column, row, off_x, off_y in layout.LOOKOUTS:
        px, py = layout.from_frlg(column, row)
        d = np.hypot(x - (px + off_x), y - (py + off_y))
        height = height + 5.0 * smoothstep(26, 8, d)
    return height


# --- Étape 4 : chemins praticables et plateformes des bâtiments hors des villes ------------

def _window(height, x0, x1, y0, y1):
    """Morceau de la grille (lignes = y, colonnes = x) qui couvre le rectangle en mètres."""
    r0, r1 = max(0, int(y0 / STEP)), min(SIZE, int(y1 / STEP) + 2)
    c0, c1 = max(0, int(x0 / STEP)), min(SIZE, int(x1 / STEP) + 2)
    rows, cols = np.mgrid[r0:r1, c0:c1]
    return (slice(r0, r1), slice(c0, c1)), cols * STEP, rows * STEP


def _height_along(height, xs, ys):
    return sample(height, xs / STEP, ys / STEP, 0.0)


def _in_towns(x, y, margin):
    inside = np.zeros(x.shape, bool)
    for name, column, row, half_east, half_north, _ in layout.AREAS:
        if name == "Forêt de Jade":
            continue
        cx, cy = layout.from_frlg(column, row)
        inside |= (np.abs(x - cx) < half_north + margin) & (np.abs(y - cy) < half_east + margin)
    return inside


def carve_paths(height):
    """Le chemin suit un profil sans à-coups (pente limitée, jamais sous l'eau), et le
    terrain le rejoint en pente douce de part et d'autre. Pas dans les villes (déjà à plat)."""
    base = height.copy()
    best = np.zeros(height.shape, np.float32)  # poids du chemin le plus proche déjà appliqué
    for name, points in layout.PATHS:
        starts, xs, ys = [], [], []
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            count = max(1, int(np.hypot(bx - ax, by - ay) / STEP))
            t = np.arange(count) / count
            starts.append((sum(len(part) for part in xs), count))
            xs.append(ax + (bx - ax) * t)
            ys.append(ay + (by - ay) * t)
        xs = np.concatenate(xs + [[points[-1][0]]])
        ys = np.concatenate(ys + [[points[-1][1]]])
        profile = _height_along(base, xs, ys)
        window = max(1, int(30 / STEP))
        smooth = np.convolve(np.pad(profile, window, mode="edge"), np.ones(window) / window, "same")
        profile = np.maximum(smooth[window:-window], PATH_MIN_ALTITUDE)
        rise = PATH_MAX_SLOPE * STEP
        for i in range(1, len(profile)):
            profile[i] = min(max(profile[i], profile[i - 1] - rise), profile[i - 1] + rise)
        for i in range(len(profile) - 2, -1, -1):
            profile[i] = min(max(profile[i], profile[i + 1] - rise), profile[i + 1] + rise)

        for ((ax, ay), (bx, by)), (first, count) in zip(zip(points, points[1:]), starts):
            reach = PATH_FALLOFF + 2
            part, x, y = _window(height, min(ax, bx) - reach, max(ax, bx) + reach,
                                 min(ay, by) - reach, max(ay, by) + reach)
            dx, dy = bx - ax, by - ay
            t = np.clip(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0, 1)
            distance = np.hypot(x - (ax + dx * t), y - (ay + dy * t))
            target = profile[first + np.rint(t * count).astype(int)]
            weight = smoothstep(PATH_FALLOFF, PATH_FLAT, distance) * ~_in_towns(x, y, 5)
            better = weight > best[part]
            carved = base[part] + (target - base[part]) * weight
            height[part] = np.where(better, carved, height[part])
            best[part] = np.where(better, weight, best[part])


def building_pads(height):
    """Plateforme plate sous les bâtiments et les postes (sinon pente ou mer), sauf dans les
    villes en terrasses."""
    # Les villes en terrasses (V1) ont déjà leurs niveaux : on n'y touche pas.
    sites = [(x, y, depth, width) for zone, _, x, y, depth, width, *_ in layout.BUILDINGS
             if zone not in layout.TERRACES]
    sites += [(x, y, 12.0, 12.0) for _, x, y, _ in layout.GATES]
    for x0, y0, depth, width in sites:
        reach = max(depth, width) / 2 + PAD_FALLOFF + 4
        part, x, y = _window(height, x0 - reach, x0 + reach, y0 - reach, y0 + reach)
        under = height[part][(np.abs(x - x0) < depth / 2) & (np.abs(y - y0) < width / 2)]
        level = max(float(np.median(under)), PATH_MIN_ALTITUDE)
        box = np.maximum(np.abs(x - x0) - depth / 2 - 3, np.abs(y - y0) - width / 2 - 3)
        weight = smoothstep(PAD_FALLOFF, 0, box)
        height[part] = height[part] + (level - height[part]) * weight


# --- Images et fichiers ---------------------------------------------------------------------

def write_png(path, width, height, rows, bit_depth, color_type):
    """PNG sans filtre de ligne (kanto_v1_layout.height_at sait le relire)."""
    def chunk(kind, data):
        payload = kind + data
        return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)

    compressor = zlib.compressobj(9)
    parts = [compressor.compress(b"\x00" + row) for row in rows]
    parts.append(compressor.flush())
    header = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
    with open(path, "wb") as png:
        png.write(b"\x89PNG\r\n\x1a\n")
        png.write(chunk(b"IHDR", header))
        png.write(chunk(b"IDAT", b"".join(parts)))
        png.write(chunk(b"IEND", b""))


def north_up(image):
    """[ligne = est, colonne = nord] -> image nord en haut, est à droite."""
    return np.transpose(image, (1, 0, 2) if image.ndim == 3 else (1, 0))[::-1]


def forest_mask(height, corridor):
    """Où planter la forêt (0 à 255), sur une grille 4 fois plus large que la heightmap,
    dans le même sens (ligne = est, colonne = nord) : hors des routes, au-dessus des plages,
    sous la limite des arbres et pas sur les pentes de rocher."""
    h = height[::4, ::4]
    spacing = STEP * 4
    slope = np.hypot(*np.gradient(h, spacing))
    off_road = smoothstep(2, 12, -corridor[::4, ::4])
    dry = smoothstep(2.0, 5.0, h)
    tree_line = smoothstep(260, 180, h)
    rock = smoothstep(1.4, 0.9, slope)
    return np.clip(off_road * dry * tree_line * rock * 255, 0, 255).astype(np.uint8)


def previews(height, corridor, forest):
    h = height[::4, ::4]
    spacing = STEP * 4
    d_north = np.gradient(h, spacing, axis=1)
    d_east = np.gradient(h, spacing, axis=0)
    slope = np.hypot(d_north, d_east)
    shade = np.clip(1.0 + 1.4 * (d_east - d_north) / 1.414, 0.4, 1.4)

    def mix(a, b, t):
        t = np.clip(t, 0, 1)[..., None]
        return np.array(a) * (1 - t) + np.array(b) * t

    color = mix((125, 165, 80), (100, 145, 70), h / 80.0)               # prairies des routes
    color = mix(color, (45, 90, 45), forest / 255.0)                    # forêt
    color = np.where((slope > 0.8)[..., None], mix(color, (130, 118, 104), (slope - 0.8) * 1.5), color)
    color = np.where((h > 220)[..., None], mix(color, (125, 115, 100), (h - 220) / 60), color)
    color = np.where((h > 330)[..., None], mix((125, 115, 100), (230, 228, 225), (h - 330) / 60), color)
    route = corridor[::4, ::4] > 0
    color = np.where(route[..., None], mix(color, (205, 200, 140), 0.15), color)
    color = np.where(((h < 2.0) & (h >= layout.SEA_LEVEL))[..., None], np.array((220, 205, 155)), color)
    color = color * shade[..., None]
    sea = mix((120, 215, 210), (40, 120, 175), -h / 6.0)               # haut-fond turquoise
    sea = mix(sea, (25, 70, 135), (-h - 6) / 9.0)
    color = np.where((h < layout.SEA_LEVEL)[..., None], sea, color)
    image = north_up(np.clip(color, 0, 255).astype(np.uint8))
    write_png(os.path.join(OUT, "kanto_v1_preview.png"), image.shape[1], image.shape[0],
              [r.tobytes() for r in image], 8, 2)

    # Heightmap en gris contrastés (noir = fond de la mer, blanc = sommets).
    grey = np.clip((h + 20) / (h.max() + 20), 0, 1) ** 0.6 * 255
    image = north_up(grey.astype(np.uint8))
    write_png(os.path.join(OUT, "kanto_v1_heightmap_apercu.png"), image.shape[1], image.shape[0],
              [r.tobytes() for r in image], 8, 0)


def export_lakes(lake_mask, height):
    """Lacs en hauteur pour Unreal : rectangle en mètres et niveau de l'eau.
    Lus sur le relief final (grille de 6 m), donc à leur place après la déformation."""
    from collections import deque
    spacing = STEP * 4
    lakes, seen = [], np.zeros(lake_mask.shape, bool)
    for r0, c0 in zip(*np.nonzero(lake_mask)):
        if seen[r0, c0]:
            continue
        seen[r0, c0] = True
        queue, cells = deque([(r0, c0)]), []
        while queue:
            r, c = queue.popleft()
            cells.append((r, c))
            for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                rr, cc = r + dr, c + dc
                if (0 <= rr < lake_mask.shape[0] and 0 <= cc < lake_mask.shape[1]
                        and lake_mask[rr, cc] and not seen[rr, cc]):
                    seen[rr, cc] = True
                    queue.append((rr, cc))
        if len(cells) < 6:
            continue
        rs, cs = np.array(cells).T  # ligne = est (y), colonne = nord (x)
        bottom = height[rs, cs]
        if np.percentile(bottom, 90) - np.percentile(bottom, 10) > 3.0:
            continue  # rivière en pente : une seule plaque d'eau déborderait
        level = float(np.median(bottom)) + 1.9
        lakes.append({"x0": round((cs.min() - 0.5) * spacing, 1), "x1": round((cs.max() + 0.5) * spacing, 1),
                      "y0": round((rs.min() - 0.5) * spacing, 1), "y1": round((rs.max() + 0.5) * spacing, 1),
                      "niveau": round(level, 2), "cases": len(cells)})
    with open(layout.LAKES_PATH, "w", encoding="utf-8") as handle:
        json.dump(lakes, handle, ensure_ascii=False, indent=1)
    return len(lakes)


def grid_xy(rows, count=SIZE, spacing=STEP):
    """Grilles x (nord, par colonne) et y (est, par ligne) en mètres."""
    y = np.asarray(rows, np.float64)[:, None] * spacing
    x = np.arange(count, dtype=np.float64)[None, :] * spacing
    return np.broadcast_to(x, (len(y), count)), np.broadcast_to(y, (len(y), count))


def main():
    start = time.time()
    fields = cell_fields()
    print(f"Carte et altitudes des routes : {time.time() - start:.0f} s")

    height = np.empty((SIZE, SIZE))
    erosion = np.empty((SIZE, SIZE), np.float32)
    lowest = np.empty((SIZE, SIZE), np.float32)
    corridor = np.empty((SIZE, SIZE), np.float32)
    wet = np.empty((SIZE, SIZE), bool)
    for row0 in range(0, SIZE, 256):
        part = slice(row0, min(SIZE, row0 + 256))
        x, y = grid_xy(range(part.start, part.stop))
        height[part], erosion[part], lowest[part], corridor[part], wet[part] = relief(x, y, fields)
    print(f"Relief : {time.time() - start:.0f} s")

    before = height.copy()
    # Grande érosion sur une grille 4 fois plus large (les vallées), puis petite (les ravines).
    coarse = height[::4, ::4].copy()
    scaled = coarse / (STEP * 4)
    erode(scaled, int(900_000 * DROPS), SEED, 80, 3, 0.05, 4.0, 0.01, 0.3, 0.25, 0.012, 4.0, 0.6)
    delta = scaled * STEP * 4 - coarse
    count = coarse.shape[0]
    for row0 in range(0, SIZE, 256):
        part = slice(row0, min(SIZE, row0 + 256))
        x, y = grid_xy(range(part.start, part.stop))
        d = sample(delta, x / (STEP * 4), y / (STEP * 4), 0.0)
        height[part] += d * erosion[part]
    height = np.maximum(height, lowest)
    scaled = height / STEP
    erode(scaled, int(5_000_000 * DROPS), SEED + 1, 48, 2, 0.1, 4.0, 0.01, 0.25, 0.3, 0.02, 4.0, 0.4)
    height += (scaled * STEP - height) * erosion
    height = np.maximum(height, lowest)
    # L'érosion ne creuse jamais sous le niveau de la mer ni sous le fond d'origine (pas de fossés
    # au pied des collines, côte propre).
    height = np.maximum(height, np.minimum(before, 0.3))
    del before
    print(f"Érosion : {time.time() - start:.0f} s ({count} × {count} puis {SIZE} × {SIZE})")

    for row0 in range(0, SIZE, 256):
        part = slice(row0, min(SIZE, row0 + 256))
        x, y = grid_xy(range(part.start, part.stop))
        # Léger bruit au sol (quelques dizaines de cm) pour casser le côté lisse.
        # Pas dans l'eau ; les villes sont remises à plat juste après par town_terraces.
        bumps = (fbm(x, y, 30, 3, SEED + 50) * GROUND_NOISE_LARGE
                 + fbm(x, y, 9, 2, SEED + 51) * GROUND_NOISE_SMALL)
        dry = smoothstep(0.5, 3.0, height[part]) * ~wet[part]
        height[part] += bumps * dry
        height[part] = lookouts(town_terraces(height[part], x, y), x, y)
    carve_paths(height)
    building_pads(height)
    print(f"Villes, chemins et plateformes : {time.time() - start:.0f} s")
    limit = 32767 / layout.HEIGHT_UNITS_PER_METER
    height = np.clip(height, -40.0, limit)

    # Ligne r de l'image = axe Y du Landscape (est), colonne c = axe X (nord).
    values = np.clip(np.round(32768 + height * layout.HEIGHT_UNITS_PER_METER), 0, 65535).astype(">u2")
    forest = forest_mask(height, corridor)
    if TEST:
        np.save(os.path.join(OUT, "test_height.npy"), height.astype(np.float32))
        np.save(os.path.join(OUT, "test_corridor.npy"), corridor.astype(np.float32))
        previews(height, corridor, forest)
        print(f"Essai : altitude min {height.min():.1f} m, max {height.max():.1f} m "
              f"({time.time() - start:.0f} s)")
        return
    write_png(layout.HEIGHTMAP_PATH, SIZE, SIZE, [r.tobytes() for r in values], 16, 0)
    write_png(layout.FOREST_MASK_PATH, forest.shape[1], forest.shape[0], [r.tobytes() for r in forest], 8, 0)
    step = int(layout.CORRIDOR_STEP / STEP)
    sd = np.clip(np.round(corridor[::step, ::step]), -127, 127).astype(np.int8)
    with open(layout.CORRIDORS_PATH, "wb") as handle:
        handle.write(zlib.compress(struct.pack("<4sH", b"CORR", sd.shape[0]) + sd.tobytes(), 9))
    lakes = export_lakes(wet[::4, ::4], height[::4, ::4])
    previews(height, corridor, forest)
    print(f"Altitude min {height.min():.1f} m, max {height.max():.1f} m ; {lakes} lacs "
          f"({time.time() - start:.0f} s)")


if __name__ == "__main__":
    main()
