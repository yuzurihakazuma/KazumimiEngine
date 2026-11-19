# -*- coding: utf-8 -*-
"""裏から見る側（囲いの手前の辺）の、夕方の書き割りを作る。

  python tools/stage_intro/make_evening_assets.py

  テクスチャ（project/resources/craft/textures）
    paper_sky_evening.png … 夕焼けの紙の空。上が藍 → 紫 → 橙 → 地平線の近くは明るい黄色（縦 1 枚のグラデーション。横はくり返す）
    felt_hill_1_dusk.png  … 遠い丘（夕もやの藤色）
    felt_hill_2_dusk.png  … 近い丘（暮れた青紫）
    paper_cloud_evening.png … 夕焼けの雲（桃色がかった橙）
  モデル（project/resources/craft/models。形は元のモデルと同じ。画像だけ違う）
    sky_backdrop_evening  … 空の板。縦の UV を「板の下から上で 1 枚」に直した物（元は 4m ごとにくり返していて、グラデーションが縞になる）
    hill_backdrop_far_dusk / hill_backdrop_00_dusk / hill_backdrop_01_dusk / cloud_cutout_00_evening / cloud_cutout_01_evening
      … 元の .bin をそのまま使い、画像だけ差し替えた .gltf

  紙・フェルトのざらつきは、元の画像の明るさのむらをそのまま使う（色だけ変える）。
  置くのは make_backdrop_ring.py（手前の辺だけ夕方。表で遊んでいる時には見えない）
"""
import json
import os
import struct

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
TEX = os.path.join(PROJECT, "resources", "craft", "textures")
MODELS = os.path.join(PROJECT, "resources", "craft", "models")

# 空の色（板の上 0 → 下 1。sRGB）。板は y -20〜60m に立つので、0.75 あたり（y 0m）が地平線
SKY_STOPS = [
    (0.00, (70, 62, 140)),
    (0.30, (150, 92, 150)),
    (0.55, (238, 130, 98)),
    (0.75, (252, 182, 112)),
    (1.00, (255, 216, 152)),
]
FAR_HILL = (176, 140, 172)    # 遠い丘：夕もやの藤色
NEAR_HILL = (112, 104, 148)   # 近い丘：暮れた青紫
CLOUD = (255, 192, 172)       # 雲：桃色がかった橙


def load(name):
    return np.asarray(Image.open(os.path.join(TEX, name)).convert("RGB")).astype(np.float64)


def grain(rgb):
    """元の画像の明るさのむら（平均 1。強すぎるむらは抑える）"""
    lum = rgb @ np.array([0.299, 0.587, 0.114])
    return np.clip(lum / lum.mean(), 0.8, 1.2)


