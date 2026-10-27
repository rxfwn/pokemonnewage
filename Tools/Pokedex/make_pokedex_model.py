"""
Modèle 3D du Pokédex rouge du menu en jeu (SCRUM-64), construit par script dans Blender.

Lancer (sans ouvrir Blender) :
    blender --background --python Tools/Pokedex/make_pokedex_model.py -- <dossier_de_sortie>
ou dans Blender : onglet Scripting > Ouvrir ce fichier > Lancer le script (le .fbx va à côté du .blend).

Sortie :
    Pokedex.fbx           toutes les pièces, une mesh par pièce (à importer avec import_pokedex_unreal.py)
    Pokedex.blend         la scène, pour retoucher à la main
    pokedex_apercu.png    rendu de contrôle (si --render est passé après le dossier)

Repère : on écrit tout en centimètres dans le repère d'Unreal (X vers l'arrière de l'appareil,
Y vers la droite, Z vers le haut ; la face avant regarde vers -X, vers la caméra).
L'origine est l'axe de la charnière : le volet tourne autour de l'axe Z qui passe par (0, 0).
Blender et Unreal n'ont pas le même sens pour Y : on écrit y -> -y, l'import FBX le remet.

Chaque pièce porte un nom « Pokedex_<Pièce> » que le C++ (APokedexDevice) reconnaît, et des
matériaux « MAT_<Rôle> » que le C++ remplace par M_Pokedex avec les bonnes couleurs.
"""

import math
import os
import sys

import bpy
import bmesh

# ---------------------------------------------------------------- Dimensions (cm)

AXIS_X = -0.9          # l'axe de la charnière est 0,9 cm devant la face avant du boîtier

BODY_Y = (-12.0, -0.8)  # boîtier gauche
BODY_Z = (-8.0, 8.0)
BODY_X = (0.0, 2.6)     # face avant à x = 0, dos à x = 2,6
BODY_RADIUS = 1.2

LID_Y = (0.8, 12.0)     # volet droit, ouvert à plat
LID_X = (0.0, 1.4)      # même plan avant que le boîtier quand il est ouvert
LID_RADIUS = 1.0

SCREEN_CENTER = (-6.4, 1.2)   # (y, z) du centre de l'écran
SCREEN_SIZE = (7.6, 7.4)      # cadre noir
LENS_CENTER = (-9.4, 6.0)
MENU_COLUMNS = (3.75, 9.05)   # y des deux colonnes de boutons du volet
MENU_ROWS = (2.9, -0.7, -4.3) # z des trois rangées
MENU_SIZE = (4.7, 2.7)


def to_blender(x, y, z):
    """Centimètres, repère Unreal, origine = charnière -> mètres, repère Blender."""
    return ((x - AXIS_X) / 100.0, -y / 100.0, z / 100.0)


# ---------------------------------------------------------------- Matériaux (aperçu seulement)

MATERIAL_COLORS = {
    "MAT_RedPlastic": ((0.62, 0.05, 0.035), 0.45, 0.0, 0.0),
    "MAT_RedDark": ((0.40, 0.03, 0.025), 0.55, 0.0, 0.0),
    "MAT_Black": ((0.015, 0.015, 0.018), 0.40, 0.0, 0.0),
    "MAT_Metal": ((0.70, 0.70, 0.72), 0.22, 1.0, 0.0),
    "MAT_LensGlass": ((0.05, 0.18, 0.55), 0.04, 0.0, 0.6),
    "MAT_LedRed": ((0.9, 0.05, 0.05), 0.2, 0.0, 2.0),
    "MAT_LedYellow": ((0.95, 0.75, 0.05), 0.2, 0.0, 2.0),
    "MAT_LedGreen": ((0.1, 0.85, 0.2), 0.2, 0.0, 2.0),
    "MAT_PillRed": ((0.85, 0.10, 0.25), 0.35, 0.0, 0.0),
    "MAT_PillBlue": ((0.25, 0.55, 0.95), 0.35, 0.0, 0.0),
    "MAT_PanelGlass": ((0.50, 0.45, 0.90), 0.10, 0.0, 0.0),
    "MAT_Screen": ((0.02, 0.025, 0.03), 0.15, 0.0, 0.0),
    "MAT_ButtonPlate": ((0.035, 0.035, 0.04), 0.35, 0.0, 0.0),
    "MAT_Glow": ((0.3, 0.6, 1.0), 0.3, 0.0, 3.0),
}


