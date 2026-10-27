# -*- coding: utf-8 -*-
"""
ゲームオーバー演出（ゲームオーバー演出 企画・仕様書 Rev 1.1）の素材を作る。

  python tools/gameover/make_gameover_assets.py

  ゲームオーバーの画面（ヨッシー クラフトワールドの画面のように、フェルトと段ボールの工作）
    project/resources/gameover/screen_bg.png        紫のフェルトの背景（画面いっぱいに伸ばす）
    project/resources/gameover/screen_board.png     ギザギザの縁の、緑のフェルトの板
    project/resources/gameover/screen_title.png     「GAME OVER」
    project/resources/gameover/button_retry.png / button_retry_on.png   「やりなおす」（ふつう / 選んでいる）
    project/resources/gameover/button_stage.png / button_stage_on.png   「ステージをえらぶ」
    project/resources/gameover/cursor_hand.png      指さしの手袋（選んでいるボタンをさす）
    project/resources/gameover/screen_dino.png      しょんぼり寝ころぶ恐竜（ステージ開始カードの切り絵を寝かせる）
    （画面の置き場所は、ゲームが 1920×1080 を基準に並べる。GameOverScreen.cpp の値と合わせる）
  project/resources/gameover/heart.png             体力の♡（恐竜のまわりの輪に並べる）
  project/resources/gameover/se_crumple.wav        画面がくしゃっとなる「グシャッ」
  project/resources/gameover/se_basa.wav            紙玉が開く「バサッ」
  project/resources/gameover/gameover.json         段ごとの秒数（フル版・短縮版）と、紙・カメラ・体力の値
"""
import json
import math
import os
import random
import struct
import wave

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
OUT = os.path.join(PROJECT, "resources", "gameover")
os.makedirs(OUT, exist_ok=True)

FONT = r"C:\Windows\Fonts\BIZ-UDGothicB.ttc"
CREAM = (255, 250, 236)
BROWN = (92, 58, 30)


