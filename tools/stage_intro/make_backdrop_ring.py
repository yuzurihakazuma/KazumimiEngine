# -*- coding: utf-8 -*-
"""ステージ全体を、段ボールの書き割りでぐるりと囲む（ジオラマの舞台。今までの奥の空の板を、四方へ広げる）。

  python tools/stage_intro/make_backdrop_ring.py [stage1]

  ・今ある背景（project/resources/map/<ステージ>_backdrop.json）に、囲いの物を足す。前に足した囲いの物（"ring": true）は消してから足し直す。
    ほかの物（地面・飾り・奥の丘・空の板）には触らない（背景エディタの手直し <ステージ>_backdrop_edit.json がずれないように）
  ・囲い：奥の辺は今の空の板のまま。手前・左・右の 3 辺に、空の板（200m×80m）を内側へ向けて立てる。
          どの辺も、裏の世界（背景エディタで足した物）を含めた全部の物より外側（道と囲いの間に何も挟まらない）
  ・辺ごとの景色：空の板の手前に、遠い丘・近い丘・雲を並べる（奥の辺と同じ作り。地面が無い所なので、すそは段ボールの底から）
  ・手前の辺だけ夕方（カメラ反転で裏から見た時に見える辺。表で遊んでいる時には見えない）：
          夕焼けの空の板・暮れた丘・夕焼けの雲（make_evening_assets.py の _evening / _dusk）と、裏の世界の正面に沈む紙の太陽
  ・底：囲いの内側の全部に、段ボールの底（ブロックを裏返して、段ボールの面を上に。今の底と同じ高さ・同じ升目）
  make_stage_backdrop.py で背景を作り直した時は、この後にもう一度流す
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
STAGE = sys.argv[1] if len(sys.argv) > 1 else "stage1"
BASE = os.path.join(PROJECT, "resources", "map", STAGE + "_backdrop.json")
EDIT = os.path.join(PROJECT, "resources", "map", STAGE + "_backdrop_edit.json")
MAP = os.path.join(PROJECT, "resources", "map", STAGE + ".json")

MARGIN = 35.0         # 全部の物の外側から、囲いの空の板までの距離(m)
FLOOR_Y = -11.5       # 段ボールの底の高さ（make_stage_backdrop.py と同じ）
FLOOR_TILE = 12.0
BLOCK_W = 2.02
SKY_STEP = 90.0       # 空の板（幅 200m）を並べる間（make_stage_backdrop.py と同じ。重ねて継ぎ目を見せない）
SKY_SCALE = [5.0, 8.0, 5.0]   # 高さ 80m（-20〜60）
SKY_Y = -20.0
SUN_MODEL = "goal_sun"        # 夕日（クリアの太陽と同じ紙の太陽。高さ 4.4m・幅 4.2m）
SUN_SCALE = 6.0
SUN_Y = -6.0                  # 下半分を丘に沈める
SUN_INSET = 4.0               # 空の板から内側へ(m)（遠い丘より奥）
BACK_GROUP = 7001             # 裏の世界（make_back_world.py）。夕日はこの真ん中の正面に置く


def evening(model):
    """手前の辺（裏から見える側）の夕方の物"""
    if model == "sky_backdrop":
        return "sky_backdrop_evening"
    if model.startswith("hill_backdrop"):
        return model + "_dusk"
    if model.startswith("cloud_cutout"):
        return model + "_evening"
    return model


def main():
    base = json.load(open(BASE, encoding="utf-8"))
    objects = [o for o in base["objects"] if not o.get("ring")]
    removed = len(base["objects"]) - len(objects)

    # 中身の広がり：道・今の背景（底と奥の空の板は除く）・背景エディタで足した物（裏の世界など）
    level = json.load(open(MAP, encoding="utf-8"))
    points = [(p[0], p[2]) for line in level["railLines"] for p in line]
    for o in objects:
        if o["model"] == "sky_backdrop" or (o["model"] == "ground_block_step" and o["pos"][1] < FLOOR_Y + 0.5):
            continue
        points.append((o["pos"][0], o["pos"][2]))
    if os.path.exists(EDIT):
        for o in json.load(open(EDIT, encoding="utf-8")).get("added", []):
            points.append((o["pos"][0], o["pos"][2]))
    min_x = min(p[0] for p in points) - MARGIN
    max_x = max(p[0] for p in points) + MARGIN
    min_z = min(p[1] for p in points) - MARGIN
    far = [o for o in objects if o["model"] == "sky_backdrop"]
    max_z = min(o["pos"][2] for o in far) if far else max(p[1] for p in points) + MARGIN  # 奥の辺＝今の空の板
    print("囲い x %.1f〜%.1f / z %.1f〜%.1f" % (min_x, max_x, min_z, max_z))

    rng = random.Random(77)
    ring = []

    def add(model, pos, scale, rot_y):
        s = scale if isinstance(scale, list) else [scale] * 3
        ring.append({"model": model, "pos": [round(v, 3) for v in pos], "rotY": round(rot_y, 1),
                     "scale": [round(v, 3) for v in s], "ring": True})

    # 辺：(始まりの点, 終わりの点, 内側へ向く rotY, 内側の向き, 夕方か)
    sides = [
        ((min_x - 100, min_z), (max_x + 100, min_z), 0.0, (0.0, 1.0), True),     # 手前（内側＝+Z）。裏から見える＝夕方
        ((min_x, min_z - 20), (min_x, max_z + 20), 90.0, (1.0, 0.0), False),    # 左（内側＝+X）
        ((max_x, min_z - 20), (max_x, max_z + 20), -90.0, (-1.0, 0.0), False),  # 右（内側＝-X）
    ]
    for (ax, az), (bx, bz), rot, (nx, nz), dusk in sides:
        first = len(ring)
        length = math.hypot(bx - ax, bz - az)
        tx, tz = (bx - ax) / length, (bz - az) / length
        # 空の板：1 枚おきに少しだけ内側へ（重なった所がちらつかない）
        count = max(1, int(math.ceil(length / SKY_STEP)) + 1)
        for i in range(count):
            d = min(i * SKY_STEP, length)
            inset = 0.6 * (i % 2)
            add("sky_backdrop", [ax + tx * d + nx * inset, SKY_Y, az + tz * d + nz * inset], SKY_SCALE, rot)
        # 遠い丘（板の 8m 内側）・近い丘（18m 内側）・雲（5m 内側）。すそは段ボールの底から
        d = rng.uniform(0, 20)
        while d < length:
            add("hill_backdrop_far", [ax + tx * d + nx * 8, FLOOR_Y - 0.5, az + tz * d + nz * 8], rng.uniform(5.5, 6.5), rot)
            d += rng.uniform(55, 70)
        d = rng.uniform(0, 30)
        while d < length:
            add(rng.choice(["hill_backdrop_00", "hill_backdrop_01"]), [ax + tx * d + nx * 18, FLOOR_Y - 0.5, az + tz * d + nz * 18],
                rng.uniform(5.0, 6.0), rot)
            d += rng.uniform(32, 42)
        d = rng.uniform(0, 30)
        while d < length:
            add(rng.choice(["cloud_cutout_00", "cloud_cutout_01"]), [ax + tx * d + nx * 5, rng.uniform(14, 24), az + tz * d + nz * 5],
                rng.uniform(4.0, 5.5), rot)
            d += rng.uniform(40, 60)
        if dusk:
            for o in ring[first:]:
                o["model"] = evening(o["model"])

    # 夕日：裏の世界の真ん中の正面（手前の辺）。無ければ道の真ん中
    edit = json.load(open(EDIT, encoding="utf-8")) if os.path.exists(EDIT) else {}
    back = [o["pos"][0] for o in edit.get("added", []) if o.get("group") == BACK_GROUP]
    sun_x = (min(back) + max(back)) / 2 if back else (min(p[0] for p in points) + max(p[0] for p in points)) / 2
    add(SUN_MODEL, [sun_x, SUN_Y, min_z + SUN_INSET], SUN_SCALE, 0.0)

    # 底：今の底と同じ升目で、囲いの内側の足りない所だけ
    floor = [o for o in objects if o["model"] == "ground_block_step" and o["pos"][1] < FLOOR_Y + 0.5]
    have = {(round(o["pos"][0], 1), round(o["pos"][2], 1)) for o in floor}
    ox, oz = (floor[0]["pos"][0], floor[0]["pos"][2]) if floor else (0.0, 0.0)
    kx0, kx1 = math.floor((min_x - ox) / FLOOR_TILE), math.ceil((max_x - ox) / FLOOR_TILE)
    kz0, kz1 = math.floor((min_z - oz) / FLOOR_TILE), math.ceil((max_z - oz) / FLOOR_TILE)
    added_floor = 0
    for kx in range(kx0, kx1 + 1):
        for kz in range(kz0, kz1 + 1):
            cx, cz = ox + kx * FLOOR_TILE, oz + kz * FLOOR_TILE
            if (round(cx, 1), round(cz, 1)) in have:
                continue
            s = FLOOR_TILE / BLOCK_W * 1.005
            ring.append({"model": "ground_block_step", "pos": [round(cx, 3), FLOOR_Y, round(cz, 3)], "rotY": 180.0,
                         "scale": [round(s, 3), 1.0, round(s, 3)], "rot": [180.0, 180.0, 0.0], "ring": True})
            added_floor += 1

    base["objects"] = objects + ring
    with open(BASE, "w", encoding="utf-8") as f:
        json.dump(base, f, ensure_ascii=False, separators=(",", ":"))
    counts = {}
    for o in ring:
        counts[o["model"]] = counts.get(o["model"], 0) + 1
    print("囲いの物", len(ring), counts, "（底の足し", added_floor, "／前の囲いを消した", removed, "）")


if __name__ == "__main__":
    main()
