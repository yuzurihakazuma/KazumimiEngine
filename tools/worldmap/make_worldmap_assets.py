# -*- coding: utf-8 -*-
"""
ワールドマップ（ステージ選択）の置き場所と画像を作る（ステージ選択・飛び込み演出 企画書）。

  python tools/worldmap/make_worldmap_assets.py

  project/resources/worldmap/worldmap.json        マス（ステージ）・道・台座・プレビューのカメラ
  project/resources/worldmap/worldmap_backdrop.json  まわりの地面・木・丘・雲・空（StageBackdrop で描く）
  project/resources/worldmap/dive.json            飛び込みの時刻（段ごとの長さ・高さ・カメラ）。後から調整できる
  project/resources/worldmap/label_<番号>.png     台座の前に貼るステージ番号の紙
  project/resources/worldmap/label_lock.png       まだ遊べないマスの蓋に貼る「？」
  project/resources/worldmap/panel.png            左の情報パネル（段ボールの板。名前と数字はゲームが重ねる）
  project/resources/worldmap/panel_locked.png     まだ遊べないマスの情報パネル
  project/resources/worldmap/digits.png           パネルに重ねる数字（0〜9・「/」・「-」を横に並べた1枚。1文字 64×96）
  道は SplineRail（布ロード）で引く。マスに立つ所は道の上の点
"""
import json
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
OUT = os.path.join(PROJECT, "resources", "worldmap")
TEX = os.path.join(PROJECT, "resources", "craft", "textures")
os.makedirs(OUT, exist_ok=True)
FONT = r"C:\Windows\Fonts\UDDigiKyokashoN-B.ttc"
BROWN = (70, 45, 20)
KRAFT = (198, 152, 100)
CREAM = (250, 244, 230)

# --- マスと道 ---------------------------------------------------------------
SPACING = 10.0     # マスの間(m)
PEDESTAL_BACK = 3.4  # 立つ所から台座の中心までの奥行き(m)
PEDESTAL_SIZE = [7.0, 0.7, 2.4]  # 台座の幅・高さ・奥行き(m)
STAGES = [
    {"id": "stage1", "label": "1", "stage": "stage1", "unlocked": True},
    {"id": "stage2", "label": "2", "stage": "", "unlocked": False},
    {"id": "stage3", "label": "3", "stage": "", "unlocked": False},
]
nodes = []
for i, s in enumerate(STAGES):
    x = i * SPACING
    z = 1.4 if i % 2 == 1 else 0.0  # 少しうねる道
    nodes.append([x, 0.0, z])
road = [[-6.0, 0.0, 0.0]]
for i, n in enumerate(nodes):
    road.append(n)
    if i + 1 < len(nodes):
        m = nodes[i + 1]
        road.append([(n[0] + m[0]) / 2, 0.0, (n[2] + m[2]) / 2 + (0.6 if i % 2 == 0 else 0.4)])
road.append([nodes[-1][0] + 6.0, 0.0, nodes[-1][2]])

world = {
    "road": road,
    "roadSkin": {"width": 1.6, "color": [0.95, 0.55, 0.28]},
    "pedestal": {"back": PEDESTAL_BACK, "size": PEDESTAL_SIZE},  # 奥行きは上限（ゲームが箱庭の形に合わせて縮める）
    "camera": {"offset": [0.0, 5.2, -8.5], "lookAhead": [0.0, 0.8, 2.6], "fovY": 0.62},
    "stages": [],
}
for s, n in zip(STAGES, nodes):
    world["stages"].append({
        "id": s["id"], "stage": s["stage"], "label": s["label"], "unlocked": s["unlocked"],
        "node": n,
        "dioramaMargin": 0.94,   # 台座の幅に対して、箱庭が占める割合
        "preview": {"distance": 3.2, "height": 2.4, "orbitSeconds": 10.0},  # 布の道をなぞる所を追い、左右にゆっくり振れる
    })
