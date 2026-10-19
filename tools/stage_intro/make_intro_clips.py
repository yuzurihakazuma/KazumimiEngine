# -*- coding: utf-8 -*-
"""
ステージ開始演出の、プレイヤーの演技クリップ（IntroWave / IntroPush / IntroJump）を player.gltf に足す。

  python tools/stage_intro/make_intro_clips.py

  ・元の player.gltf / player.bin は tools/stage_intro/player_original/ に一度だけ残す（2回目からは、そこから作り直す）
  ・既存のクリップ・メッシュ・ボーンのバイト列は変えない。新しいクリップのデータは player.bin の後ろに足すだけ
  ・クリップは、どれも Idle の最初の姿勢から始まって、同じ姿勢で終わる（切り替えても飛ばない）
  ・ボーンの回す向きは既存のクリップ（Walk の弾み、Throw の腕上げ、Idle の頭）から読み取った軸を使う

  IntroWave（約1.2秒）：うなずく → 小さく跳ねる → 右手を振る
  IntroPush（約1.0秒）：前かがみで両手を前へ → ぷるぷる力む → 押し切る
  IntroJump（約1.1秒）：ため → 両手を開いて 0.9m 跳ぶ → 着地でつぶれる
"""
import json
import math
import os
import shutil
import struct

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PLAYER = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "player"))
BACKUP = os.path.join(HERE, "player_original")
FPS = 24

os.makedirs(BACKUP, exist_ok=True)
for name in ("player.gltf", "player.bin"):
    if not os.path.exists(os.path.join(BACKUP, name)):
        shutil.copy2(os.path.join(PLAYER, name), os.path.join(BACKUP, name))

gltf = json.load(open(os.path.join(BACKUP, "player.gltf"), encoding="utf-8"))
binary = bytearray(open(os.path.join(BACKUP, "player.bin"), "rb").read())
nodes = {n.get("name"): i for i, n in enumerate(gltf["nodes"])}
joints = gltf["skins"][0]["joints"]


def accessor(index):
    a = gltf["accessors"][index]
    view = gltf["bufferViews"][a["bufferView"]]
    width = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[a["type"]]
    offset = view.get("byteOffset", 0) + a.get("byteOffset", 0)
    return np.frombuffer(bytes(binary), dtype=np.float32, count=a["count"] * width, offset=offset).reshape(a["count"], width)


# --- 基準の姿勢：Idle の最初のフレーム（ボーンの初期値は立ち姿と違う。腕は 32° ずれている） ---
idle = next(a for a in gltf["animations"] if a["name"] == "Idle")
base = {}
for channel in idle["channels"]:
    sampler = idle["samplers"][channel["sampler"]]
    base[(channel["target"]["node"], channel["target"]["path"])] = accessor(sampler["output"])[0].astype(np.float64)


# --- 四元数 ---
def q_axis(axis, degrees):
    axis = np.array(axis, np.float64)
    axis /= np.linalg.norm(axis)
    half = math.radians(degrees) * 0.5
    return np.array([*(axis * math.sin(half)), math.cos(half)])


def q_mul(a, b):
    x1, y1, z1, w1 = a
    x2, y2, z2, w2 = b
    return np.array([w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                     w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
                     w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
                     w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2])


# ボーンのローカル軸（既存のクリップから読み取った向き）
ARM_RAISE = {"Arm_L": (0.96, -0.22, -0.15), "Arm_R": (0.96, 0.22, 0.15)}  # Throw で腕を上げる軸
ARM_SWING = {"Arm_L": (0.0, 0.0, 1.0), "Arm_R": (0.0, 0.0, -1.0)}         # 上げた腕を横へ振る軸（体の前後の軸。親の軸で回す）
LEAN = (1.0, 0.0, 0.0)   # 体・頭を前へ倒す軸（上が前へ）
TILT = (0.0, 0.0, 1.0)   # 首をかしげる軸
FOOT = (1.0, 0.0, 0.0)   # 足を曲げる軸（Walk）


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def bump(t, start, peak, end):
    """start で 0、peak で 1、end で 0 に戻る山（なめらか）"""
    if t <= start or t >= end:
        return 0.0
    if t < peak:
        return smooth((t - start) / (peak - start))
    return 1.0 - smooth((t - peak) / (end - peak))


