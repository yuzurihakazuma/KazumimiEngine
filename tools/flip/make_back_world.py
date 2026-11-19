# -*- coding: utf-8 -*-
"""
カメラ反転の「裏の世界」を作る（stage1）。何度流しても同じ結果になる（前に作った分を置き換える）。

  python tools/flip/make_back_world.py

  1. 裏の道（レール22）とつなぐ道（レール23）
     ・レール22 を -x 側へ延ばす（裏から見ると画面の右＝進む向き）。つなぐ道との交差点から右へ約30mのコース
     ・道の見た目：台座つきのクリーム色の布（緑の地面・夕方の色の中で道がはっきり見える）
     ・コース（レール22 の上）：コイン・階段・すり抜け床・どんぐりん・ジャンプ台＋コインの柱・かみばな・？ブロック
       （レール22 の上の物は、流すたびに全部作り直す）
  2. 裏の世界の地面と飾り：project/resources/map/stage1_backdrop_edit.json の "added"（group 7001）
     ・表の地面が切れる所（z≈-1.6）から奥（-z）へ地面を敷く。奥は段々に上げる
     ・飾りは裏のカメラ（+z 側）を向ける：道ばたの草・柵と花・木の林・岩・丘・雲
     ・背景エディタで手直しした物（group 7001 以外）は消さない
"""
import json
import math
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
MAP = os.path.join(PROJECT, "resources", "map", "stage1.json")
BACKDROP = os.path.join(PROJECT, "resources", "map", "stage1_backdrop.json")
EDIT = os.path.join(PROJECT, "resources", "map", "stage1_backdrop_edit.json")

BACK_RAIL, LINK_RAIL = 22, 23
GROUP = 7001
ROAD_Y = 2.290457248687744
ROAD_Z = -4.57568359375
LINK_X = 16.944864440917968
X_FROM, X_TO = -13.56, 21.44      # 裏の道の両端（-x 側が進む先）
GROUND_TOP = ROAD_Y - 0.06        # 地面の上面（道の上面より少し下。make_stage_backdrop.py の GROUND_DROP）
BLOCK_W, BLOCK_H, BLOCK_SY = 2.02, 0.62, 1.8
OVERLAP = 1.005
CREAM = [0.97, 0.9, 0.72]

rng = random.Random(2026)


def rnd(a, b):
    return a + (b - a) * rng.random()


# =====================================================================
#  1. 道とコース
# =====================================================================
level = json.load(open(MAP, encoding="utf-8"))
nodes = []
x = X_FROM
while x < X_TO - 1e-3:
    nodes.append([x, ROAD_Y, ROAD_Z]); x += 2.5
nodes.append([X_TO, ROAD_Y, ROAD_Z])
level["railLines"][BACK_RAIL] = nodes
level["railNodeHoles"][BACK_RAIL] = [0] * len(nodes)
for rail in (BACK_RAIL, LINK_RAIL):
    level["railSkins"][rail] = {"color": CREAM, "curl": 0.035, "layer": 1, "lift": 0.012, "stitch": 1,
                                "style": 2, "width": 2.0}
arcs = [0.0]
for a, b in zip(nodes, nodes[1:]):
    arcs.append(arcs[-1] + math.dist(a, b))
JUNCTION = LINK_X - X_FROM  # つなぐ道が付く所（レール22 の距離）


def d_of(steps_from_junction):
    """交差点から画面の右（-x）へ steps m 進んだ所の、レール22 の距離"""
    return round(JUNCTION - steps_from_junction, 3)


def block(dist, lv, kind):
    anchor = max(k for k, a in enumerate(arcs) if a <= dist + 1e-6)
    return {"anchor": anchor, "anchorOff": dist - arcs[anchor], "dist": float(dist), "level": lv,
            "rail": BACK_RAIL, "side": 0.0, "type": kind}


