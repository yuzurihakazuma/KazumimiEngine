# -*- coding: utf-8 -*-
"""ゴールの舞台（クリア演出）の工作モデルを Blender で作る。

  "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe" -b --factory-startup -P tools/clear/make_goal_set.py
  "…/blender.exe" -b --factory-startup -P tools/clear/make_goal_set.py -- --preview   … 確認用の絵も描く（tools/clear/preview_set_*.png）

出力（タイトル・背景と同じクラフトの素材。テクスチャは resources/craft/textures を共有）
  resources/craft/models/goal_gate.gltf        … たまごの門（段ボールの柱・紙の虹のアーチ・旗・花・木・ゴールのマット）。動かない部分
  resources/craft/models/goal_egg_bottom.gltf  … 門の上の大きな卵の下半分（巣の中）
  resources/craft/models/goal_egg_top.gltf     … 卵の上半分（くぐるとぽんと割れて開く）
  resources/craft/models/goal_tape_left/right  … ゴールテープ（紙のリボン）。くぐると左右に切れて垂れる
  resources/craft/models/goal_letter_0〜3      … 立体の切り文字「ゴ」「ー」「ル」「！」（原点＝文字の上のはし。糸はゲームが描く）
  resources/craft/models/goal_string.gltf      … 糸（長さ 1m・原点＝上のはし。ゲームが縦に伸ばす）
  resources/craft/models/goal_star_0〜2        … 棒付きの星（原点＝棒の根元）
  resources/clear/clear.json の "gate"         … 門の寸法（くぐる所・アーチ・卵のちょうつがい・テープの根元）。ゲームが読む

座標（Blender）：X＝道の横（-X＝遊んでいる時のカメラの側）、-Y＝門の表（恐竜が抜けていく向き）、Z＝上。原点＝門の真ん中の足元
  ゲームへ入ると (x, y, z) → (-x, z, -y)：門の表はエンジンの +Z（クラフトの素材の決まりと同じ）
"""
import bpy, bmesh, math, random, os, sys, json
import numpy as np
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RES = os.path.join(ROOT, "project", "resources")
TEX = os.path.join(RES, "craft", "textures")
MODELS = os.path.join(RES, "craft", "models")
CLEAR_JSON = os.path.join(RES, "clear", "clear.json")
FONT = r"C:\Windows\Fonts\BIZ-UDGothicB.ttc"
ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PREVIEW = "--preview" in ARGV

# ---------------------------------------------------------------- 寸法（ゲームも clear.json の "gate" で同じ値を使う）
PILLAR_X = 1.75          # 柱の真ん中（道の真ん中から横へ）
PILLAR_W = (0.92, 0.88, 0.84)  # 柱の箱（下から）の幅
BOX_H = 0.9              # 箱 1 つの高さ
PILLAR_TOP = BOX_H * 3   # 2.7
ARCH_CY = PILLAR_TOP     # アーチの円の中心の高さ
R_IN = PILLAR_X - PILLAR_W[2] / 2   # 1.33：くぐる所の半幅
R_OUT = PILLAR_X + PILLAR_W[2] / 2  # 2.17
ARCH_D = 0.62            # アーチの厚み（道の向き）
EGG_A, EGG_C = 0.56, 0.74  # 卵の横の半径・縦の半径
EGG_Z = ARCH_CY + R_OUT + 0.08 + EGG_C * 0.86  # 卵の真ん中の高さ（巣に少し沈む）
CRACK_PHI = 1.22         # 割れ目の高さ（上から測った角度 rad。0＝てっぺん）
TAPE_Z = 1.05            # ゴールテープの高さ
LETTER_H = 1.05          # 切り文字の高さ
STAR_H, STICK = 0.78, 1.9  # 星の大きさ・棒の長さ
LETTER_COLORS = ("paper_blue", "paper_pink", "paper_orange", "paper_purple")  # 虹（赤・黄・緑）と別の色にして、アーチの前で目立たせる
GATE_TURN = 15.0         # 門の入口の面（裏 +Y。恐竜が近づいてくる側）を、遊んでいる時のカメラ（-X）の方へ回す角度（ゲームの stage1.json の gateTurn と同じ）

PALETTE = {
    "paper_red": "#E64838", "paper_yellow": "#FCD640", "paper_grass": "#60AA32", "paper_blue": "#78B4E8",
    "paper_orange": "#F6842C", "paper_white": "#F6F3EA",
}
NEW_COLORS = {"paper_pink": "#F28CB4", "paper_purple": "#9C78D8", "paper_teal": "#3CBEB0"}


# ================================================================ テクスチャ（numpy）
TS = 512


