# -*- coding: utf-8 -*-
"""
本編ステージの箱庭（resources/stage/<名前>.stage.json）に、クラフトの背景を敷く下書きを作る。

  python tools/craftkit/make_stage_backdrop.py stage1 [--force]

  ・机の天板（一番低い道のすぐ下。落ちて天板に着いたらやり直し）
  ・空（カメラにぴったり付いてくる：parallax 1.0）
  ・遠い山（ゆっくり：0.8）・近い山（もっとゆっくり：0.6）・雲（0.9）
    → カメラより遅く動くので、横へ進むと奥行きが出る
  背景の追従の基準（environment.backdropAnchor）は、スタート地点に立った時のプレイ中カメラの位置。
  マップ（resources/map/<名前>.json）のレールの範囲から、背景を並べる範囲を決める。
  既にステージのファイルがある時は止まる（--force で上書き。上書きすると手で置いた物も消える）
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))

name = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else "stage1"
force = "--force" in sys.argv
map_path = os.path.join(PROJECT, "resources", "map", name + ".json")
stage_path = os.path.join(PROJECT, "resources", "stage", name + ".stage.json")
if os.path.exists(stage_path) and not force:
    sys.exit("ステージのファイルが既にあります（上書きするなら --force）: " + stage_path)

with open(map_path, encoding="utf-8") as f:
    level = json.load(f)
points = [p for line in level["railLines"] for p in line]
min_x = min(p[0] for p in points); max_x = max(p[0] for p in points)
min_y = min(p[1] for p in points)
min_z = min(p[2] for p in points); max_z = max(p[2] for p in points)
start = level["railLines"][level.get("startRailIndex", 0)][level.get("startNodeIndex", 0)]

# プレイ中カメラの基準（PlayCameraController：プレイヤーの後ろ 10m・上 3.5m）
anchor = [start[0], start[1] + 3.5, start[2] - 10.0]
travel = max_x - min_x  # 横へ進む距離（この分だけ、遅く動く背景が横へずれる）

rng = random.Random(7)
objects = []


def add(asset, pos, scale, parallax=None, yaw=180.0):
    obj = {"id": "%016x" % rng.getrandbits(64), "asset": asset, "layer": "backdrop",
           "pos": [round(v, 3) for v in pos], "rot": [0.0, yaw, 0.0], "scale": [scale] * 3}
    if parallax:
        obj["params"] = {"parallax": parallax}
    objects.append(obj)


ax, ay, az = anchor
# 机の天板：一番低い道のすぐ下（道の厚み 0.25m＋すき間）。カメラには付いてこない。
#   道が机から浮きすぎると、どこを走っているか分かりにくいので近づける。
#   天板の高さは environment.floorY にも書き、プレイヤーは天板に落ちた時点でやり直しになる
desk_y = round(min_y - 0.7, 3)
add("craftDesk", [(min_x + max_x) * 0.5, desk_y - 0.1, (min_z + max_z) * 0.5 + 40.0], 1.0, yaw=0.0)
objects[-1]["scale"] = [travel + 300.0, 0.2, (max_z - min_z) + 260.0]

# 空：カメラから 100m 奥。カメラにぴったり付いてくる
add("sky_backdrop", [ax, desk_y, az + 100.0], 4.0, parallax=1.0)

# 遠い山：85m 奥（天板の上に立つので、空を隠しすぎない大きさ）。0.8 倍で追従（横へ進むと少しずつ流れる）
far_lag = travel * 0.2
x = ax - 70.0
while x < ax + far_lag + 80.0:
    add("hill_backdrop_far", [x, desk_y, az + 85.0], 3.4, parallax=0.8)
    x += 46.0

# 近い山：55m 奥。0.6 倍で追従（遠い山より速く流れる＝奥行き）
near_lag = travel * 0.4
x = ax - 60.0
index = 0
while x < ax + near_lag + 70.0:
    add("hill_backdrop_00" if index % 2 == 0 else "hill_backdrop_01", [x, desk_y, az + 62.0], 2.3 + 0.3 * (index % 3), parallax=0.6)
    x += 17.0
    index += 1

# 雲：90m 奥・高め。0.9 倍
cloud_lag = travel * 0.1
x = ax - 40.0
index = 0
while x < ax + cloud_lag + 60.0:
    add("cloud_cutout_00" if index % 2 == 0 else "cloud_cutout_01", [x, ay + rng.uniform(4.0, 9.0), az + 90.0], 4.0, parallax=0.9)
    x += rng.uniform(35.0, 55.0)
    index += 1

objects.sort(key=lambda o: o["id"])
os.makedirs(os.path.dirname(stage_path), exist_ok=True)
with open(stage_path, "w", encoding="utf-8") as f:
    json.dump({"version": 1, "name": name,
               "environment": {"lighting": "day", "backdrop": "meadow", "backdropAnchor": [round(v, 3) for v in anchor], "floorY": desk_y},
               "objects": objects, "markers": []}, f, ensure_ascii=False, indent=2)
print("wrote", stage_path, len(objects), "objects / anchor", [round(v, 2) for v in anchor], "/ travel %.0fm" % travel)