def enemy(dist, kind, patrol=None):
    e = {"biteRange": -1.0, "bobAmp": 0.0, "bobSpeed": 2.0, "chaseRange": 0.0, "chaseSpeedMul": 1.5,
         "distance": float(dist), "hoverHeight": -1.0, "name": "", "patrol": 0, "patrolMax": -1.0,
         "patrolMin": -1.0, "railIndex": BACK_RAIL, "scale": 1.0, "speed": 0.0, "startDir": -1,
         "turnWait": 0.0, "type": kind}
    if patrol:
        e.update({"patrol": 1, "patrolMin": float(patrol[0]), "patrolMax": float(patrol[1])})
    return e


def coin(dist, height):
    return {"dist": round(float(dist), 3), "height": float(height), "rail": BACK_RAIL}


B = math.floor(JUNCTION)  # 交差点の手前の整数の距離（ブロックは 1m のマス。ここから画面の右＝距離の小さい方へ並べる）
course_blocks, course_coins, course_enemies = [], [], []
# 交差点の先：ようこその3枚
course_coins += [coin(B - k, 0.6) for k in (3, 4, 5)]
# 階段（1段→2段）を登って、すり抜け床の上のコイン
course_blocks += [block(B - 7, 0, 0), block(B - 8, 0, 0), block(B - 8, 1, 0)]
course_blocks += [block(B - k, 1, 6) for k in (10, 11, 12)]
course_coins += [coin(B - k, 2.6) for k in (10, 11, 12)]
# どんぐりん（行ったり来たり）
course_enemies.append(enemy(B - 15.5, 0, (B - 18.0, B - 13.0)))
# ジャンプ台と、その上のコインの柱
course_blocks.append(block(B - 20, 0, 4))
course_coins += [coin(B - 20, h) for h in (2.4, 3.2, 4.0, 4.8)]
# 横長の箱と、その先のかみばな
course_blocks.append(block(B - 23, 0, 7))
course_enemies.append(enemy(B - 25.5, 1))
# 最後：？ブロック（下から頭突き）と、行き止まりの広場の手前のコイン
course_blocks.append(block(B - 27, 2, 5))
course_coins += [coin(B - k, 0.6) for k in (28, 29)]

level["blocks"] = [b for b in level["blocks"] if b["rail"] != BACK_RAIL] + course_blocks
level["coins"] = [c for c in level["coins"] if c["rail"] != BACK_RAIL] + course_coins
level["enemies"] = [e for e in level["enemies"] if e["railIndex"] != BACK_RAIL] + course_enemies
with open(MAP, "w", encoding="utf-8", newline="") as f:  # ゲームの保存と同じ形（CRLF・最後に改行なし）
    f.write(json.dumps(level, ensure_ascii=False, indent=4).replace(chr(10), chr(13) + chr(10)))
print("rail22: %d nodes, %.1fm, junction %.2f / blocks %d coins %d enemies %d"
      % (len(nodes), arcs[-1], JUNCTION, len(course_blocks), len(course_coins), len(course_enemies)))

# =====================================================================
#  2. 地面と飾り（stage1_backdrop_edit.json の added, group 7001）
# =====================================================================
base = json.load(open(BACKDROP, encoding="utf-8"))
grid = base["grid"]
CELL = grid["cell"]
OX, OZ = grid["origin"]
ground = [o for o in base["objects"] if o["model"].startswith("ground_block")]


def scale_of(o):
    s = o.get("scale", 1.0)
    return s if isinstance(s, list) else [s, s, s]


def covered(x, z):
    """表の自動配置の地面（穴の底より上）がもうある所"""
    for o in ground:
        s = scale_of(o)
        if abs(o["pos"][0] - x) <= 1.01 * s[0] and abs(o["pos"][2] - z) <= 1.01 * s[2]:
            if o["pos"][1] + BLOCK_H * s[1] > 0.0:
                return True
    return False


def terrace(x, z):
    """奥（-z）ほど段々に上がる地面の上面"""
    beyond = ROAD_Z - z  # 裏の道から奥への距離
    wave = 0.7 * math.sin(x * 0.31) + 0.35 * math.sin(x * 0.83 + 1.3)
    top = GROUND_TOP
    for at, up in ((6.5, 0.55), (11.5, 1.1), (17.0, 1.65)):
        if beyond > at + wave:
            top = GROUND_TOP + up
    return round(top, 2)


