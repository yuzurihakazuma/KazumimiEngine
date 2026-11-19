# -*- coding: utf-8 -*-
"""渡し板（列車の駅の橋）の工作モデルを Blender で作る。

  "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe" -b --factory-startup -P tools/train/make_board.py
  "…/blender.exe" -b --factory-startup -P tools/train/make_board.py -- --preview   … 確認用の絵も描く（tools/train/preview_board.png）

出力（project/resources/craft/models。キットのモデルなのでゲームが自動で読む。テクスチャは resources/craft/textures を共有）
  board_grass … 草の板の 1m ぶん：段ボールの板に緑のフェルトの草を貼り、両側にぎざぎざの草のふちを垂らす。
                原点＝板のはし（ちょうつがいの側）の上の面の真ん中、板はエンジンの +Z へ 1m。ゲームが長さだけ並べる
  board_roll  … 巻いた草のマット（転がる前）：半径 1・幅 1.8 の筒。軸＝X、原点＝真ん中。巻いた渦が見える両はし

寸法（ゲームの game/rail/TrainVisuals.cpp と同じ）：板の幅 1.8m・板の厚み 0.12m・草のふちの垂れ 0.26m
座標（Blender）：X＝横、-Y＝前（エンジンの +Z）、Z＝上。ゲームへ入ると (x, y, z) → (-x, z, -y)
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RES = os.path.join(ROOT, "project", "resources")
TEX = os.path.join(RES, "craft", "textures")
MODELS = os.path.join(RES, "craft", "models")
ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PREVIEW = "--preview" in ARGV

WIDTH = 1.8
THICK = 0.12
FELT = 0.035          # 草のフェルトの厚み
FRINGE = 0.26         # 草のふちが垂れる長さ
TEETH = 4             # 1m あたりのぎざぎざの数

bpy.ops.wm.read_factory_settings(use_empty=True)
_mats = {}


def mat(key, rough=0.9):
    name = "M_" + key
    if name in _mats:
        return _mats[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = bpy.data.images.load(os.path.join(TEX, key + ".png"), check_existing=True)
    nt.links.new(t.outputs["Color"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = rough
    b.inputs["Specular IOR Level"].default_value = 0.2
    _mats[name] = m
    return m


GRASS = lambda: mat("felt_grass", 1.0)
KRAFT = lambda: mat("kraft")
EDGE = lambda: mat("kraft_edge")


def link(ob):
    bpy.context.scene.collection.objects.link(ob)
    return ob


def mesh_object(name, bm, mats):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    return link(bpy.data.objects.new(name, me))


def box_uv(ob, s=1.0):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    uvl = bm.loops.layers.uv.verify()
    for f in bm.faces:
        ax = max(range(3), key=lambda i: abs(f.normal[i]))
        for l in f.loops:
            c = l.vert.co
            u, v = (c.y, c.z) if ax == 0 else (c.x, c.z) if ax == 1 else (c.x, c.y)
            l[uvl].uv = (u * s, v * s)
    bm.to_mesh(ob.data)
    bm.free()


def box(name, x0, x1, y0, y1, z0, z1, top, side=None):
    side = side or top
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector(((x0 + x1) / 2 + v.co.x * (x1 - x0), (y0 + y1) / 2 + v.co.y * (y1 - y0), (z0 + z1) / 2 + v.co.z * (z1 - z0)))
    for f in bm.faces:
        f.material_index = 0 if abs(f.normal.z) > 0.5 else 1
    ob = mesh_object(name, bm, [top, side])
    box_uv(ob)
    return ob


def side_sheet(name, x0, x1, pts, m):
    """横から見た形 pts（Y, Z）を、X の x0〜x1 へ薄く押し出す（垂れる草のふち）"""
    bm = bmesh.new()
    A = [bm.verts.new((x0, y, z)) for y, z in pts]
    B = [bm.verts.new((x1, y, z)) for y, z in pts]
    bm.faces.new(A)
    bm.faces.new(B[::-1])
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((A[i], A[j], B[j], B[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = mesh_object(name, bm, [m])
    box_uv(ob)
    return ob


def cylinder_x(name, radius, width, cap, side, segments=36, x=0.0):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius, depth=width)
    uvl = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        is_cap = abs(f.normal.z) > 0.9
        f.material_index = 0 if is_cap else 1
        for l in f.loops:
            c = l.vert.co
            l[uvl].uv = (c.x * 0.5 + 0.5, c.y * 0.5 + 0.5) if is_cap else ((math.atan2(c.y, c.x) / math.pi + 1) * radius * math.pi, c.z)
    bmesh.ops.transform(bm, matrix=Matrix.Translation((x, 0, 0)) @ Matrix.Rotation(math.radians(90), 4, "Y"), verts=bm.verts)
    return mesh_object(name, bm, [cap, side])


def join(name, parts):
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1:
        bpy.ops.object.join()
    ob = bpy.context.active_object
    ob.name = ob.data.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(30))
    return ob


def export(ob):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    path = os.path.join(MODELS, ob.name + ".gltf")
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_yup=True, export_apply=True,
                              export_format="GLTF_SEPARATE", export_keep_originals=True)
    print("wrote", os.path.relpath(path, ROOT))


def build_board():
    """草の板の 1m（Blender の y＝0〜-1。少しすき間をあけて、並べると板の継ぎ目が見える）"""
    y0, y1 = -0.015, -0.985
    parts = [
        box("plank", -WIDTH / 2, WIDTH / 2, y1, y0, -THICK, -FELT, KRAFT(), EDGE()),
        box("felt", -WIDTH / 2 + 0.02, WIDTH / 2 - 0.02, y1 + 0.01, y0 - 0.01, -FELT, 0.0, GRASS(), GRASS()),
    ]
    # 両側のぎざぎざの草のふち（板の横から垂れる三角）
    pts = [(y0, 0.0)]
    step = (y1 - y0) / TEETH
    for k in range(TEETH):
        a = y0 + step * k
        pts.append((a + step * 0.5, -FRINGE))
        pts.append((a + step, -THICK * 0.6))
    pts[-1] = (y1, -THICK * 0.6)
    pts.append((y1, 0.0))
    for sx in (-1, 1):
        x = sx * (WIDTH / 2 + 0.005)
        parts.append(side_sheet("fringe", x - 0.012, x + 0.012, pts, GRASS()))
    return join("board_grass", parts)


def build_roll():
    """巻いた草のマット：外は草のフェルト、両はしは段ボールの渦（輪を重ねて渦に見せる）"""
    parts = [cylinder_x("roll", 1.0, WIDTH, KRAFT(), GRASS(), 40)]
    for sx in (-1, 1):
        for k, r in enumerate((0.78, 0.55, 0.33, 0.12)):
            x = sx * (WIDTH / 2 + 0.006 * (k + 1))
            parts.append(cylinder_x("ring", r, 0.01, EDGE() if k % 2 == 0 else GRASS(), GRASS(), 32, x))
    return join("board_roll", parts)


def preview(models):
    sc = bpy.context.scene
    for ob in models.values():
        ob.hide_render = True

    def put(src, loc, rx=0.0, s=(1, 1, 1)):
        ob = bpy.data.objects.new(src.name + "_pv", src.data)
        ob.location = loc
        ob.rotation_euler = (rx, 0, 0)
        ob.scale = s
        link(ob)
        return ob

    for i in range(4):
        put(models["board_grass"], (0, -i, 0))
    put(models["board_roll"], (3.0, -1.0, 0.45 - 0.1), rx=0.6, s=(1, 0.45, 0.45))
    world = bpy.data.worlds.new("w")
    world.color = (0.8, 0.85, 0.9)
    sc.world = world
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 3.0
    so = bpy.data.objects.new("sun", sun)
    link(so)
    so.rotation_euler = (math.radians(45), 0, math.radians(35))
    sc.render.engine = "BLENDER_EEVEE_NEXT"
    sc.render.resolution_x, sc.render.resolution_y = 1280, 720
    sc.view_settings.view_transform = "Standard"
    cam_data = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cam_data)
    link(cam)
    sc.camera = cam
    cam_data.angle_y = 0.6
    cam_data.sensor_fit = "VERTICAL"
    cam.location = (7.0, 3.0, 3.0)
    cam.rotation_euler = (Vector((1.5, -2.0, -0.2)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = os.path.join(HERE, "preview_board.png")
    bpy.ops.render.render(write_still=True)
    print("wrote", sc.render.filepath)


def main():
    models = {}
    for build in (build_board, build_roll):
        ob = build()
        export(ob)
        models[ob.name] = ob
    if PREVIEW:
        preview(models)


main()
