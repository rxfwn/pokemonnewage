"""Fabrique le relief de Kanto (SCRUM-62) à partir de kanto_v1_layout.py.

Sortie :
- kanto_v1_heightmap.png : heightmap 16 bits en niveaux de gris, à importer dans le Landscape ;
- kanto_v1_preview.png : aperçu en couleurs (nord en haut) pour vérifier le plan.

Lancement : python Tools/Blockout/make_heightmap.py (Python 3, sans bibliothèque à installer,
quelques minutes : le calcul est réparti sur tous les cœurs du processeur).
Valeur 32768 = altitude 0 ; 128 unités = 1 m avec l'échelle Z de 100 du Landscape.
"""

import multiprocessing
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kanto_v1_layout as layout  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def write_png(path, width, height, rows, bit_depth, color_type):
    def chunk(kind, data):
        payload = kind + data
        return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + row for row in rows)
    header = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
    with open(path, "wb") as png:
        png.write(b"\x89PNG\r\n\x1a\n")
        png.write(chunk(b"IHDR", header))
        png.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        png.write(chunk(b"IEND", b""))


def _mix(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def preview_color(height, x, y, shade):
    """Couleur d'un point de l'aperçu, avec un ombrage (lumière venant du nord-ouest)."""
    if height < layout.SEA_LEVEL:
        depth = min(1.0, -height / 25.0)
        color = _mix((95, 165, 200), (35, 85, 150), depth)
        shade = 1.0
    else:
        zone = layout.find_zone(x, y)
        if zone == "Forêt de Jade":
            color = (40, 90, 40)
        elif zone is not None:
            color = (150, 185, 95)
        elif height < 2.5 and layout.land_at(x, y)[2] < 40:
            color = (215, 200, 150)  # plage
        elif height < 60:
            color = _mix((105, 150, 70), (55, 110, 50), height / 60.0)
        elif height < 140:
            color = _mix((55, 110, 50), (125, 105, 80), (height - 60) / 80.0)
        else:
            color = _mix((125, 105, 80), (210, 205, 200), (height - 140) / 90.0)
    return tuple(int(max(0, min(255, c * shade))) for c in color)


def _row(row):
    size, step = layout.HEIGHTMAP_SIZE, layout.METERS_PER_PIXEL
    y = row * step
    return [layout.height_at(column * step, y) for column in range(size)]


def main():
    size = layout.HEIGHTMAP_SIZE
    step = layout.METERS_PER_PIXEL
    # Ligne r de l'image = axe Y du Landscape (est), colonne c = axe X (nord).
    with multiprocessing.Pool() as pool:
        heights = pool.map(_row, range(size), chunksize=16)

    rows16 = []
    for line in heights:
        values = [max(0, min(65535, int(round(32768 + h * 128)))) for h in line]
        rows16.append(struct.pack(f">{size}H", *values))
    write_png(os.path.join(HERE, "kanto_v1_heightmap.png"), size, size, rows16, 16, 0)

    # Aperçu : nord en haut, est à droite (on tourne le repère du Landscape), un point sur deux.
    preview_rows = []
    for column in reversed(range(0, size, 2)):
        x = column * step
        pixels = bytearray()
        for row in range(0, size, 2):
            h = heights[row][column]
            # Pente vers le nord (colonne) et vers l'est (ligne), en m par m.
            north = (heights[row][min(column + 2, size - 1)] - heights[row][max(column - 2, 0)]) / (4 * step)
            east = (heights[min(row + 2, size - 1)][column] - heights[max(row - 2, 0)][column]) / (4 * step)
            # Lumière du nord-ouest : éclaire les pentes tournées vers le nord et l'ouest.
            shade = 1.0 + 1.2 * (east - north) / 1.414
            shade = max(0.45, min(1.35, shade))
            pixels.extend(preview_color(h, x, row * step, shade))
        preview_rows.append(bytes(pixels))
    half = (size + 1) // 2
    write_png(os.path.join(HERE, "kanto_v1_preview.png"), half, half, preview_rows, 8, 2)

    flat = [h for line in heights for h in line]
    print(f"Altitude min {min(flat):.1f} m, max {max(flat):.1f} m")


if __name__ == "__main__":
    main()