def pose(rot=None, body_up=0.0, body_scale=(1.0, 1.0, 1.0), root_up=0.0, parent_rot=None):
    """基準の姿勢に、ボーンごとの回転と、体の上下・つぶれ、全身の上下を足した姿勢。
    rot＝ボーン自身の軸で回す / parent_rot＝親（体）の軸で回す（上げた腕を横へ振るなど）。
    root_up＝全身（足も）の上下。足は Body ではなく Root の子なので、跳ぶ高さは Root で出す"""
    rot = rot or {}
    parent_rot = parent_rot or {}
    out = {}
    for joint in joints:
        name = gltf["nodes"][joint]["name"]
        r = base[(joint, "rotation")].copy()
        for axis, degrees in rot.get(name, []):
            if abs(degrees) > 1e-6:
                r = q_mul(r, q_axis(axis, degrees))
        for axis, degrees in parent_rot.get(name, []):
            if abs(degrees) > 1e-6:
                r = q_mul(q_axis(axis, degrees), r)
        t = base[(joint, "translation")].copy()
        s = base[(joint, "scale")].copy()
        if name == "Body":
            t[2] += body_up  # 体の上下（Root の中で z が上）
            s = s * np.array(body_scale)
        if name == "Root":
            t[1] += root_up  # 全身の上下（Rig の中で y が上）
        out[(joint, "rotation")] = r
        out[(joint, "translation")] = t
        out[(joint, "scale")] = s
    return out


def intro_push(t):
    lean = smooth(t / 0.2) * (1.0 - smooth((t - 0.88) / 0.12))     # 前かがみ
    strain = bump(t, 0.2, 0.3, 0.78)                               # 力んでいる間
    shake = math.sin(t * 2 * math.pi * 14) * strain                # ぷるぷる
    push = bump(t, 0.74, 0.82, 0.95)                               # 押し切る
    arms = lean * 78.0 + push * 14.0
    return pose({"Body": [(LEAN, lean * 20.0 + push * 8.0 + shake * 1.6)],
                 "Head": [(LEAN, -lean * 10.0 + shake * 2.0)],
                 "Arm_L": [(ARM_RAISE["Arm_L"], arms)], "Arm_R": [(ARM_RAISE["Arm_R"], arms)],
                 "Tail": [(TILT, shake * 6.0)],
                 "Foot_L": [(FOOT, lean * 12.0)], "Foot_R": [(FOOT, -lean * 8.0)]},
                body_up=-0.04 * lean,
                body_scale=(1.0 + 0.03 * strain + 0.015 * shake, 1.0 - 0.05 * strain, 1.0 + 0.03 * strain))


def intro_jump(t):
    crouch = bump(t, 0.0, 0.18, 0.3)                               # ため
    up = t - 0.26
    height = max(0.0, 4.0 * 0.9 * (up / 0.5) * (1.0 - up / 0.5)) if 0.0 < up < 0.5 else 0.0  # 0.9m 跳ぶ（0.5秒）
    air = bump(t, 0.24, 0.4, 0.8)                                  # 空中：両手を開く
    land = bump(t, 0.74, 0.8, 0.98)                                # 着地でつぶれる
    stretch = bump(t, 0.25, 0.32, 0.5)                             # 跳び上がりで伸びる
    arms = air * 125.0 - crouch * 18.0
    return pose({"Arm_L": [(ARM_RAISE["Arm_L"], arms)],
                 "Arm_R": [(ARM_RAISE["Arm_R"], arms)],
                 "Head": [(LEAN, crouch * 8.0 - air * 10.0)],
                 "Body": [(LEAN, crouch * 6.0)],
                 "Foot_L": [(FOOT, -air * 28.0)], "Foot_R": [(FOOT, -air * 28.0)],
                 "Tail": [(LEAN, air * 20.0)]},
                root_up=height, body_up=-0.06 * crouch - 0.04 * land,
                parent_rot={"Arm_L": [(ARM_SWING["Arm_L"], air * 20.0)], "Arm_R": [(ARM_SWING["Arm_R"], air * 20.0)]},
                body_scale=(1.0 + 0.1 * (crouch + land) - 0.05 * stretch, 1.0 - 0.18 * (crouch + land) + 0.1 * stretch,
                            1.0 + 0.1 * (crouch + land) - 0.05 * stretch))


