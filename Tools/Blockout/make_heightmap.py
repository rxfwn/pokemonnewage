"""Fabrique le relief de Kanto (SCRUM-62) à partir de kanto_v1_layout.py.

Sortie :
- kanto_v1_heightmap.png : heightmap 16 bits en niveaux de gris, à importer dans le Landscape.
  Unreal (blockout_kanto_v1.py) relit ce même fichier pour poser les objets sur le sol ;
- kanto_v1_preview.png : aperçu en couleurs et ombré (nord en haut) pour vérifier le plan.

Lancement : pip install numpy numba, puis python Tools/Blockout/make_heightmap.py
(quelques minutes). Valeur 32768 = altitude 0 ; 128 unités = 1 m avec l'échelle Z de 100.

Étapes, comme pour un vrai terrain :
1. la forme de Kanto (terre / mer) vient de la carte de Let's Go, avec des côtes déformées ;
2. le relief de base : massifs en crêtes, collines, plaines, falaises et plages ;
3. l'érosion par la pluie, en deux passes (grandes vallées, puis ravines) : des milliers de
   gouttes coulent sur la pente, creusent et redéposent la terre ;
4. les lieux de la V1 sont posés au fond de larges vallées douces (sol plat dans les villes,
   légères ondulations sur les routes), puis l'eau est creusée (étang, ruisseau, mer).
"""

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
COARSE = 4  # la grande érosion se fait sur une grille 4 fois plus large (6 m)

# Réglages du relief (mètres).
MOUNTAIN_HEIGHT = 260.0
HILL_HEIGHT = 45.0
VALLEY_WIDTH = 200.0  # distance sur laquelle le relief remonte autour des lieux de la V1
SEED = 151


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
    """Bruit en crêtes (0 à 1) : arêtes vives comme de vraies montagnes."""
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


# --- Forme de Kanto -----------------------------------------------------------------

def _land_planes():
    planes = [np.frombuffer(p, np.uint8).reshape(layout._LAND_H, layout._LAND_W).astype(np.float32)
              for p in (layout._LAND_FINE, layout._LAND_WIDE, layout._LAND_INLAND)]
    return planes[0] / 255.0, planes[1] / 255.0, planes[2] * layout.MAP_SCALE


def sample(plane, u, v):
    """Lecture interpolée d'un plan de la carte ; 0 hors de la carte (la mer)."""
    h, w = plane.shape
    inside = (u >= 0) & (u < w - 1) & (v >= 0) & (v < h - 1)
    uc, vc = np.clip(u, 0, w - 1.001), np.clip(v, 0, h - 1.001)
    iu, iv = uc.astype(np.int64), vc.astype(np.int64)
    tu, tv = uc - iu, vc - iv
    a, b = plane[iv, iu], plane[iv, iu + 1]
    c, d = plane[iv + 1, iu], plane[iv + 1, iu + 1]
    value = (a * (1 - tu) + b * tu) * (1 - tv) + (c * (1 - tu) + d * tu) * tv
    return np.where(inside, value, 0.0)


def to_map(x, y):
    return (y - layout.MAP_OFFSET_Y) / layout.MAP_SCALE, layout.MAP_HEIGHT - (x - layout.MAP_OFFSET_X) / layout.MAP_SCALE


def _warped(x, y):
    """Coordonnées déformées : côtes et massifs moins réguliers."""
    wx = x + fbm(x, y, 700, 3, SEED + 1) * 140
    wy = y + fbm(x, y, 700, 3, SEED + 2) * 140
    return to_map(wx, wy)


def coast_and_sea(x, y, wide_v):
    """(part de terre entre 0 et 1, profondeur de la mer) : la côte ondule grâce au bruit."""
    sea = layout.SEA_LEVEL - 2.0 - 28.0 * (1 - wide_v)
    edge = wide_v + fbm(x, y, 300, 4, SEED + 7) * 0.45
    coast = smoothstep(0.42, 0.58, edge)
    for _, sx, sy, half_x, half_y in layout.SEA:
        norm = np.hypot((x - sx) / half_x, (y - sy) / half_y)
        outside = smoothstep(0.8, 1.15, norm + fbm(x, y, 120, 3, SEED + 8) * 0.12)
        coast *= outside
        sea = np.minimum(sea, -2.0 - 14.0 * (1 - outside))
    return coast, sea


