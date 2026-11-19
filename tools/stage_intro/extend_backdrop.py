# -*- coding: utf-8 -*-
"""今の自動配置を変えずに、地面の無い所（あとから足した道・列車の線路のまわり）にだけ、地面と飾りを足す。

  python tools/stage_intro/extend_backdrop.py [stage1] [--rails 20,21]

  1. make_stage_backdrop.py で、今と同じ升目のまま一時ファイルに作り直す（囲いは足さない）
  2. 新しく作った物のうち、次の物だけを今の自動配置（<ステージ>_backdrop.json）に足す
       ・今の地面（自動配置の地面・背景エディタで足した地面）の升目の上に無い
       ・いちばん近い道が --rails の道（省くと、どの道でもよい）
       ・地面のブロック・草花・木・柵・看板など（空の板・遠い丘・雲・段ボールの底は今のまま）
     地面のブロックが今の地面に少しかかる時は、かからない升目だけを 1 升ずつ置く
  3. 列車の線路の下の地面を線路の高さまで下げ（carve_track.py）、ステージのまわりの囲い（make_backdrop_ring.py）を足し直す
  今ある物は動かさないので、背景エディタの手直し（<ステージ>_backdrop_edit.json）はそのまま当たる
"""
import json
import math
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
ARGS = [a for a in sys.argv[1:]]
RAILS = None
if "--rails" in ARGS:
    k = ARGS.index("--rails")
    RAILS = {int(v) for v in ARGS[k + 1].split(",") if v.strip()}
    del ARGS[k:k + 2]
STAGE = ARGS[0] if ARGS else "stage1"
MAP_DIR = os.path.join(PROJECT, "resources", "map")
BASE = os.path.join(MAP_DIR, STAGE + "_backdrop.json")
EDIT = os.path.join(MAP_DIR, STAGE + "_backdrop_edit.json")
LEVEL = os.path.join(MAP_DIR, STAGE + ".json")

FLOOR_Y = -11.5
BLOCK_W = 2.02
SKIP = ("sky_backdrop", "hill_backdrop", "cloud_cutout")  # 遠くの書き割りは今のまま


def is_floor(o):
    return o["model"] == "ground_block_step" and o["pos"][1] < FLOOR_Y + 0.5


def is_ground_block(o):
    return o["model"].startswith("ground_block") and not is_floor(o)