def grain(size, seed, scale=3):
    """フェルトのけば（小さなむら）"""
    r = random.Random(seed)
    small = Image.new("L", (max(1, size[0] // scale), max(1, size[1] // scale)))
    small.putdata([r.randint(0, 255) for _ in range(small.width * small.height)])
    return small.resize(size, Image.BILINEAR).filter(ImageFilter.GaussianBlur(0.8))


def felt(size, color, seed, patches=0, patch_shift=14):
    """フェルトの面：けば＋（あれば）色の少し違う切れはしを貼り重ねる"""
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


def shadow_under(img, offset=(10, 14), blur=10, alpha=110):
    """形の下に、やわらかい影を付ける"""
    pad = blur * 3
    out = Image.new("RGBA", (img.width + pad * 2, img.height + pad * 2), (0, 0, 0, 0))
    shadow = Image.new("L", out.size, 0)
    shadow.paste(img.split()[3].point(lambda v: alpha if v > 0 else 0), (pad + offset[0], pad + offset[1]))
    shadow = shadow.filter(ImageFilter.GaussianBlur(blur))
    out.paste((40, 20, 40, 255), (0, 0), shadow)
    out.alpha_composite(img, (pad, pad))
    return out


def save_bg():
    """紫のフェルトの背景（四すみを少し暗く）"""
    w, h = 960, 540
    img = felt((w, h), (92, 66, 138), 3, patches=14, patch_shift=10).convert("RGBA")
    vignette = Image.new("L", (w, h), 0)
    ImageDraw.Draw(vignette).ellipse([-w * 0.2, -h * 0.25, w * 1.2, h * 1.25], fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(60))
    dark = Image.new("RGBA", (w, h), (40, 24, 60, 255))
    img = Image.composite(img, dark, vignette.point(lambda v: 150 + v * 105 // 255))
    img.save(os.path.join(OUT, "screen_bg.png"))


def save_board():
    """緑のフェルトの板：まわりはクリームの縁に、茶色とオレンジのギザギザ（三角）"""
    w, h = 1400, 800
    frame = 54
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w - 1, h - 1], fill=(246, 222, 170, 255))
    # ギザギザ：縁にそって三角を交互に
    step = 54
    for side in range(4):
        length = w if side % 2 == 0 else h
        for i in range(0, length, step):
            color = (196, 112, 46, 255) if (i // step) % 2 == 0 else (128, 70, 34, 255)
            a, b, tip = i, min(i + step, length), i + step / 2
            if side == 0:
                pts = [(a, 0), (b, 0), (tip, frame - 8)]
            elif side == 1:
                pts = [(w, a), (w, b), (w - frame + 8, tip)]
            elif side == 2:
                pts = [(a, h), (b, h), (tip, h - frame + 8)]
            else:
                pts = [(0, a), (0, b), (frame - 8, tip)]
            d.polygon(pts, fill=color)
    inner = felt((w - frame * 2, h - frame * 2), (38, 104, 82), 7, patches=22, patch_shift=16)
    img.paste(inner, (frame, frame))
    # 内側のふちに、白いステッチ
    for x in range(frame + 14, w - frame - 14, 22):
        d.line([(x, frame + 12), (x + 11, frame + 12)], fill=(220, 240, 220, 255), width=3)
        d.line([(x, h - frame - 13), (x + 11, h - frame - 13)], fill=(220, 240, 220, 255), width=3)
    for y in range(frame + 14, h - frame - 14, 22):
        d.line([(frame + 12, y), (frame + 12, y + 11)], fill=(220, 240, 220, 255), width=3)
        d.line([(w - frame - 13, y), (w - frame - 13, y + 11)], fill=(220, 240, 220, 255), width=3)
    shadow_under(img, (12, 16), 14, 140).save(os.path.join(OUT, "screen_board.png"))


def save_title():
    """「GAME OVER」：白い太い文字に、うすい影"""
    font = ImageFont.truetype(FONT, 190)
    w, h = 1100, 260
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.text((w // 2, h // 2), "GAME OVER", font=font, fill=(255, 255, 255, 255), anchor="mm", stroke_width=4, stroke_fill=(255, 255, 255, 255))
    shadow_under(img, (8, 10), 6, 120).save(os.path.join(OUT, "screen_title.png"))


def save_button(name, text, selected):
    """フェルトのボタン：うす緑（選んでいる時は明るく、文字は黄色）に、白いステッチ"""
    w, h = 560, 112
    fill = (150, 214, 160) if selected else (112, 176, 132)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], 26, fill=255)
    base = felt((w, h), fill, 21 if selected else 22, patches=3, patch_shift=8).convert("RGBA")
    img.paste(base, (0, 0), mask)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, h - 18, w - 1, h - 1], 20, fill=tuple(int(c * 0.78) for c in fill) + (255,))  # 下の厚み
    d.rounded_rectangle([10, 10, w - 11, h - 25], 18, outline=(236, 250, 236, 255), width=3)
    for x in range(30, w - 30, 20):
        d.line([(x, 10), (x + 8, 10)], fill=fill + (255,), width=4)  # 点線に見せる
    font = ImageFont.truetype(FONT, 56)
    color = (255, 232, 92) if selected else (255, 255, 255)
    d.text((w // 2, h // 2 - 6), text, font=font, fill=color, anchor="mm", stroke_width=6, stroke_fill=(46, 112, 70))
    shadow_under(img, (6, 8), 6, 110).save(os.path.join(OUT, name))


def save_hand():
    """指さしの手袋（右をさす）"""
    s = 4
    w, h = 220 * s, 150 * s
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    line = (70, 70, 80, 255)
    white = (252, 252, 252, 255)
    lw = 6 * s
    # 袖口
    d.rounded_rectangle([10 * s, 40 * s, 52 * s, 120 * s], 10 * s, fill=(232, 236, 244, 255), outline=line, width=lw)
    for y in (58, 78, 98):
        d.line([(16 * s, y * s), (46 * s, y * s)], fill=(160, 166, 186, 255), width=3 * s)
    # 手のひら
    d.rounded_rectangle([46 * s, 34 * s, 128 * s, 128 * s], 34 * s, fill=white, outline=line, width=lw)
    # 人さし指（右へ）
    d.rounded_rectangle([96 * s, 34 * s, 210 * s, 66 * s], 16 * s, fill=white, outline=line, width=lw)
    # 曲げた指
    for y in (66, 88):
        d.rounded_rectangle([100 * s, y * s, 140 * s, (y + 24) * s], 12 * s, fill=white, outline=line, width=lw)
    # 親指
    d.ellipse([60 * s, 6 * s, 112 * s, 52 * s], fill=white, outline=line, width=lw)
    d.rounded_rectangle([54 * s, 36 * s, 120 * s, 60 * s], 10 * s, fill=white)
    img = img.resize((220, 150), Image.LANCZOS)
    shadow_under(img, (5, 7), 5, 100).save(os.path.join(OUT, "cursor_hand.png"))


def save_dino():
    """しょんぼり寝ころぶ恐竜：ステージ開始カードの切り絵を、あおむけに寝かせる"""
    src = os.path.join(PROJECT, "resources", "kamishibai", "intro_char_dino.png")
    dino = Image.open(src).convert("RGBA")
    lying = dino.rotate(-82, expand=True, resample=Image.BICUBIC)
    lying.save(os.path.join(OUT, "screen_dino.png"))


def heart_shape(size, inset):
    """ハートの形（L）"""
    scale = 4
    s = size * scale
    mask = Image.new("L", (s, s), 0)
    pts = []
    for i in range(400):
        t = i / 400 * 2 * math.pi
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        k = (s / 2 - inset * scale) / 17.5
        pts.append((s / 2 + x * k, s / 2 - y * k + s * 0.04))
    ImageDraw.Draw(mask).polygon(pts, fill=255)
    return mask.resize((size, size), Image.LANCZOS)


def save_heart():
    """体力の♡：赤いフェルトに白いステッチと茶色のふち"""
    size = 160
    outer = heart_shape(size, 4)
    inner = heart_shape(size, 16)
    stitch = heart_shape(size, 26)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    img.paste((92, 40, 28, 255), (0, 0), outer)
    red = felt((size, size), (230, 64, 70), 41).convert("RGBA")
    img.paste(red, (0, 0), inner)
    shine = Image.new("L", (size, size), 0)
    ImageDraw.Draw(shine).ellipse([42, 40, 70, 62], fill=150)
    img.paste((255, 200, 200, 255), (0, 0), shine.filter(ImageFilter.GaussianBlur(4)))
    edge = ImageChops.subtract(stitch, stitch.filter(ImageFilter.MinFilter(5)))
    dots = Image.new("L", (size, size), 0)
    dots.putdata([255 if (i % size + i // size) // 7 % 2 == 0 else 0 for i in range(size * size)])
    img.paste((255, 246, 236, 255), (0, 0), ImageChops.multiply(edge, dots))
    img.save(os.path.join(OUT, "heart.png"))


# --- 音 ----------------------------------------------------------------------
RATE = 44100


def write_wav(name, samples):
    peak = max(1e-6, max(abs(s) for s in samples))
    with wave.open(os.path.join(OUT, name), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s / peak * 0.85)) * 32767)) for s in samples))


def crinkle(duration, bursts, seed, brightness=0.6):
    """紙のくしゃくしゃ：短いノイズの粒をたくさん"""
    r = random.Random(seed)
    n = int(RATE * duration)
    out = [0.0] * n
    for _ in range(bursts):
        start = int(r.random() ** 0.7 * (n - 2000))
        length = r.randint(200, 1400)
        amp = r.uniform(0.3, 1.0) * (1.0 - start / n) ** 0.6
        prev = 0.0
        for k in range(length):
            prev = prev + (r.uniform(-1, 1) - prev) * brightness
            out[start + k] += prev * amp * (1 - k / length) ** 2
    return out


def basa(seed):
    """紙玉が開く「バサッ」：低めの風切り＋紙のはためき"""
    r = random.Random(seed)
    n = int(RATE * 0.28)
    out = []
    lp = 0.0
    for i in range(n):
        t = i / n
        env = math.sin(min(t * 5, 1) * math.pi / 2) * (1 - t) ** 1.5
        lp += (r.uniform(-1, 1) - lp) * 0.25
        out.append(lp * env * (0.6 + 0.4 * math.sin(i / RATE * 2 * math.pi * 38)))
    return out


# 前の作り方で出していた物（クレヨンの文字・選ぶ丸・空の♡・クレヨンの音・シールの音）は使わなくなったので消す
for old in ["text.png", "text_order.png", "ring_retry.png", "ring_retry_order.png", "ring_stage.png", "ring_stage_order.png",
            "heart_empty.png", "se_crayon.wav", "se_peel.wav", "se_stick.wav"]:
    path = os.path.join(OUT, old)
    if os.path.exists(path):
        os.remove(path)

save_bg()
save_board()
save_title()
save_button("button_retry.png", "やりなおす", False)
save_button("button_retry_on.png", "やりなおす", True)
save_button("button_stage.png", "ステージをえらぶ", False)
save_button("button_stage_on.png", "ステージをえらぶ", True)
save_hand()
save_dino()
save_heart()
write_wav("se_crumple.wav", crinkle(0.55, 90, 31))
write_wav("se_basa.wav", basa(32))

# --- 段ごとの秒数と、紙・カメラ・体力 --------------------------------------------
settings = {
    # 丸める（P3＋P5。画面がふちから真ん中へ丸まって紙玉に）→ 開く（紙玉が弾んで開くと看板）→ 看板に文字とボタン（screen）
    #   → 選ぶ → やりなおす：看板が丸まって紙玉に（crumple×0.75）→ 開くとスタート地点の画面（rebuild）
    # reveal（カメラが引いて箱のふちを見せる）は 0＝しない。引くと遠くの空・丘だけ止まって見え、背景がついてくるようで不自然だった
    "full":  {"freeze": 0.5, "reveal": 0.0, "crumple": 0.8, "open": 0.7, "screen": 0.8, "rebuild": 0.8},
    "short": {"freeze": 0.3, "reveal": 0.0, "crumple": 0.55, "open": 0.5, "screen": 0.5, "rebuild": 0.55},
    "sheetDistance": 4.0,     # 写し取った絵を貼る紙（シール）を、カメラからどれだけ前に置くか(m)
    "reveal": {"back": 6.0, "up": 3.0, "fov": 0.18},  # 箱の種明かしで、カメラが引く量
    "boxRim": 1.4,            # 箱のふちが地面からどれだけ上に出るか(m)
    "hearts": 12,             # 体力（恐竜のまわりの輪の♡の数）
    "damage": 3,              # 1回ぶつかる・落ちると減る♡の数
}
json.dump(settings, open(os.path.join(OUT, "gameover.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
print("wrote", OUT)