def save(name, rgb):
    Image.fromarray(np.clip(rgb + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(TEX, name))
    print("wrote textures/" + name)


def recolor(src, dst, color):
    rgb = load(src)
    save(dst, grain(rgb)[..., None] * np.array(color, dtype=np.float64))


def sky_texture():
    detail = 1.0 + (grain(load("paper_sky.png")) - 1.0) * 0.45  # むらは弱め（縦に引きのばされて、すじに見えた）
    detail = np.vstack([detail, detail])          # 縦 1024（板は横 20m・縦 80m で 1 枚。むらが縦にのびすぎないよう 2 回重ねる）
    h, w = detail.shape
    t = (np.arange(h) + 0.5) / h
    stops_t = [s[0] for s in SKY_STOPS]
    column = np.stack([np.interp(t, stops_t, [s[1][c] for s in SKY_STOPS]) for c in range(3)], axis=1)
    save("paper_sky_evening.png", column[:, None, :] * detail[..., None])


def copy_gltf(src, dst, images, buffer_uri=None):
    """元の .gltf の画像だけ差し替えた物を書く（images：元の画像名 → 新しい画像名）。.bin は元の物（buffer_uri で別にもできる）"""
    g = json.load(open(os.path.join(MODELS, src + ".gltf"), encoding="utf-8"))
    for image in g["images"]:
        name = image.get("name") or os.path.basename(image["uri"])
        if name in images:
            image["name"] = images[name]
            image["uri"] = "../textures/" + images[name]
    for material in g.get("materials", []):
        for old, new in images.items():
            if material.get("name") == "M_" + os.path.splitext(old)[0]:
                material["name"] = "M_" + os.path.splitext(new)[0]
    g["buffers"][0]["uri"] = buffer_uri or g["buffers"][0]["uri"]
    if g.get("meshes"):
        g["meshes"][0]["name"] = dst
    for node in g.get("nodes", []):
        if node.get("name") == src:
            node["name"] = dst
    with open(os.path.join(MODELS, dst + ".gltf"), "w", encoding="utf-8") as f:
        json.dump(g, f, ensure_ascii=False, indent=1)
    print("wrote models/" + dst + ".gltf")
    return g


def sky_model():
    """空の板：画像の差し替え＋紙の面の縦の UV を「下から上で 1 枚」に（横のくり返しはそのまま）"""
    g = json.load(open(os.path.join(MODELS, "sky_backdrop.gltf"), encoding="utf-8"))
    data = bytearray(open(os.path.join(MODELS, g["buffers"][0]["uri"]), "rb").read())

    def view(index):
        a = g["accessors"][index]
        v = g["bufferViews"][a["bufferView"]]
        count = {"VEC3": 3, "VEC2": 2}[a["type"]]
        offset = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = v.get("byteStride", 4 * count)
        return a, offset, stride, count

    primitive = next(p for p in g["meshes"][0]["primitives"] if p.get("material") == 0)
    pos_a, pos_off, pos_stride, _ = view(primitive["attributes"]["POSITION"])
    uv_a, uv_off, uv_stride, _ = view(primitive["attributes"]["TEXCOORD_0"])
    ys = [struct.unpack_from("<f", data, pos_off + i * pos_stride + 4)[0] for i in range(pos_a["count"])]
    y0, y1 = min(ys), max(ys)
    vs = []
    for i, y in enumerate(ys):
        v = 0.004 + 0.992 * (1.0 - (y - y0) / (y1 - y0))   # 上＝画像の上。端はくり返しのにじみを避けて少し内側
        struct.pack_into("<f", data, uv_off + i * uv_stride + 4, v)
        vs.append(v)
    if "min" in uv_a:
        uv_a["min"][1], uv_a["max"][1] = min(vs), max(vs)
    open(os.path.join(MODELS, "sky_backdrop_evening.bin"), "wb").write(bytes(data))
    print("wrote models/sky_backdrop_evening.bin")

    g["images"][0].update({"name": "paper_sky_evening.png", "uri": "../textures/paper_sky_evening.png"})
    g["materials"][0]["name"] = "M_paper_sky_evening"
    g["buffers"][0]["uri"] = "sky_backdrop_evening.bin"
    g["meshes"][0]["name"] = "sky_backdrop_evening"
    for node in g.get("nodes", []):
        if node.get("name") == "sky_backdrop":
            node["name"] = "sky_backdrop_evening"
    with open(os.path.join(MODELS, "sky_backdrop_evening.gltf"), "w", encoding="utf-8") as f:
        json.dump(g, f, ensure_ascii=False, indent=1)
    print("wrote models/sky_backdrop_evening.gltf")


def main():
    sky_texture()
    recolor("felt_hill_1.png", "felt_hill_1_dusk.png", FAR_HILL)
    recolor("felt_hill_2.png", "felt_hill_2_dusk.png", NEAR_HILL)
    recolor("paper_white.png", "paper_cloud_evening.png", CLOUD)
    sky_model()
    copy_gltf("hill_backdrop_far", "hill_backdrop_far_dusk", {"felt_hill_1.png": "felt_hill_1_dusk.png"})
    for name in ("hill_backdrop_00", "hill_backdrop_01"):
        copy_gltf(name, name + "_dusk", {"felt_hill_2.png": "felt_hill_2_dusk.png"})
    for name in ("cloud_cutout_00", "cloud_cutout_01"):
        copy_gltf(name, name + "_evening", {"paper_white.png": "paper_cloud_evening.png"})


if __name__ == "__main__":
    main()
