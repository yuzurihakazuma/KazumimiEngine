# -*- coding: utf-8 -*-
"""
操作開始の合図「スタート！」の画像と音を作る。

  python tools/kamishibai/make_ui_prompts.py

  project/resources/ui/start_pop.png   「スタート！」（画面の真ん中にポンと出て消える）
      タイトルのロゴ（resources/title/logo.png「たまごザウルスの大冒険」）と同じ作り：
      1文字ずつ色の違うフェルト、文字の真ん中を通る白い縫い目、クリーム色のフェルトのふち、やわらかい影。
      文字は少しずつ傾けて、手で貼ったように
  project/resources/ui/se_start.wav    操作開始の合図の音（短い上がる音）
"""
import math
import os
import struct
import wave

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
OUT = os.path.join(PROJECT, "resources", "ui")
TEX = os.path.join(PROJECT, "resources", "craft", "textures")
os.makedirs(OUT, exist_ok=True)
FONT = r"C:\Windows\Fonts\BIZ-UDGothicB.ttc"

# ロゴの文字の色（たまごザウルス：赤・橙・黄・緑・青）
LETTERS = [
    ("ス", (226, 62, 52)),
    ("タ", (246, 138, 40)),
    ("ー", (250, 206, 46)),
    ("ト", (66, 170, 58)),
    ("！", (52, 128, 222)),
]
CREAM = (246, 240, 226)
SIZE = 230          # 文字の大きさ(px)
BORDER = 15         # クリーム色のふちの太さ(px)
ROUND = 9           # 角の丸め(px)
BOLD = 11           # 線を太らせる量(px)（ロゴのようなぽってりした太さに）


def disk(r):
    y, x = np.ogrid[-r:r + 1, -r:r + 1]
    return x * x + y * y <= r * r


def felt(shape, color, seed):
    """フェルトのざらざら（キットの felt_grass の明るさ）に色を掛ける"""
    src = Image.open(os.path.join(TEX, "felt_grass.png")).convert("L").resize((shape[1], shape[0]))
    g = np.asarray(src).astype(np.float32)
    g = np.roll(g, seed * 37, axis=1)
    g = (g - g.mean()) / (g.std() + 1e-5)
    shade = np.clip(1.0 + 0.06 * g, 0.8, 1.2)
    return np.stack([np.clip(c * shade, 0, 255) for c in color], -1)


def letter(ch, color, seed):
    """1文字ぶん（RGBA）"""
    font = ImageFont.truetype(FONT, SIZE)
    pad = BORDER + BOLD + 20
    box = font.getbbox(ch)
    w, h = box[2] - box[0] + pad * 2, box[3] - box[1] + pad * 2
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).text((pad - box[0], pad - box[1]), ch, font=font, fill=255)
    glyph = np.asarray(m) > 127
    glyph = ndimage.binary_dilation(glyph, structure=disk(BOLD)) # ぽってり太く
    # 角を丸くする（フェルトを切り抜いたように）
    glyph = ndimage.binary_opening(glyph, structure=disk(ROUND))
    glyph = ndimage.binary_closing(glyph, structure=disk(4))
    outline = ndimage.binary_dilation(glyph, structure=disk(BORDER))

    rgb = np.zeros((h, w, 3), np.float32)
    alpha = np.zeros((h, w), np.float32)
    # クリーム色のふち（少しだけ外側を暗く）
    edge_shade = ndimage.distance_transform_edt(outline)
    cream = felt((h, w), CREAM, seed + 3) * np.clip(0.88 + edge_shade[..., None] / 40.0, 0.88, 1.0)
    rgb[outline] = cream[outline]
    alpha[outline] = 255
    # 色のフェルト（ふちに近い所を少し暗く＝厚みがあるように）
    inside = ndimage.distance_transform_edt(glyph)
    body = felt((h, w), color, seed) * np.clip(0.78 + inside[..., None] / 30.0, 0.78, 1.0)
    rgb[glyph] = body[glyph]
    # 縫い目：文字の太さの真ん中（距離の尾根）を、白い破線で
    # 尾根は少し広めに拾う（細かく途切れないように）。ふちから 12px 以上内側だけ
    ridge = (inside >= ndimage.maximum_filter(inside, size=7) - 1.0) & (inside > 12)
    ridge = ndimage.binary_dilation(ridge, structure=disk(1))
    yy, xx = np.mgrid[0:h, 0:w]
    dash = ((xx + yy) // 15) % 2 == 0
    stitch = ridge & dash & glyph
    stitch = ndimage.binary_dilation(stitch, structure=disk(1)) & glyph
    rgb[stitch] = (252, 250, 244)
    img = Image.fromarray(np.dstack([rgb, alpha]).astype(np.uint8), "RGBA")
    return img


def start_pop():
    pieces = [letter(ch, color, i) for i, (ch, color) in enumerate(LETTERS)]
    angles = [-5, 4, -2, 5, -6]   # 手で貼ったように少しずつ傾ける
    lifts = [6, -8, 4, -6, 2]
    rotated = [p.rotate(a, resample=Image.BICUBIC, expand=True) for p, a in zip(pieces, angles)]
    rotated = [r.crop(r.getchannel("A").getbbox()) for r in rotated] # 透明な余白を切る
    gap = -10                       # ふちどうしが少し重なるくらい
    total = sum(r.width for r in rotated) + gap * (len(rotated) - 1) + 60
    height = max(r.height for r in rotated) + 60
    canvas = Image.new("RGBA", (total, height), (0, 0, 0, 0))
    x = 30
    placed = []
    for r, lift in zip(rotated, lifts):
        placed.append((r, x, (height - r.height) // 2 + lift))
        x += r.width + gap
    # 影（全体をまとめて、右下へずらしてぼかす）
    shadow = Image.new("L", canvas.size, 0)
    for r, px, py in placed:
        shadow.paste(r.getchannel("A"), (px + 7, py + 10), r.getchannel("A"))
    shadow = shadow.filter(ImageFilter.GaussianBlur(7)).point(lambda v: int(v * 0.45))
    canvas.paste((40, 25, 10, 255), (0, 0), shadow)
    for r, px, py in placed:
        canvas.alpha_composite(r, (px, py))
    canvas.save(os.path.join(OUT, "start_pop.png"))
    print("start_pop.png", canvas.size)


def start_sound():
    """ポン・ポン・ポーン と上がる短い音（木琴ふう）"""
    rate = 44100
    notes = [(0.00, 784.0, 0.16), (0.11, 988.0, 0.16), (0.22, 1319.0, 0.5)]
    length = int(rate * 0.8)
    out = np.zeros(length, np.float32)
    for start, freq, dur in notes:
        n = int(rate * dur)
        t = np.arange(n) / rate
        tone = np.sin(2 * math.pi * freq * t) + 0.35 * np.sin(2 * math.pi * freq * 2 * t) + 0.12 * np.sin(2 * math.pi * freq * 3 * t)
        env = np.exp(-t * (7.0 if dur < 0.3 else 4.5)) * np.minimum(1.0, t / 0.004)
        i = int(rate * start)
        out[i:i + n] += (tone * env)[: length - i]
    out = out / np.max(np.abs(out)) * 0.55
    with wave.open(os.path.join(OUT, "se_start.wav"), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        f.writeframes(b"".join(struct.pack("<h", int(v * 32767)) for v in out))
    print("se_start.wav")


start_pop()
start_sound()