def base_terrain(x, y, planes):
    """Relief naturel avant érosion (mètres), sur des grilles x (nord), y (est)."""
    fine, wide, inland = planes
    u, v = _warped(x, y)
    wide_v = sample(wide, u, v)
    inland_v = sample(inland, u, v)

    # Part de montagne : massifs de la carte, union douce, et un peu de hasard partout.
    mountain = np.zeros_like(x)
    for cu, cv, ru, rv, weight in layout.MOUNTAINS:
        norm = np.hypot((u - cu) / ru, (v - cv) / rv)
        mountain = 1 - (1 - mountain) * (1 - weight * smoothstep(1.0, 0.15, norm))
    extra = smoothstep(0.15, 0.45, fbm(x, y, 1200, 2, SEED + 3)) * 0.6
    mountain = 1 - (1 - mountain) * (1 - extra)
    mountain *= smoothstep(20, 260, inland_v)

    peaks = MOUNTAIN_HEIGHT * mountain * (0.2 + 0.8 * ridged(x, y, 650, 6, SEED + 4)) * (0.55 + 0.45 * mountain)
    hills = HILL_HEIGHT * (0.5 + 0.5 * fbm(x, y, 420, 6, SEED + 5)) ** 1.4 * smoothstep(0, 180, inland_v)
    plain = 1.0 + np.minimum(inland_v, 600) * 0.012
    land = plain + hills + peaks

    # Côtes : falaises par endroits, plages ailleurs.
    cliff = smoothstep(0.1, 0.45, fbm(x, y, 900, 3, SEED + 6))
    shore = smoothstep(0.5, 0.56 + 0.44 * (1 - cliff), wide_v)
    land = 0.6 + (land - 0.6) * shore

    coast, sea = coast_and_sea(x, y, wide_v)
    return sea + (land - sea) * coast


# --- Érosion par gouttes de pluie (numba) ------------------------------------------

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


def keep_land(before, after):
    """L'érosion ne doit pas faire entrer la mer dans les terres : les fonds de vallée
    s'arrêtent un peu au-dessus de l'eau, comme des plaines de rivière."""
    floor = np.minimum(before, 0.5 + 0.02 * np.maximum(before, 0))
    return np.where(before > 0.3, np.maximum(after, floor), after)


# --- Lieux de la V1 (copie numpy de kanto_v1_layout.zone_distance) ------------------

def zone_fields(x, y):
    """(distance au lieu le plus proche en m, indice du lieu) ; négative à l'intérieur."""
    u, v = to_map(x, y)
    wobble = (fractal_noise(x, y, 60, 3, 21) - 0.5) * 2
    names = []
    best = np.full(x.shape, 1e9)
    best_index = np.zeros(x.shape, np.int32)
    for name, cu, cv, half_east, half_north, shape in layout.AREAS:
        cx, cy = layout.from_map(cu, cv)
        a, b = np.abs(y - cy) / half_east, np.abs(x - cx) / half_north
        norm = (a ** shape + b ** shape) ** (1.0 / shape)
        distance = (norm - 1 - wobble * 0.12) * min(half_east, half_north)
        better = distance < best
        best = np.where(better, distance, best)
        best_index = np.where(better, len(names), best_index)
        names.append(name)
    for name, half_width, points in layout.ROUTES:
        segment = np.full(x.shape, 1e9)
        for (au, av), (bu, bv) in zip(points, points[1:]):
            du, dv = bu - au, bv - av
            t = np.clip(((u - au) * du + (v - av) * dv) / (du * du + dv * dv), 0, 1)
            segment = np.minimum(segment, np.hypot(u - (au + du * t), v - (av + dv * t)))
        distance = segment * layout.MAP_SCALE - half_width * (1 + wobble * 0.3)
        better = distance < best
        best = np.where(better, distance, best)
        best_index = np.where(better, len(names), best_index)
        names.append(name)
    return best, best_index, names


def polyline_distance(x, y, points):
    best = np.full(x.shape, 1e9)
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        dx, dy = bx - ax, by - ay
        t = np.clip(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0, 1)
        best = np.minimum(best, np.hypot(x - (ax + dx * t), y - (ay + dy * t)))
    return best


