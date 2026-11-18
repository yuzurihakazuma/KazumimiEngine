# -*- coding: utf-8 -*-
"""
クリア演出（「たまごザウルスの大冒険」クリア演出 企画書）の素材を作る。

  python tools/clear/make_clear_assets.py            素材・モデル・音・調整値を作る
  python tools/clear/make_clear_assets.py --stars    星の絵だけ作る（clear.json は変えない。--preview も付けられる）
  python tools/clear/make_clear_assets.py --preview  作った素材を、ゲームと同じ置き場所で 1 枚に並べた絵も作る
                                                     （tools/clear/preview_result.png＝リザルト。起動しないで見た目を確かめる）
  ゴールの門・立体の切り文字・棒付きの星は Blender で作る：tools/clear/make_goal_set.py（clear.json の "gate" もそちらが書く）

  出力：project/resources/clear/
    リザルト画面（1920×1080 を基準に並べる。置き場所は clear.json の "result"）
      result_mat.png          緑のフェルトのマット（ギザギザのふち）
      result_title.png        「リザルト」
      result_row.png          行の枠（段ボールの帯。白いステッチ）
      label_band.png（コンボ花）/ label_band_flowers.png（かくれ花）/ label_combo.png / label_health.png / label_enemy.png   行の名前
      icon_combo.png / icon_health.png / icon_enemy.png                       行の絵
      flower_empty.png        帯：まだ見つけていない花（点線の丸）
      star_empty.png          星の空欄（点線の星）
      star_full.png           星（フェルトの金色の星。白いステッチ）。1 ステージに 3 つ（行ごとに 1 つ）。一度取ったら残る
      stamp_empty.png / stamp_egg.png   （前の卵スタンプ。今は使っていない）
      hanamaru.png            花丸（描き順のマスク付き：R＝濃さ・G＝形・B＝描く順。Sprite::SetReveal で一筆ずつ出す）
      prompt_next.png         決定ボタンの案内
      owned_tag.png           （前の左上の「持っているスタンプ」の札。今は使っていない）
    音
      se_gate.wav（門の卵が割れる）se_letter.wav（文字が下がる）se_star.wav（星が飛び出す）se_count.wav（数え上げ）
      se_stamp.wav（スタンプ）se_hanamaru.wav（花丸）se_add.wav（加算）se_road.wav（道が開く）
    調整値
      clear.json              場面の秒数（フル版・短縮版）・小道具の置き場所・リザルトの置き場所（"gate" は残す）
      stage1.json             ステージごと：門の場所・カメラの終点・目標連鎖数（無い時だけ作る。ゲームで直して保存した物は上書きしない）
"""
import json
import math
import os
import random
import struct
import sys
import wave

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
RES = os.path.join(PROJECT, "resources")
OUT = os.path.join(RES, "clear")
os.makedirs(OUT, exist_ok=True)

FONT = r"C:\Windows\Fonts\BIZ-UDGothicB.ttc"
CREAM = (250, 244, 230)
BROWN = (70, 45, 20)
KRAFT = (198, 152, 100)
INK_RED = (214, 52, 44)


