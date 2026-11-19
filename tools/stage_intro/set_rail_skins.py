# -*- coding: utf-8 -*-
"""
マップのレールごとに「道の見た目」（布ロード：railSkins）を、背景の地面に合わせて決める。

  python tools/stage_intro/set_rail_skins.py [stage1] [--keep]

  ・真下に背景の地面（resources/map/<stage>_backdrop.json の地面のブロック）がある道 … 布のみ（地面の上に敷く）
  ・地面が無い所を通る道（宙・坂で浮いている所・穴の上） / 動く道 / 後から出る道 … 布＋土台
  ・見えないレール・道なしのレール … 見せない（移動はできる）
  --keep を付けると、すでに railSkins があるレールの設定は変えない（新しいレールだけ決める）
  背景や道を直したら、もう一度流せば合う（エディタの「道の見た目」で1本ずつ変えてもよい）
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
args = [a for a in sys.argv[1:] if not a.startswith("--")]
STAGE = args[0] if args else "stage1"
KEEP = "--keep" in sys.argv
MAP = os.path.join(PROJECT, "resources", "map", STAGE + ".json")
BACKDROP = os.path.join(PROJECT, "resources", "map", STAGE + "_backdrop.json")

BLOCK_W, BLOCK_H, OVERLAP = 2.02, 0.62, 1.005
DEFAULT = {"style": 0, "width": 1.8, "lift": 0.012, "curl": 0.035, "color": [0.95, 0.55, 0.28], "stitch": 1, "layer": 0}

level = json.load(open(MAP, encoding="utf-8"))
lines = level["railLines"]


def at(key, i, default):
    values = level.get(key, [])
    return values[i] if i < len(values) else default


# 背景の地面のブロック（上面の高さと、上から見た四角）
grounds = []
if os.path.exists(BACKDROP):
    for o in json.load(open(BACKDROP, encoding="utf-8")).get("objects", []):
        if not o["model"].startswith("ground_block") or abs(o.get("rot", [0, 0, 0])[0]) > 1.0:
            continue
        sx, sy, sz = o["scale"] if isinstance(o["scale"], list) else [o["scale"]] * 3
        hx, hz = sx * BLOCK_W / OVERLAP / 2, sz * BLOCK_W / OVERLAP / 2
        x, y, z = o["pos"]
        grounds.append((x - hx, x + hx, z - hz, z + hz, y + BLOCK_H * sy))


def ground_top(x, z):
    tops = [g[4] for g in grounds if g[0] <= x <= g[1] and g[2] <= z <= g[3]]
    return max(tops) if tops else None


def samples(line, step=0.5):
    for a, b in zip(line, line[1:]):
        n = max(1, int(math.dist(a, b) / step))
        for k in range(n):
            t = k / n
            yield [a[j] + (b[j] - a[j]) * t for j in range(3)]
    yield line[-1]


old = level.get("railSkins", [])
skins = []
for i, line in enumerate(lines):
    skin = dict(DEFAULT)
    if KEEP and i < len(old):
        skins.append(old[i])
        continue
    moving = at("railMotionTypes", i, 0) != 0 or any(abs(v) > 1e-6 for v in list(at("railMotions", i, [0, 0, 0, 2]))[:3])
    if len(line) < 2 or at("railVisible", i, 1) == 0 or at("railRoadModes", i, 0) != 0:
        skin["style"] = 3
        why = "見えない・道なし"
    elif moving or at("railAppearTriggers", i, -1) >= 0 or any(at("railNodeHoles", i, [])):
        skin["style"] = 2
        why = "動く・後から出る・穴あり"
    else:
        pts = list(samples(line))
        ok = 0
        for p in pts:
            top = ground_top(p[0], p[2])
            if top is not None and -0.2 <= top - p[1] <= 0.02:
                ok += 1
        ratio = ok / max(1, len(pts))
        skin["style"] = 1 if ratio >= 0.9 else 2
        why = "地面の上 %.0f%%" % (ratio * 100)
    skins.append(skin)
    print("レール%d: %s（%s）" % (i, ["土台のみ", "布のみ", "布＋土台", "見せない"][skin["style"]], why))


# ファイルの書き方（ゲームの保存と同じ：4字下げ・CRLF・キーは名前順）はそのままに、"railSkins" だけを差し替える
raw = open(MAP, "rb").read().decode("utf-8")
nl = "\r\n" if "\r\n" in raw else "\n"
text = raw.replace("\r\n", "\n")
block = json.dumps(skins, ensure_ascii=False, indent=4).replace("\n", "\n    ")
entry = '    "railSkins": ' + block + ",\n"
rows = text.split("\n")
start = next((k for k, row in enumerate(rows) if row.startswith('    "railSkins":')), None)
if start is not None:
    # 既にある：次のキー（4字下げで始まる行）の手前まで消す
    end = start + 1
    while end < len(rows) and not rows[end].startswith('    "'):
        end += 1
    del rows[start:end]
    text = "\n".join(rows)
later = [row for row in text.split("\n") if row.startswith('    "') and row[5:].split('"')[0] > "railSkins"]
anchor = later[0] + "\n"
text = text.replace(anchor, entry + anchor, 1)
json.loads(text)  # 壊していないか
open(MAP, "wb").write(text.replace("\n", nl).encode("utf-8"))
print("wrote", MAP)
