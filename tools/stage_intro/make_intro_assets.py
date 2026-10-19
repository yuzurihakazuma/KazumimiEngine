# -*- coding: utf-8 -*-
"""
ステージ開始の演技で使う素材を作る（ロールとキラキラ）。

  python tools/stage_intro/make_intro_assets.py

  project/resources/stage_intro/roll/Roll_Felt.obj / .mtl / .png
      巻いた布のロール。X が筒の軸（長さ 1.8・半径 1。ゲームが半径の大きさへ縮める）。
      テクスチャの左半分が筒の横（フェルト）、右半分が端（巻いた渦）
  project/resources/stage_intro/fx/fx_sparkle_yellow.png / fx_sparkle_pink.png
      頭の上に出すキラキラ（4つの角の星。パーティクルは加算で描くので、ふちはやわらかく暗く）
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
TEX = os.path.join(PROJECT, "resources", "craft", "textures")
ROLL_DIR = os.path.join(PROJECT, "resources", "stage_intro", "roll")
FX_DIR = os.path.join(PROJECT, "resources", "stage_intro", "fx")
os.makedirs(ROLL_DIR, exist_ok=True)
os.makedirs(FX_DIR, exist_ok=True)

FELT = (0.62, 0.84, 0.24)  # ステージ開始カードの名前の帯と同じ黄緑（resources/stage_intro/stage1.json の bannerColor）


def felt(size, color):
    src = Image.open(os.path.join(TEX, "felt_grass.png")).convert("L")
    out = Image.new("L", size)
    for y in range(0, size[1], src.height):
        for x in range(0, size[0], src.width):
            out.paste(src, (x, y))
    g = np.asarray(out).astype(np.float32)
    g = 0.9 + 0.1 * (g - g.mean()) / (g.std() + 1e-5) * 0.5
    rgb = np.stack([g * 255 * color[0], g * 255 * color[1], g * 255 * color[2]], -1)
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))


def make_roll_texture():
    w, h = 512, 256
    img = Image.new("RGB", (w, h))
    side = felt((256, 256), FELT)
    d = ImageDraw.Draw(side)
    for k in range(-2, 7):  # 巻いた布の重なりの線（斜め）と縫い目
        x = k * 48
        d.line([(x, 0), (x + 60, 256)], fill=(70, 95, 25), width=5)
        d.line([(x + 8, 0), (x + 68, 256)], fill=(220, 240, 170), width=2)
    for y in (22, 234):
        for x in range(0, 256, 22):
            d.line([(x, y), (x + 12, y)], fill=(250, 250, 240), width=4)
    img.paste(side, (0, 0))
    # 端：巻いた布の渦（濃い・薄いの層を重ねる）
    cap = felt((256, 256), (FELT[0] * 0.9, FELT[1] * 0.9, FELT[2] * 0.9))
    cd = ImageDraw.Draw(cap)
    cx = cy = 128
    pts = []
    for i in range(900):
        t = i / 900 * 5 * 2 * math.pi
        r = 8 + t / (2 * math.pi) * 22
        pts.append((cx + r * math.cos(t), cy + r * math.sin(t)))
    cd.line(pts, fill=(60, 85, 20), width=6)
    cd.line([(x + 2, y + 2) for x, y in pts], fill=(225, 240, 180), width=2)
    cd.ellipse([2, 2, 254, 254], outline=(50, 70, 18), width=6)
    img.paste(cap, (256, 0))
    img.save(os.path.join(ROLL_DIR, "Roll_Felt.png"))


def make_roll_obj():
    seg = 32
    half = 0.9
    v, vt, vn, f = [], [], [], []
    # 横（筒の周り）
    for i in range(seg + 1):
        a = 2 * math.pi * i / seg
        y, z = math.cos(a), math.sin(a)
        for x in (-half, half):
            v.append((x, y, z))
            vn.append((0.0, y, z))
            vt.append((0.5 * (x + half) / (2 * half), 1.0 - i / seg))  # 左半分（u 0〜0.5）
    for i in range(seg):
        a, b, c, e = 2 * i, 2 * i + 1, 2 * (i + 1), 2 * (i + 1) + 1
        f.append((a, c, b))
        f.append((b, c, e))
    # 両端（巻いた渦）。テクスチャの右半分の円
    for sign in (-1.0, 1.0):
        center = len(v)
        v.append((sign * half, 0.0, 0.0))
        vn.append((sign, 0.0, 0.0))
        vt.append((0.75, 0.5))
        ring = len(v)
        for i in range(seg):
            a = 2 * math.pi * i / seg
            v.append((sign * half, math.cos(a), math.sin(a)))
            vn.append((sign, 0.0, 0.0))
            vt.append((0.75 + 0.24 * math.cos(a), 0.5 - 0.48 * math.sin(a)))
        for i in range(seg):
            p, q = ring + i, ring + (i + 1) % seg
            f.append((center, p, q) if sign > 0 else (center, q, p))
    with open(os.path.join(ROLL_DIR, "Roll_Felt.mtl"), "w", encoding="utf-8") as mtl:
        mtl.write("newmtl roll_felt\nKa 1.0 1.0 1.0\nKd 1.0 1.0 1.0\nKs 0.0 0.0 0.0\nNs 1.0\nmap_Kd Roll_Felt.png\n")
    with open(os.path.join(ROLL_DIR, "Roll_Felt.obj"), "w", encoding="utf-8") as obj:
        obj.write("# 巻いた布のロール（make_intro_assets.py。X が筒の軸、長さ 1.8・半径 1）\nmtllib Roll_Felt.mtl\no Roll_Felt\nusemtl roll_felt\n")
        for x, y, z in v:
            obj.write("v %.5f %.5f %.5f\n" % (x, y, z))
        for u, w in vt:
            obj.write("vt %.5f %.5f\n" % (u, w))
        for x, y, z in vn:
            obj.write("vn %.5f %.5f %.5f\n" % (x, y, z))
        for a, b, c in f:
            obj.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a + 1, a + 1, a + 1, b + 1, b + 1, b + 1, c + 1, c + 1, c + 1))


def make_sparkle(name, color):
    """4つの角の星（中心は白く、まわりに色の光）"""
    s = 4
    size = 128 * s
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    glow = Image.new("L", (size, size), 0)
    ImageDraw.Draw(glow).ellipse([size * 0.3, size * 0.3, size * 0.7, size * 0.7], fill=150)
    glow = glow.filter(ImageFilter.GaussianBlur(size * 0.08))
    star = Image.new("L", (size, size), 0)
    c = size / 2
    pts = []
    for i in range(8):
        r = size * (0.48 if i % 2 == 0 else 0.11)
        a = math.pi * i / 4 - math.pi / 2
        pts.append((c + r * math.cos(a), c + r * math.sin(a)))
    ImageDraw.Draw(star).polygon(pts, fill=255)
    star = star.filter(ImageFilter.GaussianBlur(s * 1.5))
    core = Image.new("L", (size, size), 0)
    ImageDraw.Draw(core).ellipse([c - size * 0.07, c - size * 0.07, c + size * 0.07, c + size * 0.07], fill=255)
    core = core.filter(ImageFilter.GaussianBlur(s * 3))
    base = Image.new("RGBA", (size, size), color + (255,))
    base.putalpha(Image.fromarray(np.maximum(np.asarray(glow), np.asarray(star))))
    img.alpha_composite(base)
    white = Image.new("RGBA", (size, size), (255, 255, 250, 255))
    white.putalpha(core)
    img.alpha_composite(white)
    img.resize((128, 128), Image.LANCZOS).save(os.path.join(FX_DIR, name))


make_roll_texture()
make_roll_obj()
make_sparkle("fx_sparkle_yellow.png", (255, 220, 60))
make_sparkle("fx_sparkle_pink.png", (255, 120, 180))
print("wrote", ROLL_DIR, FX_DIR)
