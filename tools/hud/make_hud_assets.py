# -*- coding: utf-8 -*-
"""
プレイ中の HUD（左上）の素材を作る。ワールドマップの情報パネル・ゲームオーバーの画面と同じ、
段ボールの札とフェルトの部品・ふち付きの太い数字にそろえる。

  python tools/hud/make_hud_assets.py

  project/resources/ui/hud_board.png       段ボールの札（白いステッチ。左上に1枚、横に伸ばして使う）
  project/resources/ui/hud_egg.png         持っている卵（ステージ開始カードの卵の切り絵を小さく）
  project/resources/ui/hud_egg_empty.png   空いている所（点線の卵の形）
  project/resources/ui/hud_belly.png       お腹にためた分（オレンジのフェルトの丸）
  project/resources/ui/hud_coin.png        コイン（金色のフェルト）
  project/resources/ui/hud_digits.png      数字の並び「0123456789/×+」（1文字 64×96。白い字に茶色のふち）
"""
import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
OUT = os.path.join(PROJECT, "resources", "ui")
os.makedirs(OUT, exist_ok=True)

FONT = r"C:\Windows\Fonts\BIZ-UDGothicB.ttc"
BROWN = (70, 45, 20)
KRAFT = (198, 152, 100)
CREAM = (250, 244, 230)
DIGITS = "0123456789/×+"


def grain(size, seed, scale=2):
    r = random.Random(seed)
    small = Image.new("L", (max(1, size[0] // scale), max(1, size[1] // scale)))
    small.putdata([r.randint(0, 255) for _ in range(small.width * small.height)])
    return small.resize(size, Image.BILINEAR).filter(ImageFilter.GaussianBlur(0.7))


def felt_circle(size, color, seed, rim):
    """フェルトの丸：けば＋茶色のふち＋左上のつや"""
    s = size * 4
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([2, 2, s - 3, s - 3], fill=BROWN + (255,))
    inner = Image.new("L", (s, s), 0)
    ImageDraw.Draw(inner).ellipse([rim * 4, rim * 4, s - rim * 4 - 1, s - rim * 4 - 1], fill=255)
    g = grain((s, s), seed, 6).point(lambda v: 220 + v // 8)
    face = Image.merge("RGB", [ImageChops.multiply(Image.new("L", (s, s), c), g) for c in color])
    img.paste(face, (0, 0), inner)
    shine = Image.new("L", (s, s), 0)
    ImageDraw.Draw(shine).ellipse([s * 0.25, s * 0.2, s * 0.45, s * 0.36], fill=140)
    img.paste((255, 255, 240, 255), (0, 0), shine.filter(ImageFilter.GaussianBlur(s * 0.03)))
    return img.resize((size, size), Image.LANCZOS)


def save_board():
    """段ボールの札：角の丸い板に白いステッチ（左右を伸ばしても崩れないよう、ステッチは横に続く点線）"""
    w, h = 420, 150
    card = Image.new("RGB", (w, h), KRAFT)
    d = ImageDraw.Draw(card)
    for y in range(0, h, 7):
        d.line([(0, y), (w, y)], fill=(188, 142, 92), width=2)
    d.rounded_rectangle([6, 6, w - 7, h - 7], 20, outline=CREAM, width=3)
    for x in range(22, w - 22, 16):
        d.line([(x, 15), (x + 8, 15)], fill=CREAM, width=3)
        d.line([(x, h - 16), (x + 8, h - 16)], fill=CREAM, width=3)
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], 26, fill=255)
    pad = 14
    img = Image.new("RGBA", (w + pad * 2, h + pad * 2), (0, 0, 0, 0))
    shadow = Image.new("L", img.size, 0)
    ImageDraw.Draw(shadow).rounded_rectangle([pad + 4, pad + 7, pad + w + 4, pad + h + 7], 26, fill=120)
    img.paste((30, 18, 10, 255), (0, 0), shadow.filter(ImageFilter.GaussianBlur(5)))
    img.paste(card, (pad, pad), mask)
    img.save(os.path.join(OUT, "hud_board.png"))


def save_eggs():
    src = os.path.join(PROJECT, "resources", "kamishibai", "intro_char_egg.png")
    egg = Image.open(src).convert("RGBA")
    egg.thumbnail((96, 118), Image.LANCZOS)
    egg.save(os.path.join(OUT, "hud_egg.png"))
    # 空いている所：卵の形の、うすい点線
    w, h = egg.size
    alpha = egg.split()[3].point(lambda v: 255 if v > 128 else 0)
    edge = ImageChops.subtract(alpha, alpha.filter(ImageFilter.MinFilter(9)))
    dots = Image.new("L", (w, h), 0)
    dots.putdata([255 if ((i % w) + (i // w)) // 6 % 2 == 0 else 0 for i in range(w * h)])
    empty = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    empty.paste((255, 255, 255, 70), (0, 0), alpha)
    empty.paste((90, 60, 30, 200), (0, 0), ImageChops.multiply(edge, dots))
    empty.save(os.path.join(OUT, "hud_egg_empty.png"))


def save_coin():
    img = felt_circle(96, (250, 200, 46), 5, 7)
    d = ImageDraw.Draw(img)
    d.ellipse([28, 28, 68, 68], outline=(214, 150, 26, 255), width=4)
    d.text((48, 49), "★", font=ImageFont.truetype(r"C:\Windows\Fonts\msgothic.ttc", 30), fill=(214, 150, 26, 255), anchor="mm")
    img.save(os.path.join(OUT, "hud_coin.png"))


def save_belly():
    felt_circle(48, (252, 150, 50), 9, 5).save(os.path.join(OUT, "hud_belly.png"))


def save_digits():
    """数字の並び（ワールドマップの digits.png と同じ書き方。×と＋を足した）"""
    cw, ch = 64, 96
    img = Image.new("RGBA", (cw * len(DIGITS), ch), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(FONT, 72)
    for i, c in enumerate(DIGITS):
        size_font = font if c not in "×+" else ImageFont.truetype(FONT, 60)
        d.text((i * cw + cw / 2, ch / 2 + 2), c, font=size_font, fill=(255, 255, 250), anchor="mm", stroke_width=6, stroke_fill=BROWN)
    img.save(os.path.join(OUT, "hud_digits.png"))


save_board()
save_eggs()
save_coin()
save_belly()
save_digits()
print("wrote", OUT)
