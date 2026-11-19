# -*- coding: utf-8 -*-
"""
敵のモデル（glb）に、材質の色を並べた小さな画像（パレット）を貼る。

  python tools/enemy/bake_enemy_palette.py

  敵の glb は画像を持たず、色を材質ごとの色（baseColorFactor）だけで持っている。
  エンジンは材質の色を読まないので、画像が無い時の代わりの uvChecker.png が貼られて虹色になっていた。
  プレイヤー（player.gltf ＋ player_palette.png）と同じやり方にそろえる：
    ・材質の色を 16px の四角で並べた <名前>_palette.png を作る（glTF の色は linear なので sRGB にして書く）
    ・各部品の UV を、その材質の色の四角の真ん中へ全部寄せる
    ・glb から外の画像（uri）を指す。エンジンは glb の中に埋め込んだ画像は読めないため
  元の glb は tools/enemy/source/ に残し、いつもそこから作り直す（何回実行しても同じ結果）
"""
import json
import os
import shutil
import struct

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
ENEMY = os.path.join(PROJECT, "resources", "enemy")
SOURCE = os.path.join(HERE, "source")
NAMES = ["enemy_ground", "enemy_air", "enemy_plant"]
CELL, GRID = 16, 4  # 1色 16px の四角を 4×4 に並べる（16色まで）


def to_srgb(c):
    c = max(0.0, min(1.0, c))
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def read_glb(path):
    data = open(path, "rb").read()
    length = struct.unpack("<I", data[12:16])[0]
    content = json.loads(data[20:20 + length])
    offset = 20 + length
    bin_length = struct.unpack("<I", data[offset:offset + 4])[0]
    binary = bytearray(data[offset + 8:offset + 8 + bin_length])
    return content, binary


def write_glb(path, content, binary):
    text = json.dumps(content, separators=(",", ":")).encode("utf-8")
    text += b" " * ((4 - len(text) % 4) % 4)
    binary = bytes(binary) + b"\0" * ((4 - len(binary) % 4) % 4)
    total = 12 + 8 + len(text) + 8 + len(binary)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(text), 0x4E4F534A) + text)
        f.write(struct.pack("<II", len(binary), 0x004E4942) + binary)


def bake(name):
    os.makedirs(SOURCE, exist_ok=True)
    source = os.path.join(SOURCE, name + ".glb")
    if not os.path.exists(source):
        shutil.copy2(os.path.join(ENEMY, name + ".glb"), source)  # 初回だけ、元の glb を取っておく
    content, binary = read_glb(source)
    materials = content["materials"]
    assert len(materials) <= GRID * GRID, "色が多すぎる"

    # パレット：材質の色を順に並べる（色が無い材質は白）
    palette = Image.new("RGBA", (CELL * GRID, CELL * GRID), (255, 255, 255, 255))
    centers = []
    for index, material in enumerate(materials):
        pbr = material.setdefault("pbrMetallicRoughness", {})
        color = pbr.pop("baseColorFactor", [1.0, 1.0, 1.0, 1.0])
        rgba = tuple(int(round(to_srgb(c) * 255)) for c in color[:3]) + (255,)
        column, row = index % GRID, index // GRID
        palette.paste(rgba, (column * CELL, row * CELL, (column + 1) * CELL, (row + 1) * CELL))
        centers.append(((column + 0.5) / GRID, (row + 0.5) / GRID))
        pbr["baseColorTexture"] = {"index": 0}
    palette.save(os.path.join(ENEMY, name + "_palette.png"))

    # 各部品の UV を、その材質の色の四角の真ん中へ
    for mesh in content["meshes"]:
        for primitive in mesh["primitives"]:
            u, v = centers[primitive.get("material", 0)]
            accessor = content["accessors"][primitive["attributes"]["TEXCOORD_0"]]
            assert accessor["componentType"] == 5126 and accessor["type"] == "VEC2"
            view = content["bufferViews"][accessor["bufferView"]]
            stride = view.get("byteStride", 8)
            start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
            for k in range(accessor["count"]):
                struct.pack_into("<ff", binary, start + k * stride, u, v)
            accessor["min"], accessor["max"] = [u, v], [u, v]

    content["images"] = [{"uri": name + "_palette.png"}]
    content["samplers"] = [{"magFilter": 9728, "minFilter": 9728}]
    content["textures"] = [{"source": 0, "sampler": 0}]
    write_glb(os.path.join(ENEMY, name + ".glb"), content, binary)
    print("baked", name, len(materials), "colors")


for enemy in NAMES:
    bake(enemy)
