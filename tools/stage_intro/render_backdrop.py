# -*- coding: utf-8 -*-
"""ステージの背景（<ステージ>_backdrop.json と手直しの _backdrop_edit.json の足した物）を Blender に並べて絵にする（ゲームを起動せずに確かめる）。

  "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe" -b --factory-startup -P tools/stage_intro/render_backdrop.py -- [stage1]
     … tools/stage_intro/preview_backdrop_top.png（上から）/ _far.png（手前の高い所から引いて）/ _play.png（遊んでいる時の横から）
       / _flip.png・_evening.png・_evening_wide.png（裏から。夕方の光で）

  座標：エンジン (x, y, z) → Blender (-x, -z, y)。rotY θ → Blender の Z まわりに -θ。拡縮 (sx, sy, sz) → (sx, sz, sy)
"""
import bpy, json, math, os, sys
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
STAGE = ARGS[0] if ARGS else "stage1"
MAP_DIR = os.path.join(PROJECT, "resources", "map")
MODELS = os.path.join(PROJECT, "resources", "craft", "models")

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

base = json.load(open(os.path.join(MAP_DIR, STAGE + "_backdrop.json"), encoding="utf-8"))["objects"]
edit_path = os.path.join(MAP_DIR, STAGE + "_backdrop_edit.json")
added = json.load(open(edit_path, encoding="utf-8")).get("added", []) if os.path.exists(edit_path) else []
items = base + added

# モデルを 1 回ずつ読む（1 つの物にまとめて、データを共有して並べる）
library = {}
hidden = bpy.data.collections.new("library")
sc.collection.children.link(hidden)
hidden.hide_render = True
for name in sorted({o["model"] for o in items}):
    path = os.path.join(MODELS, name + ".gltf")
    if not os.path.exists(path):
        continue
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    made = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    if not made:
        continue
    bpy.ops.object.select_all(action="DESELECT")
    for o in made:
        o.select_set(True)
    bpy.context.view_layer.objects.active = made[0]
    if len(made) > 1:
        bpy.ops.object.join()
    ob = bpy.context.active_object
    ob.parent = None
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for c in ob.users_collection:
        c.objects.unlink(ob)
    hidden.objects.link(ob)
    library[name] = ob

shown = bpy.data.collections.new("stage")
sc.collection.children.link(shown)
for o in items:
    src = library.get(o["model"])
    if not src:
        continue
    ob = bpy.data.objects.new(o["model"], src.data)
    x, y, z = o["pos"]
    ob.location = (-x, -z, y)
    rot = o.get("rot", [0.0, o.get("rotY", 180.0), 0.0])
    ob.rotation_mode = "XYZ"
    ob.rotation_euler = (math.radians(rot[0]), 0.0, -math.radians(rot[1]))
    s = o.get("scale", [1, 1, 1])
    s = s if isinstance(s, list) else [s, s, s]
    ob.scale = (s[0], s[2], s[1])
    shown.objects.link(ob)

# 道（線で）
level = json.load(open(os.path.join(MAP_DIR, STAGE + ".json"), encoding="utf-8"))
mat = bpy.data.materials.new("road")
mat.diffuse_color = (1.0, 0.45, 0.1, 1.0)
for line in level["railLines"]:
    if len(line) < 2:
        continue
    curve = bpy.data.curves.new("rail", "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = 0.5
    spline = curve.splines.new("POLY")
    spline.points.add(len(line) - 1)
    for i, p in enumerate(line):
        spline.points[i].co = (-p[0], -p[2], p[1], 1.0)
    rail = bpy.data.objects.new("rail", curve)
    rail.data.materials.append(mat)
    shown.objects.link(rail)

world = bpy.data.worlds.new("w")
world.color = (0.59, 0.80, 0.97)  # 無地の紙の空の色
sc.world = world
sun = bpy.data.lights.new("sun", "SUN")
sun.energy = 2.5
so = bpy.data.objects.new("sun", sun)
sc.collection.objects.link(so)
so.rotation_euler = (math.radians(50), 0, math.radians(30))
sc.render.engine = "BLENDER_EEVEE_NEXT"
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
sc.view_settings.view_transform = "Standard"
cam_data = bpy.data.cameras.new("cam")
cam = bpy.data.objects.new("cam", cam_data)
sc.collection.objects.link(cam)
sc.camera = cam
cam_data.clip_end = 2000


def engine(x, y, z):
    return Vector((-x, -z, y))


def shoot(name, eye, look, fov=0.78, ortho=0.0):
    cam.location = eye
    cam.rotation_euler = (Vector(look) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
    if ortho:
        cam_data.type = "ORTHO"
        cam_data.ortho_scale = ortho
    else:
        cam_data.type = "PERSP"
        cam_data.angle_y = fov
        cam_data.sensor_fit = "VERTICAL"
    sc.render.filepath = os.path.join(HERE, "preview_backdrop_%s.png" % name)
    bpy.ops.render.render(write_still=True)
    print("wrote", sc.render.filepath)


xs = [p[0] for line in level["railLines"] for p in line]
zs = [p[2] for line in level["railLines"] for p in line]
cx, cz = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2
shoot("top", engine(cx, 400, cz) + Vector((0, 0.01, 0)), engine(cx, 0, cz), ortho=520)
shoot("far", engine(cx - 60, 70, min(zs) - 70), engine(cx, 0, cz + 20), fov=0.9)
shoot("play", engine(60, 7.5, -12), engine(60, 4.0, 0), fov=0.78)
shoot("flip", engine(60, 7.5, 52), engine(60, 4.0, 40), fov=0.78)
# 裏の世界から手前の辺（夕方の書き割り）を見る：夕方の光で
sun.color = (1.0, 0.6, 0.38)
world.color = (0.55, 0.38, 0.40)
shoot("evening", engine(4, 9, 14), engine(4, 6, -40), fov=0.78)
shoot("evening_wide", engine(4, 30, 40), engine(4, 0, -60), fov=1.0)
