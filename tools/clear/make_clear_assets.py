# -*- coding: utf-8 -*-
"""
クリア演出（「たまごザウルスの大冒険」クリア演出 企画書）の素材を作る。

  python tools/clear/make_clear_assets.py            素材・モデル・音・調整値を作る
  python tools/clear/make_clear_assets.py --preview  作った素材を、ゲームと同じ置き場所で 1 枚に並べた絵も作る
                                                     （tools/clear/preview_result.png＝リザルト / preview_goal.png＝「ゴール！」の場面 /
                                                       preview_props.png＝小道具。起動しないで見た目を確かめる）

  出力：project/resources/clear/
    リザルト画面（1920×1080 を基準に並べる。置き場所は clear.json の "result"）
      result_mat.png          緑のフェルトのマット（ギザギザのふち）
      result_title.png        「リザルト」
      result_row.png          行の枠（段ボールの帯。白いステッチ）
      label_band.png（コンボ花）/ label_band_flowers.png（かくれ花）/ label_combo.png / label_health.png / label_enemy.png   行の名前
      icon_combo.png / icon_health.png / icon_enemy.png                       行の絵
      flower_empty.png        帯：まだ見つけていない花（点線の丸）
      stamp_empty.png         卵スタンプの空欄（点線の卵）
      stamp_egg.png           卵スタンプ（押した所。赤いインク）
      hanamaru.png            花丸（描き順のマスク付き：R＝濃さ・G＝形・B＝描く順。Sprite::SetReveal で一筆ずつ出す）
      prompt_next.png         決定ボタンの案内
      owned_tag.png           左上の「持っているスタンプ」の札
    ゴールの祝福（3D の小道具。板に貼る絵）
      goal_letter_0〜3.png    「ゴ」「ー」「ル」「！」の切り文字（1 文字ずつ色が違う。上へ糸が付いている）
      goal_star_0〜2.png      棒付きの紙の星（色ちがい）
      goal_ring.png           卵の殻の輪の絵（殻の面と、段ボールの切り口）
      goal_ring_top.obj / goal_ring_bottom.obj   卵の殻の輪（ギザギザの割れ目で上下に分かれる）
    音
      se_ring.wav（輪が割れる）se_letter.wav（文字が下がる）se_star.wav（星が飛び出す）se_count.wav（数え上げ）
      se_stamp.wav（スタンプ）se_hanamaru.wav（花丸）se_add.wav（加算）se_road.wav（道が開く）
    調整値
      clear.json              場面の秒数（フル版・短縮版）・小道具の置き場所・リザルトの置き場所
      stage1.json             ステージごと：輪の場所・カメラの終点・目標連鎖数（無い時だけ作る。ゲームで直して保存した物は上書きしない）
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
#  ゴールの祝福：切り文字・棒付きの星
# =====================================================================
def disk(r):
    y, x = np.ogrid[-r:r + 1, -r:r + 1]
    return x * x + y * y <= r * r


def felt_array(shape, color, seed):
    """フェルトのざらざら（キットの felt_grass の明るさ）に色を掛ける"""
    src = Image.open(os.path.join(RES, "craft", "textures", "felt_grass.png")).convert("L").resize((shape[1], shape[0]))
    g = np.asarray(src).astype(np.float32)
    g = np.roll(g, seed * 37, axis=1)
    g = (g - g.mean()) / (g.std() + 1e-5)
    shade = np.clip(1.0 + 0.06 * g, 0.8, 1.2)
    return np.stack([np.clip(c * shade, 0, 255) for c in color], -1)


def felt_shape(glyph, color, seed, border=15, stitch=True):
    """形（真偽の配列）を、クリーム色のふち付きのフェルトにする（タイトルのロゴ・「スタート！」と同じ作り）"""
    h, w = glyph.shape
    outline = ndimage.binary_dilation(glyph, structure=disk(border))
    rgb = np.zeros((h, w, 3), np.float32)
    alpha = np.zeros((h, w), np.float32)
    edge_shade = ndimage.distance_transform_edt(outline)
    cream = felt_array((h, w), (246, 240, 226), seed + 3) * np.clip(0.88 + edge_shade[..., None] / 40.0, 0.88, 1.0)
    rgb[outline] = cream[outline]
    alpha[outline] = 255
    inside = ndimage.distance_transform_edt(glyph)
    body = felt_array((h, w), color, seed) * np.clip(0.78 + inside[..., None] / 30.0, 0.78, 1.0)
    rgb[glyph] = body[glyph]
    if stitch:
        ridge = (inside >= ndimage.maximum_filter(inside, size=7) - 1.0) & (inside > 12)
        ridge = ndimage.binary_dilation(ridge, structure=disk(1))
        yy, xx = np.mgrid[0:h, 0:w]
        marks = ridge & (((xx + yy) // 15) % 2 == 0) & glyph
        marks = ndimage.binary_dilation(marks, structure=disk(1)) & glyph
        rgb[marks] = (252, 250, 244)
    return Image.fromarray(np.dstack([rgb, alpha]).astype(np.uint8), "RGBA")


GOAL_LETTERS = [("ゴ", (226, 62, 52)), ("ー", (246, 138, 40)), ("ル", (66, 170, 58)), ("！", (52, 128, 222))]
LETTER_PX = 230            # 文字の大きさ(px)
LETTER_STRING = 2.6        # 文字の上に付ける糸の長さ（文字の絵の高さに対して）
LETTER_CELL = 340          # 文字 1 つの絵の幅(px)。全部同じ幅にして、並べやすくする
DAKUTEN = {"ゴ": "コ"}      # 濁点を別に描く字 → 本体の字
DAKUTEN_ROOM = 84          # 濁点の分、絵を右へ広げる幅(px)


def make_goal_letters():
    """切り文字：1 文字ずつ、上へ糸が付いた縦長の絵。糸はセロハンテープで留めてある。絵の大きさを返す"""
    font = ImageFont.truetype(FONT, LETTER_PX)
    bold, border, pad = 11, 15, 46
    sizes = []
    for index, (ch, color) in enumerate(GOAL_LETTERS):
        # 濁点の付く字は、濁点を別に描く（太らせると本体にくっついて「コ」に見えるため）：本体を少し小さくして、右上に点を 2 つ
        base_char = DAKUTEN.get(ch, ch)
        base_font = font if base_char == ch else ImageFont.truetype(FONT, int(LETTER_PX * 0.9))
        box = base_font.getbbox(base_char)
        extra = 0 if base_char == ch else DAKUTEN_ROOM
        gw, gh = box[2] - box[0] + pad * 2 + extra, box[3] - box[1] + pad * 2
        m = Image.new("L", (gw, gh), 0)
        ImageDraw.Draw(m).text((pad - box[0], pad - box[1]), base_char, font=base_font, fill=255)
        glyph = np.asarray(m) > 127
        glyph = ndimage.binary_dilation(glyph, structure=disk(bold))
        glyph = ndimage.binary_opening(glyph, structure=disk(9))
        glyph = ndimage.binary_closing(glyph, structure=disk(4))
        if base_char != ch:
            marks = Image.new("L", (gw, gh), 0)
            md = ImageDraw.Draw(marks)
            left = gw - pad - extra + 26          # 本体の右はしから、ふちの分だけ離す
            for k in range(2):
                x0, y0 = left + k * 40, pad - 6
                x1, y1 = x0 + 14, y0 + 40
                md.line([(x0, y0), (x1, y1)], fill=255, width=24)
                for cx, cy in ((x0, y0), (x1, y1)):
                    md.ellipse([cx - 12, cy - 12, cx + 12, cy + 12], fill=255)
            glyph = glyph | (np.asarray(marks) > 127)
        piece = felt_shape(glyph, color, index, border)
        piece = piece.crop(piece.getchannel("A").getbbox())
        # どの文字も同じ高さのマス（LETTER_CELL 四方）の真ん中へ置く
        cell = LETTER_CELL
        string = int(cell * LETTER_STRING)
        canvas = Image.new("RGBA", (cell, cell + string), (0, 0, 0, 0))
        px, py = (cell - piece.width) // 2, string + (cell - piece.height) // 2
        d = ImageDraw.Draw(canvas)
        top_of_letter = py + 14
        d.line([(cell // 2, 0), (cell // 2, top_of_letter)], fill=(96, 72, 52, 255), width=5)     # 糸
        shadow = Image.new("L", canvas.size, 0)
        shadow.paste(piece.getchannel("A"), (px + 7, py + 10), piece.getchannel("A"))
        shadow = shadow.filter(ImageFilter.GaussianBlur(7)).point(lambda v: int(v * 0.4))
        canvas.paste((40, 25, 10, 255), (0, 0), shadow)
        canvas.alpha_composite(piece, (px, py))
        tape = Image.new("RGBA", (70, 30), (255, 250, 220, 150))                                   # テープ
        ImageDraw.Draw(tape).rectangle([0, 0, 69, 29], outline=(230, 220, 180, 200), width=2)
        canvas.alpha_composite(tape.rotate(8, resample=Image.BICUBIC, expand=True), (cell // 2 - 38, top_of_letter - 4))
        save(canvas, "goal_letter_%d.png" % index)
        sizes.append(canvas.size)
    return sizes


STAR_COLORS = [(250, 206, 46), (246, 128, 150), (96, 190, 230)]
STAR_PX = 300
STAR_STICK = 2.6           # 棒の長さ（星の絵の高さに対して）


def make_goal_stars():
    """棒付きの紙の星：フェルトの星の下に、木の棒。星の真ん中から下へ"""
    sizes = []
    for index, color in enumerate(STAR_COLORS):
        s = STAR_PX
        m = Image.new("L", (s * 2, s * 2), 0)
        pts = []
        for i in range(10):
            a = -math.pi / 2 + i * math.pi / 5
            r = (0.80 if i % 2 == 0 else 0.40) * s
            pts.append((s + math.cos(a) * r, s + math.sin(a) * r))
        ImageDraw.Draw(m).polygon(pts, fill=255)
        glyph = np.asarray(m.resize((s, s), Image.LANCZOS)) > 127
        glyph = ndimage.binary_opening(glyph, structure=disk(8))      # 角を丸く
        glyph = ndimage.binary_dilation(glyph, structure=disk(6))
        star = felt_shape(np.pad(glyph, 24), color, 20 + index, 13)
        stick = int(s * STAR_STICK)
        canvas = Image.new("RGBA", (star.width, star.height + stick), (0, 0, 0, 0))
        d = ImageDraw.Draw(canvas)
        cx = star.width // 2
        d.rounded_rectangle([cx - 9, star.height // 2, cx + 9, star.height + stick - 4], 8, fill=(206, 168, 116, 255),
                            outline=(150, 112, 70, 255), width=3)   # 棒
        canvas.alpha_composite(star, (0, 0))
        save(canvas, "goal_star_%d.png" % index)
        sizes.append(canvas.size)
    return sizes


# =====================================================================
#  卵の殻の輪（モデル）
# =====================================================================
RING_HALF_WIDTH = 1.30     # 輪の外側の半幅(m)
RING_TOP = 1.80            # 外側：真ん中から上のてっぺんまで
RING_BOTTOM = 1.42         # 外側：真ん中から下まで
RING_THICK = 0.34          # 殻の太さ（外側から内側まで）
RING_DEPTH = 0.26          # 厚み（くぐる向き）
RING_SINK = 0.02           # 内側の下のふちを、地面よりこれだけ下げる（足元に段を作らない）
RING_CRACK = 0.62          # 割れ目の高さ（真ん中より上へ m。上のふた・下のうつわに分かれる）
RING_TEETH = 6             # 割れ目のギザギザの数（殻の太さの中に）
RING_TOOTH = 0.20          # ギザギザの大きさ（角度 rad）


def ring_point(theta, u):
    """輪の上の点。theta＝てっぺんから時計回りの角度、u＝0 内側 … 1 外側。返り値は (x, y)（y は真ん中が 0）"""
    inset = RING_THICK * (1.0 - u)
    c, s = math.cos(theta), math.sin(theta)
    tall = (RING_TOP if c >= 0 else RING_BOTTOM) - inset
    wide = (RING_HALF_WIDTH - inset) * (1.0 - 0.10 * c)      # 上は少し細く（卵の形）
    return s * wide, c * tall


def crack_theta(u, side):
    """割れ目の角度（右側。左は鏡写し）。殻の太さの方向 u に沿ってギザギザ"""
    base = math.acos(RING_CRACK / RING_TOP)
    k = u * RING_TEETH
    tooth = abs((k % 2.0) - 1.0) * 2.0 - 1.0                  # -1〜1 の三角波
    return base + tooth * RING_TOOTH * 0.5


def build_ring_piece(top):
    """上のふた（top=True）か、下のうつわ。頂点・UV・法線・面を返す"""
    verts, uvs, norms, faces = [], [], [], []
    center_y = RING_BOTTOM - RING_THICK - RING_SINK      # 真ん中の高さ（内側の下のふちが、地面より少し下）
    us = [i / (RING_TEETH * 2) for i in range(RING_TEETH * 2 + 1)]
    steps = 40 if top else 64
    half = RING_DEPTH * 0.5

    def angles(u):
        c = crack_theta(u, 1)
        if top:
            return [-c + (2 * c) * j / steps for j in range(steps + 1)]
        return [c + (2 * math.pi - 2 * c) * j / steps for j in range(steps + 1)]

    grid = [[ring_point(t, u) for t in angles(u)] for u in us]   # [u][j] → (x, y)

    def add(p, uv, n):
        verts.append(p); uvs.append(uv); norms.append(n)
        return len(verts)

    def face_uv(x, y):
        # 殻の面：絵の左 3/4 に、正面から見たまま貼る
        return (0.02 + 0.70 * (x / (2 * RING_HALF_WIDTH) + 0.5), 0.02 + 0.96 * (1.0 - (y + RING_BOTTOM) / (RING_TOP + RING_BOTTOM)))

    def quad(a, b, c, d):
        faces.append((a, b, c)); faces.append((a, c, d))

    # 表（-z）と裏（+z）
    for z, normal, flip in ((-half, (0, 0, -1), False), (half, (0, 0, 1), True)):
        index = [[add((x, y + center_y, z), face_uv(x, y), normal) for (x, y) in row] for row in grid]
        for i in range(len(us) - 1):
            for j in range(steps):
                a, b, c, d = index[i][j], index[i + 1][j], index[i + 1][j + 1], index[i][j + 1]
                quad(a, b, c, d) if not flip else quad(a, d, c, b)

    def strip(points, normals, flip):
        """切り口（段ボールの色）：点の列を、厚みの方向へ帯にする。絵の右 1/4 を使う"""
        length = 0.0
        prev = None
        row = []
        for (x, y), n in zip(points, normals):
            if prev is not None:
                length += math.hypot(x - prev[0], y - prev[1])
            prev = (x, y)
            v = (length * 0.9) % 1.0
            row.append((add((x, y + center_y, -half), (0.78, v), n), add((x, y + center_y, half), (0.97, v), n)))
        for (a0, a1), (b0, b1) in zip(row, row[1:]):
            quad(a0, b0, b1, a1) if not flip else quad(a0, a1, b1, b0)

    def radial(points, sign):
        out = []
        for k, (x, y) in enumerate(points):
            x0, y0 = points[max(k - 1, 0)]
            x1, y1 = points[min(k + 1, len(points) - 1)]
            tx, ty = x1 - x0, y1 - y0
            l = math.hypot(tx, ty) or 1.0
            out.append((sign * ty / l, -sign * tx / l, 0.0))
        return out

    outer, inner = grid[-1], grid[0]
    strip(outer, radial(outer, 1.0), False)     # 外側のふち
    strip(inner, radial(inner, -1.0), True)     # 内側のふち
    # 割れ目の切り口（両はし）
    for j, flip in ((0, True), (steps, False)):
        edge = [grid[i][j] for i in range(len(us))]
        strip(edge, radial(edge, -1.0 if flip else 1.0), flip)
    return verts, uvs, norms, faces


def write_obj(name, piece):
    verts, uvs, norms, faces = piece
    with open(os.path.join(OUT, name + ".obj"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# goal ring (tools/clear/make_clear_assets.py)\nmtllib goal_ring.mtl\no %s\n" % name)
        for p in verts:
            f.write("v %.5f %.5f %.5f\n" % p)
        for t in uvs:
            f.write("vt %.5f %.5f\n" % (t[0], 1.0 - t[1]))   # OBJ の v は下が 0
        for n in norms:
            f.write("vn %.5f %.5f %.5f\n" % n)
        f.write("usemtl goal_ring\n")
        for a, b, c in faces:
            f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c))
    return len(verts), len(faces)


def make_ring():
    with open(os.path.join(OUT, "goal_ring.mtl"), "w", encoding="utf-8", newline="\n") as f:
        f.write("newmtl goal_ring\nKd 1.0 1.0 1.0\nmap_Kd goal_ring.png\n")
    # 絵：左 3/4＝殻の面（クリーム色の紙に、緑の水玉）、右 1/4＝段ボールの切り口
    size = 512
    img = Image.new("RGB", (size, size), (246, 240, 222))
    d = ImageDraw.Draw(img)
    r = random.Random(3)
    for _ in range(46):
        cx, cy, rad = r.uniform(0, size * 0.74), r.uniform(0, size), r.uniform(13, 26)
        d.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], fill=(84, 176, 62))
    g = grain((size, size), 13, 3).point(lambda v: 232 + v // 11)
    img = Image.merge("RGB", [ImageChops.multiply(c, g) for c in img.split()])
    d = ImageDraw.Draw(img)
    d.rectangle([int(size * 0.75), 0, size, size], fill=KRAFT)
    for y in range(0, size, 9):
        d.line([(int(size * 0.75), y), (size, y)], fill=(168, 124, 78), width=3)
    save(img.convert("RGBA"), "goal_ring.png")
    top = write_obj("goal_ring_top", build_ring_piece(True))
    bottom = write_obj("goal_ring_bottom", build_ring_piece(False))
    # ふたを開く時のちょうつがい（右の割れ目の、殻の太さの真ん中）
    hx, hy = ring_point(crack_theta(0.5, 1), 0.5)
    center_y = RING_BOTTOM - RING_THICK - RING_SINK
    print("ring top %d verts %d tris / bottom %d verts %d tris" % (top + bottom))
    return {"hinge": [round(hx, 4), round(hy + center_y, 4)], "halfWidth": RING_HALF_WIDTH,
            "height": round(center_y + RING_TOP, 4), "innerHalfWidth": RING_HALF_WIDTH - RING_THICK,
            "innerHeight": round(center_y + RING_TOP - RING_THICK, 4)}


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
    write_wav("se_ring.wav", mix(0.6, [(0.0, noise_burst(0.09, 1, 0.8, 40) * 1.2), (0.05, noise_burst(0.07, 2, 0.7, 45)),
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
def make_settings(letter_sizes, star_sizes, ring):
    letter_w, letter_h = letter_sizes[0]
    star_w, star_h = star_sizes[0]
    settings = {
        # 場面の長さ(秒)。企画書の配分：輪をくぐる 0〜2 → 「ゴール！」2〜12 → リザルトへ 12〜13 → リザルト 13〜28 → 加算 28〜36 → 幕
        "full":  {"ring": 2.0, "goal": 10.0, "toResult": 1.0, "leave": 0.6},
        "short": {"ring": 1.4, "goal": 2.6, "toResult": 0.7, "leave": 0.4},   # 同じステージの 2 回目から（「ゴール！」まで約 4 秒）
        "jump": {"takeOff": 0.2, "land": 1.3, "pose": 1.0},     # GoalJump クリップの中の時刻・GoalPose の長さ（make_intro_clips.py と合わせる）
        "ring": ring,
        "ringOpen": {"time": 0.7, "angle": 1.15, "hop": 0.35},  # ふたが開く：秒・開く角度(rad)・ぽんと跳ねる高さ(m)
        "letters": {
            "count": len(GOAL_LETTERS),
            "textureSize": [letter_w, letter_h],
            "height": 1.25,          # 文字のマスの高さ(m)。絵はこの (1 + 糸) 倍の縦長
            "string": LETTER_STRING,
            "spacing": 1.12,         # 文字の間(m)
            "centerHeight": 3.5,     # 文字の真ん中の高さ（着地する所の地面から。輪のてっぺん 3.2m に少し掛かるくらい）
            "back": 0.9,             # 着地する所から、輪の方へ下げる(m)
            "start": 0.15,           # 「ゴール！」の場面に入ってから、最初の文字が下り始めるまで(秒)
            "interval": 0.14,        # 文字ごとの遅れ(秒)
            "dropTime": 0.75,        # 下りる時間(秒)
            "dropHeight": 4.6,       # 画面の上の外からの高さ(m)
            "swing": 0.28,           # 下りた時の揺れ(rad)
            "sway": 0.035,           # その後の、ゆっくりした揺れ(rad)
        },
        "stars": {
            "textureSize": [star_w, star_h],
            "height": 0.95,          # 星の絵の高さ(m)。絵はこの (1 + 棒) 倍の縦長
            "stick": STAR_STICK,
            "perSide": 3,
            "pivotSide": 4.3,        # 棒の根元：真ん中から横へ(m)
            "pivotHeight": -0.6,     # 棒の根元の高さ(m)
            "back": 1.3,
            "angles": [0.42, 0.78, 1.14],   # 扇の角度（真上から内側へ rad）
            "start": 0.75,           # 「ゴール！」の場面に入ってから(秒)
            "interval": 0.09,
            "popTime": 0.5,
        },
        "confetti": {"burst": 46, "rain": 5, "rainInterval": 0.22, "height": 5.2, "spread": 3.6},
        # リザルト画面（1920×1080 の画素。x・y は真ん中）
        "result": {
            "dim": [0.03, 0.13, 0.08, 0.74],       # 景色を沈める色（暗い緑）
            "mat": {"x": 960, "y": 566, "scale": 1.0},
            "title": {"x": 470, "y": 230, "scale": 1.0},
            "name": {"x": 1010, "y": 230, "width": 600},
            "hanamaru": {"x": 1532, "y": 246, "size": 236},
            "rowX": 960, "rowYs": [404, 540, 676, 812],   # 帯・1 行目・2 行目・3 行目
            "iconX": 366, "iconSize": 92, "labelX": 430,
            "valueRight": 1330, "valueHeight": 78,
            "stampX": 1516, "stampSize": 112,
            "flowerX": 800, "flowerGap": 124, "flowerSize": 100,
            "prompt": {"x": 1560, "y": 1034},
            "owned": {"x": 200, "y": 64, "numberX": 150, "numberHeight": 62},
            "matIn": 0.5, "itemIn": 0.3, "flowerInterval": 0.4, "countTime": 0.9, "stampTime": 0.35, "rowGap": 0.25,
            "hanamaruTime": 1.25, "addTime": 1.4,
        },
        "sounds": {"ring": "resources/clear/se_ring.wav", "letter": "resources/clear/se_letter.wav",
                   "star": "resources/clear/se_star.wav", "count": "resources/clear/se_count.wav",
                   "stamp": "resources/clear/se_stamp.wav", "hanamaru": "resources/clear/se_hanamaru.wav",
                   "add": "resources/clear/se_add.wav", "road": "resources/clear/se_road.wav"},
    }
    with open(os.path.join(OUT, "clear.json"), "w", encoding="utf-8") as f:
        json.dump(settings, f, ensure_ascii=False, indent=2)
    # ステージごと：ゲームのインスペクターで直して保存するので、ある時は上書きしない
    stage = os.path.join(OUT, "stage1.json")
    if not os.path.exists(stage):
        with open(stage, "w", encoding="utf-8") as f:
            json.dump({
                "ringOffset": -3.2,      # ゴールの点から、レールに沿って輪を置く所(m)。マイナス＝手前
                "landAhead": 1.4,        # 輪をくぐってから、着地する所（輪から先へ m）
                "camera": {"distance": 7.2, "height": 1.7, "side": 0.0, "lookHeight": 1.9, "fovY": 0.72},
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
            put("stamp_empty.png", layout["stampX"], y, height=layout["stampSize"])
            if stamp:
                put("stamp_egg.png", layout["stampX"], y, height=layout["stampSize"])
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
    put("owned_tag.png", layout["owned"]["x"], layout["owned"]["y"])
    number("×4", layout["owned"]["x"] + layout["owned"]["numberX"], layout["owned"]["y"], layout["owned"]["numberHeight"])
    canvas.convert("RGB").save(os.path.join(HERE, "preview_result.png"))
    # 小道具（文字と星）も並べて見る
    sheet = Image.new("RGBA", (1700, 1300), (150, 190, 230, 255))
    x = 30
    for i in range(len(GOAL_LETTERS)):
        img = Image.open(os.path.join(OUT, "goal_letter_%d.png" % i)).convert("RGBA")
        sheet.alpha_composite(img, (x, 20))
        x += img.width + 10
    for i in range(len(STAR_COLORS)):
        img = Image.open(os.path.join(OUT, "goal_star_%d.png" % i)).convert("RGBA")
        sheet.alpha_composite(img, (x, 20))
        x += img.width + 10
    sheet.convert("RGB").resize((850, 650), Image.LANCZOS).save(os.path.join(HERE, "preview_props.png"))
    print("wrote preview_result.png / preview_props.png")


def preview_goal(settings):
    """「ゴール！」の場面を、ゲームと同じ計算（ClearFlow のカメラの終点・GoalProps の板の置き方）で 1 枚の絵にする。
    輪を原点に置き、+X へくぐった時。文字と星は下り切った・出切った所"""
    stage = json.load(open(os.path.join(OUT, "stage1.json"), encoding="utf-8"))
    cam, letters, stars, ring = stage["camera"], settings["letters"], settings["stars"], settings["ring"]
    W, H = 1280, 720
    to_camera = np.array([1.0, 0.0, 0.0])
    land = to_camera * stage["landAhead"]
    up = np.array([0.0, 1.0, 0.0])
    right = np.cross(up, -to_camera)
    eye = land + to_camera * cam["distance"] + up * cam["height"] + right * cam["side"]
    look = land + up * cam["lookHeight"]
    f = (look - eye) / np.linalg.norm(look - eye)
    cr = np.cross(up, f); cr /= np.linalg.norm(cr)
    cu = np.cross(f, cr)
    focal = (H / 2) / math.tan(cam["fovY"] / 2)

    def project(p):
        v = np.asarray(p) - eye
        z = v @ f
        return (W / 2 + (v @ cr) / z * focal, H / 2 - (v @ cu) / z * focal)

    def stage_point(x, y, z):
        return land + right * x + up * y + to_camera * z

    canvas = Image.new("RGBA", (W, H), (140, 190, 235, 255))
    d = ImageDraw.Draw(canvas)
    # 地面（着地する所の高さ）
    ground = [project(stage_point(x, 0, z)) for x, z in ((-9, -8), (9, -8), (9, 3), (-9, 3))]
    d.polygon(ground, fill=(120, 180, 90, 255))

    def paste_quad(img, corners):
        """絵を、画面の四すみ corners（左上・右上・右下・左下）へ貼る"""
        (x0, y0), (x1, y1), (x2, y2), (x3, y3) = corners
        w, h = img.size
        src = [(0, 0), (w, 0), (w, h), (0, h)]
        a = []
        b = []
        for (X, Y), (sx, sy) in zip(corners, src):
            a.append([X, Y, 1, 0, 0, 0, -sx * X, -sx * Y]); b.append(sx)
            a.append([0, 0, 0, X, Y, 1, -sy * X, -sy * Y]); b.append(sy)
        coeff = np.linalg.solve(np.array(a, float), np.array(b, float))
        warped = img.transform((W, H), Image.PERSPECTIVE, tuple(coeff), Image.BICUBIC)
        canvas.alpha_composite(warped)

    def board(img, pivot, angle, width, height, hangs):
        half = height * 0.5 * (-1.0 if hangs else 1.0)
        cx, cy = pivot[0] - math.sin(angle) * half, pivot[1] + math.cos(angle) * half
        ax, ay = math.cos(angle), math.sin(angle)       # 板の横の向き
        bx, by = -math.sin(angle), math.cos(angle)      # 板の上の向き
        corners = []
        for sx, sy in ((-1, 1), (1, 1), (1, -1), (-1, -1)):
            x = cx + ax * sx * width / 2 + bx * sy * height / 2
            y = cy + ay * sx * width / 2 + by * sy * height / 2
            corners.append(project(stage_point(x, y, pivot[2])))
        paste_quad(img, corners)

    # 輪（閉じた形の輪かく）
    center_y = RING_BOTTOM - RING_THICK - RING_SINK
    for u, color in ((1.0, (246, 240, 222, 255)), (0.0, (140, 190, 235, 255))):
        pts = []
        for i in range(72):
            x, y = ring_point(i / 72 * 2 * math.pi, u)
            pts.append(project(np.array([0.0, y + center_y, 0.0]) + right * x))
        d.polygon(pts, fill=color, outline=(120, 100, 70, 255))
    # 星（奥）→ 文字 → 恐竜
    star_h = stars["height"] * stars["textureSize"][1] / stars["textureSize"][0]
    for i in range(stars["perSide"] * 2):
        slot, side = i // 2, (-1.0 if i % 2 == 0 else 1.0)
        img = Image.open(os.path.join(OUT, "goal_star_%d.png" % (i % len(STAR_COLORS)))).convert("RGBA")
        angle = side * stars["angles"][min(slot, len(stars["angles"]) - 1)]
        board(img, (side * stars["pivotSide"], stars["pivotHeight"], -stars["back"]), angle, stars["height"], star_h, False)
    letter_h = letters["height"] * (1.0 + letters["string"])
    letter_w = letter_h * letters["textureSize"][0] / letters["textureSize"][1]
    for i in range(letters["count"]):
        img = Image.open(os.path.join(OUT, "goal_letter_%d.png" % i)).convert("RGBA")
        across = (i - (letters["count"] - 1) * 0.5) * letters["spacing"]
        top = letters["centerHeight"] - letters["height"] * 0.5 + letter_h
        board(img, (across, top, -letters["back"]), 0.0, letter_w, letter_h, True)
    dino = Image.open(os.path.join(RES, "kamishibai", "intro_char_dino.png")).convert("RGBA")
    board(dino, (0.0, 0.0, 0.0), 0.0, 1.45 * dino.width / dino.height, 1.45, False)
    canvas.convert("RGB").save(os.path.join(HERE, "preview_goal.png"))
    print("wrote preview_goal.png")


if __name__ == "__main__":
    make_mat()
    make_row()
    make_labels()
    make_icons()
    make_stamps()
    make_flower_empty()
    make_prompt()
    make_owned_tag()
    make_hanamaru()
    letters = make_goal_letters()
    stars = make_goal_stars()
    ring_info = make_ring()
    make_sounds()
    made = make_settings(letters, stars, ring_info)
    print("wrote", OUT)
    if "--preview" in sys.argv:
        preview(made)
        preview_goal(made)
