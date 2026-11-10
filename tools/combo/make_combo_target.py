# -*- coding: utf-8 -*-
"""
コンボの的（combo_target）：卵のコンボの「始まり」の印。棒に付いた紙の看板（menu_target と同じ形）に、紙の的の絵を貼る。

  python tools/combo/make_combo_target.py

  project/resources/craft/textures/combo_target_face.png  紙の的（赤と白の輪・真ん中に黄色い星）
  project/resources/craft/models/combo_target.gltf         menu_target.gltf の写し（形のデータ menu_target.bin を共有。顔の絵だけ差し替え）
"""
import json
import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
CRAFT = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "craft"))
RED = (222, 72, 60)
WHITE = (250, 246, 236)
YELLOW = (250, 205, 60)
BROWN = (80, 50, 25)


def grain(size, seed, scale=3):
    r = random.Random(seed)
    small = Image.new("L", (size[0] // scale, size[1] // scale))
    small.putdata([r.randint(0, 255) for _ in range(small.width * small.height)])
    return small.resize(size, Image.BILINEAR).filter(ImageFilter.GaussianBlur(1.0))


def paper(color, size, seed):
    g = grain(size, seed).point(lambda v: 225 + v // 9)
    return Image.merge("RGB", [ImageChops.multiply(Image.new("L", size, c), g) for c in color])


def face():
    size = (512, 512)
    base = Image.open(os.path.join(CRAFT, "textures", "menu_placeholder.png")).convert("RGB").resize(size)
    c = 256
    # 紙の輪を外から順に重ねる（それぞれ少しずらした茶色の影＝切り抜いて貼った感じ）
    rings = [(212, RED), (170, WHITE), (128, RED), (86, WHITE), (46, RED)]
    for i, (r, color) in enumerate(rings):
        mask = Image.new("L", size, 0)
        ImageDraw.Draw(mask).ellipse([c - r, c - r, c + r, c + r], fill=255)
        shadow = mask.filter(ImageFilter.GaussianBlur(4))
        base.paste(BROWN, (4, 6), shadow.point(lambda v: v * 0.45))
        base.paste(paper(color, size, 7 + i), (0, 0), mask)
        ImageDraw.Draw(base).ellipse([c - r, c - r, c + r, c + r], outline=BROWN, width=4)
    # 真ん中の黄色い星
    star = []
    for k in range(10):
        a = -math.pi / 2 + k * math.pi / 5
        rr = 34 if k % 2 == 0 else 15
        star.append((c + math.cos(a) * rr, c + math.sin(a) * rr))
    ImageDraw.Draw(base).polygon(star, fill=YELLOW, outline=BROWN, width=3)
    base.save(os.path.join(CRAFT, "textures", "combo_target_face.png"))


def model():
    src = os.path.join(CRAFT, "models", "menu_target.gltf")
    text = open(src, encoding="utf-8").read()
    g = json.loads(text)
    for image in g["images"]:
        if image.get("uri", "").endswith("menu_placeholder.png"):
            image["uri"] = "../textures/combo_target_face.png"
    for node in g.get("nodes", []):
        if node.get("name") == "menu_target":
            node["name"] = "combo_target"
    for mesh in g.get("meshes", []):
        if mesh.get("name") == "menu_target":
            mesh["name"] = "combo_target"
    # 形のデータ（.bin）は menu_target.bin をそのまま使う（uri は同じフォルダの menu_target.bin のまま）
    json.dump(g, open(os.path.join(CRAFT, "models", "combo_target.gltf"), "w", encoding="utf-8"), indent=1)


face()
model()
print("wrote combo_target.gltf / combo_target_face.png")
