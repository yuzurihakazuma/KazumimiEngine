# -*- coding: utf-8 -*-
"""
タイトルのマップ（project/resources/map/title.json）を作る。

  python tools/craftkit/make_title_map.py

中身は2つ：
  objects   … 背景の配置。CraftKit v2 の craftkit.py（build_title_scene）と同じ並べ方
              （乱数の種も同じなので、草・花の位置は title_scene.blend / preview_title.png と一致する）
  railLines … 恐竜が走るレール。手前と奥の横レール2本を、左右の縦レール2本でつないだ四角
              （本編と同じ「横レール⇔縦レールの乗り換え」をタイトルで見せるため）

エンジンで動かす物（ロゴ看板 title_board・メニューの的 menu_target）はここには入れない。
それらは TitleScene が自分で持ち、アニメーションさせる。

大きさ：キットは小さなジオラマ用の寸法（恐竜の背丈 0.8m 想定）なので、全体を SCALE 倍して
  本編と同じ大きさ（恐竜 1.5m・道幅 2m）に合わせる。game/title/TitleLayout.h の kScale と同じ値にすること

座標の変換（Blender: Z-up 右手 → エンジン: Y-up 左手）
  位置   (bx, by, bz) → (bx, bz, by) × SCALE
  回転   Z軸まわり rz → Y軸まわり (π - rz)
    モデルは読み込み時に X が反転し、正面が +Z を向く。カメラは -Z 側から +Z を見るので、
    全部を Y軸で半回転させて正面をカメラへ向ける（π）。rz の符号が逆になるのは左右の手系の違い
"""
import json
import math
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
MANIFEST = os.path.join(PROJECT, "resources", "craft", "manifest.json")
OUT = os.path.join(PROJECT, "resources", "map", "title.json")

SCALE = 1.8  # TitleLayout.h の kScale と同じ値

with open(MANIFEST, encoding="utf-8") as f:
    manifest = json.load(f)
BUILT = sorted(a["name"] for a in manifest["assets"])
G = manifest["ground_top"]  # 地面タイルの上面の高さ（ジオラマの寸法）

# --- レール（ジオラマの寸法で書く。書き出す時に SCALE 倍する）---
RAIL_X = 3.0        # 左右の縦レールの位置
RAIL_FRONT = -2.45  # 手前の横レール（メニューの的 by=-1.4 の手前）
RAIL_BACK = 0.9     # 奥の横レール（ロゴ看板 by=2.2 の手前）
ROAD_HALF = 1.0 / SCALE  # 道の半幅（本編の道は幅2m）
ROAD_LIFT = 0.12    # 道の上面を地面から浮かせる高さ(m。エンジンの寸法)

objects = []


def on_road(x, y, margin):
    """(x, y) が道の上（または margin 以内）にあるか。草や花が道に生えないようにする"""
    half = ROAD_HALF + margin
    inside_x = abs(x) <= RAIL_X + half
    inside_y = RAIL_FRONT - half <= y <= RAIL_BACK + half
    if not (inside_x and inside_y):
        return False
    on_horizontal = abs(y - RAIL_FRONT) <= half or abs(y - RAIL_BACK) <= half
    on_vertical = abs(abs(x) - RAIL_X) <= half
    return on_horizontal or on_vertical


def pick(name):
    """craftkit.py の pick と同じ：同名が無ければ、番号つきの最初のバリエーションを使う"""
    if name in BUILT:
        return name
    variants = [k for k in BUILT if k.startswith(name + "_")]
    return variants[0] if variants else None


def place(name, loc, rz=0.0, s=1.0, var=None):
    key = f"{name}_{var:02d}" if var is not None and f"{name}_{var:02d}" in BUILT else name
    model = pick(key)
    if model is None:
        return
    bx, by, bz = loc
    objects.append({
        "type": model,
        "translation": [round(bx * SCALE, 4), round(bz * SCALE, 4), round(by * SCALE, 4)],
        "rotation": [0.0, round(math.pi - math.radians(rz), 5), 0.0],
        "scale": [round(s * SCALE, 4)] * 3,
    })