def get_material(name):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    color, roughness, metallic, emission = MATERIAL_COLORS[name]
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission > 0.0:
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission
    mat.diffuse_color = (*color, 1.0)
    return mat


# ---------------------------------------------------------------- Contours 2D (y, z)

def rounded_rect(y0, y1, z0, z1, radius, segments=8, cut_corner=None):
    """Rectangle aux coins arrondis, sens trigonométrique. cut_corner : (coin, taille) pour un pan coupé."""
    radius = min(radius, (y1 - y0) / 2.0, (z1 - z0) / 2.0)
    corners = [
        ("bl", y0 + radius, z0 + radius, 180.0),
        ("br", y1 - radius, z0 + radius, 270.0),
        ("tr", y1 - radius, z1 - radius, 0.0),
        ("tl", y0 + radius, z1 - radius, 90.0),
    ]
    points = []
    for name, cy, cz, start in corners:
        if cut_corner and cut_corner[0] == name:
            size = cut_corner[1]
            if name == "bl":
                points += [(y0, z0 + size), (y0 + size, z0)]
            continue
        for i in range(segments + 1):
            angle = math.radians(start + 90.0 * i / segments)
            points.append((cy + radius * math.cos(angle), cz + radius * math.sin(angle)))
    return points


def s_curve(y_start, y_end, z_start, z_end, steps=12):
    """Courbe en S lisse de (y_start, z_start) à (y_end, z_end), sans le premier point."""
    points = []
    for i in range(1, steps + 1):
        t = i / steps
        s = t * t * (3.0 - 2.0 * t)
        points.append((y_start + (y_end - y_start) * t, z_start + (z_end - z_start) * s))
    return points


def top_band_outline():
    """Bandeau du haut du boîtier (objectif et voyants) : bord bas en S comme sur l'image de référence."""
    y0, y1 = BODY_Y[0] + 0.25, BODY_Y[1] - 0.25
    z_top = BODY_Z[1] - 0.25
    points = [(y0, 3.6)]
    points += [(y0 + 4.6, 3.6)]
    points += s_curve(y0 + 4.6, y0 + 7.0, 3.6, 5.2)
    points += [(y1, 5.2), (y1, z_top - 0.8)]
    radius = 0.8
    for i in range(1, 9):
        a = math.radians(90.0 * i / 8)
        points.append((y1 - radius + radius * math.cos(a), z_top - radius + radius * math.sin(a)))
    for i in range(0, 9):
        a = math.radians(90.0 + 90.0 * i / 8)
        points.append((y0 + radius + radius * math.cos(a), z_top - radius + radius * math.sin(a)))
    return points


def lid_outline(inset=0.0):
    """Volet : bord haut en marche d'escalier arrondie (plus haut côté charnière)."""
    y0, y1 = LID_Y[0] + inset, LID_Y[1] - inset
    z0, z_high, z_low = BODY_Z[0] + 0.4 + inset, BODY_Z[1] - 0.6 - inset, BODY_Z[1] - 2.0 - inset
    r = LID_RADIUS
    points = []
    for i in range(9):  # bas gauche
        a = math.radians(180.0 + 90.0 * i / 8)
        points.append((y0 + r + r * math.cos(a), z0 + r + r * math.sin(a)))
    for i in range(9):  # bas droite
        a = math.radians(270.0 + 90.0 * i / 8)
        points.append((y1 - r + r * math.cos(a), z0 + r + r * math.sin(a)))
    for i in range(9):  # haut droite (bas de la marche)
        a = math.radians(90.0 * i / 8)
        points.append((y1 - r + r * math.cos(a), z_low - r + r * math.sin(a)))
    points += s_curve(y1 - r, y0 + 4.2, z_low, z_high)[:-1]
    for i in range(9):  # haut gauche
        a = math.radians(90.0 + 90.0 * i / 8)
        points.append((y0 + r + r * math.cos(a), z_high - r + r * math.sin(a)))
    return points


