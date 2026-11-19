# -*- coding: utf-8 -*-
"""かくれ花が隠れている木の一覧（game/egg/EggCombo.cpp と同じ決め方）。撮影やステージ作りで場所を探す用。

  python tools/hud/find_hidden_flowers.py [stage1]
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAP = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "map"))
stage = sys.argv[1] if len(sys.argv) > 1 else "stage1"
M = 0xFFFFFFFF


def hash32(x):
    x ^= x >> 16; x = (x * 0x7feb352d) & M; x ^= x >> 15; x = (x * 0x846ca68b) & M; x ^= x >> 16
    return x


def lround(v):
    return int(v + 0.5) if v >= 0 else -int(-v + 0.5)


items = json.load(open(os.path.join(MAP, stage + "_backdrop.json"), encoding="utf-8"))["objects"]
edit = os.path.join(MAP, stage + "_backdrop_edit.json")
if os.path.exists(edit):
    items += json.load(open(edit, encoding="utf-8")).get("added", [])
found = []
for item in items:
    if not item["model"].startswith("tree_"):
        continue
    x, z = item["pos"][0], item["pos"][2]
    h = hash32(((lround(x * 10) * 73856093) & M) ^ ((lround(z * 10) * 19349663) & M))
    if h % 5 == 0:
        found.append((round(x, 2), round(item["pos"][1], 2), round(z, 2), item["model"]))
for f in sorted(found):
    print(f)
print(len(found), "flowers")
