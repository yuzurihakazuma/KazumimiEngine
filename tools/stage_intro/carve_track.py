# -*- coding: utf-8 -*-
"""列車の線路が地面に埋まらないように、線路の下の地面を線路の高さまで下げる（切り通し）。線路の上の草花・柵・木もどける。

  python tools/stage_intro/carve_track.py [stage1]

  ・線路＝列車にしたガイド追従の足場の、ガイドのレール（ゲームの SplineRail::isTrack と同じ決め方）
  ・線路の真ん中から BAND m の帯にかかる地面の升目で、上の面が「枕木の下」（ガイドの高さ − TRACK_DROP）より高い所を、
    その高さまで下げる。大きいブロック（2〜3 升）は 1 升ずつに割ってから、かかる升目だけを下げる
  ・帯の中の飾り（草・花・柵・木・岩・看板）は消す
  ・自動配置（<ステージ>_backdrop.json）だけを直す（背景エディタで足した物は、背景エディタで直す）。何度流しても同じ
  make_stage_backdrop.py / extend_backdrop.py が最後に呼ぶ
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
MAP_DIR = os.path.join(PROJECT, "resources", "map")

TRACK_DROP = 2.07 + 0.14   # ガイドから枕木の下まで（game/rail/ToyTrain の屋根の高さ＋枕木の厚み）
BAND = 1.7                 # 線路の真ん中から、地面を下げる幅(m)（枕木の半分 1.25 ＋少し）
CLEAR = 2.0                # 線路の真ん中から、飾りをどける幅(m)
FLOOR_Y = -11.5
BLOCK_W, BLOCK_H, BLOCK_SY = 2.02, 0.62, 1.8  # make_stage_backdrop.py と同じ
DECOR = ("grass_tuft", "flower_", "fence_", "tree_", "paper_rock", "sign_")


def catmull(p0, p1, p2, p3, t):
    return [0.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t * t
                   + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t * t * t) for j in range(3)]


def curve(line, straight, fine=40):
    """道の形（ゲームと同じ：Catmull-Rom、端は折り返し。直線モードは折れ線）"""
    out = []
    last = len(line) - 1
    for k in range(last):
        p1, p2 = line[k], line[k + 1]
        p0 = line[k - 1] if k >= 1 else [2 * p1[j] - p2[j] for j in range(3)]
        p3 = line[k + 2] if k + 2 <= last else [2 * p2[j] - p1[j] for j in range(3)]
        for m in range(fine):
            t = m / fine
            out.append([p1[j] + (p2[j] - p1[j]) * t for j in range(3)] if straight else catmull(p0, p1, p2, p3, t))
    out.append(list(line[-1]))
    return out


def track_points(level):
    lines = level["railLines"]

    def at(key, i, default):
        values = level.get(key, [])
        return values[i] if i < len(values) else default
    guides = set()
    for j, train in enumerate(level.get("railTrains", [])):
        g = at("railGuideRails", j, -1)
        if train.get("use") and at("railMotionTypes", j, 0) == 3 and 0 <= g < len(lines) and len(lines[g]) >= 2:
            guides.add(g)
    points = []
    for g in sorted(guides):
        dense = curve(lines[g], at("railLineModes", g, 0) == 1)
        # 0.5m ごとに
        last = dense[0]
        points.append(last)
        for p in dense[1:]:
            if math.dist(p, last) >= 0.5:
                points.append(p)
                last = p
        points.append(dense[-1])
    return points


def main(stage="stage1", path=None):
    path = path or os.path.join(MAP_DIR, stage + "_backdrop.json")
    level = json.load(open(os.path.join(MAP_DIR, stage + ".json"), encoding="utf-8"))
    points = track_points(level)
    if not points:
        print("線路なし（列車にした足場がない）")
        return
    data = json.load(open(path, encoding="utf-8"))
    grid = data.get("grid")
    if not grid:
        print("grid が無いので線路の下を下げられません")
        return
    ox, oz = grid["origin"]
    cell = grid["cell"]

    # 線路の点の升目引き（近い点を早く探す）
    buckets = {}
    for p in points:
        buckets.setdefault((math.floor(p[0] / 4.0), math.floor(p[2] / 4.0)), []).append(p)

    def nearest(x, z):
        """線路のいちばん近い点と、そこまでの水平の距離"""
        kx, kz = math.floor(x / 4.0), math.floor(z / 4.0)
        best, best_d = None, 1e9
        for ax in (-1, 0, 1):
            for az in (-1, 0, 1):
                for p in buckets.get((kx + ax, kz + az), []):
                    d = math.hypot(p[0] - x, p[2] - z)
                    if d < best_d:
                        best, best_d = p, d
        return best, best_d

    lowered = split = removed = 0
    out = []
    for o in data["objects"]:
        model = o["model"]
        x, y, z = o["pos"]
        if model.startswith(DECOR):
            p, d = nearest(x, z)
            if p is not None and d <= CLEAR and y > p[1] - TRACK_DROP - 0.3:
                removed += 1
                continue
            out.append(o)
            continue
        if model != "ground_block_step" or y < FLOOR_Y + 0.5 or o.get("ring"):
            out.append(o)
            continue
        # 地面のブロック：かかる升目ごとに、線路の帯の中なら高さを確かめる
        half = o["scale"][0] * BLOCK_W / 1.005 * 0.5
        size = max(1, round(half * 2 / cell))
        top = y + BLOCK_H * BLOCK_SY
        c0x = math.floor((x - half + cell * 0.5 - ox) / cell)
        c0z = math.floor((z - half + cell * 0.5 - oz) / cell)
        tops = {}
        need = False
        for a in range(size):
            for b in range(size):
                cx, cz = ox + (c0x + a + 0.5) * cell, oz + (c0z + b + 0.5) * cell
                p, d = nearest(cx, cz)
                limit = p[1] - TRACK_DROP if ( p is not None and d <= BAND ) else None
                if limit is not None and top > limit + 0.02:
                    tops[(a, b)] = round(limit, 2)
                    need = True
                else:
                    tops[(a, b)] = top
        if not need:
            out.append(o)
            continue
        split += 1
        s = cell / BLOCK_W * 1.005
        for (a, b), t in tops.items():
            if t != top:
                lowered += 1
            out.append({"model": model, "pos": [round(ox + (c0x + a + 0.5) * cell, 3), round(t - BLOCK_H * BLOCK_SY, 3),
                                                round(oz + (c0z + b + 0.5) * cell, 3)],
                        "rotY": o.get("rotY", 180.0), "scale": [round(s, 3), o["scale"][1], round(s, 3)]})
    data["objects"] = out
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
    print("線路の下：下げた升目 %d（割ったブロック %d）・どけた飾り %d" % (lowered, split, removed))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "stage1")
