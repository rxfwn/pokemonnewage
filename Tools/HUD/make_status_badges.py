"""Badges de statut de l'écran de combat (BRU, PAR, GEL, PSN, SOM) d'après la maquette Figma de L'amie (2026-11-03).

Lancer : python3 Tools/HUD/make_status_badges.py (Pillow). Écrit Tools/HUD/Assets/T_Battle_Status_<Statut>.png,
importés dans /Game/UI/HUD par Tools/HUD/import_hud_unreal.py (tout seul à l'ouverture de l'éditeur).

CSS de la maquette (px Figma) : pastille 99 x 38, coins 10 ; icône blanche 34 x 34 en (6, 2) avec drop-shadow
(0 4px 4px noir 25 %, 3px 4px pour la brûlure) ; texte Futura MdCn BT 32 px majuscules centré dans la zone x 26 -> 99,
ombre 3px 4px 4px noir 25 %. Couleurs : BRU #D86426, PAR #DBA902, GEL #4990E0, PSN #4F1F93, SOM #6D6D6D.
Icônes de L'amie dans Tools/HUD/Source/Status (Plan de travail 45 = feu, copie 4 = éclair, copie = flocon,
copie 2 = poison, copie 3 = sommeil). K.O. n'est pas dans la maquette : pastille rouge sombre sans icône (provisoire).

Dessiné au double (nettes en 1440p / 4K) avec une marge de PAD px tout autour pour les ombres : le C++ affiche
l'image en 99 + 2 PAD x 38 + 2 PAD, décalée de PAD (BattleHUD::StatusBadgeSize / StatusBadgePad).
"""

import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

SCALE = 2
W, H, RADIUS = 99, 38, 10
PAD = 8
ICON_POS, ICON_SIZE = (6, 2), 34
TEXT_LEFT, TEXT_RIGHT, FONT_SIZE = 26, 99, 32
SHADOW_ALPHA = 0.25

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Assets")
ICONS = os.path.join(HERE, "Source", "Status")
FONT = os.path.join(HERE, "Assets", "fonts", "FF_Futura_MediumCondensed.ttf")

# Nom de fichier, texte, couleur, icône, décalage de l'ombre de l'icône (x, y)
BADGES = [
    ("Burn", "BRU", "#D86426", "Status_Burn.png", (3, 4)),
    ("Paralysis", "PAR", "#DBA902", "Status_Paralysis.png", (0, 4)),
    ("Freeze", "GEL", "#4990E0", "Status_Freeze.png", (0, 4)),
    ("Poison", "PSN", "#4F1F93", "Status_Poison.png", (0, 4)),
    ("Sleep", "SOM", "#6D6D6D", "Status_Sleep.png", (0, 4)),
    ("Fainted", "K.O.", "#8E1F1F", None, (0, 4)),
]


def px(value):
    return round(value * SCALE)


def hex_rgb(value):
    value = value.lstrip("#")
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def shadow_of(layer, offset, blur=4):
    """drop-shadow CSS : silhouette noire 25 % décalée et floutée (le flou CSS de 4px ~ sigma 2)"""
    alpha = layer.getchannel("A").point(lambda a: round(a * SHADOW_ALPHA))
    shadow = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    shadow.putalpha(alpha)
    moved = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    moved.paste(shadow, (px(offset[0]), px(offset[1])))
    return moved.filter(ImageFilter.GaussianBlur(px(blur) / 2))


def pill(color):
    size = (px(W + 2 * PAD), px(H + 2 * PAD))
    big = Image.new("L", (size[0] * 4, size[1] * 4), 0)
    ImageDraw.Draw(big).rounded_rectangle(
        (px(PAD) * 4, px(PAD) * 4, (px(PAD + W)) * 4 - 1, (px(PAD + H)) * 4 - 1), radius=px(RADIUS) * 4, fill=255)
    mask = big.resize(size, Image.LANCZOS)
    layer = Image.new("RGBA", size, hex_rgb(color) + (255,))
    layer.putalpha(mask)
    return layer


def icon_layer(file_name, size):
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    source = Image.open(os.path.join(ICONS, file_name)).convert("RGBA")
    icon = source.resize((px(ICON_SIZE), px(ICON_SIZE)), Image.LANCZOS)
    layer.alpha_composite(icon, (px(PAD + ICON_POS[0]), px(PAD + ICON_POS[1])))
    return layer


def draw_text(text, size, centered):
    font = ImageFont.truetype(FONT, px(FONT_SIZE))
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    left, right = (0, W) if centered else (TEXT_LEFT, TEXT_RIGHT)
    box = draw.textbbox((0, 0), text, font=font, anchor="ls")
    cap = font.getbbox("H", anchor="ls")
    width = box[2] - box[0]
    x = px(PAD + (left + right) / 2) - width / 2 - box[0]
    baseline = px(PAD + H / 2 - 0.5) - (cap[1] + cap[3]) / 2
    draw.text((x, baseline), text, font=font, fill=(255, 255, 255, 255), anchor="ls")
    return layer


def badge(name, text, color, icon_file, icon_shadow):
    base = pill(color)
    size = base.size
    out = Image.new("RGBA", size, (0, 0, 0, 0))
    out.alpha_composite(base)
    if icon_file:
        icon = icon_layer(icon_file, size)
        out.alpha_composite(shadow_of(icon, icon_shadow))
        out.alpha_composite(icon)
    label = draw_text(text, size, centered=icon_file is None)
    out.alpha_composite(shadow_of(label, (3, 4)))
    out.alpha_composite(label)
    path = os.path.join(OUT, "T_Battle_Status_%s.png" % name)
    out.save(path)
    print(path, out.size)


def main():
    for entry in BADGES:
        badge(*entry)


if __name__ == "__main__":
    main()