def menu_button_outline(cy, cz, grow=0.0):
    """Plaque d'un bouton du volet, avec un onglet en haut à gauche comme un intercalaire."""
    w, h = MENU_SIZE[0] / 2.0 + grow, MENU_SIZE[1] / 2.0 + grow
    tab_w, tab_h = 1.7 + grow, 0.35
    return [
        (cy - w, cz - h),
        (cy + w, cz - h),
        (cy + w, cz + h - 0.5),
        (cy + w - 0.5, cz + h),
        (cy - w + tab_w + 0.35, cz + h),
        (cy - w + tab_w, cz + h + tab_h),
        (cy - w, cz + h + tab_h),
    ]


# ---------------------------------------------------------------- Construction des meshes

def new_object(name):
    mesh = bpy.data.meshes.new(name)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def add_prism(bm, outline, x0, x1, material_index=0):
    """Extrude un contour (y, z) entre x0 (avant) et x1 (arrière)."""
    front = [bm.verts.new(to_blender(x0, y, z)) for y, z in outline]
    back = [bm.verts.new(to_blender(x1, y, z)) for y, z in outline]
    faces = [bm.faces.new(front), bm.faces.new(list(reversed(back)))]
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        faces.append(bm.faces.new((front[j], front[i], back[i], back[j])))
    for face in faces:
        face.material_index = material_index
    return faces


def add_cylinder_x(bm, cy, cz, radius, x0, x1, segments=32, material_index=0):
    """Cylindre dont l'axe suit X (bouton rond, objectif, voyant)."""
    outline = [(cy + radius * math.cos(2 * math.pi * i / segments), cz + radius * math.sin(2 * math.pi * i / segments))
               for i in range(segments)]
    return add_prism(bm, outline, x0, x1, material_index)


def add_dome_x(bm, cy, cz, radius, x_base, height, rings=8, segments=32, material_index=0):
    """Calotte bombée vers l'avant (-X) : verre de l'objectif, voyants."""
    rows = []
    for r in range(rings + 1):
        t = r / rings
        ring_radius = radius * math.cos(t * math.pi / 2.0)
        x = x_base - height * math.sin(t * math.pi / 2.0)
        if r == rings:
            rows.append([bm.verts.new(to_blender(x, cy, cz))])
            break
        rows.append([bm.verts.new(to_blender(x, cy + ring_radius * math.cos(2 * math.pi * i / segments),
                                             cz + ring_radius * math.sin(2 * math.pi * i / segments)))
                     for i in range(segments)])
    faces = [bm.faces.new(list(reversed(rows[0])))]
    for r in range(rings):
        a, b = rows[r], rows[r + 1]
        for i in range(segments):
            j = (i + 1) % segments
            if len(b) == 1:
                faces.append(bm.faces.new((a[i], a[j], b[0])))
            else:
                faces.append(bm.faces.new((a[i], a[j], b[j], b[i])))
    for face in faces:
        face.material_index = material_index
    return faces


def add_cylinder_z(bm, cy, cx, radius, z0, z1, segments=24, material_index=0):
    """Cylindre vertical (charnière)."""
    bottom, top = [], []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        x, y = cx + radius * math.cos(a), cy + radius * math.sin(a)
        bottom.append(bm.verts.new(to_blender(x, y, z0)))
        top.append(bm.verts.new(to_blender(x, y, z1)))
    faces = [bm.faces.new(bottom), bm.faces.new(list(reversed(top)))]
    for i in range(segments):
        j = (i + 1) % segments
        faces.append(bm.faces.new((bottom[i], bottom[j], top[j], top[i])))
    for face in faces:
        face.material_index = material_index
    return faces


def finish(obj, bm, materials, bevel=0.0, bevel_segments=3):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    for name in materials:
        obj.data.materials.append(get_material(name))
    for poly in obj.data.polygons:
        poly.use_smooth = True
    if bevel > 0.0:
        mod = obj.modifiers.new("Biseau", "BEVEL")
        mod.width = bevel / 100.0
        mod.segments = bevel_segments
        mod.limit_method = "ANGLE"
        mod.angle_limit = math.radians(40.0)
        mod.harden_normals = True
    weighted = obj.modifiers.new("Normales", "WEIGHTED_NORMAL")
    weighted.keep_sharp = True
    return obj