GX0, GX1, GZ0, GZ1 = -30.0, 48.0, -26.0, 0.5
cols = range(int(math.floor((GX0 - OX) / CELL)), int(math.ceil((GX1 - OX) / CELL)))
rows = range(int(math.floor((GZ0 - OZ) / CELL)), int(math.ceil((GZ1 - OZ) / CELL)))
cells = {}
for c in cols:
    for r in rows:
        cx, cz = OX + (c + 0.5) * CELL, OZ + (r + 0.5) * CELL
        if not covered(cx, cz):
            cells[(c, r)] = terrace(cx, cz)

added = []


def ground_tile(c, r, n, top):
    cx, cz = OX + (c + n * 0.5) * CELL, OZ + (r + n * 0.5) * CELL
    s = n * CELL / BLOCK_W * OVERLAP
    added.append({"model": "ground_block_step", "pos": [round(cx, 3), round(top - BLOCK_H * BLOCK_SY, 3), round(cz, 3)],
                  "rot": [0.0, 0.0, 0.0], "scale": [round(s, 3), BLOCK_SY, round(s, 3)], "group": GROUP})


used = set()
for (c, r), top in sorted(cells.items()):
    if (c, r) in used:
        continue
    for n in (3, 2, 1):  # 同じ高さの n×n がそろう所は大きいブロック1つ
        block_cells = [(c + i, r + j) for i in range(n) for j in range(n)]
        if all(k in cells and k not in used and cells[k] == top for k in block_cells):
            used.update(block_cells)
            ground_tile(c, r, n, top)
            break


def ground_top_at(x, z):
    c, r = int(math.floor((x - OX) / CELL)), int(math.floor((z - OZ) / CELL))
    return cells.get((c, r), GROUND_TOP)


def put(model, x, z, scale, yaw=0.0, y=None):
    """裏のカメラ（+z）を向けて置く。yaw は 0＝裏のカメラの方"""
    added.append({"model": model, "pos": [round(x, 3), round(ground_top_at(x, z) if y is None else y, 3), round(z, 3)],
                  "rot": [0.0, round(yaw, 1), 0.0], "scale": [round(scale, 3)] * 3, "group": GROUP})


def near_road(x, z, clear):
    on_back = X_FROM - 1.5 <= x <= X_TO + 1.5 and abs(z - ROAD_Z) < clear
    on_link = abs(x - LINK_X) < clear and z > ROAD_Z - 1.5
    return on_back or on_link


# 道ばたの草むら（手前は低く小さく・奥は大きく）
x = X_FROM - 1.0
while x < X_TO + 1.0:
    if abs(x - LINK_X) > 2.2:
        for _ in range(rng.choice((1, 2, 2, 3))):
            put(rng.choice(("grass_tuft_00", "grass_tuft_01", "grass_tuft_02")), x + rnd(-0.5, 0.5),
                ROAD_Z - rnd(1.35, 1.8), rnd(1.3, 1.85), rnd(-25, 25))
        if rng.random() < 0.6:
            put(rng.choice(("grass_tuft_00", "grass_tuft_02")), x + rnd(-0.5, 0.5), ROAD_Z + rnd(1.5, 2.2),
                rnd(0.8, 1.15), rnd(-25, 25))
    x += rnd(1.4, 2.6)

# 柵（奥側の道ばた。ところどころ抜き、交差点の近くに看板）と、柵ぎわの花だん
x = X_FROM + 0.5
while x < X_TO:
    if abs(x - LINK_X) > 2.6 and rng.random() < 0.8:
        put("fence_sticks", x, ROAD_Z - 1.8, 1.8)
        for _ in range(rng.choice((0, 1, 2))):
            put(rng.choice(("flower_orange", "flower_white", "flower_red")), x + rnd(-1.0, 1.0),
                ROAD_Z - rnd(2.1, 2.5), rnd(1.3, 1.8), rnd(-20, 20))
    x += 2.4