r = random.Random(42)
for ix in range(-4, 5):
    for iy in range(-1, 2):
        # キットでは中央に紙の小道（ground_tile_path）があるが、本編の道を敷くので全部草のタイルにする
        place("ground_tile_grass", (ix * 2, iy * 2, 0))
    place("ground_block_step", (ix * 2, 4, 0))
place("hill_backdrop", (-2.5, 7.5, 0), var=0)
place("hill_backdrop", (5.0, 8.2, 0), s=1.1, var=1)
place("hill_backdrop_far", (0, 11.5, 0), s=1.3)
place("sky_backdrop", (0, 14.0, 0))
place("cloud_cutout", (-3.6, 9.0, 3.3), var=0)
place("cloud_cutout", (4.2, 10.5, 4.0), s=1.3, var=1)
place("tree_cutout", (-5.2, 4.3, 0.6), var=0)
place("tree_cutout", (5.6, 4.4, 0.6), s=0.85, var=1)
place("tree_round_small", (-5.0, 1.6, G), var=0)   # キットは (-3.4, 1.4)。道に重なるので外側へ
place("tree_round_small", (3.4, 4.0, 0.6), s=1.2, var=1)
for k in range(3):
    place("fence_sticks", (-1.3 + (k * 1.25) - 1.25, 2.9, 0.6))
place("paper_cup", (5.0, 1.2, G))                   # キットは (4.2, 1.2)
place("paper_rock", (-4.6, -0.8, G), var=0)         # キットは (-4.4, -0.8)
place("paper_rock", (5.0, -2.0, G), s=0.6, var=1)   # キットは (2.8, -1.8)
for i in range(18):
    x = r.uniform(-7, 7)
    if abs(x) < 1.0:
        x += 2.0 if x > 0 else -2.0
    y = r.uniform(-2.2, 2.2)
    rz = r.uniform(-30, 30)
    s = r.uniform(0.7, 1.2)
    if not on_road(x, y, 0.35):
        place("grass_tuft", (x, y, G), rz=rz, s=s, var=i % 3)
for i in range(8):
    x = r.uniform(-6.5, 6.5)
    if abs(x) < 1.2:
        x += 2.4 if x > 0 else -2.4
    y = r.uniform(-2.0, 2.0)
    rz = r.uniform(-25, 25)
    s = r.uniform(0.8, 1.2)
    if not on_road(x, y, 0.2):
        place("flower_orange" if i % 3 else "flower_red", (x, y, G), rz=rz, s=s)

# 机の天板（キットには無いので、エンジン側で作る "titleDesk"＝1m角の箱を引き伸ばす。上面が高さ0）
objects.append({
    "type": "titleDesk",
    "translation": [0.0, -0.05, 8.0 * SCALE],
    "rotation": [0.0, 0.0, 0.0],
    "scale": [60.0 * SCALE, 0.1, 40.0 * SCALE],
})


def rail(points):
    height = G * SCALE + ROAD_LIFT
    return [[round(x * SCALE, 4), round(height, 4), round(y * SCALE, 4)] for x, y in points]


# 0=手前（左→右。真ん中のノードがスタート地点）/ 1=右（手前→奥）/ 2=奥（右→左）/ 3=左（奥→手前）
rail_lines = [
    rail([(-RAIL_X, RAIL_FRONT), (0.0, RAIL_FRONT), (RAIL_X, RAIL_FRONT)]),
    rail([(RAIL_X, RAIL_FRONT), (RAIL_X, RAIL_BACK)]),
    rail([(RAIL_X, RAIL_BACK), (0.0, RAIL_BACK), (-RAIL_X, RAIL_BACK)]),
    rail([(-RAIL_X, RAIL_BACK), (-RAIL_X, RAIL_FRONT)]),
]
count = len(rail_lines)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    json.dump({
        "name": "title",
        "objects": objects,
        "railLines": rail_lines,
        "railTypes": [-1] * count,        # -1=向きから自動（横/縦）
        "railLineModes": [1] * count,     # 1=直線でつなぐ
        "railGroundTypes": [0] * count,   # 0=端で止まる（落ちない）
        "startRailIndex": 0,
        "startNodeIndex": 1,
    }, f, ensure_ascii=False, indent=1)
print("wrote", OUT, len(objects), "objects,", count, "rails")
