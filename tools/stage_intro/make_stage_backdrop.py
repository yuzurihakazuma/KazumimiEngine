# -*- coding: utf-8 -*-
"""
本編のステージ全体に置くクラフトの背景を、マップの道から作る（タイトル・スタートのまわりと同じ雰囲気）。

  python tools/stage_intro/make_stage_backdrop.py [stage1]

  出力：project/resources/map/<ステージ>_backdrop.json（ゲームは StageBackdrop で読む）

  ・地面：道に沿って、草の段ボールのブロックを敷く。高さはそばの道に合わせる（道の上面より少し下）。
          道の手前 9m・奥 16m まで。坂や山なりの道の下は、その道の低い所の高さ（道が橋のように浮く）
  ・穴：落ちられる道（地面タイプ＝穴あり）のつながっていない端の先と、道の途中の穴は地面を空ける
          （落ちる所が崖に見える。下に地面があると、落ちた恐竜が地面をすり抜けて見えてしまう）。
          落ちない道（安全）の行き止まりは、地面を続ける（スタートの後ろなど）
  ・動く道・見えない道（リフト）の下には地面を作らない（宙を動く足場のまま）。
          動く道が通る所で、地面より低くなる所は地面を空ける（足場が地面に埋まって見えないように）
  ・奥の地面は段々に上げる（いちばん奥の道の後ろ 6m・11m で1段ずつ。段の線はゆるく波打たせる）
  ・飾りは、ばらまかずに「固まり」で置く（ジオラマの作り方）
      道ばたの草むら（道のふちに 2〜4本ずつ。手前側は低く小さく）／柵（ところどころ抜き、看板）と柵ぎわの花だん／
      木の林（大きい木＋小さい木＋根元の草むら・岩。2列目は上の段に）／空き地の花畑／手前のふちの小さな草
  ・ずっと奥に、近い丘・遠い丘・雲・空を、ステージの端から端まで並べる
  ・ずっと下（落ちてミスになる高さより下）に、裏返したブロックで段ボールの底を敷く
          （穴をのぞくと、深い穴の底が見える。何も無いと画面のクリア色が見えてしまう）
  道を直したら、もう一度流せば合う
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
STAGE = sys.argv[1] if len(sys.argv) > 1 else "stage1"
MAP = os.path.join(PROJECT, "resources", "map", STAGE + ".json")
OUT = os.path.join(PROJECT, "resources", "map", STAGE + "_backdrop.json")

SUB = 1.2                 # 地面を敷く升目(m)。ブロックは 1〜3升（1.2 / 2.4 / 3.6m 角）
FRONT, BACK = 9.0, 16.0   # 道の手前・奥に地面を敷く幅(m)
GROUND_DROP = 0.06        # 地面の上面は、道の上面より少しだけ下（重なってちらつかないように）
BLOCK_W, BLOCK_H = 2.02, 0.62  # ground_block_step の大きさ（拡縮1倍）
BLOCK_SY = 1.8            # ブロックの厚みの拡縮（1.1m。穴や段差のふちが段ボールの崖に見える）
HOLE_HALF = 2.4           # 道の端の先を空ける幅の半分(m)
HOLE_MAX = 9.0            # 行き止まりの先を空ける長さ(m)（落ちている間に、奥の地面をすり抜けないくらい）
ROAD_CLEAR = 1.6          # 草花を置かない、道からの距離(m)
ROAD_HALF = 1.0           # 道の半幅(m)（game/rail/road/RoadProfile.h の kHalfWidth）
TERRACE = [(6.0, 0.55), (11.0, 1.1)]  # 奥の段々：いちばん奥の道からの距離(m)と上げる高さ(m)
LIFT_CLEAR = 0.45         # 動く道がこれより地面に近づく所は、地面を空ける(m)
LIFT_HALF = 1.6           # 動く道のために空ける幅の半分(m)
FLOOR_Y = -11.5           # 段ボールの底の高さ（ミスになる高さ -10 より下。落ちた恐竜が底に着く前に消える）
FLOOR_TILE = 12.0         # 底の板の大きさ(m)
SKY_LIMIT = 150.0         # 空を置いてよい、手前のカメラからの奥行き(m)（ゲームの遠くの見える限界 200m より手前。斜めから見ても切れない）

rng = random.Random(7)
level = json.load(open(MAP, encoding="utf-8"))
lines = level["railLines"]
n = len(lines)


def at(key, i, default):
    values = level.get(key, [])
    return values[i] if i < len(values) else default


def is_moving(i):
    """動く道（往復・円・ガイド追従。種類 0 でも振れ幅があれば往復する）"""
    return at("railMotionTypes", i, 0) != 0 or list(at("railMotions", i, [0, 0, 0]))[:3] != [0, 0, 0]


def is_ground_rail(i):
    """地面を敷く道：動かない・見える・後から出てこない道"""
    return (len(lines[i]) >= 2 and not is_moving(i) and at("railVisible", i, 1) != 0
            and at("railAppearTriggers", i, -1) < 0)


def catmull(p0, p1, p2, p3, t):
    return [0.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t * t
                   + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t * t * t) for j in range(3)]


def curve(i, fine=24):
    """道の形（ゲームと同じ：Catmull-Rom、端は折り返し。直線モードは折れ線）。(点, ノード番号の t) の並び"""
    line = lines[i]
    out = []
    last = len(line) - 1
    for k in range(last):
        p1, p2 = line[k], line[k + 1]
        p0 = line[k - 1] if k >= 1 else [2 * p1[j] - p2[j] for j in range(3)]
        p3 = line[k + 2] if k + 2 <= last else [2 * p2[j] - p1[j] for j in range(3)]
        for m in range(fine):
            t = m / fine
            if at("railLineModes", i, 0) == 1:
                out.append(([p1[j] + (p2[j] - p1[j]) * t for j in range(3)], k + t))
            else:
                out.append((catmull(p0, p1, p2, p3, t), k + t))
    out.append((list(line[-1]), float(last)))
    return out


def sample(i, step=0.5):
    """道を細かく区切った点（点, 道の始点からの距離, ノード番号の t）と全長。だいたい step ごと"""
    dense = curve(i)
    dist = [0.0]
    for a, b in zip(dense, dense[1:]):
        dist.append(dist[-1] + math.dist(a[0], b[0]))
    out, next_d = [], 0.0
    for k, ((p, t), d) in enumerate(zip(dense, dist)):
        if d >= next_d or k == len(dense) - 1 or abs(t - round(t)) < 1e-9:
            out.append((p, d, t))
            next_d = d + step
    return out, dist[-1]


def hole_spans(i, points):
    """道の途中の穴（ノードの穴の印）：距離の区間の並び（ゲームの SplineRail::GetHoleIntervals と同じ決め方）"""
    flags = at("railNodeHoles", i, [])
    count = min(len(lines[i]), len(flags))
    node_d = {int(round(t)): d for p, d, t in points if abs(t - round(t)) < 1e-9}
    total = points[-1][1]
    spans, start = [], -1
    for k in range(count + 1):
        hole = k < count and flags[k] != 0
        if hole and start < 0:
            start = k
        if not hole and start >= 0:
            end = k - 1
            d0 = 0.0 if start == 0 else (node_d[start - 1] + node_d[start]) * 0.5
            d1 = total if (end == count - 1 and end == len(lines[i]) - 1) else (node_d[end] + node_d[end + 1]) * 0.5
            spans.append((d0, d1))
            start = -1
    return spans


# --- 道の点 ---
ground = []      # (x, y, z, 低い所の高さ, 道の番号)
gap_points = []  # 道の途中の穴の点（まわりの地面を空ける）
every = []       # 全部の道（奥行きを決めるのに使う。動く道は動く範囲も入れる）
for i, line in enumerate(lines):
    if len(line) < 2:
        continue
    points, _ = sample(i)
    for p, _, _ in points:
        every.append(p)
    if not is_ground_rail(i):
        continue
    spans = hole_spans(i, points)
    for p, d, _ in points:
        if any(d0 <= d <= d1 for d0, d1 in spans):
            gap_points.append(p)
            continue
        low = min(q[1] for q, e, _ in points if abs(e - d) <= 12.0)  # 前後 12m の低い所（山なりの道は橋に）
        ground.append((p[0], p[1], p[2], low, i))
    if spans:
        print("道%d の途中の穴" % i, spans)


# --- 動く道が通る所（足場の点が動く範囲）。ゲームの RailMotion と同じ動き ---
def at_distance(table, d):
    points, total = table
    d = min(max(d, 0.0), total)
    for (a, da, _), (b, db, _) in zip(points, points[1:]):
        if da <= d <= db:
            t = (d - da) / (db - da) if db > da else 0.0
            return [a[j] + (b[j] - a[j]) * t for j in range(3)]
    return points[-1][0]


moving = []  # 動く道の足場の点（動いている間のすべての位置）
for i, line in enumerate(lines):
    kind = at("railMotionTypes", i, 0)
    amp = list(at("railMotions", i, [0, 0, 0, 2]))[:3]
    if len(line) < 2 or not is_moving(i):
        continue
    own = sample(i, 0.25)
    body = [p for p, _, _ in sample(i, 1.0)[0]]
    pivot = at_distance(own, own[1] * 0.5)
    mid_a, mid_b = at_distance(own, own[1] * 0.5 - 0.1), at_distance(own, own[1] * 0.5 + 0.1)
    rest_yaw = math.atan2(mid_b[0] - mid_a[0], mid_b[2] - mid_a[2])
    offsets = []  # (ずらし, 回転)
    if kind == 3:
        g = at("railGuideRails", i, -1)
        if g < 0 or g >= n or len(lines[g]) < 2:
            continue
        table = sample(g, 0.25)
        s0 = min(max(at("railGuideStarts", i, 0.0), 0.0), table[1])
        s1 = at("railGuideEnds", i, -1.0)
        s1 = table[1] if s1 < 0 else min(max(s1, 0.0), table[1])
        if s1 < s0:
            s0, s1 = s1, s0
        if s1 - s0 < 0.01:
            s0, s1 = 0.0, table[1]
        base = at_distance(table, s0)
        align = at("railGuideAligns", i, 0) == 1
        steps = max(2, int((s1 - s0) / 0.5))
        yaw = 0.0
        for k in range(steps + 1):
            d = s0 + (s1 - s0) * k / steps
            p = at_distance(table, d)
            if align:
                a, b = at_distance(table, d - 0.2), at_distance(table, d + 0.2)
                if math.hypot(b[0] - a[0], b[2] - a[2]) > 0.06:  # ほぼ真上・真下の所は向きを保つ
                    yaw = math.atan2(b[0] - a[0], b[2] - a[2]) - rest_yaw
            offsets.append(([p[j] - base[j] for j in range(3)], yaw))
    else:
        for k in range(48):
            u = k / 48
            if kind == 2:
                th = u * 2 * math.pi
                offsets.append(([amp[0] * math.cos(th), amp[1] * math.sin(th), amp[2] * math.sin(th)], 0.0))
            else:
                w = math.sin(u * 2 * math.pi)
                offsets.append(([amp[0] * w, amp[1] * w, amp[2] * w], 0.0))
    for off, yaw in offsets:
        c, s_ = math.cos(yaw), math.sin(yaw)
        for p in body:
            dx, dz = p[0] - pivot[0], p[2] - pivot[2]
            moving.append((pivot[0] + dx * c + dz * s_ + off[0], p[1] + off[1], pivot[2] - dx * s_ + dz * c + off[2]))
every += [list(p) for p in moving[::4]]
print("動く道の通る点", len(moving))


def nearest_ground(x, z, exclude=-1):
    best, best_d = None, 1e9
    for g in ground:
        if g[4] == exclude:
            continue
        d = (g[0] - x) ** 2 + (g[2] - z) ** 2
        if d < best_d:
            best, best_d = g, d
    return best, math.sqrt(best_d)


# --- つながっていない道の端：その先を空ける（穴・崖） ---
holes = []  # (端の点, 向き, 長さ)
for i, line in enumerate(lines):
    if not is_ground_rail(i) or at("railGroundTypes", i, 0) != 1:
        continue  # 落ちない道の行き止まりは、恐竜が端で止まる（地面を続ける）
    for end, prev in ((line[0], line[1]), (line[-1], line[-2])):
        _, d = nearest_ground(end[0], end[2], exclude=i)
        if d <= 1.0:
            continue  # ほかの道とつながっている
        tx, tz = end[0] - prev[0], end[2] - prev[2]
        length = math.hypot(tx, tz)
        if length < 1e-4:
            continue
        tx, tz = tx / length, tz / length
        reach = HOLE_MAX
        for k in range(1, int(HOLE_MAX / 0.5) + 1):  # 先にほかの道があれば、そこまで（向こう岸）
            px, pz = end[0] + tx * k * 0.5, end[2] + tz * k * 0.5
            _, d2 = nearest_ground(px, pz, exclude=i)
            if d2 <= 1.2:
                reach = k * 0.5
                break
        holes.append((end, (tx, tz), reach))
        print("穴・崖：道%d の端 (%.1f, %.1f) から %.1fm" % (i, end[0], end[2], reach))


def in_hole(x, z, pad):
    for end, (tx, tz), reach in holes:
        dx, dz = x - end[0], z - end[2]
        along = dx * tx + dz * tz
        side = abs(-dx * tz + dz * tx)
        if -0.2 - pad <= along <= reach + pad and side <= HOLE_HALF + pad:
            return True
    for p in gap_points:
        if (p[0] - x) ** 2 + (p[2] - z) ** 2 <= (HOLE_HALF + pad) ** 2:
            return True
    return False


# 動く道の足場の点を 2m の升目ごとに（地面より低くなる所を探す）
lift_cells = {}
for p in moving:
    lift_cells.setdefault((int(math.floor(p[0] / 2.0)), int(math.floor(p[2] / 2.0))), []).append(p)


def lift_low(x, z):
    """(x, z) のそばを通る動く道の、いちばん低い高さ（通らなければ None）"""
    key = (int(math.floor(x / 2.0)), int(math.floor(z / 2.0)))
    low = None
    for ox in (-1, 0, 1):
        for oz in (-1, 0, 1):
            for p in lift_cells.get((key[0] + ox, key[1] + oz), []):
                if (p[0] - x) ** 2 + (p[2] - z) ** 2 <= LIFT_HALF ** 2:
                    low = p[1] if low is None else min(low, p[1])
    return low


# --- 地面の升目 ---
def rear_z(x, span=8.0):
    """x のあたりで、いちばん奥の（地面を敷く）道"""
    zs = [g[2] for g in ground if abs(g[0] - x) <= span]
    return max(zs) if zs else None


def wave(x, period, seed):
    """なめらかな揺らぎ（-1〜1）。段の線や飾りの濃さを、場所ごとにゆっくり変える"""
    k = math.floor(x / period)
    t = x / period - k

    def value(i):
        return random.Random(seed * 100003 + i).uniform(-1.0, 1.0)
    t = t * t * (3 - 2 * t)
    return value(k) * (1 - t) + value(k + 1) * t


def terrace(x, z):
    """奥の段々で上げる高さ"""
    rz = rear_z(x)
    if rz is None:
        return 0.0
    lift = 0.0
    block = (math.floor(x / 3.6) + 0.5) * 3.6  # 段の線は 3.6m ごとに揃える（升目ごとにガタガタしない）
    for k, (dist, up) in enumerate(TERRACE):
        if z - rz > dist + 1.2 * wave(block, 14.0, 11 + k):
            lift = up
    return lift


min_x = min(g[0] for g in ground) - BACK
max_x = max(g[0] for g in ground) + BACK
min_z = min(g[2] for g in ground) - FRONT
max_z = max(g[2] for g in ground) + BACK
cols = int((max_x - min_x) / SUB) + 1
rows = int((max_z - min_z) / SUB) + 1
height = {}  # (列, 行) → 地面の上面
carved = 0
for c in range(cols):
    for r in range(rows):
        x = min_x + (c + 0.5) * SUB
        z = min_z + (r + 0.5) * SUB
        g, d = nearest_ground(x, z)
        if d > BACK or z - g[2] < -FRONT:
            continue
        if in_hole(x, z, SUB * 0.5):
            continue
        top = round(g[3] - GROUND_DROP + terrace(x, z), 2)
        low = lift_low(x, z)
        if low is not None and low < top + LIFT_CLEAR:
            carved += 1
            continue  # 動く道が地面に埋まる所
        height[(c, r)] = top

objects = []


def add(model, pos, scale, rot_y=180.0, rot_x=None):
    s = scale if isinstance(scale, list) else [scale] * 3
    entry = {"model": model, "pos": [round(v, 3) for v in pos], "rotY": round(rot_y, 1), "scale": [round(v, 3) for v in s]}
    if rot_x is not None:
        entry["rot"] = [rot_x, round(rot_y, 1), 0.0]
    objects.append(entry)


# ブロックを敷く（大きい物から。同じ高さの升目だけをまとめる）
covered = set()
for r in range(rows):
    for c in range(cols):
        if (c, r) not in height or (c, r) in covered:
            continue
        top = height[(c, r)]
        for size in (3, 2, 1):
            cells = [(c + a, r + b) for a in range(size) for b in range(size)]
            if all(cell in height and cell not in covered and height[cell] == top for cell in cells):
                break
        covered.update(cells)
        w = size * SUB
        cx = min_x + c * SUB + w / 2
        cz = min_z + r * SUB + w / 2
        sxz = w / BLOCK_W * 1.005
        add("ground_block_step", [cx, top - BLOCK_H * BLOCK_SY, cz], [sxz, BLOCK_SY, sxz])
print("地面のブロック", len(objects), "（動く道のために空けた升目", carved, "）")


def ground_top(x, z):
    return height.get((int((x - min_x) / SUB), int((z - min_z) / SUB)))


def near_road(x, z, clear):
    return nearest_ground(x, z)[1] < clear


# --- 飾り：ばらまかずに「固まり」で置く ---
TUFTS = ["grass_tuft_00", "grass_tuft_01", "grass_tuft_02"]
FLOWERS = ["flower_orange", "flower_red", "flower_white"]
ROCKS = ["paper_rock_00", "paper_rock_01"]
BIG_TREES = ["tree_cutout_00", "tree_cutout_01", "tree_cutout_02"]
SMALL_TREES = ["tree_round_small_00", "tree_round_small_01"]
occupied = []  # 置いた物 (x, z, 半径)。重ねすぎないように
stage_min_x = min(g[0] for g in ground)
stage_max_x = max(g[0] for g in ground)


def free(x, z, r):
    return all((x - ox) ** 2 + (z - oz) ** 2 >= (r + orad) ** 2 for ox, oz, orad in occupied)


def put(model, x, z, scale, r=0.3, turn=20.0, clear=ROAD_HALF + 0.15, check=True):
    """地面の上に1つ置く（地面が無い・穴・道の上・ほかの物と重なる所には置かない）"""
    top = ground_top(x, z)
    if top is None or in_hole(x, z, 0.3) or near_road(x, z, clear):
        return False
    if check and not free(x, z, r):
        return False
    add(model, [x, top, z], scale, 180 + rng.uniform(-turn, turn))
    occupied.append((x, z, r))
    return True


def bush(x, z, count, scale, spread=0.55, flower=0.0, clear=ROAD_HALF + 0.15):
    """草むら：草を何本か重ねて1つの固まりに（真ん中ほど大きい）。flower=花をまぜる割合"""
    placed = 0
    for k in range(count):
        a = rng.uniform(0, 2 * math.pi)
        d = spread * math.sqrt(rng.random()) if k else 0.0
        size = scale * (1.0 - 0.25 * d / max(spread, 1e-3)) * rng.uniform(0.9, 1.1)
        model = rng.choice(FLOWERS) if rng.random() < flower else rng.choice(TUFTS)
        placed += put(model, x + math.cos(a) * d, z + math.sin(a) * d * 0.7, size, 0.18, 25.0, clear, check=False)
    if placed:
        occupied.append((x, z, spread + 0.2))
    return placed


# 柵：いちばん奥の道の後ろ。ところどころ抜いて（同じ物がずっと続かない）、看板を少し
fence_x = stage_min_x - 6
count = 0
fence_line = []  # 柵を置いた所（柵ぎわの花だんに使う）
while fence_x < stage_max_x + 6:
    rz = rear_z(fence_x)
    if rz is not None:
        fz = rz + 1.8
        gap = wave(fence_x, 14.0, 3) < -0.55  # 柵の切れ目
        top = ground_top(fence_x, fz)
        if not gap and top is not None and not near_road(fence_x, fz, 1.4) and not in_hole(fence_x, fz, 0.5):
            count += 1
            add("sign_arrow" if count % 20 == 7 else "fence_sticks", [fence_x, top, fz], 1.8)
            occupied.append((fence_x, fz, 1.1))
            fence_line.append((fence_x, fz))
    fence_x += 2.4

# 道ばたの草むら：道のふちに沿って、2〜4本ずつ。手前（カメラ側）は低く小さく、奥は大きく・花も
for i, line in enumerate(lines):
    if not is_ground_rail(i):
        continue
    points, total = sample(i, 0.5)
    next_d = rng.uniform(1.0, 4.0)
    for k in range(1, len(points)):
        p, d, _ = points[k]
        if d < next_d:
            continue
        q = points[k - 1][0]
        tx, tz = p[0] - q[0], p[2] - q[2]
        length = math.hypot(tx, tz) or 1.0
        nx, nz = -tz / length, tx / length  # 道の横向き
        density = wave(p[0] + p[2], 11.0, 5 + i)  # 濃い所と薄い所
        next_d = d + (rng.uniform(2.0, 3.5) if density > 0 else rng.uniform(4.5, 8.0))
        for side in (1, -1):
            sx, sz = nx * side, nz * side
            back = sz > 0.3 or (abs(sz) <= 0.3 and rng.random() < 0.5)  # 奥側（縦の道は左右どちらも）
            if not back and rng.random() < 0.55:
                continue  # 手前側は控えめ（恐竜とカメラの間をふさがない）
            off = ROAD_HALF + rng.uniform(0.25, 0.65)
            x, z = p[0] + sx * off + rng.uniform(-0.4, 0.4) * tx / length, p[2] + sz * off
            if back:
                bush(x, z, rng.randint(2, 4), rng.uniform(1.6, 2.1), 0.5, 0.25 if density > 0.2 else 0.0)
            else:
                bush(x, z, rng.randint(1, 2), rng.uniform(1.1, 1.4), 0.35)

# 柵ぎわの花だん：柵のすぐ後ろに、草と花の固まり（同じ色の花を2〜4本まとめる）
for fx, fz in fence_line:
    if wave(fx, 10.0, 7) < -0.1 or rng.random() < 0.35:
        continue
    color = rng.choice(FLOWERS)
    x, z = fx + rng.uniform(-1.0, 1.0), fz + rng.uniform(0.6, 1.3)
    for k in range(rng.randint(2, 4)):
        put(color, x + rng.uniform(-0.6, 0.6), z + rng.uniform(-0.3, 0.3), rng.uniform(1.5, 1.9), 0.2, 20.0, check=False)
    bush(x + rng.uniform(-0.8, 0.8), z + 0.3, rng.randint(2, 3), rng.uniform(1.7, 2.1), 0.4)


# 木の林：大きい木1本＋小さい木1〜2本＋根元の草むら（ときどき岩）。1列目は柵の後ろ、2列目は上の段に小さめ
def grove(x, z, scale):
    if not put(rng.choice(BIG_TREES), x, z, scale, 1.3, 8.0, 3.0):
        return False
    for k in range(rng.randint(1, 2)):
        side = rng.choice((-1, 1))
        put(rng.choice(SMALL_TREES + BIG_TREES[:1]), x + side * rng.uniform(1.4, 2.4), z + rng.uniform(-1.2, 1.0),
            scale * rng.uniform(0.7, 0.85), 0.9, 8.0, 3.0, check=False)  # 林の中どうしは重なってよい
    bush(x + rng.uniform(-0.5, 0.5), z - rng.uniform(0.6, 1.0), rng.randint(3, 5), rng.uniform(2.2, 2.7), 0.7, 0.15, 2.5)
    if rng.random() < 0.4:
        put(rng.choice(ROCKS), x + rng.choice((-1, 1)) * rng.uniform(1.0, 1.8), z - rng.uniform(0.4, 1.2),
            rng.uniform(1.0, 1.4), 0.6, 180.0, 2.5, check=False)
    return True


tree_x = stage_min_x - 8
while tree_x < stage_max_x + 8:
    rz = rear_z(tree_x)
    if rz is not None:
        grove(tree_x, rz + rng.uniform(3.6, 5.5), rng.uniform(2.0, 2.5))
    tree_x += rng.uniform(9.0, 16.0)
tree_x = stage_min_x - 4
while tree_x < stage_max_x + 8:
    rz = rear_z(tree_x)
    if rz is not None and rng.random() < 0.75:
        put(rng.choice(BIG_TREES + SMALL_TREES), tree_x, rz + rng.uniform(12.0, 15.0), rng.uniform(1.7, 2.1), 1.0, 8.0, 3.0)
    tree_x += rng.uniform(5.0, 9.0)

# 手前のふち：カメラのすぐ前（いちばん手前の道より 4〜8m 手前）に、低い草むらと花を点々と（画面の下が平らすぎないように）
def front_z(x, span=8.0):
    zs = [g[2] for g in ground if abs(g[0] - x) <= span]
    return min(zs) if zs else None


edge_x = stage_min_x - 8
while edge_x < stage_max_x + 8:
    fz = front_z(edge_x)
    if fz is not None:
        z = fz - rng.uniform(4.0, 8.0)
        if rng.random() < 0.3:
            for k in range(rng.randint(2, 3)):
                put(rng.choice(FLOWERS), edge_x + rng.uniform(-0.7, 0.7), z + rng.uniform(-0.4, 0.4), rng.uniform(1.3, 1.6), 0.2, 25.0, check=False)
        bush(edge_x, z, rng.randint(2, 4), rng.uniform(1.3, 1.7), 0.6)
    edge_x += rng.uniform(5.0, 9.0)

# 空き地の花畑：道から離れた広い所に、草むらと花の固まりを点々と（固まりどうしは 5m 以上離す）
for (c, r), top in sorted(height.items()):
    if rng.random() > 0.05:
        continue
    x = min_x + (c + 0.5) * SUB
    z = min_z + (r + 0.5) * SUB
    rz = rear_z(x)
    if near_road(x, z, 3.0) or (rz is not None and z > rz + 3.0) or not free(x, z, 2.5):
        continue  # 柵より奥は林と花だんに任せる
    if z < (rz if rz is not None else z) - 9.0 + 3.0:
        bush(x, z, rng.randint(1, 2), rng.uniform(1.1, 1.4), 0.4)  # 手前のふち：低く小さく
    else:
        bush(x, z, rng.randint(3, 6), rng.uniform(1.5, 1.9), 0.9, 0.45)

# --- ずっと奥：近い丘・遠い丘・雲・空（ステージの端から端まで。リフトの道も含めて一番奥の道の後ろ） ---
all_min_x = min(p[0] for p in every) - 40
all_max_x = max(p[0] for p in every) + 60
FAR_SLOPE = 0.5  # 奥の道から横へ離れるほど、丘の並びを手前へ（急に前後へ跳ばない）


def far_z(x):
    """x のあたりで、いちばん奥の道（動く道の通る所も入れる）。横へなだらかに下がる線（ステージの端より外は端と同じ）"""
    x = min(max(x, min(g[0] for g in ground)), max(p[0] for p in every))
    return max(p[2] - FAR_SLOPE * abs(p[0] - x) for p in every)


def low_top_near(x, span=30.0):
    """x のあたりの地面のいちばん低い上面（丘のすそを地面の高さに合わせる）"""
    tops = [g[3] - GROUND_DROP for g in ground if abs(g[0] - x) <= span]
    return min(tops) if tops else min(height.values())


x = all_min_x
while x < all_max_x:
    add(rng.choice(["hill_backdrop_00", "hill_backdrop_01"]), [x, low_top_near(x) - 0.6, far_z(x) + 22 + rng.uniform(-1.5, 1.5)], rng.uniform(2.5, 2.9))
    x += rng.uniform(15.0, 19.0)
x = all_min_x + 10
while x < all_max_x:
    add("hill_backdrop_far", [x, low_top_near(x) - 0.9, far_z(x) + 31], rng.uniform(3.0, 3.4))
    x += 32.0
x = all_min_x
while x < all_max_x:
    add(rng.choice(["cloud_cutout_00", "cloud_cutout_01"]), [x, rng.uniform(8.5, 12.5), far_z(x) + rng.uniform(27, 35)], rng.uniform(2.2, 2.8))
    x += rng.uniform(20.0, 32.0)
def near_camera_z(x, span=60.0):
    """x のあたりで、カメラがいちばん手前に来る位置（手前の道から 10m 後ろ）"""
    zs = [g[2] for g in ground if abs(g[0] - x) <= span]
    return (min(zs) if zs else min(g[2] for g in ground)) - 10.0


# 空：幅 200m の板を 90m ごとに重ねて、全部同じ奥行きに並べる（奥行きが違うと、継ぎ目から後ろが見えたり、
#   手前の板が奥の丘を隠したりする）。丘・雲のいちばん奥より後ろで、カメラの遠くの見える限界(GamePlayScene の kFarClip)より手前。
#   重なった所がちらつかないように、1枚おきに少しだけずらす。下を深くまで（穴の奥をのぞいた先も空）、
#   上も高くまで（低い所から見上げるイントロのカメラでも、空の上の端が見えないように）
far_objects = [o for o in objects if o["model"].startswith(("hill_", "cloud_"))]
sky_z = max(o["pos"][2] for o in far_objects) + 4.0
x = all_min_x
sky_index = 0
while x < all_max_x + 90:
    limit = near_camera_z(x, 100.0) + SKY_LIMIT
    if sky_z + 0.6 > limit:
        print("空の板 x=%.0f：奥行き %.0f が遠くの見える限界(%.0f)より奥" % (x, sky_z, limit))
    add("sky_backdrop", [x, -20.0, sky_z + 0.6 * (sky_index % 2)], [5.0, 8.0, 5.0])  # 高さ 80m（-20〜60）
    x += 90.0
    sky_index += 1

# --- ずっと下：段ボールの底（ブロックを裏返して、段ボールの面を上に） ---
floor_min_x = min(min_x, min(p[0] for p in every) - 20)
floor_max_x = max(max_x, max(p[0] for p in every) + 20)
floor_min_z = min(min_z, min(p[2] for p in every) - 12)
floor_max_z = max(max_z, max(p[2] for p in every) + 60)  # 穴の奥を浅い角度でのぞいても、底が見えるように
fx = floor_min_x
while fx < floor_max_x:
    fz = floor_min_z
    while fz < floor_max_z:
        add("ground_block_step", [fx + FLOOR_TILE / 2, FLOOR_Y, fz + FLOOR_TILE / 2],
            [FLOOR_TILE / BLOCK_W * 1.005, 1.0, FLOOR_TILE / BLOCK_W * 1.005], 180.0, rot_x=180.0)
        fz += FLOOR_TILE
    fx += FLOOR_TILE

counts = {}
for o in objects:
    counts[o["model"]] = counts.get(o["model"], 0) + 1
with open(OUT, "w", encoding="utf-8") as f:
    # grid＝地面の升目（背景エディタの「地面」ブラシが同じ升目で塗る）
    json.dump({"stage": STAGE, "grid": {"origin": [round(min_x, 4), round(min_z, 4)], "cell": SUB}, "objects": objects},
              f, ensure_ascii=False, separators=(",", ":"))
print("wrote", OUT, len(objects), "objects", counts)
