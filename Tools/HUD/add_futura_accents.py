"""Ajoute les lettres accentuées françaises aux polices Futura du projet (FF_Futura_Bold.ttf, FF_Futura_Medium.ttf).

Les fichiers fournis (Futura Cyrillic Bold et Medium) n'ont ni É, ni é, ni ç, ni ©, ni « » : Unreal affichait à leur place
un carré « LATIN 1 ». Ce script les fabrique à partir des glyphes de la police elle-même (lettre de base + accent
grave retourné, accent circonflexe réduit, points, virgule...) et écrit les fichiers FF_*.ttf.

    python Tools/HUD/add_futura_accents.py            (pip install fonttools)

Relançable : il repart toujours des polices d'origine gardées dans fonts/original/ (hors import).
"""

import os

from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS_DIR = os.path.join(HERE, "Assets", "fonts")

# (police d'origine sans accents, police écrite pour Unreal)
FONTS = [
    ("Futura_Bold_sans_accents.ttf", "FF_Futura_Bold.ttf"),
    ("Futura_Medium_sans_accents.ttf", "FF_Futura_Medium.ttf"),
]


def main():
    for original, target in FONTS:
        source = os.path.join(FONTS_DIR, "original", original)
        if os.path.exists(source):
            add_accents(source, os.path.join(FONTS_DIR, target))


