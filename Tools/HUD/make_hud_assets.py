"""Fabrique les images du HUD en jeu (maquette de L'amie : Desktop - 1, fichiers du projet hud/).

    python Tools/HUD/make_hud_assets.py [chemin/vers/pokedex_icon.png]

Écrit dans Tools/HUD/Assets/ :
  - T_HUD_PokedexIcon.png : icône du Pokédex en haut à droite (réduite à 256 px depuis l'image de L'amie) ;
  - T_HUD_ViewCone.png : cône de vue de la boussole (blanc 46 % près du joueur, transparent au loin).
Les pictos météo T_HUD_Weather_Sun/Cloud/Rain/Wind.png sont ceux de L'amie (« Plan de travail 1 ... »), copiés tels quels.
Et dans Tools/Pokedex/Assets/icons/ les icônes provisoires des deux nouveaux boutons du Pokédex
(Appareil photo, Compteur de pas), à remplacer par celles de L'amie.
Les images sont ensuite importées par Tools/HUD/import_hud_unreal.py.
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Assets")
POKEDEX_ICONS = os.path.join(HERE, "..", "Pokedex", "Assets", "icons")
SS = 4  # suréchantillonnage pour des bords lisses


def save(image, folder, name):
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name)
    image.save(path)
    print("écrit :", os.path.relpath(path, os.path.join(HERE, "..", "..")))


def pokedex_icon(source):
    image = Image.open(source).convert("RGBA")
    box = image.getbbox()
    if box:
        image = image.crop(box)
    side = max(image.size)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(image, ((side - image.width) // 2, (side - image.height) // 2))
    save(square.resize((256, 256), Image.LANCZOS), OUT, "T_HUD_PokedexIcon.png")


def view_cone():
    # Maquette : triangle 252 x 119, pointe sur le joueur (en bas), dégradé blanc 46 % -> 0 à mi-hauteur
    w, h = 256, 128
    mask = Image.new("L", (w * SS, h * SS), 0)
    ImageDraw.Draw(mask).polygon([(0, 0), (w * SS, 0), (w * SS / 2, h * SS)], fill=255)
    mask = mask.resize((w, h), Image.LANCZOS)
    alpha = Image.new("L", (w, h), 0)
    pixels = alpha.load()
    for y in range(h):
        from_tip = 1.0 - y / (h - 1)  # 0 sur la pointe, 1 au bout du cône
        value = max(0.0, 1.0 - from_tip / 0.52) * 0.46
        for x in range(w):
            pixels[x, y] = int(255 * value)
    cone = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    cone.putalpha(Image.composite(alpha, Image.new("L", (w, h), 0), mask))
    save(cone, OUT, "T_HUD_ViewCone.png")


def shaded_icon(draw_function, name):
    """Icône provisoire du Pokédex : forme pleine avec un peu de relief, dans l'esprit de la planche de L'amie."""
    s = 256 * SS
    base = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw_function(ImageDraw.Draw(base), s)
    shadow = base.split()[3].filter(ImageFilter.GaussianBlur(s * 0.02))
    out = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    out.paste((0, 0, 0, 90), (int(s * 0.015), int(s * 0.025)), shadow)
    out = Image.alpha_composite(out, base)
    save(out.resize((256, 256), Image.LANCZOS), POKEDEX_ICONS, name)


def draw_camera(draw, s):
    body, dark, lens = (238, 120, 40, 255), (60, 60, 70, 255), (70, 140, 220, 255)
    draw.rounded_rectangle([s * 0.08, s * 0.28, s * 0.92, s * 0.82], radius=s * 0.08, fill=body, outline=dark, width=int(s * 0.02))
    draw.rounded_rectangle([s * 0.3, s * 0.18, s * 0.56, s * 0.3], radius=s * 0.03, fill=body, outline=dark, width=int(s * 0.02))
    draw.ellipse([s * 0.3, s * 0.36, s * 0.7, s * 0.76], fill=dark)
    draw.ellipse([s * 0.36, s * 0.42, s * 0.64, s * 0.7], fill=lens)
    draw.ellipse([s * 0.42, s * 0.47, s * 0.5, s * 0.55], fill=(220, 240, 255, 255))
    draw.ellipse([s * 0.76, s * 0.34, s * 0.86, s * 0.42], fill=(255, 230, 120, 255))


def draw_steps(draw, s):
    body, dark, screen = (230, 60, 60, 255), (60, 60, 70, 255), (190, 230, 160, 255)
    draw.rounded_rectangle([s * 0.18, s * 0.12, s * 0.82, s * 0.9], radius=s * 0.16, fill=body, outline=dark, width=int(s * 0.02))
    draw.rounded_rectangle([s * 0.28, s * 0.24, s * 0.72, s * 0.52], radius=s * 0.04, fill=screen, outline=dark, width=int(s * 0.015))
    # Deux empreintes de pas sous l'écran
    for x, y in ((0.4, 0.66), (0.6, 0.74)):
        draw.ellipse([s * (x - 0.06), s * (y - 0.07), s * (x + 0.06), s * (y + 0.07)], fill=(255, 255, 255, 255))
        draw.ellipse([s * (x - 0.035), s * (y - 0.13), s * (x + 0.035), s * (y - 0.08)], fill=(255, 255, 255, 255))


def main():
    source = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "Source", "pokedex_icon.png")
    if os.path.exists(source):
        pokedex_icon(source)
    else:
        print("pas d'image source pour l'icône Pokédex (%s) : T_HUD_PokedexIcon.png gardé tel quel" % source)
    view_cone()
    shaded_icon(draw_camera, "T_PokedexIcon_Camera.png")
    shaded_icon(draw_steps, "T_PokedexIcon_Steps.png")


if __name__ == "__main__":
    main()
