"""Fabrique le relief de la V1 (SCRUM-62) à partir de kanto_v1_layout.py.

Sortie :
- kanto_v1_heightmap.png : heightmap 16 bits en niveaux de gris, à importer dans le Landscape ;
- kanto_v1_preview.png : aperçu en couleurs (nord en haut) pour vérifier le plan.

Lancement : python Tools/Blockout/make_heightmap.py (Python 3, sans bibliothèque à installer).
Valeur 32768 = altitude 0 ; 128 unités = 1 m avec l'échelle Z de 100 du Landscape.
"""

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


def preview_color(height, x, y):
    if height < layout.SEA_LEVEL:
        depth = min(1.0, -height / 20.0)
        return (int(70 - 40 * depth), int(130 - 60 * depth), int(190 - 60 * depth))
    zone = layout.find_zone(x, y)
    if zone == "Forêt de Jade":
        return (45, 100, 45)
    if zone is not None:
        return (120, 175, 85)
    if height > 60:
        shade = min(1.0, (height - 60) / 120.0)
        return (int(130 + 90 * shade), int(120 + 95 * shade), int(105 + 110 * shade))
    shade = min(1.0, height / 60.0)
    return (int(80 + 60 * shade), int(130 - 20 * shade), int(60 + 30 * shade))


def main():
    size = layout.HEIGHTMAP_SIZE
    step = layout.METERS_PER_PIXEL
    heights = []
    # Ligne r de l'image = axe Y du Landscape (est), colonne c = axe X (nord).
    for row in range(size):
        y = row * step
        heights.append([layout.height_at(column * step, y) for column in range(size)])
        if row % 100 == 0:
            print(f"relief : {row}/{size}")

    rows16 = []
    for line in heights:
        values = [max(0, min(65535, int(round(32768 + h * 128)))) for h in line]
        rows16.append(struct.pack(f">{size}H", *values))
    write_png(os.path.join(HERE, "kanto_v1_heightmap.png"), size, size, rows16, 16, 0)

    # Aperçu : nord en haut, est à droite (on tourne le repère du Landscape).
    preview_rows = []
    for column in reversed(range(size)):
        x = column * step
        pixels = bytearray()
        for row in range(size):
            pixels.extend(preview_color(heights[row][column], x, row * step))
        preview_rows.append(bytes(pixels))
    write_png(os.path.join(HERE, "kanto_v1_preview.png"), size, size, preview_rows, 8, 2)

    flat = [h for line in heights for h in line]
    print(f"Altitude min {min(flat):.1f} m, max {max(flat):.1f} m")


if __name__ == "__main__":
    main()
