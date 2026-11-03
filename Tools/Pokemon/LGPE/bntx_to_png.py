"""Lit une texture Switch .bntx (Pokémon Let's Go) et l'enregistre en PNG (premier niveau de mip seulement).

Formats gérés : RGBA8, BC1 à BC5, BC7 (décodés avec le paquet PyPI texture2ddecoder).
Le stockage « block linear » du Tegra X1 est remis à plat (même règle que la doc NVIDIA : GOB de 64 × 8 octets).

    pip install texture2ddecoder numpy pillow
    python Tools/Pokemon/LGPE/bntx_to_png.py <fichier.bntx> <sortie.png>
"""

import struct
import sys

import numpy as np
import texture2ddecoder
from PIL import Image

# format (octet haut) -> (largeur bloc, hauteur bloc, octets par bloc, décodeur)
FORMATS = {
    0x0B: (1, 1, 4, None),
    0x1A: (4, 4, 8, texture2ddecoder.decode_bc1),
    0x1C: (4, 4, 16, texture2ddecoder.decode_bc3),
    0x1D: (4, 4, 8, texture2ddecoder.decode_bc4),
    0x1E: (4, 4, 16, texture2ddecoder.decode_bc5),
    0x20: (4, 4, 16, texture2ddecoder.decode_bc7),
}
# BRTI : magic, taille, taille64, tileMode, dim, flags, swizzle, numMips, unk, format, unk, largeur, hauteur,
# unk, faces, sizeRange, 6 inconnus, imageSize, alignement, compSel, type, nom, parent, pointeurs
BRTI = struct.Struct("<4siq2b3H3I5i6I4i3q")


def div_up(n, d):
    return (n + d - 1) // d


def deswizzle(data, width_blocks, height_blocks, bpp, block_height):
    """Block linear -> lignes. Adresse d'un élément (x en octets, y en lignes de blocs), d'après le TRM du Tegra X1."""
    gobs_x = div_up(width_blocks * bpp, 64)
    y = np.arange(height_blocks)[:, None]
    xb = (np.arange(width_blocks) * bpp)[None, :]
    gob = ((y // (8 * block_height)) * 512 * block_height * gobs_x
           + (xb // 64) * 512 * block_height
           + (y % (8 * block_height) // 8) * 512)
    addr = gob + ((xb % 64) // 32) * 256 + ((y % 8) // 2) * 64 + ((xb % 32) // 16) * 32 + (y % 2) * 16 + (xb % 16)
    src = np.frombuffer(data, np.uint8)
    need = int(addr.max()) + bpp
    if need > len(src):
        src = np.concatenate([src, np.zeros(need - len(src), np.uint8)])
    idx = addr[..., None] + np.arange(bpp)
    return src[idx].tobytes()


def read_bntx(buf):
    """Renvoie une liste de (nom, image PIL)."""
    if buf[:4] != b"BNTX" or buf[0xC:0xE] != b"\xFF\xFE":
        raise ValueError("pas un BNTX little-endian")
    nx = 0x20
    count, info_ptrs = struct.unpack_from("<Iq", buf, nx + 4)
    out = []
    for i in range(count):
        pos = struct.unpack_from("<q", buf, info_ptrs + i * 8)[0]
        f = BRTI.unpack_from(buf, pos)
        tile_mode, fmt, width, height, size_range = f[3], f[9], f[11], f[12], f[15]
        image_size, name_addr, ptrs = f[22], f[26], f[28]
        name_len = struct.unpack_from("<H", buf, name_addr)[0]
        name = buf[name_addr + 2:name_addr + 2 + name_len].decode("utf-8")
        data_addr = struct.unpack_from("<q", buf, ptrs)[0]
        bw, bh, bpp, decoder = FORMATS[fmt >> 8]
        wb, hb = div_up(width, bw), div_up(height, bh)
        raw = buf[data_addr:data_addr + image_size]
        if tile_mode == 0:
            pitch = (wb * bpp + 31) // 32 * 32
            lin = b"".join(raw[y * pitch:y * pitch + wb * bpp] for y in range(hb))
        else:
            # hauteur de bloc : 1 << sizeRange pour le niveau 0
            lin = deswizzle(raw, wb, hb, bpp, 1 << size_range)
        if decoder is None:
            img = Image.frombytes("RGBA", (width, height), lin[:width * height * 4])
        else:
            bgra = decoder(lin, wb * bw, hb * bh)
            img = Image.frombytes("RGBA", (wb * bw, hb * bh), bgra, "raw", "BGRA").crop((0, 0, width, height))
        out.append((name, img))
    return out


def main():
    src, dst = sys.argv[1:3]
    with open(src, "rb") as fh:
        textures = read_bntx(fh.read())
    textures[0][1].save(dst)
    print(f"{src} -> {dst} {textures[0][1].size}")


if __name__ == "__main__":
    main()
