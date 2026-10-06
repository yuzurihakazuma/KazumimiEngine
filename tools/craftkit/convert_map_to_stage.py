# -*- coding: utf-8 -*-
"""
エディタのマップ（resources/map/*.json）に置いてある物を、箱庭のステージ（resources/stage/*.stage.json）へ移す。

  python tools/craftkit/convert_map_to_stage.py title

  ・マップの objects をステージの objects へ（id を振り、回転はラジアン → 度、層は manifest の分類から）
  ・移した物はマップから消す（レールなど、それ以外の中身はそのまま）
  ・ステージのファイルが既にある時は、上書きせずに止まる（--force で上書き）
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
MANIFEST = os.path.join(PROJECT, "resources", "craft", "manifest.json")

name = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else "title"
force = "--force" in sys.argv
map_path = os.path.join(PROJECT, "resources", "map", name + ".json")
stage_path = os.path.join(PROJECT, "resources", "stage", name + ".stage.json")

if os.path.exists(stage_path) and not force:
    sys.exit("ステージのファイルが既にあります（上書きするなら --force）: " + stage_path)

with open(MANIFEST, encoding="utf-8") as f:
    categories = {a["name"]: a["category"] for a in json.load(f)["assets"]}
with open(map_path, encoding="utf-8") as f:
    level = json.load(f)


def layer_of(asset):
    category = categories.get(asset, "builtin")
    if category == "ground":
        return "ground"
    if category in ("backdrop", "builtin"):
        return "backdrop"
    return "decor"


def r4(values):
    return [round(v, 4) for v in values]


rng = random.Random()
objects = []
for obj in level.get("objects", []):
    objects.append({
        "id": "%016x" % rng.getrandbits(64),
        "asset": obj["type"],
        "layer": layer_of(obj["type"]),
        "pos": r4(obj.get("translation", [0, 0, 0])),
        "rot": r4([math.degrees(v) for v in obj.get("rotation", [0, 0, 0])]),
        "scale": r4(obj.get("scale", [1, 1, 1])),
    })
objects.sort(key=lambda o: o["id"])

os.makedirs(os.path.dirname(stage_path), exist_ok=True)
with open(stage_path, "w", encoding="utf-8") as f:
    json.dump({"version": 1, "name": name, "environment": {"lighting": "day", "backdrop": "meadow"},
               "objects": objects, "markers": []}, f, ensure_ascii=False, indent=2)

level["objects"] = []
with open(map_path, "w", encoding="utf-8") as f:
    json.dump(level, f, ensure_ascii=False, indent=1)
print("moved", len(objects), "objects:", map_path, "->", stage_path)
