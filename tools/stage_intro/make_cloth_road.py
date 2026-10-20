# -*- coding: utf-8 -*-
"""
布ロード（レールの上に敷く布の道）の画像を作る。

  python tools/stage_intro/make_cloth_road.py

  project/resources/road/cloth/cloth_felt.png        端にステッチ（縫い目）のあるフェルト
  project/resources/road/cloth/cloth_felt_plain.png  ステッチなし
    横（u）＝道に沿う向き（1.2m で1回くり返す。テクスチャは横だけくり返して貼られる）
    縦（v）＝道の幅の向き（0＝左のすそ / 0.035＝左のめくれ / 0.10＝左のステッチ / 0.90＝右のステッチ / 1＝右のすそ）
    色は白っぽいフェルト（0.72）。ゲームがレールごとの「布の色」を掛ける（ステッチは 1.0 なので少し明るく見える）
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
TEX = os.path.join(PROJECT, "resources", "craft", "textures")
OUT = os.path.join(PROJECT, "resources", "road", "cloth")
os.makedirs(OUT, exist_ok=True)

W, H = 256, 128
FELT = 0.72


def felt_noise():
    """フェルトのざらざら（キットの felt_grass の明るさを使う。横にくり返してもつながるように）"""
    src = Image.open(os.path.join(TEX, "felt_grass.png")).convert("L").resize((W, W))
    g = np.asarray(src).astype(np.float32)[:H, :]
    g = (g - g.mean()) / (g.std() + 1e-5)
    return np.clip(FELT + 0.045 * g, 0.0, 1.0)


def make(stitch):
    v = (np.arange(H) + 0.5) / H
    shade = np.ones(H, np.float32)
    # 端のめくれ（外へ向かって少し暗く）と、すそ（地面に入る所はもっと暗く）
    shade = np.where(v < 0.035, 0.62 + 0.3 * (v / 0.035), shade)
    shade = np.where(v > 0.965, 0.62 + 0.3 * ((1.0 - v) / 0.035), shade)
    shade = np.where((v >= 0.035) & (v < 0.07), 0.92 + 0.08 * ((v - 0.035) / 0.035), shade)
    shade = np.where((v <= 0.965) & (v > 0.93), 0.92 + 0.08 * ((0.965 - v) / 0.035), shade)
    img = felt_noise() * shade[:, None]
    gray = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8)).convert("RGB")
    if stitch:
        d = ImageDraw.Draw(gray)
        for vy in (0.10, 0.90):
            y = int(vy * H)
            # 縫い目の穴（少し暗い点）と糸（明るい破線）。横にくり返してもつながる間隔（256/16）
            for x in range(0, W, 16):
                d.line([(x + 2, y), (x + 11, y)], fill=(255, 255, 250), width=3)
                d.ellipse([x - 1, y - 1, x + 1, y + 1], fill=(120, 120, 120))
    gray = gray.filter(ImageFilter.GaussianBlur(0.4))
    return gray


make(True).save(os.path.join(OUT, "cloth_felt.png"))
make(False).save(os.path.join(OUT, "cloth_felt_plain.png"))
print("wrote", OUT)