json.dump(world, open(os.path.join(OUT, "worldmap.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)

# --- 飛び込みの時刻（企画書の5段。仮の値・調整前提） ----------------------------
dive = {
    "crouch": 0.3,      # 1. 構え：こっちを向いてしゃがむ
    "jump": 0.5,        # 2. ジャンプ：放物線で箱庭の上空へ
    "apex": 0.2,        # 3. 頂点：くるっと前転・一瞬スロー
    "dive": 0.4,        # 4. 飛び込み：頭から落ちながら小さくなる
    "absorb": 0.3,      # 5. 吸い込み：箱庭の中に消える／画面を閉じ始める
    "apexHeight": 3.2,  # 箱庭の上、どれだけ高く跳ぶか(m)
    "endScale": 0.06,   # 飛び込み終わりの恐竜の大きさ（箱庭と同じくらい小さく）
    "irisTime": 0.35,   # 円を閉じる時間（吸い込みの始めから）
    "irisHold": 0.2,    # 閉じ切ってから、ステージ開始のカードへ移るまで
    "camera": {
        "apex": [1.2, 4.6, -7.0],   # 頂点：跳ぶ弧と箱庭が両方入る、斜め前の高い位置（箱庭の中心から）
        "dive": [0.0, 2.4, -3.0],   # 飛び込み：箱庭へ急に寄って見下ろす
    },
}
json.dump(dive, open(os.path.join(OUT, "dive.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)

# --- まわりの地面・木・丘・雲・空（ステージの背景と同じキット） ---------------------
rng = random.Random(3)
objects = []


def add(model, pos, scale, rot_y=180.0):
    s = scale if isinstance(scale, list) else [scale] * 3
    objects.append({"model": model, "pos": [round(v, 3) for v in pos], "rotY": round(rot_y, 1), "scale": [round(v, 3) for v in s]})


CELL = 2.4
x0, x1 = -16.0, nodes[-1][0] + 16.0
z0, z1 = -12.0, 16.0
top = -0.06  # 地面の上面（布の道より少し下）
x = x0
while x < x1:
    z = z0
    while z < z1:
        s = CELL / 2.02 * 1.005
        add("ground_block_step", [x + CELL / 2, top - 0.62 * 1.8, z + CELL / 2], [s, 1.8, s])
        z += CELL
    x += CELL
# 台座の後ろの木と、道ばたの草花
for k in range(26):
    tx = rng.uniform(x0 + 2, x1 - 2)
    tz = rng.uniform(7.5, 13.5)
    add(rng.choice(["tree_cutout_00", "tree_cutout_01", "tree_cutout_02", "tree_round_small_00", "tree_round_small_01"]),
        [tx, top, tz], rng.uniform(1.6, 2.2), 180 + rng.uniform(-10, 10))
for k in range(90):
    gx = rng.uniform(x0 + 1, x1 - 1)
    gz = rng.uniform(-9, 6.5)
    near_road = min(abs(gz - (0.7 if int(gx / SPACING) % 2 else 0.0)) for _ in [0])
    if abs(gz) < 1.6 or (5.0 < gz < 6.6 and min(abs(gx - n[0]) for n in nodes) < 3.0):
        continue
    if min(max(abs(gx - n[0]) - PEDESTAL_SIZE[0] / 2, abs(gz - (n[2] + PEDESTAL_BACK)) - PEDESTAL_SIZE[2] / 2) for n in nodes) < 0.8:
        continue  # 台座のまわりは空ける
    model = rng.choice(["grass_tuft_00", "grass_tuft_01", "grass_tuft_02", "grass_tuft_00", "flower_orange", "flower_red", "flower_white"])
    add(model, [gx, top, gz], rng.uniform(1.4, 1.9), 180 + rng.uniform(-25, 25))
for k in range(5):
    add("paper_rock_00" if k % 2 else "paper_rock_01", [rng.uniform(x0, x1), top, rng.uniform(-10, -4)], rng.uniform(1.2, 1.7), rng.uniform(0, 360))
x = x0 - 6
while x < x1 + 6:
    add(rng.choice(["hill_backdrop_00", "hill_backdrop_01"]), [x, top - 0.4, 20 + rng.uniform(-1, 1)], rng.uniform(2.4, 2.8))
    x += rng.uniform(9, 13)
x = x0
while x < x1:
    add("hill_backdrop_far", [x, top - 0.6, 27], 3.1)
    x += 22
x = x0
while x < x1:
    add(rng.choice(["cloud_cutout_00", "cloud_cutout_01"]), [x, rng.uniform(7.5, 10), rng.uniform(23, 30)], rng.uniform(2.0, 2.6))
    x += rng.uniform(10, 16)
x = x0 - 20
while x < x1 + 30:
    add("sky_backdrop", [x, -20, 36], [5.0, 8.0, 5.0])
    x += 90
json.dump({"objects": objects}, open(os.path.join(OUT, "worldmap_backdrop.json"), "w", encoding="utf-8"), separators=(",", ":"))


# --- 画像 --------------------------------------------------------------------
def kraft_board(w, h, radius=26):
    """段ボールの板（白いふちの縫い目つき）。RGB で描いてから丸く切る"""
    card = Image.new("RGB", (w, h), KRAFT)
    d = ImageDraw.Draw(card)
    for y in range(0, h, 7):
        d.line([(0, y), (w, y)], fill=(188, 142, 92), width=2)
    d.rounded_rectangle([5, 5, w - 6, h - 6], radius - 6, outline=CREAM, width=4)
    for x in range(22, w - 22, 18):
        d.line([(x, 16), (x + 9, 16)], fill=CREAM, width=3)
        d.line([(x, h - 17), (x + 9, h - 17)], fill=CREAM, width=3)
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius, fill=255)
    img = Image.new("RGBA", (w + 16, h + 16), (0, 0, 0, 0))
    shadow = Image.new("L", img.size, 0)
    ImageDraw.Draw(shadow).rounded_rectangle([10, 12, w + 10, h + 10], radius, fill=120)
    shadow = shadow.filter(ImageFilter.GaussianBlur(5))
    img.paste((0, 0, 0, 255), (0, 0), shadow)
    img.paste(card, (0, 0), mask)
    return img


def text(d, xy, s, size, fill=(255, 255, 250), stroke=BROWN, anchor="la", width=5):
    d.text(xy, s, font=ImageFont.truetype(FONT, size), fill=fill, anchor=anchor, stroke_width=width, stroke_fill=stroke)


def key(d, x, y, s, size=26):
    font = ImageFont.truetype(FONT, size)
    w = d.textlength(s, font=font) + 26
    d.rounded_rectangle([x, y - 19, x + w, y + 19], 8, fill=(250, 246, 236), outline=BROWN, width=3)
    d.text((x + w / 2, y + 1), s, font=font, fill=BROWN, anchor="mm")
    return x + w


def panel(path, locked):
    W, H = 620, 470
    img = kraft_board(W, H)
    d = ImageDraw.Draw(img)
    if locked:
        text(d, (W / 2, 120), "？？？", 84, anchor="mm", width=7)
        text(d, (W / 2, 250), "まだ あそべない", 44, anchor="mm")
        text(d, (W / 2, 312), "つくっているところ…", 30, anchor="mm", width=4)
    else:
        # 名前はゲームが重ねる（intro_name_<stage>.png）。下の段に「コイン」と「ベスト」
        coin_y, best_y = 228, 304
        d.ellipse([46, coin_y - 26, 98, coin_y + 26], fill=(250, 206, 46), outline=BROWN, width=4)
        d.ellipse([60, coin_y - 12, 84, coin_y + 12], outline=(230, 170, 30), width=3)
        text(d, (118, coin_y), "コイン", 40, anchor="lm")
        d.polygon([(72, best_y - 28), (80, best_y - 8), (102, best_y - 8), (84, best_y + 5), (91, best_y + 26),
                   (72, best_y + 13), (53, best_y + 26), (60, best_y + 5), (42, best_y - 8), (64, best_y - 8)],
                  fill=(255, 150, 60), outline=BROWN)
        text(d, (118, best_y), "ベスト", 40, anchor="lm")
    # 操作の案内
    y = H - 62
    if not locked:
        x = key(d, 40, y, "SPACE")
        text(d, (x + 10, y), "とびこむ", 30, anchor="lm", width=4)
    x = key(d, 330 if not locked else 200, y, "A / D")
    text(d, (x + 10, y), "えらぶ", 30, anchor="lm", width=4)
    img.save(os.path.join(OUT, path))


def label(path, s):
    """台座の前に貼る紙：白いフェルトのふちに番号"""
    W = H = 256
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([10, 10, W - 10, H - 10], 34, fill=CREAM, outline=(210, 196, 170), width=6)
    for x in range(36, W - 36, 18):
        d.line([(x, 28), (x + 9, 28)], fill=(200, 180, 150), width=3)
        d.line([(x, H - 29), (x + 9, H - 29)], fill=(200, 180, 150), width=3)
    color = (226, 62, 52) if s != "？" else (120, 100, 80)
    d.text((W / 2, H / 2 + 4), s, font=ImageFont.truetype(r"C:\Windows\Fonts\BIZ-UDGothicB.ttc", 170), fill=color, anchor="mm",
           stroke_width=8, stroke_fill=CREAM)
    img.save(os.path.join(OUT, path))


def digits(path):
    """数字の並び：0123456789/-（1文字 64×96。白い字に茶色のふち）"""
    CW, CH = 64, 96
    chars = "0123456789/-"
    img = Image.new("RGBA", (CW * len(chars), CH), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(r"C:\Windows\Fonts\BIZ-UDGothicB.ttc", 72)
    for i, c in enumerate(chars):
        d.text((i * CW + CW / 2, CH / 2 + 2), c, font=font, fill=(255, 255, 250), anchor="mm", stroke_width=6, stroke_fill=BROWN)
    img.save(os.path.join(OUT, path))


digits("digits.png")
panel("panel.png", False)
panel("panel_locked.png", True)
for s in STAGES:
    label("label_%s.png" % s["label"], s["label"])
label("label_lock.png", "？")
print("wrote", OUT, len(objects), "backdrop objects")
