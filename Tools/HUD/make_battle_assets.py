"""Images de l'écran de combat d'après la maquette Combat_Hud de L'amie (CSS Figma du 2026-11-01).

Lancer : python3 Tools/HUD/make_battle_assets.py (Pillow). Écrit Tools/HUD/Assets/T_Battle_*.png, importés dans
/Game/UI/HUD par Tools/HUD/import_hud_unreal.py (tout seul à l'ouverture de l'éditeur).
Dessinées au double de la taille de la maquette (nettes en 1440p / 4K) ; le C++ les affiche à la taille de la maquette.
  - T_Battle_Button_Fight / Bag / Pokemon / Run : boutons 265 x 85, coins 10, dégradés du CSS ;
  - T_Battle_HPFill_Green / Orange / Red : barres de PV 452 x 27 en forme de pilule, comme le rendu Figma de Life_Bar
    (relevé au pixel le 2026-11-01) : dégradé du CSS, contour intérieur clair de 1 px, relief (plus sombre à 6 px du bord,
    plus clair au centre et en bas) et ombre portée floue (box-shadow 0 4px 4px noir 25 %). Une marge de BAR_PAD px
    tout autour garde la place de l'ombre ; le C++ étire l'image en « boîte » (les bouts restent ronds) ;
  - T_Battle_HPTrack : fond de la barre (blanc 13 % -> 7,6 %, plus clair en bas), même contour et même ombre ;
  - T_Battle_Back : flèche retour blanche 31 x 27.
"""

import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

SCALE = 2
# Barres de PV (px de la maquette) : taille et marge pour l'ombre, à garder égales à BattleHUD::HPBarSize / HPBarPad
BAR_W, BAR_H, BAR_PAD = 452, 27, 8
# Relief relevé sur Life_Bar : couleur = dégradé x RELIEF(distance au bord) + LIGHT(hauteur), contour à 1 px du bord
RELIEF = [(1, 0.903), (2, 0.889), (3, 0.868), (4, 0.85), (5, 0.836), (6, 0.832), (7, 0.837), (8, 0.848), (9, 0.864),
          (10, 0.881), (11, 0.892), (12, 0.90), (13.5, 0.904)]
LIGHT_TOP, LIGHT_BOTTOM = -7.0, 16.0
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Assets")


def hex_color(value, alpha=1.0):
    value = value.lstrip("#")
    return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16), alpha)


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(4))


def gradient(width, height, stops):
    """Dégradé horizontal (CSS 90deg) : stops = [(position 0..1, (r, g, b, a 0..1)), ...]"""
    image = Image.new("RGBA", (width, height))
    pixels = image.load()
    for x in range(width):
        t = (x + 0.5) / width
        color = stops[-1][1]
        for (p0, c0), (p1, c1) in zip(stops, stops[1:]):
            if p0 <= t <= p1:
                color = lerp(c0, c1, (t - p0) / max(p1 - p0, 1e-6))
                break
        if t < stops[0][0]:
            color = stops[0][1]
        rgba = (round(color[0]), round(color[1]), round(color[2]), round(color[3] * 255))
        for y in range(height):
            pixels[x, y] = rgba
    return image


def rounded(image, radius):
    """Coins arrondis, bords lissés (masque dessiné 4x plus grand puis réduit)"""
    width, height = image.size
    big = Image.new("L", (width * 4, height * 4), 0)
    ImageDraw.Draw(big).rounded_rectangle((0, 0, width * 4 - 1, height * 4 - 1), radius=radius * 4, fill=255)
    mask = big.resize((width, height), Image.LANCZOS)
    out = image.copy()
    out.putalpha(ImageChops.multiply(image.getchannel("A"), mask))
    return out


def save(name, image):
    path = os.path.join(OUT, name + ".png")
    image.save(path)
    print(path, image.size)


def button(name, stops):
    save(name, rounded(gradient(265 * SCALE, 85 * SCALE, stops), 10 * SCALE))


def bar_shape():
    """Distance au bord de la pilule (px de la maquette, > 0 dedans) pour chaque pixel de l'image avec sa marge"""
    width, height = (BAR_W + 2 * BAR_PAD) * SCALE, (BAR_H + 2 * BAR_PAD) * SCALE
    xs = (np.arange(width) + 0.5) / SCALE - BAR_PAD
    ys = (np.arange(height) + 0.5) / SCALE - BAR_PAD
    x, y = np.meshgrid(xs, ys)
    radius = BAR_H / 2
    cx = np.clip(x, radius, BAR_W - radius)
    distance = radius - np.hypot(x - cx, y - radius)
    return x, y, distance


def bar_shadow(distance):
    """box-shadow: 0 4px 4px rgba(0, 0, 0, 0.25) : la forme décalée de 4 px vers le bas puis floutée"""
    coverage = np.clip(distance * SCALE + 0.5, 0, 1)
    mask = Image.fromarray((coverage * 255).astype(np.uint8))
    shifted = Image.new("L", mask.size, 0)
    shifted.paste(mask, (0, 4 * SCALE))
    blurred = shifted.filter(ImageFilter.GaussianBlur(2 * SCALE))
    return np.asarray(blurred).astype(float) / 255 * 0.25


