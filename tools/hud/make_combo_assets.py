# -*- coding: utf-8 -*-
"""
卵の「奥投げコンボ」の HUD 素材（make_hud_assets.py と同じ、段ボール・フェルト・ふち付きの太い字）。

  python tools/hud/make_combo_assets.py

  project/resources/ui/hud_combo.png    「コンボ」の札（オレンジのフェルトのリボンに、白い字・茶色のふち）
  project/resources/ui/hud_flower.png   かくれ花（ピンクのフェルトの花びら・黄色の真ん中・にっこり顔）
"""
import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
OUT = os.path.join(PROJECT, "resources", "ui")
FONT = r"C:\Windows\Fonts\BIZ-UDGothicB.ttc"
BROWN = (70, 45, 20)
ORANGE = (240, 140, 50)
PINK = (245, 130, 170)
YELLOW = (250, 210, 60)


def grain(size, seed, scale=2):
    r = random.Random(seed)
    small = Image.new("L", (max(1, size[0] // scale), max(1, size[1] // scale)))
    small.putdata([r.randint(0, 255) for _ in range(small.width * small.height)])
    return small.resize(size, Image.BILINEAR).filter(ImageFilter.GaussianBlur(0.7))


def felt(mask, color, seed):
    """mask の形のフェルト（けば）"""
    g = grain(mask.size, seed, 5).point(lambda v: 215 + v // 7)
    face = Image.merge("RGB", [ImageChops.multiply(Image.new("L", mask.size, c), g) for c in color])
    out = Image.new("RGBA", mask.size, (0, 0, 0, 0))
    out.paste(face, (0, 0), mask)
    return out


def outlined_text(draw_img, xy, text, font, fill, outline, width):
    d = ImageDraw.Draw(draw_img)
    d.text(xy, text, font=font, fill=fill, stroke_width=width, stroke_fill=outline)


def save_combo():
    w, h = 520, 150
    s = 2
    img = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
    # リボン：左右の端が切り込みになった帯（茶色のふち→オレンジのフェルト）
    def ribbon(inset):
        m = Image.new("L", img.size, 0)
        d = ImageDraw.Draw(m)
        x0, x1, y0, y1 = (12 + inset) * s, (w - 12 - inset) * s, (22 + inset) * s, (h - 22 - inset) * s
        notch = 26 * s
        d.polygon([(x0, y0), (x1, y0), (x1 - notch, (y0 + y1) // 2), (x1, y1), (x0, y1), (x0 + notch, (y0 + y1) // 2)], fill=255)
        return m
    img.paste(BROWN + (255,), (0, 0), ribbon(0))
    img.alpha_composite(felt(ribbon(6), ORANGE, 11))
    # 白いステッチ（上下）
    d = ImageDraw.Draw(img)
    for y in (36 * s, (h - 36) * s):
        for x in range(60 * s, (w - 60) * s, 22 * s):
            d.line([(x, y), (x + 11 * s, y)], fill=(255, 250, 235, 230), width=3 * s)
    font = ImageFont.truetype(FONT, 78 * s)
    text = "コンボ"
    box = d.textbbox((0, 0), text, font=font, stroke_width=7 * s)
    tx = (img.width - (box[2] - box[0])) // 2 - box[0]
    ty = (img.height - (box[3] - box[1])) // 2 - box[1]
    outlined_text(img, (tx, ty), text, font, (255, 255, 255), BROWN, 7 * s)
    img = img.resize((w, h), Image.LANCZOS)
    img.save(os.path.join(OUT, "hud_combo.png"))


def save_flower():
    size = 128
    s = 4
    S = size * s
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    c = S / 2
    petal_r, petal_d = S * 0.20, S * 0.25
    # 花びら（茶色のふち → ピンクのフェルト）
    rim = Image.new("L", (S, S), 0)
    face = Image.new("L", (S, S), 0)
    for k in range(5):
        a = -math.pi / 2 + k * 2 * math.pi / 5
        px, py = c + math.cos(a) * petal_d, c + math.sin(a) * petal_d
        ImageDraw.Draw(rim).ellipse([px - petal_r, py - petal_r, px + petal_r, py + petal_r], fill=255)
        ImageDraw.Draw(face).ellipse([px - petal_r + 5 * s, py - petal_r + 5 * s, px + petal_r - 5 * s, py + petal_r - 5 * s], fill=255)
    img.paste(BROWN + (255,), (0, 0), rim)
    img.alpha_composite(felt(face, PINK, 5))
    # 真ん中（黄色）とにっこり顔
    mid = Image.new("L", (S, S), 0)
    r = S * 0.17
    ImageDraw.Draw(mid).ellipse([c - r - 4 * s, c - r - 4 * s, c + r + 4 * s, c + r + 4 * s], fill=255)
    img.paste(BROWN + (255,), (0, 0), mid)
    inner = Image.new("L", (S, S), 0)
    ImageDraw.Draw(inner).ellipse([c - r, c - r, c + r, c + r], fill=255)
    img.alpha_composite(felt(inner, YELLOW, 9))
    d = ImageDraw.Draw(img)
    for ex in (-0.35, 0.35):
        d.ellipse([c + ex * r - 3 * s, c - 0.3 * r - 5 * s, c + ex * r + 3 * s, c - 0.3 * r + 5 * s], fill=BROWN + (255,))
    d.arc([c - 0.45 * r, c - 0.2 * r, c + 0.45 * r, c + 0.5 * r], 20, 160, fill=BROWN + (255,), width=3 * s)
    img = img.resize((size, size), Image.LANCZOS)
    img.save(os.path.join(OUT, "hud_flower.png"))


save_combo()
save_flower()
print("wrote", os.path.join(OUT, "hud_combo.png"), os.path.join(OUT, "hud_flower.png"))
