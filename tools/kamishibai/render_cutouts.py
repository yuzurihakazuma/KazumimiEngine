# -*- coding: utf-8 -*-
"""
ステージ開始カードに貼る「切り絵」を、ゲームのモデルから Blender で描き出す（背景は透明）。
白いふちと影は make_kamishibai_assets.py が後から付ける。

  "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe" -b --factory-startup -P tools/kamishibai/render_cutouts.py

  出力（tools/kamishibai/cutouts/）
    dino.png          プレイヤー（右を向く）
    enemy_ground.png  地面の敵（左を向く）
    enemy_air.png     空の敵（左を向く）
    enemy_plant.png   植物の敵（左を向く）
    egg.png           卵
"""
import math
import os

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
OUT = os.path.join(HERE, "cutouts")
os.makedirs(OUT, exist_ok=True)

# 名前, ファイル, 向き（度。正で画面の右を向く）, 少し上から見る角度（度）
MODELS = [
    ("dino", os.path.join(PROJECT, "resources", "player", "player.gltf"), 38.0, 8.0),
    ("enemy_ground", os.path.join(PROJECT, "resources", "enemy", "enemy_ground.glb"), -35.0, 8.0),
    ("enemy_air", os.path.join(PROJECT, "resources", "enemy", "enemy_air.glb"), -35.0, 8.0),
    ("enemy_plant", os.path.join(PROJECT, "resources", "enemy", "enemy_plant.glb"), -35.0, 8.0),
    ("egg", os.path.join(PROJECT, "resources", "egg", "egg.obj"), 20.0, 10.0),
]
SIZE = 768


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def setup_scene():
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
    scene.view_settings.view_transform = "Standard"  # 色をそのまま（Filmic で白っぽくしない）
    # 紙人形らしく、やわらかい全体光＋斜め上からの光
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (1.0, 1.0, 1.0, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.85
    scene.world = world
    sun = bpy.data.lights.new("Sun", type="SUN")
    sun.energy = 2.2
    sun_obj = bpy.data.objects.new("Sun", sun)
    sun_obj.rotation_euler = (math.radians(50), math.radians(-25), math.radians(-30))
    scene.collection.objects.link(sun_obj)
    return scene


def import_model(path):
    before = set(bpy.data.objects)
    if path.lower().endswith(".obj"):
        bpy.ops.wm.obj_import(filepath=path)
    else:
        bpy.ops.import_scene.gltf(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def world_bounds(objects):
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    bpy.context.view_layer.update()
    for o in objects:
        if o.type != "MESH":
            continue
        depsgraph = bpy.context.evaluated_depsgraph_get()
        evaluated = o.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        for v in mesh.vertices:
            p = evaluated.matrix_world @ v.co
            lo = Vector((min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)))
            hi = Vector((max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z)))
        evaluated.to_mesh_clear()
    return lo, hi


def render(name, path, yaw_deg, pitch_deg):
    clear()
    scene = setup_scene()
    objects = import_model(path)
    # 動きのあるモデルは、最初のクリップの最初のポーズ（待機）にする
    for o in objects:
        if o.animation_data and o.animation_data.action is None and bpy.data.actions:
            o.animation_data.action = bpy.data.actions[0]
    scene.frame_set(1)
    # まとめて回す（親が無い物だけ回せば、子もついてくる）
    pivot = bpy.data.objects.new("Pivot", None)
    scene.collection.objects.link(pivot)
    for o in objects:
        if o.parent is None:
            o.parent = pivot
    pivot.rotation_euler = (0.0, 0.0, math.radians(yaw_deg))
    bpy.context.view_layer.update()
    lo, hi = world_bounds(objects)
    center = (lo + hi) * 0.5
    extent = max(hi.x - lo.x, hi.z - lo.z, hi.y - lo.y)
    # 正射影カメラ：正面（-Y 側）から、少し上から見下ろす
    cam = bpy.data.cameras.new("Camera")
    cam.type = "ORTHO"
    cam.ortho_scale = extent * 1.18
    cam_obj = bpy.data.objects.new("Camera", cam)
    scene.collection.objects.link(cam_obj)
    distance = extent * 4.0
    pitch = math.radians(pitch_deg)
    cam_obj.location = center + Vector((0.0, -distance * math.cos(pitch), distance * math.sin(pitch)))
    direction = center - cam_obj.location
    cam_obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam_obj
    scene.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True)
    print("rendered", name, "extent", round(extent, 3))


for name, path, yaw, pitch in MODELS:
    if os.path.exists(path):
        render(name, path, yaw, pitch)
    else:
        print("missing", path)
