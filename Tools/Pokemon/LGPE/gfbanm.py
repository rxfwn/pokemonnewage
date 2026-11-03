"""Lecture d'une animation Pokémon Let's Go (.gfbanm, FlatBuffers) en Python pur.

Schéma (projets communautaires d'import Blender pour Pokémon Switch) :
    Animation : 0 Info, 1 Skeleton (BoneAnimation), ...
    Info : 0 DoesLoop, 1 KeyFrames, 2 FrameRate (uint32)
    BoneAnimation : 0 Tracks (BoneTrack)
    BoneTrack : 0 Name, 1 ScaleType, 2 Scale, 3 RotateType, 4 Rotate, 5 TranslateType, 6 Translate
    Pistes (union) : 1 Fixed (Co en ligne), 2 Dynamic (Co[] : une valeur par image),
                     3 Framed16 (Frames uint16[], Co[]), 4 Framed8 (Frames uint8[], Co[])
    Vecteurs : Vec3 (3 float) ; rotations : sVec3 (3 uint16, quaternion compressé sur 48 bits)
"""

import math
import struct

from gfbmdl import Table

SCALE = 0x7FFF
PI_QUARTER = math.pi / 4.0
PI_HALF = math.pi / 2.0


def unpack_quat(x, y, z):
    """Quaternion compressé (3 composantes sur 15 bits, la plus grande reconstruite) -> (w, x, y, z)."""
    pack = (z << 32) | (y << 16) | x
    vals = [((pack >> s) & 0x7FFF) * (PI_HALF / SCALE) - PI_QUARTER for s in (3, 18, 33)]
    biggest = math.sqrt(max(1.0 - sum(v * v for v in vals), 0.0))
    vals.insert(pack & 0b11, biggest)
    q = (vals[3], vals[0], vals[1], vals[2])
    return tuple(-c for c in q) if pack & 0b100 else q


def _union(t, type_slot, value_slot):
    kind = t.scalar(type_slot, "B")
    p = t.field(value_slot)
    if not kind or not p:
        return 0, None
    return kind, Table(t.buf, p + struct.unpack_from("<I", t.buf, p)[0])


def _vector(t, slot):
    p = t.field(slot)
    if not p:
        return 0, 0
    q = p + struct.unpack_from("<I", t.buf, p)[0]
    return q + 4, struct.unpack_from("<I", t.buf, q)[0]


def _track(t, type_slot, value_slot, rotation, frames):
    """Liste de `frames` valeurs (None = pas de clé à cette image)."""
    kind, tr = _union(t, type_slot, value_slot)
    out = [None] * frames
    if tr is None:
        return out
    fmt, size = ("<3H", 6) if rotation else ("<3f", 12)

    def value(pos):
        v = struct.unpack_from(fmt, tr.buf, pos)
        return unpack_quat(*v) if rotation else v

    if kind == 1:
        p = tr.field(0)
        if p:
            out = [value(p)] * frames
    elif kind == 2:
        start, n = _vector(tr, 0)
        for i in range(min(n, frames)):
            out[i] = value(start + i * size)
    elif kind in (3, 4):
        fstart, fn = _vector(tr, 0)
        cstart, cn = _vector(tr, 1)
        ffmt, fsize = ("<H", 2) if kind == 3 else ("<B", 1)
        for i in range(min(fn, cn)):
            f = struct.unpack_from(ffmt, tr.buf, fstart + i * fsize)[0]
            if f < frames:
                out[f] = value(cstart + i * size)
    return out


def _lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def _slerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0:
        b, dot = tuple(-c for c in b), -dot
    if dot > 0.9995:
        q = _lerp(a, b, t)
    else:
        th = math.acos(dot)
        s = math.sin(th)
        wa, wb = math.sin((1 - t) * th) / s, math.sin(t * th) / s
        q = tuple(wa * x + wb * y for x, y in zip(a, b))
    n = math.sqrt(sum(c * c for c in q)) or 1.0
    return tuple(c / n for c in q)


def fill(keys, rotation):
    """Complète les images sans clé (interpolation entre clés, maintien aux bords)."""
    idx = [i for i, k in enumerate(keys) if k is not None]
    if not idx:
        return keys
    out = list(keys)
    for i in range(len(out)):
        if out[i] is not None:
            continue
        prev = max((j for j in idx if j < i), default=None)
        nxt = min((j for j in idx if j > i), default=None)
        if prev is None:
            out[i] = keys[nxt]
        elif nxt is None:
            out[i] = keys[prev]
        else:
            t = (i - prev) / (nxt - prev)
            out[i] = (_slerp if rotation else _lerp)(keys[prev], keys[nxt], t)
    return out


def read_animation(path):
    """{"frames", "fps", "loop", "tracks": {os: {"t": [...], "r": [(w,x,y,z)...], "s": [...]}}} ;
    une liste vaut None quand la piste n'existe pas (l'os garde sa valeur de repos)."""
    with open(path, "rb") as fh:
        buf = fh.read()
    root = Table(buf, struct.unpack_from("<I", buf, 0)[0])
    info_p = root.field(0)
    info = Table(buf, info_p + struct.unpack_from("<I", buf, info_p)[0])
    frames = max(info.scalar(1, "I"), 1)
    anim = {"frames": frames, "fps": info.scalar(2, "I") or 30, "loop": bool(info.scalar(0, "I")), "tracks": {}}
    skel = root.field(1)
    if not skel:
        return anim
    skel = Table(buf, skel + struct.unpack_from("<I", buf, skel)[0])
    for t in skel.tables(0):
        name = t.string(0)
        if not name:
            continue
        tracks = {}
        for key, ts, vs, rot in (("s", 1, 2, False), ("r", 3, 4, True), ("t", 5, 6, False)):
            keys = _track(t, ts, vs, rot, frames)
            tracks[key] = fill(keys, rot) if any(k is not None for k in keys) else None
        anim["tracks"][name] = tracks
    return anim


if __name__ == "__main__":
    import sys
    a = read_animation(sys.argv[1])
    print(a["frames"], "images à", a["fps"], "i/s, boucle", a["loop"], len(a["tracks"]), "pistes")
    for name, tr in list(a["tracks"].items())[:6]:
        print(name, {k: (v[0] if v else None) for k, v in tr.items()})
