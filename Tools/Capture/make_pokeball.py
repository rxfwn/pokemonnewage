"""Modèle de Poké Ball pour le lancer en 3D (SCRUM-40).

Lancer avec le Python de Blender (ou le module bpy : pip install bpy) :
    python Tools/Capture/make_pokeball.py
Sortie : Tools/Capture/Generated/SM_PokeBall.fbx et un aperçu PNG.

Un seul matériau : les couleurs viennent des couleurs de sommets, lues par M_PokeBall
(import_pokeball_unreal.py) :
    rouge (R) = moitié haute, teinte réglable par Ball (paramètre CouleurHaut)
    vert  (G) = blanc (moitié basse, bouton)
    noir      = bande du milieu et anneau du bouton
Le bouton regarde +X (l'avant dans Unreal). Diamètre 10 cm (APokeBallThrow le met à l'échelle).
"""

import math
import os

import bpy
import bmesh  # après bpy (module pip)
import mathutils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Generated")
RADIUS = 0.05
BAND = 0.003           # demi-épaisseur de la bande noire
GROOVE = 0.97          # la bande est un peu creusée
RING = 0.017           # rayon de l'anneau noir autour du bouton
BUTTON = 0.0115        # rayon du bouton blanc
BUTTON_BUMP = 0.0025   # le bouton dépasse un peu

TOP = (1.0, 0.0, 0.0, 1.0)
WHITE = (0.0, 1.0, 0.0, 1.0)
BLACK = (0.0, 0.0, 0.0, 1.0)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def add_disc_cylinder(bm, layer, radius, x_from, x_to, color):
    """Cylindre d'axe X (bouton, anneau), toutes ses faces de la même couleur."""
    result = bmesh.ops.create_cone(bm, cap_ends=True, segments=64, radius1=radius, radius2=radius,
                                   depth=x_to - x_from)
    verts = result["verts"]
    # create_cone est d'axe Z : on le couche sur X et on le pousse devant la sphère
    bmesh.ops.rotate(bm, verts=verts, cent=(0, 0, 0), matrix=mathutils.Matrix.Rotation(math.pi / 2, 3, "Y"))
    bmesh.ops.translate(bm, verts=verts, vec=((x_from + x_to) / 2, 0, 0))
    faces = {face for vertex in verts for face in vertex.link_faces}
    for face in faces:
        face.smooth = False
        for loop in face.loops:
            loop[layer] = color


def build_ball():
    mesh = bpy.data.meshes.new("SM_PokeBall")
    ball = bpy.data.objects.new("SM_PokeBall", mesh)
    bpy.context.collection.objects.link(ball)

    bm = bmesh.new()
    layer = bm.loops.layers.color.new("Col")
    bmesh.ops.create_uvsphere(bm, u_segments=96, v_segments=64, radius=RADIUS)
    for vertex in bm.verts:
        if abs(vertex.co.z) < BAND:
            vertex.co *= GROOVE
    for face in bm.faces:
        face.smooth = True
        cz = face.calc_center_median().z
        color = BLACK if abs(cz) < BAND else (TOP if cz > 0 else WHITE)
        for loop in face.loops:
            loop[layer] = color

    # Bouton : anneau noir puis bouton blanc qui dépasse un peu, face à +X
    add_disc_cylinder(bm, layer, RING, RADIUS * 0.85, RADIUS + 0.0015, BLACK)
    add_disc_cylinder(bm, layer, BUTTON, RADIUS * 0.85, RADIUS + 0.0015 + BUTTON_BUMP, WHITE)

    bm.to_mesh(mesh)
    bm.free()

    material = bpy.data.materials.new("M_PokeBall")
    mesh.materials.append(material)
    return ball


def preview_material(ball):
    """Matériau d'aperçu : reproduit M_PokeBall (rouge Poké Ball) pour le rendu PNG."""
    material = ball.data.materials[0]
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    shader = nodes["Principled BSDF"]
    shader.inputs["Roughness"].default_value = 0.3
    attribute = nodes.new("ShaderNodeVertexColor")
    attribute.layer_name = "Col"
    split = nodes.new("ShaderNodeSeparateColor")
    white = nodes.new("ShaderNodeMix")
    white.data_type = "RGBA"
    white.inputs[6].default_value = (0.02, 0.02, 0.02, 1)
    white.inputs[7].default_value = (0.9, 0.9, 0.88, 1)
    top = nodes.new("ShaderNodeMix")
    top.data_type = "RGBA"
    top.inputs[7].default_value = (0.8, 0.05, 0.04, 1)
    links.new(attribute.outputs["Color"], split.inputs["Color"])
    links.new(split.outputs["Green"], white.inputs["Factor"])
    links.new(split.outputs["Red"], top.inputs["Factor"])
    links.new(white.outputs[2], top.inputs[6])
    links.new(top.outputs[2], shader.inputs["Base Color"])


def render_preview(path):
    scene = bpy.context.scene
    camera_data = bpy.data.cameras.new("Camera")
    camera_data.lens = 85
    camera = bpy.data.objects.new("Camera", camera_data)
    scene.collection.objects.link(camera)
    camera.location = (0.42, -0.22, 0.17)
    direction = -camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera

    for name, location, energy in (("Key", (0.5, -0.5, 0.8), 25), ("Fill", (-0.6, 0.3, 0.3), 8)):
        light_data = bpy.data.lights.new(name, "AREA")
        light_data.energy = energy
        light_data.size = 0.5
        light = bpy.data.objects.new(name, light_data)
        light.location = location
        light.rotation_euler = (-light.location).to_track_quat("-Z", "Y").to_euler()
        scene.collection.objects.link(light)

    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.82, 0.86, 0.9, 1)
    scene.world = world

    scene.view_settings.view_transform = "Standard"
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 64
    scene.cycles.device = "CPU"
    scene.render.resolution_x = 640
    scene.render.resolution_y = 480
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    reset_scene()
    ball = build_ball()

    bpy.ops.object.select_all(action="DESELECT")
    ball.select_set(True)
    bpy.context.view_layer.objects.active = ball
    fbx = os.path.join(OUT, "SM_PokeBall.fbx")
    bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, apply_unit_scale=True,
                             mesh_smooth_type="FACE", colors_type="LINEAR", add_leaf_bones=False)
    print(f"Modèle : {fbx}")

    preview_material(ball)
    render_preview(os.path.join(OUT, "pokeball_apercu.png"))
    print(f"Aperçu : {os.path.join(OUT, 'pokeball_apercu.png')}")


main()