put("sign_arrow", LINK_X - 3.0, ROAD_Z - 1.75, 1.8, 0.0)  # 交差点の先：→（表の看板を裏へ向けると、裏から見て右＝進む向きを指す）

# 木の林（大きい木＋小さい木＋根元の草むら・岩）。2列目は上の段に
x = X_FROM - 12.0
while x < X_TO + 22.0:
    z = ROAD_Z - rnd(8.5, 10.5)
    if not near_road(x, z, 2.0):
        put(rng.choice(("tree_cutout_00", "tree_cutout_01", "tree_cutout_02")), x, z, rnd(1.8, 2.4), rnd(-10, 10))
        put(rng.choice(("tree_round_small_00", "tree_round_small_01")), x + rnd(1.4, 2.2), z + rnd(0.6, 1.4),
            rnd(1.6, 1.95), rnd(-10, 10))
        put(rng.choice(("grass_tuft_00", "grass_tuft_01")), x + rnd(-1.0, 1.0), z + rnd(0.8, 1.4), rnd(1.4, 1.9))
        if rng.random() < 0.35:
            put(rng.choice(("paper_rock_00", "paper_rock_01")), x - rnd(1.2, 2.0), z + rnd(0.5, 1.5), rnd(1.0, 1.35),
                rnd(0, 360))
    x += rnd(5.5, 8.0)
x = X_FROM - 8.0
while x < X_TO + 18.0:
    z = ROAD_Z - rnd(13.5, 16.0)
    put(rng.choice(("tree_cutout_00", "tree_cutout_01", "tree_cutout_02", "tree_round_small_00")), x, z,
        rnd(2.0, 2.5), rnd(-10, 10))
    x += rnd(4.5, 7.0)

# 空き地の花畑（道と林の間）
for _ in range(14):
    x, z = rnd(X_FROM - 4.0, X_TO + 6.0), ROAD_Z - rnd(3.2, 6.5)
    if near_road(x, z, 2.4):
        continue
    for _ in range(rng.choice((2, 3))):
        put(rng.choice(("flower_orange", "flower_white", "flower_red")), x + rnd(-0.6, 0.6), z + rnd(-0.5, 0.5),
            rnd(1.2, 1.7), rnd(-20, 20))

# 丘（近い丘・遠い丘）と雲。裏のカメラから見える奥（-z）に、ステージの幅いっぱい
x = X_FROM - 30.0
while x < X_TO + 40.0:
    put(rng.choice(("hill_backdrop_00", "hill_backdrop_01")), x, ROAD_Z - rnd(23.0, 26.0), rnd(2.55, 2.9), 0.0,
        y=round(rnd(1.6, 2.4), 2))
    x += rnd(15.0, 19.0)
x = X_FROM - 36.0
while x < X_TO + 46.0:
    put("hill_backdrop_far", x, ROAD_Z - rnd(32.0, 36.0), rnd(3.0, 3.3), 0.0, y=round(rnd(1.4, 2.2), 2))
    x += 32.0
x = X_FROM - 30.0
while x < X_TO + 40.0:
    put(rng.choice(("cloud_cutout_00", "cloud_cutout_01")), x, ROAD_Z - rnd(30.0, 38.0), rnd(2.2, 2.8), 0.0,
        y=round(rnd(9.0, 12.5), 2))
    x += rnd(16.0, 24.0)

edits = {"added": [], "removed": [], "moved": []}
if os.path.exists(EDIT):
    edits = json.load(open(EDIT, encoding="utf-8"))
    edits["added"] = [a for a in edits.get("added", []) if a.get("group", 0) != GROUP]
edits["added"] += added
json.dump(edits, open(EDIT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
kinds = {}
for a in added:
    kinds[a["model"]] = kinds.get(a["model"], 0) + 1
print("backdrop edit: %d added (group %d)" % (len(added), GROUP), kinds)