def _hex(c):
    c = c.lstrip("#")
    return np.array([int(c[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


def _noise(seed, k):
    r = np.random.default_rng(seed)
    a = r.random((TS, TS))
    for _ in range(k):
        a = (a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 5
    return (a - a.min()) / (a.max() - a.min() + 1e-9)


def _paper(seed):
    return 0.9 + 0.1 * _noise(seed, 3) - 0.04 * (np.random.default_rng(seed + 1).random((TS, TS)) > 0.985)


def save_png(name, rgb):
    img = bpy.data.images.new(name, TS, TS, alpha=False)
    px = np.ones((TS, TS, 4))
    px[..., :3] = np.clip(rgb, 0, 1)
    img.pixels = px[::-1].ravel()
    img.filepath_raw = os.path.join(TEX, name + ".png")
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def build_textures():
    for i, (name, c) in enumerate(NEW_COLORS.items()):
        save_png(name, _paper(40 + i)[..., None] * _hex(c))
    # 卵：クリーム色に緑の水玉（球に貼る。横が一周・縦が上から下）
    base = _paper(61)[..., None] * _hex("#FBF4DE")
    yy, xx = np.mgrid[0:TS, 0:TS] / TS
    spots = np.zeros((TS, TS))
    r = random.Random(7)
    for row in range(5):
        v = 0.12 + row * 0.19
        for k in range(5 + (row % 2)):
            u = (k + 0.5 * (row % 2) + r.uniform(-0.1, 0.1)) / (5 + (row % 2))
            rad = 0.055 + r.uniform(-0.01, 0.015)
            du = np.minimum(np.abs(xx - u), 1 - np.abs(xx - u))
            d = np.sqrt((du * 2.0) ** 2 + (yy - v) ** 2)  # 横は一周＝縦の倍なので、丸く見えるよう横を詰める
            spots = np.maximum(spots, (d < rad * (1 + 0.15 * np.sin(np.arctan2(yy - v, du) * 5))).astype(float))
    green = _paper(62)[..., None] * _hex("#5DB040")
    save_png("paper_egg", base * (1 - spots[..., None]) + green * spots[..., None])
    # ゴールテープ：赤と白の斜めのしま
    stripe = ((xx + yy) * 8 % 1.0) < 0.5
    tape = np.where(stripe[..., None], _hex("#E64838"), _hex("#F6F3EA")) * _paper(63)[..., None]
    save_png("paper_tape", tape)
    # ゴールのマット：赤とクリームの市松
    check = ((np.floor(xx * 8) + np.floor(yy * 8)) % 2) == 0
    save_png("paper_checker", np.where(check[..., None], _hex("#E64838"), _hex("#FBF0D8")) * _paper(64)[..., None])


# ================================================================ マテリアル
_mats = {}


def mat(key, rough=0.85, sheen=0.0):
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
    b.inputs["Specular IOR Level"].default_value = 0.3
    if sheen:
        b.inputs["Sheen Weight"].default_value = sheen
    _mats[name] = m
    return m


paper = lambda k: mat(k, 0.8)
felt = lambda k: mat(k, 1.0, 0.15)
KRAFT = lambda: mat("kraft", 0.9)
EDGE = lambda: mat("kraft_edge", 0.95)


# ================================================================ 形
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


def extrude(name, pts, y0, y1, face_mat, edge_mat=None, uv_scale=0.5, edge_uv=1.2):
    """XZ 平面の輪郭 pts を、Y の y0〜y1 へ押し出す（面＝face_mat、切り口＝edge_mat）"""
    edge_mat = edge_mat or face_mat
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    F = [bm.verts.new((a, y0, b)) for a, b in pts]
    B = [bm.verts.new((a, y1, b)) for a, b in pts]
    uv = {}
    for f, vs in ((bm.faces.new(F), F), (bm.faces.new(B[::-1]), B)):
        f.material_index = 0
        uv[f] = {v: (v.co.x * uv_scale, v.co.z * uv_scale) for v in vs}
    n = len(pts)
    L = [0.0]
    for i in range(n):
        L.append(L[-1] + (Vector(pts[(i + 1) % n]) - Vector(pts[i])).length)
    for i in range(n):
        j = (i + 1) % n
        f = bm.faces.new((F[i], F[j], B[j], B[i]))
        f.material_index = 1
        uv[f] = {F[i]: (L[i] * edge_uv, 0), F[j]: (L[i + 1] * edge_uv, 0), B[j]: (L[i + 1] * edge_uv, 1), B[i]: (L[i] * edge_uv, 1)}
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        for l in f.loops:
            l[uvl].uv = uv[f][l.vert]
    return mesh_object(name, bm, [face_mat, edge_mat])


def extrude_up(name, pts, z0, z1, top_mat, side_mat, uv_scale=0.6, edge_uv=1.2):
    """XY 平面の輪郭（上から見た形）を、Z の z0〜z1 へ押し出す（柱の箱など）"""
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    D = [bm.verts.new((a, b, z0)) for a, b in pts]
    U = [bm.verts.new((a, b, z1)) for a, b in pts]
    uv = {}
    for f, vs in ((bm.faces.new(U), U), (bm.faces.new(D[::-1]), D)):
        f.material_index = 0
        uv[f] = {v: (v.co.x * uv_scale, v.co.y * uv_scale) for v in vs}
    n = len(pts)
    L = [0.0]
    for i in range(n):
        L.append(L[-1] + (Vector(pts[(i + 1) % n]) - Vector(pts[i])).length)
    for i in range(n):
        j = (i + 1) % n
        f = bm.faces.new((D[i], D[j], U[j], U[i]))
        f.material_index = 1
        h = (z1 - z0)
        uv[f] = {D[i]: (L[i] * edge_uv, 0), D[j]: (L[i + 1] * edge_uv, 0), U[j]: (L[i + 1] * edge_uv, h), U[i]: (L[i] * edge_uv, h)}
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        for l in f.loops:
            l[uvl].uv = uv[f][l.vert]
    return mesh_object(name, bm, [top_mat, side_mat])


def rounded_rect(w, d, r, cx=0.0, cy=0.0, n=4):
    pts = []
    for qx, qy, a0 in ((1, 1, 0), (-1, 1, 90), (-1, -1, 180), (1, -1, 270)):
        ccx, ccy = cx + qx * (w / 2 - r), cy + qy * (d / 2 - r)
        for k in range(n + 1):
            a = math.radians(a0 + 90 * k / n)
            pts.append((ccx + r * math.cos(a), ccy + r * math.sin(a)))
    return pts


def annulus(r_in, r_out, cz, a0, a1, n=48):
    """円の帯（角度 a0→a1、XZ 平面）"""
    out = [(r_out * math.cos(a0 + (a1 - a0) * k / n), cz + r_out * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
    inn = [(r_in * math.cos(a1 - (a1 - a0) * k / n), cz + r_in * math.sin(a1 - (a1 - a0) * k / n)) for k in range(n + 1)]
    return out + inn


def blob(R, lobes, amp, seed, n=64, cx=0.0, cz=0.0, squash=1.0):
    r = random.Random(seed)
    ph = r.uniform(0, 6.28)
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        rr = R * (1 + amp * abs(math.sin(lobes * t / 2 + ph)) + 0.03 * math.sin(3 * t + ph))
        pts.append((cx + rr * math.cos(t), cz + rr * math.sin(t) * squash))
    return pts


def flower_pts(R, petals=6, n=90, cx=0.0, cz=0.0):
    return [(cx + R * (0.5 + 0.5 * abs(math.cos(petals * t / 2)) ** 0.7) * math.cos(t),
             cz + R * (0.5 + 0.5 * abs(math.cos(petals * t / 2)) ** 0.7) * math.sin(t))
            for t in (2 * math.pi * i / n for i in range(n))]


def star_pts(R, r_in, cx=0.0, cz=0.0, n=5, round_=0.0):
    pts = []
    for i in range(n * 2):
        a = math.pi / 2 + math.pi * i / n
        rr = R if i % 2 == 0 else r_in
        pts.append((cx + rr * math.cos(a), cz + rr * math.sin(a)))
    return pts


def circle(R, n=32, cx=0.0, cz=0.0):
    return [(cx + R * math.cos(2 * math.pi * i / n), cz + R * math.sin(2 * math.pi * i / n)) for i in range(n)]


def stick(name, p0, p1, radius, m, verts=8):
    """p0→p1 の細い棒"""
    p0, p1 = Vector(p0), Vector(p1)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=radius, radius2=radius, depth=(p1 - p0).length)
    uvl = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        for l in f.loops:
            l[uvl].uv = (math.atan2(l.vert.co.y, l.vert.co.x), l.vert.co.z)
    d = (p1 - p0).normalized()
    rot = d.to_track_quat("Z", "Y").to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((p0 + p1) / 2) @ rot, verts=bm.verts)
    return mesh_object(name, bm, [m])


def box_uv(ob, s=1.0):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    uvl = bm.loops.layers.uv.verify()
    for f in bm.faces:
        ax = max(range(3), key=lambda i: abs(f.normal[i]))
        for l in f.loops:
            c = l.vert.co
            l[uvl].uv = ((c.y, c.z) if ax == 0 else (c.x, c.z) if ax == 1 else (c.x, c.y))
            l[uvl].uv = (l[uvl].uv[0] * s, l[uvl].uv[1] * s)
    bm.to_mesh(ob.data)
    bm.free()


def place(ob, loc=(0, 0, 0), rz=0.0, rx=0.0, ry=0.0, s=1.0):
    ob.location = loc
    ob.rotation_euler = (math.radians(rx), math.radians(ry), math.radians(rz))
    ob.scale = (s, s, s)
    return ob


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
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35))
    return ob


# ---------------------------------------------------------------- 飾り（小さな部品）
def paper_flower(tag, x, y, z, R, petal, facing_y=-1, center="paper_yellow"):
    """表（-Y）か裏（+Y）を向いた紙の花"""
    t = 0.012
    y0 = y if facing_y < 0 else y - t
    p = extrude(tag + "p", flower_pts(R, 6, cx=x, cz=z), y0, y0 + t, paper(petal))
    c = extrude(tag + "c", circle(R * 0.36, 24, cx=x, cz=z), y0 + facing_y * 0.012, y0 + facing_y * 0.012 + 0.03, paper(center))
    return [p, c]


def felt_bush(tag, x, y, R, color, seed):
    a = extrude(tag + "a", blob(R, 5, 0.18, seed, cx=0, cz=R * 0.8, squash=0.8), -0.03, 0.03, felt(color), EDGE())
    b = extrude(tag + "b", blob(R * 0.9, 5, 0.18, seed + 1, cx=0, cz=R * 0.75, squash=0.8), -0.03, 0.03, felt("felt_grass"), EDGE())
    place(b, rz=90)
    for ob in (a, b):
        ob.location = (x, y, 0)
    return [a, b]


def round_tree(tag, x, y, h, R, color, seed):
    trunk = stick(tag + "t", (x, y, 0), (x, y, h), 0.08, KRAFT(), 10)
    a = extrude(tag + "a", blob(R, 5, 0.15, seed, cz=h + R * 0.6), -0.04, 0.04, felt(color), EDGE())
    b = extrude(tag + "b", blob(R * 0.95, 5, 0.15, seed + 3, cz=h + R * 0.6), -0.04, 0.04, felt("felt_leaf"), EDGE())
    place(b, rz=90)
    a.location = (x, y, 0)
    b.location = (x, y, 0)
    return [trunk, a, b]


def grass(tag, x, y, w, seed, rz=0):
    r = random.Random(seed)
    pts = [(-w / 2, 0.0), (w / 2, 0.0)]
    teeth = 5
    for i in range(teeth * 2, -1, -1):
        pts.append((-w / 2 + w * i / (teeth * 2), (0.3 * r.uniform(0.7, 1.0)) if i % 2 else 0.07))
    g = extrude(tag, pts, -0.006, 0.006, paper("paper_grass"))
    place(g, (x, y, 0), rz=rz)
    return [g]


# ================================================================ 門（動かない部分）
def build_gate():
    parts = []
    bands = ("paper_red", "paper_yellow", "paper_blue")
    r = random.Random(3)
    for side in (-1, 1):
        x = side * PILLAR_X
        for k in range(3):
            w = PILLAR_W[k]
            z0 = BOX_H * k
            box = extrude_up(f"box{side}{k}", rounded_rect(w, w * 0.92, 0.07, x, 0.0), z0, z0 + BOX_H - 0.015, KRAFT(), EDGE())
            box.rotation_euler = (0, 0, math.radians(r.uniform(-5, 5)))
            box.location = (0, 0, 0)
            # 箱の真ん中を中心に回す
            box.data.transform(Matrix.Translation((-x, 0, 0)))
            box.location = (x, 0, 0)
            parts.append(box)
            # 色紙の帯を巻く
            band = extrude_up(f"band{side}{k}", rounded_rect(w + 0.02, w * 0.92 + 0.02, 0.08, 0.0, 0.0), z0 + BOX_H * 0.32,
                              z0 + BOX_H * 0.56, paper(bands[(k + (side > 0)) % 3]), paper(bands[(k + (side > 0)) % 3]), edge_uv=0.8)
            band.rotation_euler = box.rotation_euler
            band.location = (x, 0, 0)
            parts.append(band)
        # 大きな紙の花：裏は柱の上、表は柱のいちばん上の箱の面（表の柱の上は「ゴール！」の札が下りてくるので空けておく）
        parts += paper_flower(f"pf{side}b", x, ARCH_D / 2 + 0.03, PILLAR_TOP + 0.62, 0.34, "paper_orange", 1)
        parts += paper_flower(f"pf{side}f", x, -(PILLAR_W[2] * 0.46 + 0.03), PILLAR_TOP - 0.45, 0.3, "paper_pink", -1)
    # アーチ：段ボールの帯（真ん中の芯）＋表と裏に色紙の虹（外から赤・黄・緑）
    parts.append(extrude("arch", annulus(R_IN, R_OUT, ARCH_CY, 0, math.pi, 56), -ARCH_D / 2, ARCH_D / 2, KRAFT(), EDGE(), edge_uv=0.9))
    width = (R_OUT - R_IN) / 3
    rainbow = ("paper_red", "paper_yellow", "paper_grass")
    for face in (-1, 1):
        for k, color in enumerate(rainbow):
            ro = R_OUT - k * width + (0.015 if k == 0 else 0.0)
            ri = ro - width - (0.015 if k == 0 else 0.0)
            y = face * ARCH_D / 2
            y0, y1 = (y - 0.014, y) if face < 0 else (y, y + 0.014)
            parts.append(extrude(f"rb{face}{k}", annulus(ri, ro, ARCH_CY, -0.02, math.pi + 0.02, 56), y0, y1, paper(color)))
        # アーチのふちの小さな三角の旗（外へ向けて、色をかわるがわる）
        colors = ("paper_blue", "paper_pink", "paper_yellow", "paper_teal", "paper_orange", "paper_purple")
        count = 11
        for i in range(count):
            a = math.pi * (i + 0.5) / count
            c, s = math.cos(a), math.sin(a)
            base = R_OUT + 0.01
            tip = R_OUT + 0.34
            half = 0.13
            p = [(base * c - half * s, ARCH_CY + base * s + half * c), (base * c + half * s, ARCH_CY + base * s - half * c),
                 (tip * c, ARCH_CY + tip * s)]
            yy = face * (ARCH_D / 2 - 0.12)
            parts.append(extrude(f"fl{face}{i}", p, yy - 0.006, yy + 0.006, paper(colors[i % len(colors)])))
    # 卵の巣（アーチのてっぺん）：紙の帯を丸めたうつわ
    nest = []
    for i in range(14):
        a = 2 * math.pi * i / 14
        x, y = 0.5 * math.cos(a), 0.5 * math.sin(a)
        strip = extrude(f"ns{i}", [(-0.13, 0), (0.13, 0), (0.1, 0.36), (-0.1, 0.36)], -0.01, 0.01, KRAFT() if i % 2 else paper("paper_yellow"))
        strip.rotation_euler = (math.radians(-22), 0, a + math.pi / 2)
        strip.location = (x, y, ARCH_CY + R_OUT - 0.08)
        nest.append(strip)
    parts += nest
    parts.append(extrude_up("nestbase", circle(0.52, 28), ARCH_CY + R_OUT - 0.12, ARCH_CY + R_OUT + 0.02, KRAFT(), EDGE()))
    # 足元：ゴールのマット・フェルトのしげみ・花・草
    parts.append(extrude_up("mat", rounded_rect(2.2, 1.3, 0.12), 0.0, 0.018, paper("paper_checker"), paper("paper_white"), uv_scale=0.9))
    for side in (-1, 1):
        x = side * PILLAR_X
        for face in (-1, 1):
            parts += felt_bush(f"bu{side}{face}", x + side * 0.35, face * 0.62, 0.36, "felt_hill_2" if face < 0 else "felt_leaf", 20 + side + face)
            parts += paper_flower(f"ff{side}{face}", x - side * 0.62, face * 0.55, 0.42, 0.13, "paper_white" if face < 0 else "paper_pink", face)
            parts.append(stick(f"fs{side}{face}", (x - side * 0.62, face * 0.55, 0), (x - side * 0.62, face * 0.55, 0.36), 0.015, paper("paper_grass")))
        parts += grass(f"gr{side}", x + side * 0.85, -0.15, 0.7, 30 + side, rz=10 * side)
    # 遠い側（+X。遊んでいる時のカメラから見て道の向こう）：木と星の飾り。手前側（-X）は低い物だけ
    parts += round_tree("tr0", 3.5, 0.7, 1.2, 0.85, "felt_grass", 41)
    parts += round_tree("tr1", 4.6, -0.9, 0.9, 0.62, "felt_hill_2", 43)
    parts += round_tree("tr2", 3.1, -1.6, 0.55, 0.42, "felt_hill_1", 45)
    for i, (x, y, h, color) in enumerate(((2.75, 1.4, 2.3, "paper_yellow"), (3.9, 0.2, 2.9, "paper_pink"), (2.6, -1.0, 1.8, "paper_teal"))):
        parts.append(stick(f"ss{i}", (x, y, 0), (x, y, h), 0.025, paper("wood_stick")))
        parts.append(extrude(f"st{i}", star_pts(0.32, 0.15, cx=x, cz=h + 0.22), y - 0.025, y + 0.025, paper(color), EDGE()))
    for i, (x, y) in enumerate(((-2.75, -0.6), (-3.2, 0.5), (-2.5, 0.9))):
        parts += paper_flower(f"lf{i}", x, y, 0.3, 0.12, ("paper_red", "paper_yellow", "paper_white")[i], -1)
        parts.append(stick(f"ls{i}", (x, y, 0), (x, y, 0.26), 0.015, paper("paper_grass")))
        parts += grass(f"lg{i}", x + 0.2, y - 0.1, 0.5, 50 + i)
    return join("goal_gate", parts)


# ================================================================ 卵（割れ目でふたつ）
def egg_point(theta, phi):
    rho = EGG_A * math.sin(phi) * (1.0 + 0.1 * math.cos(phi))  # 上がすこし細い
    return Vector((rho * math.cos(theta), rho * math.sin(theta), EGG_Z + EGG_C * math.cos(phi)))


def crack_phi(theta):
    k = (theta / (2 * math.pi) * 14) % 1.0
    return CRACK_PHI + 0.16 * (abs(k - 0.5) * 2 - 0.5)  # ぎざぎざ


def build_egg_half(name, top):
    seg, rows = 56, 14
    outer = mat("paper_egg", 0.8)
    inner = paper("paper_white")
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    grid = []
    for i in range(seg + 1):
        th = 2 * math.pi * i / seg
        pc = crack_phi(th)
        col = []
        for j in range(rows + 1):
            t = j / rows
            phi = (0.0 + pc * t) if top else (pc + (math.pi - pc) * t)
            col.append((th, phi))
        grid.append(col)
    for shell, m_index, shrink in ((0, 0, 1.0), (1, 1, 0.965)):
        vs = [[bm.verts.new(EGG_CENTER + (egg_point(th, phi) - EGG_CENTER) * shrink) for th, phi in col] for col in grid]
        where = {vs[a][b]: (a, b) for a in range(seg + 1) for b in range(rows + 1)}
        for i in range(seg):
            for j in range(rows):
                q = (vs[i][j], vs[i + 1][j], vs[i + 1][j + 1], vs[i][j + 1])
                if shell:
                    q = q[::-1]
                try:
                    f = bm.faces.new(q)
                except ValueError:
                    continue
                f.material_index = m_index
                for l in f.loops:
                    th, phi = grid[where[l.vert][0]][where[l.vert][1]]
                    l[uvl].uv = (th / (2 * math.pi), 1.0 - phi / math.pi)
        if shell == 0:
            outer_cols = vs
        else:
            inner_cols = vs
    # 割れ目のふち（外と内をつなぐ）
    edge_j = rows if top else 0
    for i in range(seg):
        a, b = outer_cols[i][edge_j], outer_cols[i + 1][edge_j]
        c, d = inner_cols[i + 1][edge_j], inner_cols[i][edge_j]
        try:
            f = bm.faces.new((a, b, c, d) if top else (b, a, d, c))
            f.material_index = 1
        except ValueError:
            pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    # 外の殻は外向き、内の殻は内向き（割れて開いた時に、中が白く見える）
    outer_faces = [f for f in bm.faces if f.material_index == 0]
    inner_faces = [f for f in bm.faces if f.material_index == 1]
    bmesh.ops.recalc_face_normals(bm, faces=outer_faces)
    bmesh.ops.recalc_face_normals(bm, faces=inner_faces)
    bmesh.ops.reverse_faces(bm, faces=[f for f in inner_faces if f.is_valid and (f.calc_center_median() - EGG_CENTER).dot(f.normal) > 0])
    ob = mesh_object(name, bm, [outer, inner])
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.shade_smooth()
    return ob


EGG_CENTER = Vector((0, 0, EGG_Z))


# ================================================================ ゴールテープ（左右のふたつ。くぐると根元を軸に垂れる）
def build_tape(name, side):
    x0, x1 = side * R_IN, 0.0
    n = 18
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    top, bot = [], []
    for k in range(n + 1):
        t = k / n
        x = x0 + (x1 - x0) * t
        sag = -0.07 * math.sin(math.pi * (0.5 + 0.5 * t))  # 真ん中へ行くほど少し下がる
        wave = 0.02 * math.sin(t * 9.0)
        top.append(bm.verts.new((x, wave, TAPE_Z + 0.075 + sag)))
        bot.append(bm.verts.new((x, wave, TAPE_Z - 0.075 + sag)))
    for k in range(n):
        for q, flip in (((top[k], top[k + 1], bot[k + 1], bot[k]), False), ((bot[k], bot[k + 1], top[k + 1], top[k]), True)):
            try:
                f = bm.faces.new(q)
            except ValueError:
                continue
            for l in f.loops:
                idx = (top if l.vert in top else bot).index(l.vert)
                l[uvl].uv = (idx / n * 3.0, 1.0 if l.vert in top else 0.0)
    return mesh_object(name, bm, [paper("paper_tape")])


# ================================================================ 立体の切り文字・星・糸
def text_mesh(name, body, size, depth, offset=0.0):
    curve = bpy.data.curves.new(name, "FONT")
    curve.body = body
    curve.font = bpy.data.fonts.load(FONT, check_existing=True)
    curve.size = size
    curve.extrude = depth / 2
    curve.offset = offset
    curve.align_x = "CENTER"
    curve.align_y = "CENTER"
    curve.resolution_u = 6
    ob = bpy.data.objects.new(name, curve)
    link(ob)
    ob.rotation_euler = (math.radians(90), 0, 0)  # 文字は XY に出来る → XZ（表＝-Y）へ立てる
    deps = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(deps))
    out = link(bpy.data.objects.new(name + "_m", me))
    out.matrix_world = ob.matrix_world
    bpy.data.objects.remove(ob)
    return out


def assign(ob, front, side):
    """表と裏＝front、切り口＝side（法線の向きで分ける）"""
    me = ob.data
    me.materials.clear()
    me.materials.append(front)
    me.materials.append(side)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    for p in me.polygons:
        p.material_index = 0 if abs(p.normal.y) > 0.7 else 1
    box_uv(ob, 1.2)
    return ob


def build_letter(i, ch, color):
    """白い紙の札（角の丸い四角・段ボールの切り口）に、色紙の立体の文字を貼る。アーチの虹の前でも読めるように"""
    card = extrude(f"lc{i}", [(x, z) for x, z in rounded_rect(LETTER_H, LETTER_H, 0.16, 0.0, -LETTER_H / 2)], -0.03, 0.03,
                   paper("paper_white"), EDGE())
    front = assign(text_mesh(f"lf{i}", ch, LETTER_H * 0.92, 0.06, offset=0.022), paper(color), EDGE())
    # 文字の形の真ん中を、札の真ん中へ
    xs = [v.co.x for v in front.data.vertices]
    zs = [v.co.z for v in front.data.vertices]
    front.data.transform(Matrix.Translation((-(max(xs) + min(xs)) / 2, -0.06, -LETTER_H / 2 - (max(zs) + min(zs)) / 2)))
    hook = stick(f"hk{i}", (0, 0, -0.08), (0, 0, 0.0), 0.022, paper("paper_white"))
    return join(f"goal_letter_{i}", [card, front, hook])  # 原点＝札の上のはしの真ん中（糸をつなぐ所）


def build_star(i, color):
    s = extrude(f"sf{i}", star_pts(STAR_H * 0.5, STAR_H * 0.23, cz=STICK + STAR_H * 0.42), -0.04, 0.04, paper(color), EDGE())
    eye = []
    for ex in (-0.09, 0.09):  # 小さな目（黒い紙）
        eye.append(extrude(f"se{i}{ex}", circle(0.035, 16, cx=ex, cz=STICK + STAR_H * 0.45), -0.055, -0.04, paper("paper_blue")))
    st = stick(f"sk{i}", (0, 0.06, 0), (0, 0.06, STICK + 0.1), 0.028, paper("wood_stick"))
    return join(f"goal_star_{i}", [s, st] + eye)


def build_string():
    return join("goal_string", [stick("str", (0, 0, -1.0), (0, 0, 0), 0.012, paper("paper_white"), 6)])


# ================================================================ クリアの書き割りの太陽（clear.json の "backdrop" が門の後ろに置く）
def build_sun():
    """紙の太陽：黄色の丸に、オレンジの三角の光（12 本）。裏に段ボールの切り口。原点＝丸の真ん中の下のはし"""
    R = 1.4
    parts = [extrude("sd", circle(R, 48, cz=R + 0.9), -0.05, 0.05, paper("paper_yellow"), EDGE())]
    rays = 12
    for i in range(rays):
        a = 2 * math.pi * (i + 0.5) / rays
        c, s = math.cos(a), math.sin(a)
        base, tip, half = R + 0.08, R + 0.75, 0.28
        cz = R + 0.9
        pts = [(base * c - half * s, cz + base * s + half * c), (base * c + half * s, cz + base * s - half * c), (tip * c, cz + tip * s)]
        parts.append(extrude(f"sr{i}", pts, -0.03, 0.03, paper("paper_orange" if i % 2 else "paper_yellow"), EDGE()))
    # ほっぺ（ピンクの丸）
    for x in (-0.75, 0.75):
        parts.append(extrude(f"sc{x}", circle(0.22, 20, cx=x, cz=R + 0.65), -0.065, -0.05, paper("paper_pink")))
    return join("goal_sun", parts)


# ================================================================ 書き出し
def export(ob):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    path = os.path.join(MODELS, ob.name + ".gltf")
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_yup=True, export_apply=True,
                              export_format="GLTF_SEPARATE", export_keep_originals=True)
    print("wrote", os.path.relpath(path, ROOT))


def write_gate_json():
    with open(CLEAR_JSON, encoding="utf-8") as f:
        data = json.load(f)
    data["gate"] = {
        "innerHalfWidth": round(R_IN - 0.05, 3),         # くぐる所の半幅（柱の内側）
        "innerHeight": round(ARCH_CY + R_IN - 0.1, 3),   # くぐる所の高さ
        "archCenterHeight": round(ARCH_CY, 3),           # アーチの円の中心の高さ（切り文字の糸をつなぐ所を求める）
        "archOuterRadius": round(R_OUT, 3),
        "archFront": round(ARCH_D / 2 + 0.06, 3),        # アーチの表の面（切り文字を下げる面）
        "eggHinge": [0.0, round(EGG_Z + EGG_C * math.cos(CRACK_PHI), 3), round(EGG_A * 0.9, 3)],  # 卵のちょうつがい（横・高さ・奥）
        "tapePivot": [round(R_IN, 3), TAPE_Z],           # テープの根元（横・高さ）
        "height": round(EGG_Z + EGG_C, 3),               # 門のいちばん上（カメラに収める）
        "halfWidth": 4.8,                                # 飾りまで入れた半幅（カメラに収める）
    }
    data.pop("ring", None)
    with open(CLEAR_JSON, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print("updated clear.json gate")


# ================================================================ 確認用の絵
def preview():
    sc = bpy.context.scene
    # 道と台（ゲームの布の道の代わり）
    road = extrude_up("pv_road", rounded_rect(1.8, 30, 0.3, 0, 0), -0.02, 0.0, paper("paper_orange"), paper("paper_orange"))
    base = extrude_up("pv_base", rounded_rect(12, 30, 0.5, 0, 0), -0.6, -0.02, KRAFT(), EDGE())
    # 恐竜（ゲームのモデル）
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(RES, "player", "player.gltf"))
    dino = [o for o in bpy.data.objects if o not in before]
    land = Vector((0, -1.4, 0))
    for o in dino:
        if o.parent is None:
            o.location = land
            o.rotation_euler = (0, 0, math.radians(-20 + 180))
    # 門まわり（門・卵・テープ・文字・星）を、入口の面がカメラの方へ向くよう GATE_TURN だけ回す（ゲームと同じ）
    turn = Matrix.Rotation(math.radians(GATE_TURN), 4, "Z")
    gate_parts = [bpy.data.objects[n] for n in ("goal_gate", "goal_egg_bottom", "goal_egg_top", "goal_tape_left", "goal_tape_right")]
    # 切り文字：アーチの表から糸で下がる（ClearSettings の letters と同じ値）
    import json as _json
    cfg = _json.load(open(CLEAR_JSON, encoding="utf-8"))
    lt, stc, gate = cfg["letters"], cfg["stars"], cfg["gate"]
    # 切り文字：アーチの虹の上の弧に並べる（ClearSettings の letters と同じ決め方）。糸は上へ
    for i in range(4):
        ob = bpy.data.objects[f"goal_letter_{i}"]
        s = lt["scale"]
        ob.scale = (s, s, s)
        a = math.radians(lt["arcAngles"][i])
        x = math.sin(a) * lt["arcRadius"]       # 門の表（-Y）から見て、右＝+X
        top = gate["archCenterHeight"] + math.cos(a) * lt["arcRadius"] + lt["height"] * s / 2
        y = -(gate["archFront"] + lt["front"])
        ob.location = (x, y, top)
        st = stick(f"pvs{i}", (x, y, top), (x, y, top + lt["stringLength"]), 0.012, paper("paper_white"), 6)
        gate_parts += [ob, st]
    for i in range(stc["perSide"] * 2):
        side = -1 if i % 2 == 0 else 1
        ob = bpy.data.objects[f"goal_star_{i % 3}"].copy()
        ob.data = ob.data.copy()
        link(ob)
        fan = stc["angles"][min(i // 2, len(stc["angles"]) - 1)]
        ob.location = (side * stc["pivotSide"], -(stc["front"]), stc["pivotHeight"])
        ob.rotation_euler = (0, side * fan, 0)   # 外へ倒す（Y まわりの＋は、上が +X へ）
        gate_parts.append(ob)
    bpy.context.view_layer.update()  # 置いた場所を matrix_world へ反映してから回す
    for ob in gate_parts:
        ob.matrix_world = turn @ ob.matrix_world
    # 札は、ゲームと同じく表を演出のカメラへ向ける（カメラは、くぐった向きから -X の側へ 20° 回った所）
    for i in range(4):
        ob = bpy.data.objects[f"goal_letter_{i}"]
        ob.rotation_euler = (0.0, 0.0, math.radians(-20))
    # 書き出し用に作った太陽（門の真ん中にある）は、絵に入れない（書き割りの太陽は下で読み込んで置く）
    bpy.data.objects["goal_sun"].hide_render = True
    # クリアの書き割り（clear.json の "backdrop"。ゲームの ClearBackdrop と同じ置き方：カメラから見て奥・右・上）
    cam_yaw = math.radians(20)
    back = Vector((math.sin(cam_yaw), math.cos(cam_yaw), 0.0))   # カメラから見て奥
    right = Vector((math.cos(cam_yaw), -math.sin(cam_yaw), 0.0)) # カメラから見て右
    for k, item in enumerate(cfg.get("backdrop", [])):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=os.path.join(MODELS, item["model"] + ".gltf"))
        made = [o for o in bpy.data.objects if o not in before and o.parent is None]
        sx, sy, sz = item["scale"]
        for o in made:
            o.location = back * item["back"] + right * item["side"] + Vector((0, 0, item["up"]))
            o.rotation_euler = (0.0, 0.0, -cam_yaw + item.get("turn", 0.0))
            o.scale = (sx, sz, sy)   # エンジンの (x, y＝上, z) → Blender の (x, y, z＝上)
    # 光と空
    world = bpy.data.worlds.new("w")
    sc.world = world
    world.color = (0.55, 0.78, 0.95)
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 3.2
    so = link(bpy.data.objects.new("sun", sun))
    so.rotation_euler = (math.radians(50), 0, math.radians(-30))
    sc.render.engine = "BLENDER_EEVEE_NEXT"
    sc.render.resolution_x, sc.render.resolution_y = 1280, 720
    sc.view_settings.view_transform = "Standard"
    cam = bpy.data.cameras.new("cam")
    co = link(bpy.data.objects.new("cam", cam))
    sc.camera = co

    def shoot(path, eye, look, fov):
        co.location = eye
        d = Vector(look) - Vector(eye)
        co.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
        cam.angle_y = fov
        cam.sensor_fit = "VERTICAL"
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print("wrote", path)

    # 遊んでいる時のカメラ（横から：-X 側 10m・高さ 3.5・恐竜の 1m 上を見る）。恐竜が門の 4m 手前にいる時
    shoot(os.path.join(HERE, "preview_set_side.png"), (-10.0, 4.0, 3.5), (0, 4.0, 1.0), 0.78)
    # 演出の終わりのカメラ（ClearStageSettings の既定値と同じ式）
    yaw, dist, h, look_h, fov = math.radians(20), 8.6, 2.0, 2.4, 0.82
    eye = land + Vector((-math.sin(yaw) * dist, -math.cos(yaw) * dist, h))
    look = land + Vector((0, 0.7, look_h))
    shoot(os.path.join(HERE, "preview_set_front.png"), eye, look, fov)
    if "--top" in ARGV:  # 上から見た並び（書き割りの置き場所の確認用）
        shoot(os.path.join(HERE, "preview_set_top.png"), (0.0, 8.0, 60.0), (0.0, 8.0, 0.0), 0.9)


# ================================================================
if __name__ == "__main__":
    bpy.ops.wm.read_factory_settings(use_empty=True)
    build_textures()
    gate = build_gate()
    egg_b = build_egg_half("goal_egg_bottom", False)
    egg_t = build_egg_half("goal_egg_top", True)
    tl = build_tape("goal_tape_left", -1)
    tr = build_tape("goal_tape_right", 1)
    letters = [build_letter(i, ch, c) for i, (ch, c) in enumerate(zip("ゴール！", LETTER_COLORS))]
    stars = [build_star(i, c) for i, c in enumerate(("paper_yellow", "paper_pink", "paper_orange"))]
    string = build_string()
    sun = build_sun()
    for ob in [gate, egg_b, egg_t, tl, tr, string, sun] + letters + stars:
        export(ob)
    write_gate_json()
    if PREVIEW:
        preview()
    print("DONE")
