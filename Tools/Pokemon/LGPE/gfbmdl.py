"""Lecture d'un modèle Pokémon Let's Go (.gfbmdl, FlatBuffers) en Python pur, sans dépendance.

Champs repris du schéma GFBMDL publié par la communauté (projets d'import Blender pour Pokémon Switch) :
    Model : 0 Version, 1 Bounding, 2 TextureNames, 3 ShaderNames, 4 ?, 5 MaterialNames, 6 Materials,
            7 Groups, 8 Meshes, 9 Bones, 10 CollisionGroups
    Material : 0 Name, 1 ShaderGroup, 11 TextureMaps, 12 Switches, 13 Values (float), 14 Colors
    TextureMap : 0 Sampler, 1 Index, 2 Params (table : 1 WrapModeX, 2 WrapModeY ; 0 répète, 1 bloque, 2 miroir)
    Group : 0 BoneIndex, 1 MeshIndex
    Mesh : 0 Polygons, 1 Attributes, 2 Data
    MeshPolygon : 0 MaterialIndex, 1 Faces (uint16)
    MeshAttribute : 0 VertexType, 1 BufferFormat, 2 ElementCount
    Bone : 0 Name, 1 BoneType, 2 Parent, 4 Visible, 5 Scale, 6 Rotation, 7 Translation (Vector3 en ligne),
           10 RigidCheck (présent = os rigide, absent de la table des poids)
"""

import struct

VERTEX_TYPES = {0: "position", 1: "normal", 2: "tangent", 3: "uv1", 4: "uv2", 5: "uv3", 6: "uv4",
                7: "color1", 8: "color2", 11: "bone_ids", 12: "weights", 13: "bitangent"}
# BufferFormat -> (code struct, taille, normalisé sur 255)
BUFFER_FORMATS = {0: ("f", 4, False), 1: ("e", 2, False), 3: ("B", 1, False), 5: ("H", 2, False), 8: ("B", 1, True)}


class Table:
    def __init__(self, buf, pos):
        self.buf, self.pos = buf, pos
        self.vtable = pos - struct.unpack_from("<i", buf, pos)[0]
        self.vsize = struct.unpack_from("<H", buf, self.vtable)[0]

    def field(self, slot):
        at = 4 + 2 * slot
        if at >= self.vsize:
            return 0
        off = struct.unpack_from("<H", self.buf, self.vtable + at)[0]
        return self.pos + off if off else 0

    def scalar(self, slot, fmt, default=0):
        p = self.field(slot)
        return struct.unpack_from("<" + fmt, self.buf, p)[0] if p else default

    def _vector(self, slot):
        p = self.field(slot)
        if not p:
            return 0, 0
        q = p + struct.unpack_from("<I", self.buf, p)[0]
        return q + 4, struct.unpack_from("<I", self.buf, q)[0]

    def string(self, slot):
        p = self.field(slot)
        if not p:
            return ""
        q = p + struct.unpack_from("<I", self.buf, p)[0]
        n = struct.unpack_from("<I", self.buf, q)[0]
        return self.buf[q + 4:q + 4 + n].decode("utf-8")

    def tables(self, slot):
        start, n = self._vector(slot)
        out = []
        for j in range(n):
            e = start + 4 * j
            out.append(Table(self.buf, e + struct.unpack_from("<I", self.buf, e)[0]))
        return out

    def strings(self, slot):
        start, n = self._vector(slot)
        out = []
        for j in range(n):
            e = start + 4 * j
            q = e + struct.unpack_from("<I", self.buf, e)[0]
            ln = struct.unpack_from("<I", self.buf, q)[0]
            out.append(self.buf[q + 4:q + 4 + ln].decode("utf-8"))
        return out

    def array(self, slot, fmt):
        start, n = self._vector(slot)
        size = struct.calcsize("<" + fmt)
        return list(struct.unpack_from(f"<{n}{fmt}", self.buf, start)) if n else [], start, n * size

    def vec3(self, slot):
        p = self.field(slot)
        return struct.unpack_from("<3f", self.buf, p) if p else None


WRAP_MODES = {0: "repeat", 1: "clamp", 2: "mirror"}


def map_entry(t):
    """(sampler, index de texture, répétition U, répétition V). Params : 0 ?, 1 WrapModeX, 2 WrapModeY."""
    wrap = ("repeat", "repeat")
    p = t.field(2)
    if p:
        params = Table(t.buf, p + struct.unpack_from("<I", t.buf, p)[0])
        wrap = (WRAP_MODES.get(params.scalar(1, "I"), "repeat"), WRAP_MODES.get(params.scalar(2, "I"), "repeat"))
    return (t.string(0), t.scalar(1, "i")) + wrap


def read_model(path):
    with open(path, "rb") as fh:
        buf = fh.read()
    root = Table(buf, struct.unpack_from("<I", buf, 0)[0])
    model = {"textures": root.strings(2), "materials": [], "meshes": [], "bones": [], "groups": []}

    for m in root.tables(6):
        model["materials"].append({
            "name": m.string(0),
            "shader": m.string(1),
            "maps": [map_entry(t) for t in m.tables(11)],
            "values": {v.string(0): v.scalar(1, "f") for v in m.tables(13)},
            "switches": {v.string(0): v.scalar(1, "B") for v in m.tables(12)},
        })

    for b in root.tables(9):
        model["bones"].append({
            "name": b.string(0),
            "type": b.scalar(1, "I"),
            "parent": b.scalar(2, "i", -1),
            "scale": b.vec3(5) or (1, 1, 1),
            "rotation": b.vec3(6) or (0, 0, 0),
            "translation": b.vec3(7) or (0, 0, 0),
            "rigid": bool(b.field(10)),
        })

    for g in root.tables(7):
        model["groups"].append({"bone": g.scalar(0, "I"), "mesh": g.scalar(1, "I")})

    for mesh in root.tables(8):
        attrs = [(a.scalar(0, "I"), a.scalar(1, "I"), a.scalar(2, "I")) for a in mesh.tables(1)]
        _, data_start, data_len = mesh.array(2, "B")
        stride = sum(BUFFER_FORMATS[f][1] * c for _, f, c in attrs)
        count = data_len // stride if stride else 0
        streams = {}
        offset = 0
        for vtype, fmt, n in attrs:
            code, size, norm = BUFFER_FORMATS[fmt]
            name = VERTEX_TYPES.get(vtype, f"type{vtype}")
            values = []
            for v in range(count):
                d = struct.unpack_from(f"<{n}{code}", buf, data_start + v * stride + offset)
                values.append(tuple(x / 255.0 for x in d) if norm else d)
            streams[name] = values
            offset += size * n
        polys = []
        for p in mesh.tables(0):
            faces, _, _ = p.array(1, "H")
            polys.append({"material": p.scalar(0, "I"), "faces": faces})
        model["meshes"].append({"streams": streams, "polygons": polys, "count": count})
    return model


if __name__ == "__main__":
    import sys
    m = read_model(sys.argv[1])
    print("textures", m["textures"])
    for mat in m["materials"]:
        print("mat", mat["name"], mat["shader"], mat["maps"])
        print("   values", {k: round(v, 3) for k, v in mat["values"].items()})
    print("bones", len(m["bones"]), "rigides", sum(b["rigid"] for b in m["bones"]))
    print("groups", m["groups"])
    for i, me in enumerate(m["meshes"]):
        print("mesh", i, me["count"], {k: (len(v), v[0] if v else None) for k, v in me["streams"].items()},
              [(p["material"], len(p["faces"])) for p in me["polygons"]])
