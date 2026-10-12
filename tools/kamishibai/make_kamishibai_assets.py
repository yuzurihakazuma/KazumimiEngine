# -*- coding: utf-8 -*-
"""
紙芝居遷移の仮アセットを作る（画像と効果音）。

  python tools/kamishibai/make_kamishibai_assets.py

  画像（project/resources/kamishibai/）
    frame.png      段ボールの紙芝居の枠（1920x1080。真ん中の窓は透明）
    door_left.png  観音開きの扉（左）。閉じると真ん中で卵の紋が1つになる
    door_right.png 観音開きの扉（右）
    egg.png        扉に投げる卵（2D）
    hajimari.png   「はじまり、はじまり」の紙の帯
    curtain.png    赤い幕（左の幕。右は左右反転して使う。内側＝右の辺に金の縁）
    valance.png    幕の上の飾り幕（波形の赤い布に金の房）
  効果音
    se_hyoshigi.wav 拍子木（カン、カン）
    se_door.wav     扉がバンッと開く
    se_whoosh.wav   絵が横へ抜ける「シュッ」
    se_patan.wav    段ボールが倒れた「パタン」

  窓の位置（FRAME_WINDOW）はコードの kWindow と同じにしておくこと（KamishibaiTransition.cpp）
"""
import math
import os
import random
import struct
import wave

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
OUT = os.path.join(PROJECT, "resources", "kamishibai")
KRAFT = os.path.join(PROJECT, "resources", "craft", "textures", "kraft.png")
FONT = r"C:\Windows\Fonts\UDDigiKyokashoN-B.ttc"
os.makedirs(OUT, exist_ok=True)

W, H = 1920, 1080
FRAME_WINDOW = (250, 175, 1670, 935)  # 窓（左, 上, 右, 下）
rng = random.Random(3)


def kraft(size, tint=(1.0, 1.0, 1.0)):
    """クラフト紙のテクスチャを敷き詰めて、色味を変える"""
    src = Image.open(KRAFT).convert("RGB")
    out = Image.new("RGB", size)
    for y in range(0, size[1], src.height):
        for x in range(0, size[0], src.width):
            out.paste(src, (x, y))
    r, g, b = out.split()
    r = r.point(lambda v: min(255, int(v * tint[0])))
    g = g.point(lambda v: min(255, int(v * tint[1])))
    b = b.point(lambda v: min(255, int(v * tint[2])))
    return Image.merge("RGB", (r, g, b))  # RGB のまま（半透明の線を重ねた時に混ざるように。透明にするのは最後）


