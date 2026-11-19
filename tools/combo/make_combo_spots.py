# -*- coding: utf-8 -*-
"""
卵のコンボの「コンボの場所」を作る（stage1）。何度流しても同じ結果（前に作った分を置き換える）。

  python tools/combo/make_combo_spots.py            作って書き出す
  python tools/combo/make_combo_spots.py --check    書き出さずに、はじける順だけ調べる

  1つの場所 = 手前に「コンボの的」(combo_target) ＋ 奥へ3本の枝
    ・真ん中に当てる → まっすぐ奥の枝（中くらい）
    ・左のふちに当てる → 右の枝（いちばん長い。最後の木に、かくれ花が必ず隠れている）
    ・右のふちに当てる → 左の枝（短い）
  ゲームの決め方（game/egg/EggCombo.cpp の PickNext・BounceDir と同じ）でまねて、
  どこに当てると何コンボになるかを確かめてから書く。うまくつながらない場所は止まって知らせる。
  書く所：project/resources/map/stage1_backdrop_edit.json
    "added"   … group 7100 の物（前に作った 7100 は消してから足す）
    "removed" … 場所の中にあった自動配置の木・柵・岩・看板・花（"by": "combo_spots" の印付き。流し直す時は印の物を消してから足す）
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
RES = os.path.join(PROJECT, "resources")
BASE = os.path.join(RES, "map", "stage1_backdrop.json")
EDIT = os.path.join(RES, "map", "stage1_backdrop_edit.json")
GROUP = 7100
BLOCK_H = 0.62
ROAD_Z = 7.42

# ゲームと同じ値（EggCombo.cpp）
REACH, CONE, SPREAD, MAX_COMBO = 9.0, 60.0, 0.8, 8
MOMENTUM = 0.9  # はじけるたびに残る勢い（届く距離が 1 割ずつ縮む）

# --------------------------------------------------------------------- 場所の作り
#  origin＝的の位置（x, z）、road＝プレイヤーが立って投げる所（x, z）
#  枝は (曲がる角度°, 進むm, モデル, 大きさ) の並び。最初の向きは的から ±40° / 0°
LONG = [(0, 4.2, "fence_sticks", 1.8), (10, 4.0, "flower_red", 1.7), (8, 4.0, "paper_rock_01", 1.3),
        (6, 4.0, "fence_sticks", 1.8), (4, 4.0, "flower_white", 1.7), (2, 4.0, "paper_rock_00", 1.3),
        (0, 4.2, "tree_round_small_00", 1.7)]  # いちばん長い枝（x8。最後の木にかくれ花）
SPOTS = [
    # はじまり：真ん中＝x4 / 左のふち＝長い右の枝 x8 / 右のふち＝短い左の枝 x3
    {"name": "start", "origin": (-42.0, 10.6), "road": (-42.0, ROAD_Z), "angles": (-50.0, 0.0, 50.0),
     "center": [(0, 3.8, "fence_sticks", 1.8), (6, 3.3, "paper_rock_00", 1.3), (-6, 3.3, "sign_arrow", 1.8)],
     "right": LONG,
     "left": [(0, 4.2, "flower_orange", 1.7), (-10, 4.0, "fence_sticks", 1.8)]},
    # まんなか：真ん中＝x5（最後は木）/ 左右のふち＝x3
    {"name": "middle", "origin": (2.0, 10.6), "road": (2.0, ROAD_Z), "angles": (-45.0, 0.0, 45.0),
     "center": [(0, 3.4, "paper_rock_00", 1.3), (-8, 3.0, "fence_sticks", 1.8), (8, 3.0, "flower_red", 1.7),
                (0, 2.8, "tree_round_small_01", 1.6)],
     "right": [(0, 4.2, "fence_sticks", 1.8), (10, 4.0, "flower_orange", 1.7)],
     "left": [(0, 4.2, "flower_white", 1.7), (-10, 4.0, "sign_arrow", 1.8)]},
    # 丘の道（レール5）：はじまりと同じ形
    {"name": "hill", "origin": (49.0, 16.4), "road": (49.0, 13.2), "angles": (-50.0, 0.0, 50.0),
     "center": [(0, 3.8, "fence_sticks", 1.8), (8, 3.3, "flower_red", 1.7), (-6, 3.3, "paper_rock_00", 1.3)],
     "right": LONG,
     "left": [(0, 4.2, "fence_sticks", 1.8), (-10, 4.0, "flower_red", 1.7)]},
]
# angles＝(左の枝, 真ん中の枝, 右の枝) の最初の向き（°。的から見て。+ は右＝+x 側。表のカメラから見て右）


# --------------------------------------------------------------------- 物の形
def model_bounds(name, cache={}):
    if name in cache:
        return cache[name]
    g = json.load(open(os.path.join(RES, "craft", "models", name + ".gltf"), encoding="utf-8"))
    lo, hi = [1e9] * 3, [-1e9] * 3
    for mesh in g["meshes"]:
        for prim in mesh["primitives"]:
            a = g["accessors"][prim["attributes"]["POSITION"]]
            for i in range(3):
                lo[i] = min(lo[i], a["min"][i]); hi[i] = max(hi[i], a["max"][i])
    cache[name] = (lo, hi)
    return lo, hi


def item_rot_y(item):
    if "rot" in item:
        return item["rot"][1]
    return item.get("rotY", 180.0)


def item_scale(item):
    s = item.get("scale", 1.0)
    return s if isinstance(s, list) else [s, s, s]


def world_box(item):
    lo, hi = model_bounds(item["model"])
    s = item_scale(item)
    yaw = math.radians(item_rot_y(item))
    xs, ys, zs = [], [], []
    for cx in (lo[0], hi[0]):
        for cy in (lo[1], hi[1]):
            for cz in (lo[2], hi[2]):
                x, y, z = cx * s[0], cy * s[1], cz * s[2]
                # 行ベクトル × RotY（ゲームの MakeAffine と同じ向き）
                rx = x * math.cos(yaw) - z * math.sin(yaw)
                rz = x * math.sin(yaw) + z * math.cos(yaw)
                p = item["pos"]
                xs.append(rx + p[0]); ys.append(y + p[1]); zs.append(rz + p[2])
    return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))


def kind(model):
    if model == "combo_target": return "bullseye"
    if model.startswith("tree_"): return "tree"
    if model == "fence_sticks": return "fence"
    if model.startswith("paper_rock_"): return "rock"
    if model == "sign_arrow": return "sign"
    if model.startswith("flower_"): return "flower"
    return None


def target_of(item):
    k = kind(item["model"])
    if not k:
        return None
    lo, hi = world_box(item)
    size = [hi[i] - lo[i] for i in range(3)]
    tree = k == "tree"
    center = ((lo[0] + hi[0]) / 2, lo[1] + size[1] * (0.62 if tree else 0.5), (lo[2] + hi[2]) / 2)
    radius = min(max(max(size[0], size[1]) * (0.32 if tree else 0.45), 0.5), 2.0)
    return {"center": center, "radius": radius, "model": item["model"], "item": item}


# --------------------------------------------------------------------- ゲームと同じ決め方
def norm2(v):
    l = math.hypot(v[0], v[1])
    return (v[0] / l, v[1] / l) if l > 1e-6 else (0.0, 1.0)


def bounce(center, contact, fallback):
    d = (center[0] - contact[0], center[2] - contact[2])
    if math.hypot(*d) < 1e-3:
        d = (fallback[0], fallback[1])
    return norm2(d)


def pick_next(targets, cur, out_dir, visited, depth_axis):
    # EggCombo::PickNext と同じ：届く距離は、はじけるたびに 1 割ずつ縮む。向き（±60°）の合う物が無ければ終わり
    fc = targets[cur]["center"]
    from_depth = fc[0] * depth_axis[0] + fc[2] * depth_axis[1]
    reach = REACH * MOMENTUM ** max(len(visited) - 1, 0)
    cone = math.radians(CONE)
    best, best_score = -1, 1e9
    for k, t in enumerate(targets):
        if k == cur or k in visited:
            continue
        c = t["center"]
        to = (c[0] - fc[0], c[1] - fc[1], c[2] - fc[2])
        d = math.sqrt(sum(v * v for v in to))
        if d < 1.0 or d > reach:
            continue
        if c[0] * depth_axis[0] + c[2] * depth_axis[1] < from_depth - 2.5:
            continue
        flat = norm2((to[0], to[2]))
        ang = math.acos(max(-1.0, min(1.0, flat[0] * out_dir[0] + flat[1] * out_dir[1])))
        if ang > cone:
            continue
        score = ang / cone * 0.6 + d / reach
        if score < best_score:
            best_score, best = score, k
    return best


def chain_from(targets, start, incoming, side_frac, depth_axis):
    t = targets[start]
    r = t["radius"]
    right = (incoming[1], -incoming[0])
    side = side_frac * r * SPREAD
    along = math.sqrt(max(r * r - side * side, 0.0))
    c = t["center"]
    contact = (c[0] - incoming[0] * along + right[0] * side, c[1], c[2] - incoming[1] * along + right[1] * side)
    out = bounce(c, contact, incoming)
    visited = [start]
    cur = start
    while len(visited) < MAX_COMBO:
        nxt = pick_next(targets, cur, out, visited, depth_axis)
        if nxt < 0:
            break
        out = bounce(targets[nxt]["center"], targets[cur]["center"], out)
        visited.append(nxt)
        cur = nxt
    return visited


# --------------------------------------------------------------------- 地面の高さ・かくれ花
M32 = 0xFFFFFFFF


def hash32(x):
    x ^= x >> 16; x = (x * 0x7feb352d) & M32; x ^= x >> 15; x = (x * 0x846ca68b) & M32; x ^= x >> 16
    return x


def lround(v):
    return int(math.floor(v + 0.5)) if v >= 0 else -int(math.floor(-v + 0.5))


def has_flower(x, z):
    return hash32(((lround(x * 10) * 73856093) & M32) ^ ((lround(z * 10) * 19349663) & M32)) % 5 == 0


def main():
    check_only = "--check" in sys.argv
    base = json.load(open(BASE, encoding="utf-8"))["objects"]
    edits = json.load(open(EDIT, encoding="utf-8")) if os.path.exists(EDIT) else {"added": [], "removed": [], "moved": []}
    edits["added"] = [a for a in edits.get("added", []) if a.get("group", 0) != GROUP]
    edits["removed"] = [r for r in edits.get("removed", []) if r.get("by") != "combo_spots"]

    removed_keys = {(r["model"], round(r["pos"][0], 2), round(r["pos"][2], 2)) for r in edits["removed"]}
    others = [o for o in base if (o["model"], round(o["pos"][0], 2), round(o["pos"][2], 2)) not in removed_keys] + edits["added"]
    grounds = [o for o in base + edits["added"] if o["model"].startswith("ground_block")]

    def ground_top(x, z):
        best = None
        for o in grounds:
            s = item_scale(o)
            if abs(o["pos"][0] - x) <= 1.01 * s[0] and abs(o["pos"][2] - z) <= 1.01 * s[2]:
                top = o["pos"][1] + BLOCK_H * s[1]
                if top > 0.0 and (best is None or top > best):
                    best = top
        return best

    added, removed, problems = [], [], []
    for spot in SPOTS:
        ox, oz = spot["origin"]
        nodes = []  # (x, z, model, scale, branch)
        nodes.append((ox, oz, "combo_target", 1.6, "origin"))
        for branch in ("center", "right", "left"):
            ang = spot["angles"][{"left": 0, "center": 1, "right": 2}[branch]]
            x, z = ox, oz
            for i, (turn, step, model, scale) in enumerate(spot[branch]):
                ang += turn
                x += math.sin(math.radians(ang)) * step
                z += math.cos(math.radians(ang)) * step
                if i == len(spot[branch]) - 1 and model.startswith("tree_") and branch == "right":
                    # いちばん長い枝の最後の木には、かくれ花が必ず隠れているように、少しずらして合う所を探す
                    found = None
                    for r in range(0, 8):
                        for dx in range(-r, r + 1):
                            for dz in range(-r, r + 1):
                                cx, cz = round(x + dx * 0.1, 1), round(z + dz * 0.1, 1)
                                if has_flower(cx, cz):
                                    found = (cx, cz); break
                            if found: break
                        if found: break
                    x, z = found
                nodes.append((round(x, 1), round(z, 1), model, scale, branch))

        # 場所の中にあった自動配置の的は消す（つながり方が設計どおりになるように）
        for o in list(others):
            if not kind(o["model"]) or o.get("group", 0) == GROUP or o in edits["added"]:
                continue
            if any(math.hypot(o["pos"][0] - n[0], o["pos"][2] - n[1]) < 3.2 for n in nodes):
                removed.append({"model": o["model"], "pos": o["pos"], "by": "combo_spots"})
                others.remove(o)

        for x, z, model, scale, branch in nodes:
            top = ground_top(x, z)
            if top is None:
                problems.append("%s: 地面が無い所 (%.1f, %.1f) %s" % (spot["name"], x, z, model))
                top = 2.2
            # 表のカメラ（-z）の方を向ける（自動配置と同じ 180°付近。少しずつ向きを変える）
            yaw = 180.0 + ((hash32(int(x * 10) & M32) % 30) - 15 if model != "combo_target" else 0)
            added.append({"model": model, "pos": [x, round(top, 3), z], "rot": [0.0, float(yaw), 0.0],
                          "scale": [scale] * 3, "group": GROUP, "spot": spot["name"], "branch": branch})

    # ------------------------------------------------------------- 調べる：的の真ん中・左・右に当てた時
    targets = [t for t in (target_of(o) for o in others + added) if t]
    index_of = {id(t["item"]): k for k, t in enumerate(targets)}
    ok = True
    for spot in SPOTS:
        bull = next(k for k, t in enumerate(targets) if t["item"].get("spot") == spot["name"] and t["model"] == "combo_target")
        rx, rz = spot["road"]
        c = targets[bull]["center"]
        incoming = norm2((c[0] - rx, c[2] - rz))
        print("== %s（的 %.1f, %.1f）" % (spot["name"], c[0], c[2]))
        expect = {-1.0: "right", 0.0: "center", 1.0: "left"}  # 左のふち → 右の枝
        for side, label in ((-1.0, "左のふち"), (0.0, "真ん中"), (1.0, "右のふち")):
            ch = chain_from(targets, bull, incoming, side, (0.0, 1.0))
            names = [targets[k]["item"].get("branch", "自動:" + targets[k]["model"]) for k in ch]
            want = expect[side]
            planned = min(1 + len(spot[want]), MAX_COMBO)
            spot_of = [targets[k]["item"].get("spot") for k in ch]
            prefix_ok = names[:planned] == ["origin"] + [want] * (planned - 1) and all(sp == spot["name"] for sp in spot_of[:planned])
            # 決めた枝を最後まで通れば良い（その後、まわりの背景へ続くのは「おまけ」のコンボ）
            flag = "OK" if prefix_ok else "ずれ"
            if flag != "OK":
                ok = False
            print("  %s -> x%d (%s, planned x%d)  %s" % (label, len(ch), flag, planned, " > ".join(names)))
    for p in problems:
        print("  !", p)

    if check_only:
        return
    if not ok or problems:
        print("つながり方が設計どおりでない所があるので、書き出していません")
        sys.exit(1)
    edits["added"] += added
    edits["removed"] += removed
    json.dump(edits, open(EDIT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("wrote", EDIT, "added", len(added), "removed", len(removed))


main()
