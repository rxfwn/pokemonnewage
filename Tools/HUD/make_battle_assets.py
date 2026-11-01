"""Images de l'écran de combat d'après la maquette Combat_Hud de L'amie (CSS Figma du 2026-11-01).

Lancer : python3 Tools/HUD/make_battle_assets.py (Pillow). Écrit Tools/HUD/Assets/T_Battle_*.png, importés dans
/Game/UI/HUD par Tools/HUD/import_hud_unreal.py (tout seul à l'ouverture de l'éditeur).
Dessinées au double de la taille de la maquette (nettes en 1440p / 4K) ; le C++ les affiche à la taille de la maquette.
  - T_Battle_Button_Fight / Bag / Pokemon / Run : boutons 265 x 85, coins 10, dégradés du CSS ;
  - T_Battle_HPFill_Green / Orange / Red : barres de PV 452 x 27, coins 13 (étirées en « boîte » : les bouts restent ronds) ;
  - T_Battle_HPTrack : fond de la barre (blanc 13 % -> 7,6 %) ;
  - T_Battle_Back : flèche retour blanche 31 x 27.
"""

import os

from PIL import Image, ImageChops, ImageDraw

SCALE = 2
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


def bar(name, stops):
    save(name, rounded(gradient(452 * SCALE, 27 * SCALE, stops), 13 * SCALE))


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
    bar("T_Battle_HPTrack", [(0.0, (255, 255, 255, 0.13)), (1.0, (255, 255, 255, 0.076))])
    back_arrow()


if __name__ == "__main__":
    main()