# =====================================================================
#  共通：フェルト・段ボール・影
# =====================================================================
def grain(size, seed, scale=3, blur=0.8):
    """けば・紙のむら（0〜255 の濃淡）"""
    r = random.Random(seed)
    small = Image.new("L", (max(1, size[0] // scale), max(1, size[1] // scale)))
    small.putdata([r.randint(0, 255) for _ in range(small.width * small.height)])
    return small.resize(size, Image.BILINEAR).filter(ImageFilter.GaussianBlur(blur))


def felt(size, color, seed, patches=0, patch_shift=14):
    """フェルトの面：けば＋（あれば）色の少し違う切れはし"""
    w, h = size
    img = Image.new("RGB", size, color)
    r = random.Random(seed)
    d = ImageDraw.Draw(img)
    for _ in range(patches):
        cx, cy = r.uniform(0, w), r.uniform(0, h)
        rad = r.uniform(0.12, 0.3) * min(w, h)
        pts = [(cx + math.cos(a) * rad * r.uniform(0.6, 1.2), cy + math.sin(a) * rad * r.uniform(0.6, 1.2))
               for a in [i / 7 * 2 * math.pi + r.uniform(-0.3, 0.3) for i in range(7)]]
        k = r.uniform(-1, 1) * patch_shift
        d.polygon(pts, fill=tuple(max(0, min(255, int(c + k))) for c in color))
    g = grain(size, seed + 1).point(lambda v: 222 + v // 8)
    return Image.merge("RGB", [ImageChops.multiply(c, g) for c in img.split()])


def shadow_under(img, offset=(8, 11), blur=8, alpha=120):
    """形の下に、やわらかい影を付ける（まわりに blur×3 の余白が増える）"""
    pad = blur * 3
    out = Image.new("RGBA", (img.width + pad * 2, img.height + pad * 2), (0, 0, 0, 0))
    shadow = Image.new("L", out.size, 0)
    shadow.paste(img.split()[3].point(lambda v: alpha if v > 0 else 0), (pad + offset[0], pad + offset[1]))
    shadow = shadow.filter(ImageFilter.GaussianBlur(blur))
    out.paste((30, 20, 10, 255), (0, 0), shadow)
    out.alpha_composite(img, (pad, pad))
    return out


def cardboard(size, seed, radius=22, stitch=True):
    """段ボールの板（横の筋・白いステッチ・角は丸い）"""
    w, h = size
    card = Image.new("RGB", size, KRAFT)
    d = ImageDraw.Draw(card)
    for y in range(0, h, 7):
        d.line([(0, y), (w, y)], fill=(188, 142, 92), width=2)
    g = grain(size, seed, 4).point(lambda v: 236 + v // 14)
    card = Image.merge("RGB", [ImageChops.multiply(c, g) for c in card.split()])
    d = ImageDraw.Draw(card)
    if stitch:
        for x in range(26, w - 26, 18):
            d.line([(x, 14), (x + 9, 14)], fill=CREAM, width=3)
            d.line([(x, h - 15), (x + 9, h - 15)], fill=CREAM, width=3)
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius, fill=255)
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    img.paste(card, (0, 0), mask)
    return img


def text_image(text, size, fill=(255, 255, 250), stroke=BROWN, stroke_width=7, pad=14):
    """ふち付きの太い文字（HUD の数字と同じ書き方）"""
    font = ImageFont.truetype(FONT, size)
    box = font.getbbox(text, stroke_width=stroke_width)
    w, h = box[2] - box[0] + pad * 2, box[3] - box[1] + pad * 2
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(img).text((pad - box[0], pad - box[1]), text, font=font, fill=fill, stroke_width=stroke_width, stroke_fill=stroke)
    return img


def save(img, name):
    img.save(os.path.join(OUT, name))
    return img


# =====================================================================
#  リザルト画面の部品
# =====================================================================
def make_mat():
    """緑のフェルトのマット：クリームのふちに、オレンジと緑のギザギザ"""
    w, h = 1500, 890
    frame = 54
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w - 1, h - 1], 18, fill=(246, 226, 178, 255))
    step = 54
    for side in range(4):
        length = w if side % 2 == 0 else h
        for i in range(0, length, step):
            color = (232, 142, 52, 255) if (i // step) % 2 == 0 else (122, 176, 70, 255)
            a, b, tip = i, min(i + step, length), i + step / 2
            if side == 0:
                pts = [(a, 0), (b, 0), (tip, frame - 10)]
            elif side == 1:
                pts = [(w, a), (w, b), (w - frame + 10, tip)]
            elif side == 2:
                pts = [(a, h), (b, h), (tip, h - frame + 10)]
            else:
                pts = [(0, a), (0, b), (frame - 10, tip)]
            d.polygon(pts, fill=color)
    inner = felt((w - frame * 2, h - frame * 2), (58, 134, 84), 11, patches=24, patch_shift=14)
    img.paste(inner, (frame, frame))
    d = ImageDraw.Draw(img)
    for x in range(frame + 16, w - frame - 16, 22):
        d.line([(x, frame + 12), (x + 11, frame + 12)], fill=(226, 244, 222, 255), width=3)
        d.line([(x, h - frame - 13), (x + 11, h - frame - 13)], fill=(226, 244, 222, 255), width=3)
    for y in range(frame + 16, h - frame - 16, 22):
        d.line([(frame + 12, y), (frame + 12, y + 11)], fill=(226, 244, 222, 255), width=3)
        d.line([(w - frame - 13, y), (w - frame - 13, y + 11)], fill=(226, 244, 222, 255), width=3)
    return save(shadow_under(img, (12, 16), 14, 150), "result_mat.png")


def make_row():
    return save(shadow_under(cardboard((1320, 116), 21), (5, 7), 6, 110), "result_row.png")


def make_labels():
    save(text_image("リザルト", 96, stroke_width=9), "result_title.png")
    for name, text in (("label_band", "コンボばな"), ("label_band_flowers", "かくればな"), ("label_combo", "おくなげコンボ"), ("label_health", "たいりょく"),
                       ("label_enemy", "たおした てき")):
        save(text_image(text, 54, stroke_width=6), name + ".png")


def make_icons():
    """行の絵：卵・♡・敵（どれも今ある切り絵を小さくして使う）"""
    for name, src in (("icon_combo", os.path.join(RES, "kamishibai", "intro_char_egg.png")),
                      ("icon_health", os.path.join(RES, "gameover", "heart.png")),
                      ("icon_enemy", os.path.join(RES, "kamishibai", "intro_char_enemy_ground.png"))):
        icon = Image.open(src).convert("RGBA")
        icon.thumbnail((96, 96), Image.LANCZOS)
        canvas = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
        canvas.alpha_composite(icon, ((96 - icon.width) // 2, (96 - icon.height) // 2))
        save(canvas, name + ".png")


def egg_mask(size):
    """卵の形（上が少し細い）"""
    s = size * 4
    m = Image.new("L", (s, s), 0)
    pts = []
    for i in range(96):
        a = i / 96 * 2 * math.pi
        cx, cy = math.sin(a), -math.cos(a)                     # 上から時計回り
        wide = 0.36 * (1.0 + 0.16 * cy)                        # 下ほど太い
        tall = 0.44 if cy < 0 else 0.40                        # 上の方が長い
        pts.append((s * (0.5 + cx * wide), s * (0.52 + cy * tall)))
    ImageDraw.Draw(m).polygon(pts, fill=255)
    return m.resize((size, size), Image.LANCZOS)


def make_stamps():
    size = 132
    egg = egg_mask(size)
    solid = egg.point(lambda v: 255 if v > 128 else 0)
    edge = ImageChops.subtract(solid, solid.filter(ImageFilter.MinFilter(9)))
    # 空欄：点線の卵
    dots = Image.new("L", (size, size), 0)
    dots.putdata([255 if ((i % size) + (i // size)) // 7 % 2 == 0 else 0 for i in range(size * size)])
    empty = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    empty.paste((255, 255, 255, 46), (0, 0), egg)
    empty.paste((244, 236, 214, 230), (0, 0), ImageChops.multiply(edge, dots))
    save(empty, "stamp_empty.png")
    # 押した所：赤いインク（かすれ）。卵のふちと、中の水玉
    ink = Image.new("L", (size, size), 0)
    ring = ImageChops.subtract(solid, solid.filter(ImageFilter.MinFilter(15)))
    ink.paste(255, (0, 0), ring)
    d = ImageDraw.Draw(ink)
    for cx, cy, r in ((0.40, 0.36, 0.085), (0.62, 0.46, 0.07), (0.44, 0.60, 0.075), (0.60, 0.70, 0.06), (0.52, 0.22, 0.05)):
        d.ellipse([size * (cx - r), size * (cy - r), size * (cx + r), size * (cy + r)], fill=255)
    worn = grain((size, size), 77, 2, 0.6).point(lambda v: 255 if v > 78 else 120)   # インクのかすれ
    ink = ImageChops.multiply(ink, worn).filter(ImageFilter.GaussianBlur(0.6))
    stamp = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    stamp.paste(INK_RED + (255,), (0, 0), ink)
    save(stamp.rotate(-9, resample=Image.BICUBIC), "stamp_egg.png")   # 手で押したように少し傾ける


def star_mask(size, inner=0.46, round_px=10, scale=0.9):
    """角の丸い 5 つ星の形（L）。scale＝絵の大きさに対する星の大きさ"""
    s = size * 4
    m = Image.new("L", (s, s), 0)
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = 0.5 * scale * (1.0 if i % 2 == 0 else inner)
        pts.append((s * (0.5 + r * math.cos(a)), s * (0.53 + r * math.sin(a))))
    ImageDraw.Draw(m).polygon(pts, fill=255)
    m = m.filter(ImageFilter.MaxFilter(round_px * 2 + 1)).filter(ImageFilter.MinFilter(round_px * 2 + 1))  # 角を丸く
    return m.resize((size, size), Image.LANCZOS)


def make_stars():
    """星：フェルトの金色の星（下にオレンジの重ね・白いステッチ・小さなつや）と、空欄（点線の星）"""
    size = 140
    body = star_mask(size)
    solid = body.point(lambda v: 255 if v > 128 else 0)
    # 空欄：うすい星と、点線のふち
    edge = ImageChops.subtract(solid, solid.filter(ImageFilter.MinFilter(7)))
    dots = Image.new("L", (size, size), 0)
    dots.putdata([255 if ((i % size) + (i // size)) // 7 % 2 == 0 else 0 for i in range(size * size)])
    empty = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    empty.paste((255, 255, 255, 46), (0, 0), body)
    empty.paste((244, 236, 214, 230), (0, 0), ImageChops.multiply(edge, dots))
    save(empty, "star_empty.png")
    # 取った星：下にオレンジのフェルト（少しずらして厚みに見せる）、上に金色のフェルト
    felt = grain((size, size), 91, 2, 0.7)
    star = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    back = ImageChops.offset(body, 1, 3)   # 厚み（空欄の星と同じ位置に重なるよう、ずらしは小さく）
    star.paste((214, 112, 30, 255), (0, 0), back)
    gold = Image.new("RGBA", (size, size), (252, 206, 58, 255))
    shade = felt.point(lambda v: int(205 + v * 50 / 255))  # フェルトのむら
    gold = ImageChops.multiply(gold, Image.merge("RGBA", (shade, shade, shade, Image.new("L", (size, size), 255))))
    star.paste(gold, (0, 0), body)
    # 白いステッチ（ふちから少し内側の点線）
    inner = solid.filter(ImageFilter.MinFilter(15))
    stitch_line = ImageChops.subtract(inner, inner.filter(ImageFilter.MinFilter(5)))
    dash = Image.new("L", (size, size), 0)
    dash.putdata([255 if ((i % size) * 0.7 + (i // size) * 0.7) // 5 % 2 == 0 else 0 for i in range(size * size)])
    star.paste((255, 250, 232, 235), (0, 0), ImageChops.multiply(stitch_line, dash))
    # つや（左上の小さな丸）
    shine = Image.new("L", (size, size), 0)
    ImageDraw.Draw(shine).ellipse([size * 0.36, size * 0.30, size * 0.46, size * 0.38], fill=150)
    star.paste((255, 255, 255, 255), (0, 0), ImageChops.multiply(shine.filter(ImageFilter.GaussianBlur(2)), solid))
    save(star, "star_full.png")   # 傾けない（空欄の点線の星とぴったり重なる）


def make_flower_empty():
    size = 128
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([14, 14, size - 15, size - 15], fill=(255, 255, 255, 40))
    for i in range(24):
        if i % 2 == 0:
            a0, a1 = i / 24 * 360, (i + 1) / 24 * 360
            d.arc([14, 14, size - 15, size - 15], a0, a1, fill=(244, 236, 214, 230), width=5)
    save(img, "flower_empty.png")


def make_prompt():
    """決定ボタンの案内：段ボールの札に「SPACE  つぎへ」"""
    key = text_image("SPACE", 44, fill=(92, 58, 30), stroke=CREAM, stroke_width=5)
    label = text_image("つぎへ", 50, stroke_width=6)
    w = key.width + label.width + 74
    board = cardboard((w, 96), 31, radius=26)
    cap = Image.new("RGBA", (key.width + 18, 70), (0, 0, 0, 0))
    ImageDraw.Draw(cap).rounded_rectangle([0, 0, cap.width - 1, 69], 14, fill=(250, 244, 230, 255), outline=BROWN + (255,), width=4)
    board.alpha_composite(cap, (22, 13))
    board.alpha_composite(key, (31, (96 - key.height) // 2 + 1))
    board.alpha_composite(label, (key.width + 52, (96 - label.height) // 2 + 1))
    save(shadow_under(board, (5, 7), 6, 120), "prompt_next.png")


def make_owned_tag():
    """左上の札：卵スタンプの絵（数は、ゲームが数字の絵を並べる）"""
    board = cardboard((330, 118), 41, radius=24)
    stamp = Image.open(os.path.join(OUT, "stamp_egg.png")).convert("RGBA").resize((92, 92), Image.LANCZOS)
    back = Image.new("RGBA", (100, 100), (0, 0, 0, 0))
    ImageDraw.Draw(back).ellipse([0, 0, 99, 99], fill=(250, 244, 230, 255))
    board.alpha_composite(back, (14, 9))
    board.alpha_composite(stamp, (18, 13))
    save(shadow_under(board, (5, 7), 6, 120), "owned_tag.png")


# --- 花丸（描き順のマスク付き）---------------------------------------------------
def hanamaru_path():
    """花丸の線：真ん中から外へうずまき → まわりに花びらを一周。点の並び（0〜1 の座標）"""
    pts = []
    turns = 2.35
    n = 420
    for i in range(n):                                    # うずまき
        t = i / (n - 1)
        a = -math.pi / 2 + t * turns * 2 * math.pi
        r = 0.035 + 0.215 * t
        pts.append((0.5 + math.cos(a) * r, 0.5 + math.sin(a) * r))
    start = -math.pi / 2 + turns * 2 * math.pi
    petals, m = 7, 520
    for i in range(m):                                    # 花びら（外へふくらむ山を 7 つ）
        t = i / (m - 1)
        a = start + t * 2 * math.pi * 1.03
        bulge = abs(math.sin(t * petals * math.pi * 1.03))
        r = 0.27 + 0.135 * bulge ** 0.8
        pts.append((0.5 + math.cos(a) * r, 0.5 + math.sin(a) * r))
    return pts


def make_hanamaru():
    """R＝クレヨンの濃さ・G＝形（ふちはなめらか）・B＝描く順（早いほど明るい）。A は 255。
    テクスチャは sRGB として読まれるので、G と B は「読んだ後に元の値になる」ように 1/2.2 乗して入れる"""
    size = 512
    scale = 2
    s = size * scale
    pts = [(x * s, y * s) for x, y in hanamaru_path()]
    # 道のりで並べ直す（一定の速さで描いているように）
    lengths = [0.0]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        lengths.append(lengths[-1] + math.hypot(x1 - x0, y1 - y0))
    total = lengths[-1]
    cover = np.zeros((s, s), np.float32)
    order = np.full((s, s), -1.0, np.float32)     # 描いた時刻（0〜1）。-1＝まだ
    radius = 0.021 * s                            # クレヨンの太さ
    reach = radius * 1.9                          # 描く順は、形より広く入れておく（ふちの補間で崩れない）
    yy, xx = np.mgrid[0:s, 0:s]
    step = 5
    for i in range(0, len(pts), step):
        x, y = pts[i]
        t = lengths[i] / total
        x0, x1 = int(max(0, x - reach - 2)), int(min(s, x + reach + 3))
        y0, y1 = int(max(0, y - reach - 2)), int(min(s, y + reach + 3))
        dist = np.hypot(xx[y0:y1, x0:x1] - x, yy[y0:y1, x0:x1] - y)
        c = np.clip((radius - dist) / 2.0 + 0.5, 0.0, 1.0)
        cover[y0:y1, x0:x1] = np.maximum(cover[y0:y1, x0:x1], c)
        region = order[y0:y1, x0:x1]
        fresh = (dist < reach) & (region < 0.0)          # 最初に通った時刻を残す（重なった所は先に描いた方）
        region[fresh] = t
    # クレヨンのかすれ：紙の目で、ところどころ色が乗らない
    paper = np.asarray(grain((s, s), 5, 2, 0.7)).astype(np.float32) / 255.0
    cover *= np.clip((paper - 0.26) * 3.2, 0.25, 1.0)
    shade = 0.82 + 0.18 * np.asarray(grain((s, s), 9, 3, 1.0)).astype(np.float32) / 255.0
    drawn = np.where(order >= 0.0, 1.0 - 0.9 * order, 0.0)   # 早い所ほど明るい（1.0 → 0.1）。描かない所は 0

    def to_srgb(a):
        return (np.clip(a, 0.0, 1.0) ** (1.0 / 2.2) * 255.0 + 0.5).astype(np.uint8)

    def shrink(a):
        return np.asarray(Image.fromarray(a).resize((size, size), Image.LANCZOS))

    r = shrink(to_srgb(shade))
    g = shrink(to_srgb(cover))
    b = np.asarray(Image.fromarray(to_srgb(drawn)).resize((size, size), Image.BILINEAR))
    rgba = np.dstack([r, g, b, np.full((size, size), 255, np.uint8)])
    save(Image.fromarray(rgba, "RGBA"), "hanamaru.png")


# =====================================================================
#  音
# =====================================================================
RATE = 44100


def write_wav(name, samples, gain=0.6):
    data = np.asarray(samples, np.float32)
    peak = max(1e-6, float(np.max(np.abs(data))))
    data = data / peak * gain
    with wave.open(os.path.join(OUT, name), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes((np.clip(data, -1, 1) * 32767).astype("<i2").tobytes())


def tone(freq, duration, decay, harmonics=((1, 1.0), (2, 0.35), (3, 0.12)), attack=0.004):
    t = np.arange(int(RATE * duration)) / RATE
    wave_ = sum(amp * np.sin(2 * math.pi * freq * k * t) for k, amp in harmonics)
    return wave_ * np.exp(-t * decay) * np.minimum(1.0, t / attack)


def mix(length, parts):
    out = np.zeros(int(RATE * length), np.float32)
    for start, samples in parts:
        i = int(RATE * start)
        n = min(len(samples), len(out) - i)
        if n > 0:
            out[i:i + n] += samples[:n]
    return out


def noise_burst(duration, seed, color=0.35, decay=18.0):
    r = np.random.default_rng(seed)
    n = int(RATE * duration)
    white = r.uniform(-1, 1, n)
    out = np.zeros(n)
    prev = 0.0
    for i in range(n):
        prev += (white[i] - prev) * color
        out[i] = prev
    t = np.arange(n) / RATE
    return out * np.exp(-t * decay)


def make_sounds():
    # 輪が割れる：殻のパキッ（短いノイズを 3 つ）＋ポン
    write_wav("se_gate.wav", mix(0.6, [(0.0, noise_burst(0.09, 1, 0.8, 40) * 1.2), (0.05, noise_burst(0.07, 2, 0.7, 45)),
                                       (0.1, noise_burst(0.12, 3, 0.5, 26)), (0.08, tone(523, 0.45, 7) * 0.5),
                                       (0.14, tone(784, 0.4, 7) * 0.4)]), 0.7)
    # 文字が下がる：ぽん（木琴）＋糸のびよん
    t = np.arange(int(RATE * 0.3)) / RATE
    boing = np.sin(2 * math.pi * (300 + 180 * np.exp(-t * 14)) * t) * np.exp(-t * 11) * 0.5
    write_wav("se_letter.wav", mix(0.4, [(0.0, tone(660, 0.3, 11)), (0.02, boing)]), 0.55)
    # 星が飛び出す：しゅっ＋きらっ
    write_wav("se_star.wav", mix(0.5, [(0.0, noise_burst(0.16, 5, 0.15, 16) * 0.6), (0.06, tone(1568, 0.3, 12) * 0.5),
                                       (0.12, tone(2093, 0.3, 12) * 0.4)]), 0.5)
    # 数え上げ：短いカチッ
    write_wav("se_count.wav", tone(1320, 0.06, 60, ((1, 1.0), (2, 0.2))), 0.35)
    # スタンプ：ぺたん（低いどん＋紙）
    t = np.arange(int(RATE * 0.22)) / RATE
    thud = np.sin(2 * math.pi * (150 * np.exp(-t * 16) + 60) * t) * np.exp(-t * 20)
    write_wav("se_stamp.wav", mix(0.3, [(0.0, thud), (0.0, noise_burst(0.08, 7, 0.5, 45) * 0.5)]), 0.75)
    # 花丸：クレヨンのきゅっきゅっ（ざらざらを、線を引く速さで揺らす）＋最後にきらん
    n = int(RATE * 1.25)
    t = np.arange(n) / RATE
    scratch = noise_burst(1.25, 9, 0.12, 0.0) * (0.55 + 0.45 * np.sin(2 * math.pi * 5.5 * t) ** 2) * np.minimum(1.0, t / 0.05)
    scratch *= np.clip((1.2 - t) / 0.1, 0.0, 1.0)
    write_wav("se_hanamaru.wav", mix(1.9, [(0.0, scratch * 0.6), (1.2, tone(1047, 0.6, 5) * 0.6), (1.28, tone(1568, 0.6, 5) * 0.5)]), 0.6)
    # 加算：ちゃりん（上がる 2 音）
    write_wav("se_add.wav", mix(0.5, [(0.0, tone(988, 0.25, 12)), (0.07, tone(1319, 0.4, 9))]), 0.5)
    # 道が開く：ころころ → ぽん
    n = int(RATE * 0.9)
    t = np.arange(n) / RATE
    roll = noise_burst(0.9, 11, 0.06, 0.0) * (0.5 + 0.5 * np.sin(2 * math.pi * 9 * t)) * np.minimum(1.0, t / 0.08) * np.clip((0.9 - t) / 0.15, 0, 1)
    write_wav("se_road.wav", mix(1.4, [(0.0, roll * 0.7), (0.88, tone(523, 0.45, 8) * 0.7), (0.94, tone(784, 0.45, 8) * 0.6)]), 0.6)


# =====================================================================
#  調整値
# =====================================================================
def make_settings():
    settings = {
        # 場面の長さ(秒)。企画書の配分：輪をくぐる 0〜2 → 「ゴール！」2〜12 → リザルトへ 12〜13 → リザルト 13〜28 → 加算 28〜36 → 幕
        "full":  {"gate": 1.8, "goal": 4.5, "toResult": 0.8, "leave": 0.5},   # 長すぎて待たせていたので短く（「ゴール！」まで約 6 秒）
        "short": {"gate": 1.4, "goal": 2.6, "toResult": 0.7, "leave": 0.4},   # 同じステージの 2 回目から（「ゴール！」まで約 4 秒）
        "jump": {"takeOff": 0.2, "land": 1.3, "pose": 1.0},
        # 提出用・プレイ会用：星を全部そろえる・クリアしたステージに入り直すとゴールの手前から（押す演出なし）。終わったら false
        "playSession": {"allStars": False, "retryNearGoal": False, "retryBack": 15.0},     # GoalJump クリップの中の時刻・GoalPose の長さ（make_intro_clips.py と合わせる）
        "gateOpen": {"time": 0.8, "eggAngle": 1.25, "hop": 0.45, "tapeAngle": 1.45},  # 卵が割れて開く秒・角度(rad)・跳ねる高さ(m)・テープが垂れる角度
        "letters": {                 # 立体の切り文字（モデルは make_goal_set.py。門のアーチから糸で下がる）
            "count": 4,
            "height": 1.05,          # 文字の高さ(m)（モデルと同じ）
            "scale": 0.7,            # 札の大きさ（モデルに対する倍率）
            "arcRadius": 1.65,       # 札を並べる円の半径（アーチの円の中心から。虹の帯の上）
            "arcAngles": [-62.0, -22.0, 22.0, 62.0],   # 札の位置（てっぺんからの角度・度。左が－）
            "stringLength": 3.5,     # 札の上の糸の長さ(m)（画面の上の外まで）
            "dropHeight": 3.0,       # 下りてくる高さ(m)
            "front": 0.05,           # アーチの表の面から、さらに手前へ(m)
            "start": 0.15,           # 「ゴール！」の場面に入ってから、最初の文字が下り始めるまで(秒)
            "interval": 0.14,        # 文字ごとの遅れ(秒)
            "dropTime": 0.75,        # 下りる時間(秒)
            "swing": 0.28,           # 下りた時の揺れ(rad)
            "sway": 0.035,           # その後の、ゆっくりした揺れ(rad)
        },
        "stars": {                   # 棒付きの星（門の両わきの足元から、外へ扇状に飛び出す）
            "perSide": 3,
            "pivotSide": 2.55,       # 棒の根元：門の真ん中から横へ(m)
            "pivotHeight": 0.0,
            "front": 0.7,            # 門の面から手前へ(m)
            "angles": [0.3, 0.68, 1.05],   # 扇の角度（真上から外へ rad）
            "start": 0.75,           # 「ゴール！」の場面に入ってから(秒)
            "interval": 0.09,
            "popTime": 0.5,
        },
        "confetti": {"burst": 46, "rain": 5, "rainInterval": 0.22, "height": 5.2, "spread": 3.6},
        # クリアの間だけ空に出す飾り（太陽と雲）。門の足元から、カメラから見て奥へ back・右へ side・上へ up。
        #   地面・丘・木・空の板は置かない（遊んできたステージが後ろに見えなくなるため）
        "backdrop": [
            {"model": "goal_sun", "back": 30.0, "side": 12.0, "up": 14.0, "scale": [1.6, 1.6, 1.6]},
            {"model": "cloud_cutout_00", "back": 28.0, "side": -13.0, "up": 12.0, "scale": [3.0, 3.0, 3.0]},
            {"model": "cloud_cutout_01", "back": 29.0, "side": 3.0, "up": 15.0, "scale": [2.6, 2.6, 2.6]},
            {"model": "cloud_cutout_00", "back": 27.0, "side": 20.0, "up": 10.0, "scale": [2.2, 2.2, 2.2]},
        ],
        # リザルト画面（1920×1080 の画素。x・y は真ん中）
        "result": {
            "dim": [0.03, 0.13, 0.08, 0.74],       # 景色を沈める色（暗い緑）
            "mat": {"x": 960, "y": 566, "scale": 1.0},
            "title": {"x": 492, "y": 258, "scale": 1.0},   # 内側の点線（y 186）より下に収める
            "name": {"x": 1010, "y": 266, "width": 580},   # ふりがなも点線より下
            "hanamaru": {"x": 1520, "y": 292, "size": 190},
            "rowX": 960, "rowYs": [420, 552, 684, 816],   # 帯・1 行目・2 行目・3 行目
            "iconX": 366, "iconSize": 92, "labelX": 430,
            "valueRight": 1330, "valueHeight": 78,
            "stampX": 1516, "stampSize": 112,
            "flowerX": 800, "flowerGap": 124, "flowerSize": 100,
            "prompt": {"x": 1560, "y": 1000},   # 画面の下のはしに寄せすぎない（タスクバーに隠れた）
            "owned": {"x": 200, "y": 64, "numberX": 150, "numberHeight": 62},
            "matIn": 0.4, "itemIn": 0.3, "flowerInterval": 0.3, "countTime": 0.6, "stampTime": 0.35, "rowGap": 0.15,
            "hanamaruTime": 1.0, "addTime": 1.4,
        },
        "sounds": {"gate": "resources/clear/se_gate.wav", "letter": "resources/clear/se_letter.wav",
                   "star": "resources/clear/se_star.wav", "count": "resources/clear/se_count.wav",
                   "stamp": "resources/clear/se_stamp.wav", "hanamaru": "resources/clear/se_hanamaru.wav",
                   "add": "resources/clear/se_add.wav", "road": "resources/clear/se_road.wav"},
    }
    # 門の寸法（"gate"）は make_goal_set.py が書く。ここでは消さない
    path = os.path.join(OUT, "clear.json")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            old = json.load(f)
        if "gate" in old:
            settings["gate"] = old["gate"]
    with open(path, "w", encoding="utf-8") as f:
        json.dump(settings, f, ensure_ascii=False, indent=2)
    # ステージごと：ゲームのインスペクターで直して保存するので、ある時は上書きしない
    stage = os.path.join(OUT, "stage1.json")
    if not os.path.exists(stage):
        with open(stage, "w", encoding="utf-8") as f:
            json.dump({
                "gateOffset": -3.2,      # ゴールの点から、レールに沿って門を置く所(m)。マイナス＝手前
                "gateTurn": 20.0,        # 門を、遊んでいる時のカメラの方へ回す角度(度)。横から見ても門の表が見える
                "landAhead": 1.4,        # 門をくぐってから、着地する所（門から先へ m）
                "camera": {"yaw": 20.0, "distance": 8.6, "height": 2.0, "lookHeight": 2.4, "lookBack": 0.7, "fovY": 0.82},
                "comboTarget": 5,        # 1 行目の目標の連鎖数
                "band": "comboFlowers",  # 帯に出す集める物（comboFlowers＝コンボ花 / flowers＝かくれ花 / none＝帯なし）
                "healthRule": "full",    # 2 行目：full＝満タンでゴール / noDamage＝1 回もダメージを受けない
            }, f, ensure_ascii=False, indent=2)
    return settings


# =====================================================================
#  起動しないで見た目を確かめる絵
# =====================================================================
def preview(settings):
    layout = settings["result"]
    canvas = Image.new("RGBA", (1920, 1080), (120, 170, 220, 255))
    dim = layout["dim"]
    canvas.alpha_composite(Image.new("RGBA", (1920, 1080), tuple(int(c ** (1 / 2.2) * 255) for c in dim[:3]) + (int(dim[3] * 255),)))

    def put(name, x, y, width=None, height=None, folder=OUT):
        img = Image.open(os.path.join(folder, name)).convert("RGBA")
        if width or height:
            w = width or int(img.width * height / img.height)
            h = height or int(img.height * width / img.width)
            img = img.resize((int(w), int(h)), Image.LANCZOS)
        canvas.alpha_composite(img, (int(x - img.width / 2), int(y - img.height / 2)))
        return img

    def number(text, right, y, height):
        digits = Image.open(os.path.join(RES, "ui", "hud_digits.png")).convert("RGBA")
        chars = "0123456789/×+"
        w = height * 64 / 96 * 0.78
        x = right - w * len(text)
        for ch in text:
            cell = digits.crop((chars.index(ch) * 64, 0, chars.index(ch) * 64 + 64, 96)).resize((int(height * 64 / 96), int(height)), Image.LANCZOS)
            canvas.alpha_composite(cell, (int(x), int(y - height / 2)))
            x += w

    put("result_mat.png", layout["mat"]["x"], layout["mat"]["y"])
    put("result_title.png", layout["title"]["x"], layout["title"]["y"])
    put("intro_name_stage1.png", layout["name"]["x"], layout["name"]["y"], width=layout["name"]["width"], folder=os.path.join(RES, "kamishibai"))
    rows = [("label_band.png", None), ("label_combo.png", "icon_combo.png"), ("label_health.png", "icon_health.png"),
            ("label_enemy.png", "icon_enemy.png")]
    values = [None, "×6", "12/12", "9/13"]
    stamps = [None, True, True, False]
    for (label, icon), y, value, stamp in zip(rows, layout["rowYs"], values, stamps):
        put("result_row.png", layout["rowX"], y)
        if icon:
            put(icon, layout["iconX"], y, height=layout["iconSize"])
        img = Image.open(os.path.join(OUT, label))
        put(label, layout["labelX"] + img.width / 2, y)
        if value:
            number(value, layout["valueRight"], y, layout["valueHeight"])
            put("star_empty.png", layout["stampX"], y, height=layout["stampSize"])
            if stamp:
                put("star_full.png", layout["stampX"], y, height=layout["stampSize"])
    for i in range(3):
        x = layout["flowerX"] + i * layout["flowerGap"]
        put("flower_empty.png", x, layout["rowYs"][0], height=layout["flowerSize"])
        if i < 2:
            put("hud_combo_flower.png", x, layout["rowYs"][0], height=layout["flowerSize"], folder=os.path.join(RES, "ui"))
    number("2/3", layout["valueRight"], layout["rowYs"][0], layout["valueHeight"])
    # 花丸（描き順 7 割まで）
    hana = np.asarray(Image.open(os.path.join(OUT, "hanamaru.png")).convert("RGBA")).astype(np.float32) / 255.0
    lin = hana ** 2.2
    progress = 0.7
    drawn = np.clip((lin[..., 2] - (1.0 - progress) + 0.02) / 0.04, 0, 1)
    alpha = lin[..., 1] * drawn
    red = np.array(INK_RED) / 255.0
    rgba = np.dstack([np.tile(red, hana.shape[:2] + (1,)) * hana[..., :1], alpha])
    size = layout["hanamaru"]["size"]
    img = Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), "RGBA").resize((size, size), Image.LANCZOS)
    canvas.alpha_composite(img, (int(layout["hanamaru"]["x"] - size / 2), int(layout["hanamaru"]["y"] - size / 2)))
    put("prompt_next.png", layout["prompt"]["x"], layout["prompt"]["y"])
    canvas.convert("RGB").save(os.path.join(HERE, "preview_result.png"))
    print("wrote preview_result.png")


if __name__ == "__main__":
    if "--stars" in sys.argv:
        make_stars()
        if "--preview" in sys.argv:
            preview(json.load(open(os.path.join(OUT, "clear.json"), encoding="utf-8")))
        sys.exit(0)
    make_mat()
    make_row()
    make_labels()
    make_icons()
    make_stamps()
    make_stars()
    make_flower_empty()
    make_prompt()
    make_owned_tag()
    make_hanamaru()
    make_sounds()
    made = make_settings()
    print("wrote", OUT)
    if "--preview" in sys.argv:
        preview(made)
