# -*- coding: utf-8 -*-
"""恐竜の動き（make_intro_clips.py が player.gltf に入れたクリップ）の 1 コマを、Blender で絵にする（ゲームを起動せずに確かめる）。

  "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe" -b --factory-startup -P tools/stage_intro/render_clip_pose.py -- GoalPose 1.0 GoalDance 0.4
     … クリップ名と秒の組を並べる。tools/stage_intro/pose_<クリップ>_<秒>.png（正面・斜め・横の 3 枚を横に並べた絵）
"""
import bpy, math, os, sys
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
PLAYER = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "player", "player.gltf"))
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["GoalPose", "1.0"]
FPS = 30

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=PLAYER)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
sc = bpy.context.scene
sc.render.fps = FPS
sc.render.engine = "BLENDER_EEVEE_NEXT"
sc.render.resolution_x, sc.render.resolution_y = 480, 600
sc.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("w")
world.color = (0.8, 0.85, 0.9)
sc.world = world
sun = bpy.data.lights.new("sun", "SUN")
sun.energy = 3.0
so = bpy.data.objects.new("sun", sun)
sc.collection.objects.link(so)
so.rotation_euler = (math.radians(40), 0, math.radians(20))
cam_data = bpy.data.cameras.new("cam")
cam = bpy.data.objects.new("cam", cam_data)
sc.collection.objects.link(cam)
sc.camera = cam
cam_data.angle_y = 0.6
cam_data.sensor_fit = "VERTICAL"


def find_action(name):
    for action in bpy.data.actions:
        if action.name == name or action.name.startswith(name + "_") or action.name.split("|")[-1] == name:
            return action
    raise SystemExit("no action " + name + " in " + ", ".join(a.name for a in bpy.data.actions))


def shoot(path, direction):
    target = Vector((0, 0, 0.9))
    cam.location = target + Vector(direction).normalized() * 4.2
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


# 恐竜の正面：glTF の +Z（Blender の -Y）
VIEWS = [(0, -1, 0.15), (-0.7, -0.7, 0.2), (-1, 0, 0.1)]
for name, seconds in zip(ARGS[0::2], ARGS[1::2]):
    action = find_action(name)
    arm.animation_data_create()
    arm.animation_data.action = action
    sc.frame_set(int(round(float(seconds) * FPS)) + int(action.frame_range[0]))
    paths = []
    for i, v in enumerate(VIEWS):
        p = os.path.join(HERE, "_pose_%d.png" % i)
        shoot(p, v)
        paths.append(p)
    # 3 枚を横に並べる
    imgs = [bpy.data.images.load(p) for p in paths]
    w, h = imgs[0].size
    out = bpy.data.images.new("sheet", w * 3, h)
    import numpy as np
    sheet = np.zeros((h, w * 3, 4))
    for k, im in enumerate(imgs):
        px = np.array(im.pixels[:]).reshape(h, w, 4)
        sheet[:, k * w:(k + 1) * w] = px
    out.pixels = sheet.ravel()
    out.filepath_raw = os.path.join(HERE, "pose_%s_%s.png" % (name, seconds))
    out.file_format = "PNG"
    out.save()
    for p in paths:
        os.remove(p)
    print("wrote", out.filepath_raw)