def place_v1(height, x, y):
    """Pose les lieux de la V1 au fond de vallées douces, puis creuse l'eau."""
    distance, index, names = zone_fields(x, y)
    t = np.clip((x - layout._FLOOR_X0) / (layout._FLOOR_X1 - layout._FLOOR_X0), 0, 1)
    floor = layout.FLOOR_SOUTH + (layout.FLOOR_NORTH - layout.FLOOR_SOUTH) * t
    # Ondulations du sol : presque rien en ville, un peu sur les routes, davantage en forêt.
    bumps = np.full(x.shape, 0.6)
    for i, name in enumerate(names):
        if name in ("Bourg Palette", "Jadielle", "Argenta"):
            bumps = np.where(index == i, 0.15, bumps)
        elif name == "Forêt de Jade":
            bumps = np.where(index == i, 2.0, bumps)
    ground = floor + fbm(x, y, 90, 4, SEED + 20) * bumps

    outside = np.maximum(distance, 0.0)
    # Plus haut que la vallée : le relief remonte lentement ; plus bas (côte) : plage plus courte.
    rise = smoothstep(0, VALLEY_WIDTH, outside) ** 1.3
    fall = smoothstep(0, 45, outside)
    blend = np.where(height > ground, rise, fall)
    result = ground + (height - ground) * blend
    result = np.where(distance <= 0, ground, result)

    for _, half, points in layout.CHANNELS:
        d = polyline_distance(x, y, points) - half
        bed = -1.2  # sous le niveau de la mer : l'eau apparaît toute seule
        result = np.where(d < 2.5, np.minimum(result, bed + (result - bed) * smoothstep(-0.5, 2.5, d)), result)
    return result


# --- Images ---------------------------------------------------------------------------

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


def preview(height, path):
    """Aperçu nord en haut, un point sur quatre, coloré et ombré (soleil au nord-ouest)."""
    h = height[::4, ::4]  # [ligne = est, colonne = nord]
    spacing = STEP * 4
    d_north = np.gradient(h, spacing, axis=1)
    d_east = np.gradient(h, spacing, axis=0)
    slope = np.hypot(d_north, d_east)
    shade = np.clip(1.0 + 1.6 * (d_east - d_north) / 1.414, 0.45, 1.4)

    def mix(a, b, t):
        t = np.clip(t, 0, 1)[..., None]
        return np.array(a) * (1 - t) + np.array(b) * t

    color = mix((105, 150, 70), (50, 100, 45), h / 70.0)               # prairies puis forêts
    color = np.where((h > 90)[..., None], mix((50, 100, 45), (120, 105, 85), (h - 90) / 80), color)
    color = np.where((slope > 0.7)[..., None], mix(color, (125, 112, 98), (slope - 0.7) * 2), color)
    color = np.where((h > 190)[..., None], mix((120, 105, 85), (215, 210, 205), (h - 190) / 50), color)
    # Sable : bas et à moins de ~40 m de la mer.
    near_sea = h < layout.SEA_LEVEL
    for _ in range(7):
        near_sea = (near_sea | np.roll(near_sea, 1, 0) | np.roll(near_sea, -1, 0)
                    | np.roll(near_sea, 1, 1) | np.roll(near_sea, -1, 1))
    color = np.where(((h < 2.5) & near_sea)[..., None], np.array((215, 200, 150)), color)
    color = color * shade[..., None]
    sea = mix((95, 165, 200), (35, 85, 150), -h / 25.0)
    color = np.where((h < layout.SEA_LEVEL)[..., None], sea, color)

    # Lieux de la V1 en plus clair, pour s'y retrouver.
    rows_m = np.arange(h.shape[0])[:, None] * spacing
    cols_m = np.arange(h.shape[1])[None, :] * spacing
    distance, _, _ = zone_fields(np.broadcast_to(cols_m, h.shape), np.broadcast_to(rows_m, h.shape))
    color = np.where((distance <= 0)[..., None] & (h >= 0)[..., None], mix(color, (215, 225, 160), 0.35), color)

    image = np.clip(color, 0, 255).astype(np.uint8)
    image = np.transpose(image, (1, 0, 2))[::-1]  # nord en haut
    write_png(path, image.shape[1], image.shape[0], [r.tobytes() for r in image], 8, 2)