def compose(color, alpha, shadow):
    """Pose la barre (couleur 0-255, opacité 0-1) sur son ombre noire"""
    out_alpha = alpha + shadow * (1 - alpha)
    rgb = color * (alpha / np.maximum(out_alpha, 1e-6))[..., None]
    image = np.dstack([np.clip(rgb, 0, 255), np.clip(out_alpha * 255, 0, 255)]).astype(np.uint8)
    return Image.fromarray(image, "RGBA")


def bar(name, stops):
    """Barre pleine : dégradé du CSS sur la longueur, relief et contour clair du rendu Figma"""
    x, y, distance = bar_shape()
    t = np.clip(x / BAR_W, 0, 1)
    color = np.zeros(x.shape + (3,))
    for (p0, c0), (p1, c1) in zip(stops, stops[1:]):
        inside = (t >= p0) & (t <= p1)
        k = ((t - p0) / max(p1 - p0, 1e-6))[..., None]
        color = np.where(inside[..., None], np.array(c0[:3]) + (np.array(c1[:3]) - np.array(c0[:3])) * k, color)
    relief = np.interp(distance, [d for d, _ in RELIEF], [v for _, v in RELIEF])
    light = LIGHT_TOP + (LIGHT_BOTTOM - LIGHT_TOP) * np.clip(y / BAR_H, 0, 1)
    color = color * relief[..., None] + light[..., None]
    # Contour : 1 px de blanc à 60 % juste à l'intérieur du bord
    stroke = np.clip(1.0 - distance, 0, 1) * (distance > -0.5)
    color = color * (1 - 0.6 * stroke[..., None]) + 255 * 0.6 * stroke[..., None]
    alpha = np.clip(distance * SCALE + 0.5, 0, 1)
    save(name, compose(color, alpha, bar_shadow(distance)))


def bar_track(name):
    """Fond de la barre (Rectangle 15) : blanc 13 % -> 7,6 % sur la longueur, plus clair en bas, contour blanc à 30 %"""
    x, y, distance = bar_shape()
    t = np.clip(x / BAR_W, 0, 1)
    opacity = (0.13 + (0.076 - 0.13) * t) * (0.75 + 0.65 * np.clip(y / BAR_H, 0, 1))
    stroke = np.clip(1.0 - distance, 0, 1) * (distance > -0.5)
    opacity = opacity * (1 - stroke) + 0.3 * stroke
    alpha = np.clip(distance * SCALE + 0.5, 0, 1) * opacity
    save(name, compose(np.full(x.shape + (3,), 255.0), alpha, bar_shadow(distance)))


def back_arrow():
    """Flèche « retour » : arc vers la droite puis pointe vers la gauche en haut (comme le picto de la maquette)"""
    size = (31 * SCALE * 4, 27 * SCALE * 4)
    big = Image.new("L", size, 0)
    draw = ImageDraw.Draw(big)
    k = SCALE * 4
    stroke = int(3.2 * k)
    # Arc : du haut gauche (après la pointe) vers le bas, en tournant par la droite
    box = (int(6 * k), int(5 * k), int(28 * k), int(25 * k))
    draw.arc(box, start=-100, end=110, fill=255, width=stroke)
    # Pointe vers la gauche, au départ de l'arc (en haut)
    cx, cy = 13 * k, 6.5 * k
    draw.polygon([(cx - 9 * k, cy), (cx + 1 * k, cy - 6.5 * k), (cx + 1 * k, cy + 6.5 * k)], fill=255)
    mask = big.resize((31 * SCALE, 27 * SCALE), Image.LANCZOS)
    image = Image.new("RGBA", mask.size, (255, 255, 255, 0))
    image.putalpha(mask)
    save("T_Battle_Back", image)


def main():
    # Boutons du menu (Rectangles 24, 14, 12, 13)
    button("T_Battle_Button_Fight", [(0.0, (135, 4, 6, 0.78)), (0.4952, (239, 8, 8, 0.46)), (1.0, (255, 128, 128, 0.47))])
    button("T_Battle_Button_Bag", [(0.0, hex_color("FFB01F")), (0.4952, hex_color("F3D630")), (1.0, hex_color("FFF83B"))])
    button("T_Battle_Button_Pokemon", [(0.0, hex_color("33CD83")), (0.4952, hex_color("52D86D")), (1.0, hex_color("9EEB5F"))])
    button("T_Battle_Button_Run", [(0.0, hex_color("1143E8")), (0.4952, hex_color("3154E2")), (1.0, hex_color("2F9AFF"))])
    # Barres de PV (Rectangle 4 vert, orange, rouge de Life_Bar) et leur fond (Rectangle 15)
    bar("T_Battle_HPFill_Green", [(0.0, hex_color("4BC088")), (0.4952, hex_color("66D57C")), (1.0, hex_color("ABEF73"))])
    bar("T_Battle_HPFill_Orange", [(0.0, hex_color("682407")), (0.4952, hex_color("D86426")), (1.0, hex_color("F0AE20"))])
    bar("T_Battle_HPFill_Red", [(0.0, hex_color("680707")), (0.4952, hex_color("D82626")), (1.0, hex_color("F02035"))])
    bar_track("T_Battle_HPTrack")
    back_arrow()


if __name__ == "__main__":
    main()