def build():
    # Boîtier
    obj, bm = new_object("Pokedex_Body"), bmesh.new()
    add_prism(bm, rounded_rect(BODY_Y[0], BODY_Y[1], BODY_Z[0], BODY_Z[1], BODY_RADIUS), BODY_X[0], BODY_X[1])
    finish(obj, bm, ["MAT_RedPlastic"], bevel=0.35)

    # Bandeau du haut, en relief
    obj, bm = new_object("Pokedex_TopBand"), bmesh.new()
    add_prism(bm, top_band_outline(), -0.45, 0.05)
    finish(obj, bm, ["MAT_RedPlastic"], bevel=0.18)

    # Cadre de l'écran (l'image de l'écran est un Widget posé devant par le C++)
    obj, bm = new_object("Pokedex_Bezel"), bmesh.new()
    sy, sz = SCREEN_CENTER
    w, h = SCREEN_SIZE[0] / 2.0, SCREEN_SIZE[1] / 2.0
    add_prism(bm, rounded_rect(sy - w, sy + w, sz - h, sz + h, 0.5, cut_corner=("bl", 1.2)), -0.18, 0.05)
    finish(obj, bm, ["MAT_Screen"], bevel=0.08)

    # Objectif : bague métal + verre bombé
    obj, bm = new_object("Pokedex_LensRing"), bmesh.new()
    ly, lz = LENS_CENTER
    add_cylinder_x(bm, ly, lz, 1.45, -0.85, -0.3, segments=48)
    finish(obj, bm, ["MAT_Metal"], bevel=0.12)

    obj, bm = new_object("Pokedex_Lens"), bmesh.new()
    add_dome_x(bm, ly, lz, 1.12, -0.86, 0.45, segments=48)
    finish(obj, bm, ["MAT_LensGlass"])

    # Trois voyants
    for name, y in (("LedRed", -6.2), ("LedYellow", -5.2), ("LedGreen", -4.2)):
        obj, bm = new_object("Pokedex_" + name), bmesh.new()
        add_cylinder_x(bm, y, 6.9, 0.36, -0.55, -0.4, segments=24, material_index=0)
        add_dome_x(bm, y, 6.9, 0.3, -0.55, 0.16, rings=5, segments=24, material_index=0)
        finish(obj, bm, ["MAT_" + name])

    # Bouton rond noir (A), deux boutons allongés, croix, petite vitre violette
    obj, bm = new_object("Pokedex_ButtonRound"), bmesh.new()
    add_cylinder_x(bm, -10.0, -5.0, 0.85, -0.55, 0.0, segments=36)
    finish(obj, bm, ["MAT_Black"], bevel=0.15)

    for name, y in (("PillRed", -7.6), ("PillBlue", -5.0)):
        obj, bm = new_object("Pokedex_" + name), bmesh.new()
        add_prism(bm, rounded_rect(y - 1.0, y + 1.0, -3.95, -3.55, 0.2, segments=6), -0.35, 0.0)
        finish(obj, bm, ["MAT_" + name], bevel=0.06)

    obj, bm = new_object("Pokedex_DPad"), bmesh.new()
    cy, cz, arm, half = -3.0, -5.6, 1.35, 0.45
    cross = [(cy - half, cz - arm), (cy + half, cz - arm), (cy + half, cz - half), (cy + arm, cz - half),
             (cy + arm, cz + half), (cy + half, cz + half), (cy + half, cz + arm), (cy - half, cz + arm),
             (cy - half, cz + half), (cy - arm, cz + half), (cy - arm, cz - half), (cy - half, cz - half)]
    add_prism(bm, cross, -0.5, 0.0)
    finish(obj, bm, ["MAT_Black"], bevel=0.1)

    obj, bm = new_object("Pokedex_Panel"), bmesh.new()
    add_prism(bm, rounded_rect(-8.6, -5.4, -7.1, -5.5, 0.35, segments=6), -0.15, 0.0)
    finish(obj, bm, ["MAT_PanelGlass"], bevel=0.05)

    # Charnière : tube en trois anneaux sur l'axe
    obj, bm = new_object("Pokedex_Hinge"), bmesh.new()
    for z0, z1 in ((-7.8, -2.9), (-2.7, 2.7), (2.9, 7.8)):
        add_cylinder_z(bm, 0.0, AXIS_X, 0.95, z0, z1)
    finish(obj, bm, ["MAT_RedDark"], bevel=0.15)

    # Volet (ouvert à plat) et son panneau intérieur plus sombre
    obj, bm = new_object("Pokedex_Lid"), bmesh.new()
    add_prism(bm, lid_outline(), LID_X[0], LID_X[1])
    finish(obj, bm, ["MAT_RedPlastic"], bevel=0.3)

    obj, bm = new_object("Pokedex_LidPanel"), bmesh.new()
    add_prism(bm, lid_outline(inset=0.6), -0.12, 0.05)
    finish(obj, bm, ["MAT_RedDark"], bevel=0.08)

    # Six boutons du menu : plaque sombre + liseré lumineux (2 matériaux)
    index = 0
    for cz in MENU_ROWS:
        for cy in MENU_COLUMNS:
            obj, bm = new_object("Pokedex_MenuButton%d" % index), bmesh.new()
            add_prism(bm, menu_button_outline(cy, cz, grow=0.14), -0.3, -0.1, material_index=1)
            add_prism(bm, menu_button_outline(cy, cz), -0.4, -0.12, material_index=0)
            finish(obj, bm, ["MAT_ButtonPlate", "MAT_Glow"], bevel=0.05, bevel_segments=2)
            index += 1