WAVE_SIDE_SIGN = float(os.environ.get("WAVE_SIDE_SIGN", "-1"))  # 右手を外へ上げる向き


def intro_wave_swing(t):
    """上げた右手を横へ3回振る"""
    window = bump(t, 0.58, 0.68, 1.08)
    return math.sin((t - 0.6) * 2 * math.pi * 3.0) * 26.0 * window


def intro_wave(t):
    nod = bump(t, 0.0, 0.1, 0.24) * 16.0                           # うなずく
    hop = bump(t, 0.22, 0.34, 0.46) * 0.13                         # 小さく跳ねる
    land = bump(t, 0.44, 0.5, 0.6)                                 # 着地で少しつぶれる
    raise_ = smooth((t - 0.45) / 0.15) * (1.0 - smooth((t - 1.02) / 0.18)) * 115.0
    tilt = bump(t, 0.5, 0.75, 1.15) * 10.0
    # 右手は横から上げる（体の前後の軸で外へ）。上げたまま左右に振る
    side = raise_ * WAVE_SIDE_SIGN + intro_wave_swing(t)
    return pose({"Head": [(LEAN, nod), (TILT, tilt)],
                 "Body": [(TILT, -tilt * 0.4)],
                 "Foot_L": [(FOOT, -hop * 120)], "Foot_R": [(FOOT, -hop * 120)]},
                root_up=hop, body_up=-0.03 * land, body_scale=(1.0 + 0.06 * land, 1.0 - 0.1 * land, 1.0 + 0.06 * land),
                parent_rot={"Arm_R": [((0.0, 0.0, 1.0), side)]})


CLIPS = [("IntroWave", 1.2, intro_wave), ("IntroPush", 1.0, intro_push), ("IntroJump", 1.1, intro_jump)]


def append(data, gl_type, count, comp_type, minmax=False):
    """player.bin の後ろにデータを足し、bufferView と accessor を作る"""
    while len(binary) % 4:
        binary.append(0)
    offset = len(binary)
    raw = np.asarray(data, np.float32).tobytes()
    binary.extend(raw)
    gltf["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(raw)})
    acc = {"bufferView": len(gltf["bufferViews"]) - 1, "componentType": 5126, "count": count, "type": gl_type}
    if minmax:
        arr = np.asarray(data, np.float32).reshape(count, -1)
        acc["min"] = [float(v) for v in arr.min(0)]
        acc["max"] = [float(v) for v in arr.max(0)]
    gltf["accessors"].append(acc)
    return len(gltf["accessors"]) - 1


for name, length, func in CLIPS:
    frames = int(round(length * FPS)) + 1
    times = [i / FPS for i in range(frames)]
    poses = [func(t) for t in times]
    poses[0] = pose()   # 最初と最後は立ち姿（Idle の最初）
    poses[-1] = pose()
    time_acc = append(times, "SCALAR", frames, 5126, minmax=True)
    animation = {"name": name, "channels": [], "samplers": []}
    for joint in joints:
        for path, gl_type in (("translation", "VEC3"), ("rotation", "VEC4"), ("scale", "VEC3")):
            values = [p[(joint, path)] for p in poses]
            if path == "rotation":  # 隣のキーと同じ半球に（補間で遠回りしない）
                for i in range(1, len(values)):
                    if np.dot(values[i], values[i - 1]) < 0:
                        values[i] = -values[i]
            out_acc = append(np.array(values).flatten(), gl_type, frames, 5126)
            animation["samplers"].append({"input": time_acc, "output": out_acc, "interpolation": "LINEAR"})
            animation["channels"].append({"sampler": len(animation["samplers"]) - 1, "target": {"node": joint, "path": path}})
    gltf["animations"].append(animation)
    print("clip", name, frames, "frames")

gltf["buffers"][0]["byteLength"] = len(binary)
with open(os.path.join(PLAYER, "player.bin"), "wb") as f:
    f.write(binary)
with open(os.path.join(PLAYER, "player.gltf"), "w", encoding="utf-8") as f:
    json.dump(gltf, f, indent=2, ensure_ascii=False)
# 既存の部分が変わっていないことを確かめる
original = open(os.path.join(BACKUP, "player.bin"), "rb").read()
assert binary[:len(original)] == original, "既存のデータが変わっています"
print("player.bin", len(original), "->", len(binary), "bytes（既存の部分は同じ）")
