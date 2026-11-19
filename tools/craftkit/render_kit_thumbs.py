# -*- coding: utf-8 -*-
"""
背景エディタのパレットに出す「モデルの小さな絵」を、クラフトのキットから Blender で描き出す（背景は透明）。

  "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe" -b --factory-startup -P tools/craftkit/render_kit_thumbs.py

  入力：project/resources/craft/models/*.gltf
  出力：project/resources/craft/thumbs/<モデル名>.png（128x128）
  キットにモデルを足したら流し直す（ゲームは起動時に読む）
"""
import math
import os

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
MODELS = os.path.join(PROJECT, "resources", "craft", "models")
OUT = os.path.join(PROJECT, "resources", "craft", "thumbs")
SIZE = 128
os.makedirs(OUT, exist_ok=True)


def setup_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.render.film_transparent = True
    scene.render.resolution_x = SIZE
    scene.render.resolution_y = SIZE
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (1.0, 1.0, 1.0, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.9
    scene.world = world
    sun = bpy.data.lights.new("Sun", type="SUN")
    sun.energy = 2.0
    sun_obj = bpy.data.objects.new("Sun", sun)
    sun_obj.rotation_euler = (math.radians(50), math.radians(-25), math.radians(-30))
    scene.collection.objects.link(sun_obj)
    return scene


def bounds(objects):
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    bpy.context.view_layer.update()
    for o in objects:
        if o.type != "MESH":
            continue
        for corner in o.bound_box:
            p = o.matrix_world @ Vector(corner)
            lo = Vector((min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)))
            hi = Vector((max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z)))
    return lo, hi


def render(name):
    scene = setup_scene()
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(MODELS, name + ".gltf"))
    objects = [o for o in bpy.data.objects if o not in before]
    lo, hi = bounds(objects)
    center = (lo + hi) * 0.5
    extent = max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z)
    # 正面（ゲームのカメラ側）から、少し上・少し横から見下ろす（立体が分かるように）
    cam = bpy.data.cameras.new("Camera")
    cam.type = "ORTHO"
    cam.ortho_scale = extent * 1.15
    cam_obj = bpy.data.objects.new("Camera", cam)
    scene.collection.objects.link(cam_obj)
    yaw, pitch = math.radians(25.0), math.radians(20.0)
    distance = extent * 4.0
    cam_obj.location = center + Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), math.sin(pitch))) * distance
    cam_obj.rotation_euler = (center - cam_obj.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam_obj
    scene.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True)
    print("thumb", name)


for file in sorted(os.listdir(MODELS)):
    if file.lower().endswith(".gltf"):
        render(os.path.splitext(file)[0])