# ---------------------------------------------------------------- Aperçu

def render_preview(path):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 48
    scene.cycles.device = "CPU"
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("Fond") if not scene.world else scene.world
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.25, 0.35, 0.3, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.35

    # Caméra devant la face avant (-X Unreal = -X Blender), un peu de trois quarts
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 50
    cam = bpy.data.objects.new("Camera", cam_data)
    bpy.context.collection.objects.link(cam)
    cam.location = to_blender(-62.0, 10.0, 8.0)
    target = to_blender(0.0, 0.0, 0.0)
    direction = [t - c for t, c in zip(target, cam.location)]
    import mathutils
    cam.rotation_euler = mathutils.Vector(direction).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam

    for loc, energy in ((to_blender(-40, -25, 30), 6.0), (to_blender(-30, 30, 5), 2.5)):
        light_data = bpy.data.lights.new("Lumiere", "AREA")
        light_data.energy = energy
        light_data.size = 0.3
        light = bpy.data.objects.new("Lumiere", light_data)
        light.location = loc
        d = [t - c for t, c in zip(target, loc)]
        light.rotation_euler = mathutils.Vector(d).to_track_quat("-Z", "Y").to_euler()
        bpy.context.collection.objects.link(light)

    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if argv:
        out_dir = os.path.abspath(argv[0])
    elif bpy.data.filepath:
        out_dir = os.path.dirname(bpy.data.filepath)
    else:
        out_dir = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(out_dir, exist_ok=True)

    # Scène vide
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for mesh in list(bpy.data.meshes):
        bpy.data.meshes.remove(mesh)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0

    build()

    pokedex_objects = [o for o in bpy.data.objects if o.name.startswith("Pokedex_")]
    bpy.ops.object.select_all(action="DESELECT")
    for obj in pokedex_objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = pokedex_objects[0]

    fbx_path = os.path.join(out_dir, "Pokedex.fbx")
    bpy.ops.export_scene.fbx(
        filepath=fbx_path,
        use_selection=True,
        object_types={"MESH"},
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        bake_space_transform=False,
        axis_forward="-Z",
        axis_up="Y",
        path_mode="AUTO",
    )
    print("FBX écrit :", fbx_path)

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "Pokedex.blend"))

    if "--render" in argv:
        render_preview(os.path.join(out_dir, "pokedex_apercu.png"))
        print("Aperçu :", os.path.join(out_dir, "pokedex_apercu.png"))


if __name__ == "__main__":
    main()