def main():
    base = json.load(open(BASE, encoding="utf-8"))
    grid = base.get("grid")
    if not grid:
        sys.exit("今の自動配置に grid（地面の升目）がありません。make_stage_backdrop.py で作り直してください")
    ox, oz = grid["origin"]
    cell = grid["cell"]

    def cell_of(x, z):
        return (math.floor((x - ox) / cell), math.floor((z - oz) / cell))

    def block_cells(o):
        """地面のブロックがかかる升目"""
        half = o["scale"][0] * BLOCK_W / 1.005 * 0.5
        size = max(1, round(half * 2 / cell))
        c0 = cell_of(o["pos"][0] - half + cell * 0.5, o["pos"][2] - half + cell * 0.5)
        return [(c0[0] + a, c0[1] + b) for a in range(size) for b in range(size)], size

    # 今の地面の升目（自動配置＋背景エディタで足した地面）
    covered = set()
    old = [o for o in base["objects"] if not o.get("ring")]
    added = json.load(open(EDIT, encoding="utf-8")).get("added", []) if os.path.exists(EDIT) else []
    for o in old + added:
        if is_ground_block(o):
            covered.update(block_cells(o)[0])

    # 1. 同じ升目のまま作り直す（一時ファイル・囲いなし）
    tmp = os.path.join(tempfile.gettempdir(), "%s_backdrop_extend.json" % STAGE)
    env = dict(os.environ, BACKDROP_OUT=tmp, BACKDROP_NO_RING="1", BACKDROP_KEEP_GRID="1")
    subprocess.run([sys.executable, os.path.join(HERE, "make_stage_backdrop.py"), STAGE], check=True, env=env,
                   stdout=subprocess.DEVNULL)
    fresh = json.load(open(tmp, encoding="utf-8"))
    os.remove(tmp)
    if fresh.get("grid", {}).get("origin") != [round(ox, 4), round(oz, 4)] and \
            any(abs(a - b) > 1e-3 for a, b in zip(fresh["grid"]["origin"], (ox, oz))):
        # 升目の始まりは今より手前へ広がってもよいが、升目の位置（cell の倍数）はそろっていること
        fx, fz = fresh["grid"]["origin"]
        if abs(((fx - ox) / cell) - round((fx - ox) / cell)) > 1e-3 or abs(((fz - oz) / cell) - round((fz - oz) / cell)) > 1e-3:
            sys.exit("升目の位置がそろっていません（make_stage_backdrop.py の grid のそろえ方を確かめてください）")

    # 道の点（どの道のそばの物かを決める）：地面を敷く道と線路
    level = json.load(open(LEVEL, encoding="utf-8"))
    lines = level["railLines"]
    samples = []  # (x, z, 道)
    for i, line in enumerate(lines):
        for a, b in zip(line, line[1:]):
            steps = max(1, int(math.dist(a, b) / 0.5))
            for k in range(steps + 1):
                t = k / steps
                samples.append((a[0] + (b[0] - a[0]) * t, a[2] + (b[2] - a[2]) * t, i))

    def nearest_rail(x, z):
        best, rail = 1e18, -1
        for sx, sz, i in samples:
            d = (sx - x) ** 2 + (sz - z) ** 2
            if d < best:
                best, rail = d, i
        return rail

    def wanted(x, z):
        return RAILS is None or nearest_rail(x, z) in RAILS

    # 2. 新しい所の物だけを足す（old_covered＝今の地面。covered は足したブロックの升目も入れて、重ねて置かない）
    old_covered = set(covered)
    extra = []
    for o in fresh["objects"]:
        if is_floor(o) or o["model"].startswith(SKIP):
            continue
        if is_ground_block(o):
            cells, size = block_cells(o)
            free = [c for c in cells if c not in covered]
            if not free or not wanted(o["pos"][0], o["pos"][2]):
                continue
            if len(free) == len(cells):
                extra.append(o)
            else:  # 今の地面にかかる：かからない升目だけを 1 升ずつ
                top_y = o["pos"][1]
                s = cell / BLOCK_W * 1.005
                for cx, cz in free:
                    extra.append({"model": o["model"], "pos": [round(ox + (cx + 0.5) * cell, 3), top_y, round(oz + (cz + 0.5) * cell, 3)],
                                  "rotY": o.get("rotY", 180.0), "scale": [round(s, 3), o["scale"][1], round(s, 3)]})
            covered.update(free)
            continue
        if cell_of(o["pos"][0], o["pos"][2]) in old_covered:
            continue  # 今の地面の上（今の飾りのまま）
        if not wanted(o["pos"][0], o["pos"][2]):
            continue
        extra.append(o)

    counts = {}
    for o in extra:
        counts[o["model"]] = counts.get(o["model"], 0) + 1
    print("足した物", len(extra), dict(sorted(counts.items())))
    base["objects"] = old + extra  # 囲いは 3. で足し直す
    with open(BASE, "w", encoding="utf-8") as f:
        json.dump(base, f, ensure_ascii=False, separators=(",", ":"))

    # 3. 線路の下を線路の高さまで下げる（切り通し）と、囲い
    sys.argv = [sys.argv[0], STAGE]
    sys.path.insert(0, HERE)
    import carve_track  # noqa: E402
    carve_track.main(STAGE)
    import make_backdrop_ring  # noqa: E402
    make_backdrop_ring.main()


if __name__ == "__main__":
    main()