# --- Programme --------------------------------------------------------------------------

def grid(count, spacing, rows=None):
    """Grilles x (nord, par colonne) et y (est, par ligne) en mètres."""
    rows = range(count) if rows is None else rows
    y = np.asarray(rows, np.float64)[:, None] * spacing
    x = np.arange(count, dtype=np.float64)[None, :] * spacing
    return np.broadcast_to(x, (len(y), count)), np.broadcast_to(y, (len(y), count))


def main():
    start = time.time()
    planes = _land_planes()

    # 1. Relief de base sur la grille large (6 m), puis grande érosion : les vallées.
    coarse_count = (SIZE - 1) // COARSE + 1
    coarse_step = STEP * COARSE
    x, y = grid(coarse_count, coarse_step)
    coarse = base_terrain(x, y, planes)
    print(f"Relief de base : {time.time() - start:.0f} s")
    scaled = (coarse / coarse_step).astype(np.float64)
    erode(scaled, 700_000, SEED, 80, 3, 0.05, 4.0, 0.01, 0.3, 0.25, 0.012, 4.0, 0.6)
    coarse = keep_land(coarse, scaled * coarse_step)
    print(f"Grande érosion : {time.time() - start:.0f} s")

    # 2. Pleine résolution (1,5 m) : agrandissement, détails, puis petite érosion (ravines).
    height = np.empty((SIZE, SIZE), np.float64)
    for row0 in range(0, SIZE, 256):
        rows = range(row0, min(SIZE, row0 + 256))
        x, y = grid(SIZE, STEP, rows)
        cr = np.clip(y / coarse_step, 0, coarse_count - 1.001)
        cc = np.clip(x / coarse_step, 0, coarse_count - 1.001)
        ir, ic = cr.astype(np.int64), cc.astype(np.int64)
        tr, tc = cr - ir, cc - ic
        h = ((coarse[ir, ic] * (1 - tc) + coarse[ir, ic + 1] * tc) * (1 - tr)
             + (coarse[ir + 1, ic] * (1 - tc) + coarse[ir + 1, ic + 1] * tc) * tr)
        # La côte est recalculée à pleine résolution : pas d'escaliers de la grille large.
        u, v = _warped(x, y)
        coast, sea = coast_and_sea(x, y, sample(planes[1], u, v))
        h = sea + (np.maximum(h, 0.6) - sea) * coast
        rough = smoothstep(1, 40, h)  # pas de détail sous la mer ni sur les plages
        detail = fbm(x, y, 60, 5, SEED + 30) * (1.2 + h * 0.02) + ridged(x, y, 140, 3, SEED + 31) * h * 0.04
        height[row0:row0 + len(rows)] = h + detail * rough
    print(f"Détails : {time.time() - start:.0f} s")
    scaled = height / STEP
    erode(scaled, 6_000_000, SEED + 1, 48, 2, 0.1, 4.0, 0.01, 0.25, 0.3, 0.02, 4.0, 0.4)
    height = keep_land(height, scaled * STEP)
    print(f"Petite érosion : {time.time() - start:.0f} s")

    # 3. Lieux de la V1 et eau.
    for row0 in range(0, SIZE, 256):
        rows = range(row0, min(SIZE, row0 + 256))
        x, y = grid(SIZE, STEP, rows)
        height[row0:row0 + len(rows)] = place_v1(height[row0:row0 + len(rows)], x, y)
    height = np.clip(height, -40.0, 250.0)
    print(f"Lieux de la V1 : {time.time() - start:.0f} s")

    # Ligne r de l'image = axe Y du Landscape (est), colonne c = axe X (nord).
    values = np.clip(np.round(32768 + height * 128), 0, 65535).astype(">u2")
    write_png(os.path.join(HERE, "kanto_v1_heightmap.png"), SIZE, SIZE, [r.tobytes() for r in values], 16, 0)
    preview(height, os.path.join(HERE, "kanto_v1_preview.png"))
    print(f"Altitude min {height.min():.1f} m, max {height.max():.1f} m ({time.time() - start:.0f} s)")


if __name__ == "__main__":
    main()