def add_accents(original, target):
    font = TTFont(original)
    X_HEIGHT = font["OS/2"].sxHeight
    CAP_HEIGHT = font["OS/2"].sCapHeight
    glyphs = font.getGlyphSet()
    cmap = font.getBestCmap()
    order = list(font.getGlyphOrder())
    new = {}

    def bounds(name):
        pen = BoundsPen(glyphs)
        glyphs[name].draw(pen)
        return pen.bounds

    def width(name):
        return font["hmtx"][name][0]

    # ---- Accents : (glyphe, transformation) à poser au-dessus du centre de la lettre, bas de l'accent à y
    gx0, gy0, gx1, gy1 = bounds("grave")
    grave_center = (gx0 + gx1) / 2

    def accent_parts(kind, center, y_bottom, scale=1.0):
        """Liste de (glyphe, (xx, xy, yx, yy, dx, dy)) pour dessiner l'accent centré sur center, bas à y_bottom."""
        if kind in ("grave", "acute"):
            sx = scale if kind == "grave" else -scale
            # centre de l'accent sur center ; miroir horizontal pour l'aigu
            dx = center - sx * grave_center
            dy = y_bottom - scale * gy0
            return [("grave", (sx, 0, 0, scale, dx, dy))]
        if kind == "circumflex":
            cx0, cy0, cx1, cy1 = bounds("asciicircum")
            sx, sy = 0.56 * scale, 0.5 * scale
            dx = center - sx * (cx0 + cx1) / 2
            dy = y_bottom - sy * cy0
            return [("asciicircum", (sx, 0, 0, sy, dx, dy))]
        if kind == "dieresis":
            px0, py0, px1, py1 = bounds("period")
            s = 0.72 * scale
            dot = (px1 - px0) * s
            gap = 70 * scale
            parts = []
            for side in (-1, 1):
                left = center + side * (gap / 2 + dot / 2) - dot / 2
                parts.append(("period", (s, 0, 0, s, left - s * px0, y_bottom - s * py0)))
            return parts
        raise ValueError(kind)

    def draw_parts(pen, parts):
        for glyph_name, transform in parts:
            glyphs[glyph_name].draw(TransformPen(pen, transform))

    def compose(name, base, kind=None, below=None, base_draw=None):
        pen = TTGlyphPen(glyphs)
        if base_draw:
            base_draw(pen)
        else:
            glyphs[base].draw(pen)
        bx0, by0, bx1, by1 = bounds(base)
        center = (bx0 + bx1) / 2
        upper = base[0].isupper()
        if kind:
            y = (CAP_HEIGHT + 50) if upper else (X_HEIGHT + 75)
            draw_parts(pen, accent_parts(kind, center, y, 0.85 if upper else 1.0))
        if below == "cedilla":
            cx0, cy0, cx1, cy1 = bounds("comma")
            s = 0.75
            draw_parts(pen, [("comma", (s, 0, 0, s, center - s * (cx0 + cx1) / 2 + 10, 20 - s * cy1))])
        new[name] = (pen.glyph(), width(base))

    def dotless_i(pen):
        # le i sans son point : la barre seule
        x0, _, x1, _ = bounds("i")
        pen.moveTo((x0, 0))
        pen.lineTo((x0, X_HEIGHT))
        pen.lineTo((x1, X_HEIGHT))
        pen.lineTo((x1, 0))
        pen.closePath()

    letters = {
        0xE0: ("agrave", "a", "grave"), 0xE2: ("acircumflex", "a", "circumflex"), 0xE4: ("adieresis", "a", "dieresis"),
        0xE8: ("egrave", "e", "grave"), 0xE9: ("eacute", "e", "acute"), 0xEA: ("ecircumflex", "e", "circumflex"),
        0xEB: ("edieresis", "e", "dieresis"), 0xF4: ("ocircumflex", "o", "circumflex"), 0xF6: ("odieresis", "o", "dieresis"),
        0xF9: ("ugrave", "u", "grave"), 0xFB: ("ucircumflex", "u", "circumflex"), 0xFC: ("udieresis", "u", "dieresis"),
        0xFF: ("ydieresis", "y", "dieresis"), 0xF3: ("oacute", "o", "acute"), 0xE1: ("aacute", "a", "acute"),
        0xC0: ("Agrave", "A", "grave"), 0xC2: ("Acircumflex", "A", "circumflex"), 0xC4: ("Adieresis", "A", "dieresis"),
        0xC8: ("Egrave", "E", "grave"), 0xC9: ("Eacute", "E", "acute"), 0xCA: ("Ecircumflex", "E", "circumflex"),
        0xCB: ("Edieresis", "E", "dieresis"), 0xD4: ("Ocircumflex", "O", "circumflex"), 0xD6: ("Odieresis", "O", "dieresis"),
        0xD9: ("Ugrave", "U", "grave"), 0xDB: ("Ucircumflex", "U", "circumflex"), 0xDC: ("Udieresis", "U", "dieresis"),
        0xC1: ("Aacute", "A", "acute"), 0xCE: ("Icircumflex", "I", "circumflex"), 0xCF: ("Idieresis", "I", "dieresis"),
    }
    mapping = {}
    for code, (name, base, kind) in letters.items():
        compose(name, base, kind)
        mapping[code] = name
    compose("ccedilla", "c", below="cedilla")
    compose("Ccedilla", "C", below="cedilla")
    mapping[0xE7] = "ccedilla"
    mapping[0xC7] = "Ccedilla"
    for code, name, kind in ((0xEE, "icircumflex", "circumflex"), (0xEF, "idieresis", "dieresis")):
        compose(name, "i", kind, base_draw=dotless_i)
        # l'accent se pose sur la barre, pas sur le point : on recentre sur la barre
        mapping[code] = name

    # ---- Cercles (°, ©) dessinés à la main
    def circle(pen, cx, cy, r, clockwise):
        k = 0.5523 * r
        pts = [(cx, cy + r), (cx + r, cy), (cx, cy - r), (cx - r, cy)]
        if not clockwise:
            pts = [pts[0], pts[3], pts[2], pts[1]]
        pen.moveTo(pts[0])
        for i in range(4):
            a, b = pts[i], pts[(i + 1) % 4]
            # tangentes
            ta = (a[1] - cy, -(a[0] - cx)) if clockwise else (-(a[1] - cy), a[0] - cx)
            tb = (b[1] - cy, -(b[0] - cx)) if clockwise else (-(b[1] - cy), b[0] - cx)
            c1 = (a[0] + ta[0] / r * k, a[1] + ta[1] / r * k)
            c2 = (b[0] - tb[0] / r * k, b[1] - tb[1] / r * k)
            pen.curveTo(c1, c2, b)
        pen.closePath()

    def ring_glyph(name, w, cx, cy, r, thickness, inner=None):
        tt = TTGlyphPen(glyphs)
        pen = Cu2QuPen(tt, 1.0, reverse_direction=False)
        circle(pen, cx, cy, r, True)
        circle(pen, cx, cy, r - thickness, False)
        if inner:
            inner(pen)
        new[name] = (tt.glyph(), w)

    ring_glyph("degree", 330, 165, 560, 110, 42)
    mapping[0xB0] = "degree"

    def copyright_c(pen):
        x0, y0, x1, y1 = bounds("C")
        s = 0.5
        glyphs["C"].draw(TransformPen(pen, (s, 0, 0, s, 380 - s * (x0 + x1) / 2, 333 - s * (y0 + y1) / 2)))

    ring_glyph("copyright", 760, 380, 333, 345, 50, copyright_c)
    mapping[0xA9] = "copyright"

    # ---- Guillemets français : deux < ou > réduits
    for code, name, base in ((0xAB, "guillemotleft", "less"), (0xBB, "guillemotright", "greater")):
        pen = TTGlyphPen(glyphs)
        x0, y0, x1, y1 = bounds(base)
        s = 0.5
        for offset in (40, 230):
            glyphs[base].draw(TransformPen(pen, (s, 0, 0, s, offset - s * x0, 120 - s * y0)))
        new[name] = (pen.glyph(), 500)
        mapping[code] = name

    # ---- Espaces insécables (« Pokémon : », « 17 °C ») = espace normale
    space = cmap[0x20]
    for code in (0xA0, 0x202F):
        mapping[code] = space
    mapping[0xB7] = cmap[ord(".")] if 0xB7 not in cmap else cmap[0xB7]

    # ---- Écriture
    glyf = font["glyf"]
    hmtx = font["hmtx"]
    for name, (glyph, advance) in new.items():
        if name in order:
            continue
        order.append(name)
    font.setGlyphOrder(order)
    for name, (glyph, advance) in new.items():
        glyph.recalcBounds(glyf)
        glyf[name] = glyph
        hmtx[name] = (advance, getattr(glyph, "xMin", 0))
    for table in font["cmap"].tables:
        if table.isUnicode():
            for code, name in mapping.items():
                if code <= 0xFFFF or table.format in (12, 13):
                    table.cmap[code] = name
    if "post" in font and font["post"].formatType == 2.0:
        font["post"].extraNames = []
        font["post"].mapping = {}
    font.save(target)
    print("%s : %d glyphes ajoutés" % (os.path.basename(target), len(new)))


if __name__ == "__main__":
    main()