def corrugate(img, box, vertical=True, step=14, alpha=26):
    """段ボールの波（細い縞）"""
    draw = ImageDraw.Draw(img, "RGBA")
    x0, y0, x1, y1 = box
    if vertical:
        for x in range(x0, x1, step):
            draw.line([(x, y0), (x, y1)], fill=(60, 35, 10, alpha), width=3)
            draw.line([(x + 5, y0), (x + 5, y1)], fill=(255, 240, 210, alpha // 2), width=2)
    else:
        for y in range(y0, y1, step):
            draw.line([(x0, y), (x1, y)], fill=(60, 35, 10, alpha), width=3)
            draw.line([(x0, y + 5), (x1, y + 5)], fill=(255, 240, 210, alpha // 2), width=2)


def rough_rect(draw, box, fill, outline=None, width=0, jitter=2.5):
    """手で切ったような、少しガタつく四角"""
    x0, y0, x1, y1 = box
    pts = []
    for i in range(24):
        pts.append((x0 + (x1 - x0) * i / 24, y0 + rng.uniform(-jitter, jitter)))
    for i in range(24):
        pts.append((x1 + rng.uniform(-jitter, jitter), y0 + (y1 - y0) * i / 24))
    for i in range(24):
        pts.append((x1 - (x1 - x0) * i / 24, y1 + rng.uniform(-jitter, jitter)))
    for i in range(24):
        pts.append((x0 + rng.uniform(-jitter, jitter), y1 - (y1 - y0) * i / 24))
    draw.polygon(pts, fill=fill, outline=outline)
    if outline and width > 1:
        draw.line(pts + [pts[0]], fill=outline, width=width)


def egg_emblem(size):
    """卵の紋（クリーム色の丸に、緑の斑点の卵）"""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([4, 4, size - 4, size - 4], fill=(247, 236, 205, 255), outline=(120, 70, 30, 255), width=10)
    d.ellipse([26, 26, size - 26, size - 26], outline=(196, 70, 40, 255), width=6)
    ew, eh = size * 0.42, size * 0.54
    cx, cy = size / 2, size / 2 + 4
    d.ellipse([cx - ew / 2, cy - eh / 2, cx + ew / 2, cy + eh / 2], fill=(255, 255, 250, 255), outline=(90, 60, 30, 255), width=7)
    for sx, sy, r in [(-0.12, -0.12, 0.07), (0.1, -0.02, 0.06), (-0.05, 0.12, 0.065), (0.12, 0.17, 0.04)]:
        x, y, rr = cx + sx * size, cy + sy * size, r * size
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(92, 170, 70, 255))
    return img


def make_frame():
    img = kraft((W, H), (0.92, 0.82, 0.68))
    corrugate(img, (0, 0, W, H), vertical=False, step=22, alpha=9)
    d = ImageDraw.Draw(img, "RGBA")
    x0, y0, x1, y1 = FRAME_WINDOW
    # 外のふち（濃い茶の線）と、窓まわりの段（奥へ下がって見える影）
    d.rectangle([6, 6, W - 7, H - 7], outline=(95, 58, 28, 255), width=10)
    for i, alpha in enumerate([150, 110, 80, 55, 35, 20]):
        d.rectangle([x0 - 6 - i * 3, y0 - 6 - i * 3, x1 + 6 + i * 3, y1 + 6 + i * 3], outline=(70, 40, 15, alpha), width=3)
    # 窓のふちの細い板（明るいクラフト）
    rim = 26
    rim_img = kraft((W, H), (1.05, 0.95, 0.80))
    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    md.rectangle([x0 - rim, y0 - rim, x1 + rim, y1 + rim], fill=255)
    md.rectangle([x0, y0, x1, y1], fill=0)
    img.paste(rim_img, (0, 0), mask)
    d.rectangle([x0 - rim, y0 - rim, x1 + rim, y1 + rim], outline=(110, 66, 30, 255), width=5)
    d.rectangle([x0, y0, x1, y1], outline=(80, 45, 18, 255), width=6)
    # 四隅の鋲
    for cx, cy in [(60, 60), (W - 60, 60), (60, H - 60), (W - 60, H - 60)]:
        d.ellipse([cx - 16, cy - 16, cx + 16, cy + 16], fill=(150, 100, 50, 255), outline=(80, 45, 18, 255), width=4)
        d.ellipse([cx - 6, cy - 9, cx + 2, cy - 1], fill=(230, 200, 150, 200))
    # 上の真ん中の札「かみしばい」
    plate = (W // 2 - 190, 38, W // 2 + 190, 140)
    rough_rect(d, plate, (247, 236, 205, 255))
    d.rectangle([plate[0] + 8, plate[1] + 8, plate[2] - 8, plate[3] - 8], outline=(196, 70, 40, 255), width=4)
    font = ImageFont.truetype(FONT, 56)
    text = "かみしばい"
    tw = d.textlength(text, font=font)
    d.text((W / 2 - tw / 2, 58), text, font=font, fill=(110, 55, 25, 255))
    # 下の台（少し前に出た段）
    d.rectangle([x0 - 80, y1 + rim + 28, x1 + 80, y1 + rim + 52], fill=(120, 75, 35, 255))
    # 窓をくり抜く
    alpha = Image.new("L", (W, H), 255)
    ImageDraw.Draw(alpha).rectangle([x0, y0, x1 - 1, y1 - 1], fill=0)
    img = img.convert("RGBA")
    img.putalpha(alpha)
    img.save(os.path.join(OUT, "frame.png"))


def make_doors():
    x0, y0, x1, y1 = FRAME_WINDOW
    dw, dh = (x1 - x0) // 2, (y1 - y0)
    emblem = egg_emblem(300)
    for side in ("left", "right"):
        img = kraft((dw, dh), (0.98, 0.60, 0.40))  # 枠より赤みのある段ボール（扉だと分かるように）
        corrugate(img, (0, 0, dw, dh), vertical=True, step=18, alpha=18)
        d = ImageDraw.Draw(img, "RGBA")
        d.rectangle([0, 0, dw - 1, dh - 1], outline=(90, 52, 22, 255), width=10)
        d.rectangle([34, 34, dw - 35, dh - 35], outline=(120, 72, 32, 230), width=6)
        d.rectangle([52, 52, dw - 53, dh - 53], outline=(240, 215, 170, 120), width=3)
        # 蝶番（外側の辺）と、取っ手（内側の辺）
        hinge_x = 0 if side == "left" else dw - 26
        for hy in (110, dh - 170):
            d.rectangle([hinge_x, hy, hinge_x + 26, hy + 60], fill=(110, 85, 60, 255), outline=(60, 40, 20, 255), width=3)
        # 卵の紋の半分（閉じると1つになる）
        img = img.convert("RGBA")
        d = ImageDraw.Draw(img, "RGBA")
        if side == "left":
            img.alpha_composite(emblem.crop((0, 0, 150, 300)), (dw - 150, dh // 2 - 150))
        else:
            img.alpha_composite(emblem.crop((150, 0, 300, 300)), (0, dh // 2 - 150))
        # 合わせ目の影（不透明の濃い色。RGBA の画像へ半透明で描くと穴が開くため）
        edge_x = dw - 6 if side == "left" else 0
        d.rectangle([edge_x, 0, edge_x + 5, dh], fill=(95, 60, 28, 255))
        img.save(os.path.join(OUT, "door_%s.png" % side))


def make_egg():
    s = 4  # 大きく描いて縮める（ふちをなめらかに）
    w, h = 170 * s, 210 * s
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pad = 10 * s
    d.ellipse([pad, pad, w - pad, h - pad], fill=(255, 254, 246, 255), outline=(80, 55, 30, 255), width=9 * s)
    for sx, sy, r in [(0.32, 0.38, 0.12), (0.62, 0.30, 0.09), (0.55, 0.62, 0.13), (0.30, 0.70, 0.08), (0.75, 0.55, 0.06)]:
        x, y, rr = sx * w, sy * h, r * w
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(92, 170, 70, 255))
    d.ellipse([w * 0.24, h * 0.14, w * 0.40, h * 0.30], fill=(255, 255, 255, 200))
    img = img.resize((w // s, h // s), Image.LANCZOS)
    img.save(os.path.join(OUT, "egg.png"))


def make_banner():
    w, h = 1000, 210
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    # 影 → 紙の帯
    shadow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    rough_rect(ImageDraw.Draw(shadow), (30, 30, w - 18, h - 14), (60, 35, 10, 110), jitter=4)
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(6)))
    rough_rect(d, (18, 18, w - 30, h - 26), (250, 240, 212, 255), jitter=4)
    d.rectangle([40, 38, w - 52, h - 46], outline=(196, 70, 40, 255), width=5)
    font = ImageFont.truetype(FONT, 104)
    text = "はじまり、はじまり"
    tw = d.textlength(text, font=font)
    tx, ty = (w - 12) / 2 - tw / 2, 36
    d.text((tx, ty), text, font=font, fill=(255, 255, 255, 255), stroke_width=10, stroke_fill=(255, 255, 255, 255))
    d.text((tx, ty), text, font=font, fill=(120, 52, 22, 255), stroke_width=3, stroke_fill=(120, 52, 22, 255))
    img.save(os.path.join(OUT, "hajimari.png"))


# ---------------- 効果音 ----------------
RATE = 44100


def write_wav(name, samples):
    peak = max(1e-6, max(abs(v) for v in samples))
    scale = 0.85 / peak
    with wave.open(os.path.join(OUT, name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v * scale)) * 32767)) for v in samples))


def clack(length=0.18):
    """木を打ち合わせた「カン」：高めの減衰する音＋最初だけ雑音"""
    out = []
    for i in range(int(RATE * length)):
        t = i / RATE
        tone = (math.sin(2 * math.pi * 1180 * t) * 0.9 + math.sin(2 * math.pi * 2470 * t) * 0.55
                + math.sin(2 * math.pi * 3950 * t) * 0.3) * math.exp(-t / 0.035)
        noise = rng.uniform(-1, 1) * math.exp(-t / 0.004)
        out.append(tone + noise * 0.8)
    return out


def make_sounds():
    # 拍子木：カン（0.34秒）カン
    gap = int(RATE * 0.34)
    a = clack()
    total = [0.0] * (gap + len(a))
    for i, v in enumerate(a):
        total[i] += v
        total[gap + i] += v * 1.1
    write_wav("se_hyoshigi.wav", total)

    # 扉：バンッ（低いドン＋板が鳴る）
    out = []
    lp = 0.0
    for i in range(int(RATE * 0.45)):
        t = i / RATE
        thump = math.sin(2 * math.pi * (95 - 40 * t) * t) * math.exp(-t / 0.09)
        board = math.sin(2 * math.pi * 410 * t) * math.exp(-t / 0.05) * 0.35
        lp += (rng.uniform(-1, 1) - lp) * 0.25
        out.append(thump + board + lp * math.exp(-t / 0.02) * 1.2)
    write_wav("se_door.wav", out)

    # シュッ：だんだん高くなる雑音（ゆるく帯域を絞る）
    out = []
    lp = hp = 0.0
    n = int(RATE * 0.38)
    for i in range(n):
        t = i / n
        env = math.sin(math.pi * t) ** 1.5
        cut = 0.05 + 0.35 * t
        lp += (rng.uniform(-1, 1) - lp) * cut
        hp = lp - hp * 0.0
        out.append(lp * env)
    write_wav("se_whoosh.wav", out)

    # パタン：段ボールが倒れて地面をたたく（低めのドスッ＋乾いた紙の音）
    out = []
    lp = 0.0
    for i in range(int(RATE * 0.22)):
        t = i / RATE
        thump = math.sin(2 * math.pi * (150 - 120 * t) * t) * math.exp(-t / 0.05)
        lp += (rng.uniform(-1, 1) - lp) * 0.45
        out.append(thump * 0.9 + lp * math.exp(-t / 0.018) * 1.4)
    write_wav("se_patan.wav", out)


def make_curtain():
    """赤いビロードの幕：縦のひだ（明暗の波）＋布目のざらつき＋裾の金の房＋内側の金の縁"""
    import numpy as np
    w, h = 1024, 1080
    x = np.arange(w)[None, :].astype(np.float32)
    y = np.arange(h)[:, None].astype(np.float32)
    nrng = np.random.default_rng(5)
    # ひだ：周期の違う波を重ねる（同じ幅の繰り返しに見えないように）
    folds = (0.55 * np.sin(x / w * 2 * np.pi * 9.0 + 0.3)
             + 0.30 * np.sin(x / w * 2 * np.pi * 15.3 + 1.7)
             + 0.15 * np.sin(x / w * 2 * np.pi * 27.1 + 0.4))
    folds = (folds - folds.min()) / (folds.max() - folds.min())
    light = 0.38 + 0.62 * folds ** 1.4
    light = light * (0.82 + 0.18 * (y / h))          # 上が少し暗い（上の飾り幕の影）
    light = light * (1.0 + 0.06 * nrng.standard_normal((h, w)).astype(np.float32))
    r = np.clip(178 * light + 12, 0, 255)
    g = np.clip(18 * light + 4, 0, 255)
    b = np.clip(30 * light + 8, 0, 255)
    img = np.stack([r * np.ones((h, w)), g * np.ones((h, w)), b * np.ones((h, w)), np.full((h, w), 255.0)], axis=-1)
    # 裾：少し波打たせて、下を透明に
    hem = h - 46 + (6 * np.sin(x / w * 2 * np.pi * 9.0 + 0.3)).astype(np.float32)
    img[..., 3] = np.where(y > hem, 0, 255)
    out = Image.fromarray(img.astype(np.uint8), "RGBA")
    d = ImageDraw.Draw(out)
    # 内側（右の辺）の金の縁
    d.rectangle([w - 26, 0, w - 1, h - 40], fill=(222, 170, 60, 255))
    d.rectangle([w - 26, 0, w - 22, h - 40], fill=(150, 100, 30, 255))
    # 裾の金の房
    for fx in range(0, w, 9):
        top = int(h - 52 + 6 * math.sin(fx / w * 2 * math.pi * 9.0 + 0.3))
        d.line([(fx, top), (fx + 2, top + 40)], fill=(230, 180, 70, 255), width=4)
    d.line([(0, h - 52), (w, h - 52)], fill=(200, 150, 50, 255), width=8)
    out.save(os.path.join(OUT, "curtain.png"))


def make_valance():
    """上の飾り幕：横に並んだ半円のたるみ（スワッグ）＋金の房と、たるみの間の房飾り"""
    w, h = 1920, 230
    swag = 320
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w, 70], fill=(120, 8, 18, 255))
    for i in range(w // swag + 1):
        x0 = i * swag
        # たるみ（下へふくらんだ布）。上から下へ明るくして、布の丸みを出す
        for k in range(14):
            t = k / 13
            col = (int(110 + 80 * t), int(8 + 12 * t), int(18 + 14 * t), 255)
            d.chord([x0 - 10 + k * 2, -120 + k * 3, x0 + swag + 10 - k * 2, 170 - k * 2], 0, 180, fill=col)
        d.arc([x0 - 10, -120, x0 + swag + 10, 170], 0, 180, fill=(220, 170, 60, 255), width=9)
        # 金の房（たるみの下のふち）
        for a in range(8, 173, 6):
            rad = math.radians(a)
            cx, cy = x0 + swag / 2, 25
            rx, ry = swag / 2 + 10, 145
            px, py = cx + rx * math.cos(rad), cy + ry * math.sin(rad)
            d.line([(px, py), (px, py + 22)], fill=(235, 185, 75, 255), width=3)
        # たるみの間の房飾り
        tx = x0
        d.ellipse([tx - 16, 40, tx + 16, 72], fill=(230, 180, 70, 255), outline=(150, 100, 30, 255), width=3)
        d.polygon([(tx - 14, 70), (tx + 14, 70), (tx + 20, 150), (tx - 20, 150)], fill=(222, 170, 60, 255))
        for fx in range(-18, 20, 5):
            d.line([(tx + fx, 120), (tx + fx * 1.1, 152)], fill=(160, 110, 35, 255), width=2)
    d.rectangle([0, 0, w, 16], fill=(222, 170, 60, 255))
    img.save(os.path.join(OUT, "valance.png"))


make_frame()
make_curtain()
make_valance()
make_doors()
make_egg()
make_banner()
make_sounds()
print("wrote", OUT)
