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
    curtain.png    赤い色画用紙の蛇腹の幕（左の幕。右は左右反転して使う）
    valance.png    幕の上の段ボールの幕板と三角旗
    intro_backing_top/_bottom.png カードの台紙（読み込み中・ステージ開始で共通。左右は段ボールの切れ目、下の段に「ロードちゅう」）。
                   下の折り目の線（CARD_CUT_Y）で上と下に分けてある。シーンが始まる時、はさみでここを切って分かれる
    intro_cut.png  切れ目が開いた線（はさみが進んだところまで見せる）
    intro_scissors.png 切るはさみ
    intro_layout.json カードの部品の置き場所と、切れ目・はさみの寸法（ゲームはこれを読む）
                   帯に出す名前は resources/stage_intro/<中身>.json（ステージ＝stage1、読み込み中＝card_<行き先のシーン>）
  効果音
    se_hyoshigi.wav 拍子木（カン、カン）
    se_door.wav     扉がバンッと開く
    se_whoosh.wav   絵が横へ抜ける「シュッ」
    se_patan.wav    段ボールが倒れた「パタン」
    se_snip.wav     はさみで段ボールを切る「ザクザク」
    se_pon.wav      ステージ開始カードに紙を貼る「ポン」
    se_roll.wav     ステージ開始カードの巻いた布が転がる「コロコロ」
    intro_*.png    カードの部品（星・名前の帯と巻いた布・キャラの切り絵・名前・上下の帯）。
                   名前は resources/stage_intro/<中身>.json から作る。キャラは render_cutouts.py（Blender）の切り絵

  窓の位置（FRAME_WINDOW）はコードの kWindow と同じにしておくこと（KamishibaiTransition.cpp）
  キャラの切り絵（読み込みカード・ステージ開始カード共通）は tools/kamishibai/cutouts/（render_cutouts.py を Blender で実行）
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

    # ザクザク：はさみで段ボールを切る（短い雑音の刻みを並べる）
    out = [0.0] * int(RATE * 0.48)
    lp = 0.0
    for k in range(7):
        start = int(RATE * (0.02 + k * 0.064))
        for i in range(int(RATE * 0.05)):
            t = i / RATE
            lp += (rng.uniform(-1, 1) - lp) * 0.55
            click = math.sin(2 * math.pi * 2600 * t) * math.exp(-t / 0.002)
            if start + i < len(out):
                out[start + i] += (lp * math.exp(-t / 0.014) + click * 0.5) * (0.8 + 0.2 * (k % 2))
    write_wav("se_snip.wav", out)

    # ポン：紙を貼る（高い音から下がる短い音）
    out = []
    phase = 0.0
    for i in range(int(RATE * 0.16)):
        t = i / RATE
        freq = 380 + 620 * math.exp(-t / 0.03)
        phase += 2 * math.pi * freq / RATE
        out.append(math.sin(phase) * math.exp(-t / 0.05) + rng.uniform(-1, 1) * math.exp(-t / 0.004) * 0.3)
    write_wav("se_pon.wav", out)

    # コロコロ：巻いた布が転がる（やわらかい雑音に、転がる刻みを付ける）
    out = []
    lp = 0.0
    n = int(RATE * 0.8)
    for i in range(n):
        t = i / RATE
        lp += (rng.uniform(-1, 1) - lp) * 0.08
        bump = 0.6 + 0.4 * abs(math.sin(2 * math.pi * 7.5 * t * (1.0 - 0.35 * t)))
        env = min(1.0, t / 0.05) * (1.0 - (i / n) ** 2)
        out.append(lp * bump * env)
    write_wav("se_roll.wav", out)


TEX = os.path.join(PROJECT, "resources", "craft", "textures")


def tiled_texture(name, size, tint=(1.0, 1.0, 1.0)):
    """キットのテクスチャを敷き詰めて色味を変える（RGB）"""
    import numpy as np
    src = Image.open(os.path.join(TEX, name)).convert("RGB")
    out = Image.new("RGB", size)
    for y in range(0, size[1], src.height):
        for x in range(0, size[0], src.width):
            out.paste(src, (x, y))
    a = np.asarray(out).astype(np.float32) * np.array(tint, np.float32)
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


CURTAIN_PLEAT = 72  # 蛇腹のひと山の幅（山の面と谷の面で半分ずつ）


def make_curtain():
    """赤い色画用紙の蛇腹の幕（左の幕。右は左右反転して使う）。
    山の面と谷の面を明暗で交互に塗り、折り目に線を引く。裾は折り目に合わせたギザギザ。
    幕は幅を伸び縮みさせて開け閉めするので、蛇腹が伸びる・縮むように見える"""
    import numpy as np
    w, h = 1024, 1080
    half = CURTAIN_PLEAT // 2
    paper = tiled_texture("paper_red.png", (w, h), (0.92, 0.55, 0.55))
    x = np.arange(w)
    face = (x // half) % 2
    # 山の面は明るく、谷の面は暗く。面の中でも少しだけ明暗をつけて、折った紙の丸みを出す
    within = (x % half) / half
    light = np.where(face == 0, 1.0 - 0.08 * within, 0.70 + 0.06 * within).astype(np.float32)
    y = np.arange(h).astype(np.float32)[:, None]
    light = light[None, :] * (0.86 + 0.14 * np.clip(y / 260.0, 0.0, 1.0))  # 上の幕板の影
    a = np.asarray(paper).astype(np.float32) * light[..., None]
    img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))  # RGB のまま線を描く（半透明の線が混ざるように）
    d = ImageDraw.Draw(img, "RGBA")
    for k in range(1, w // half + 1):
        d.line([(k * half, 0), (k * half, h - 40)], fill=(120, 28, 30, 150), width=2)  # 折り目
    d.line([(w - 3, 0), (w - 3, h - 30)], fill=(80, 40, 20, 255), width=6)  # 内側の辺の輪郭（切り抜きの線）
    # 透明にするのは最後（RGBA の画像へ半透明で描くと、その線が透けて後ろの画面が見えてしまう）
    img = img.convert("RGBA")
    # 裾：折り目ごとに上下するギザギザ
    mask = Image.new("L", (w, h), 0)
    pts = [(0, 0), (w, 0), (w, h - 40)]  # 右の辺（真ん中で合わさる辺）は裾まで真っすぐ（すき間を作らない）
    for k in range(w // half, -1, -1):
        pts.append((k * half, h - 30 if k % 2 == 0 else h - 48))
    ImageDraw.Draw(mask).polygon(pts, fill=255)
    img.putalpha(mask)
    img.save(os.path.join(OUT, "curtain.png"))


def make_valance():
    """上の幕板：段ボールの板（下のふちに段ボールの断面）と、色画用紙の三角旗"""
    w, h = 1920, 200
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    img.paste(tiled_texture("kraft.png", (w, 110), (0.95, 0.85, 0.72)).convert("RGBA"), (0, 0))
    edge = Image.open(os.path.join(TEX, "kraft_edge.png")).convert("RGBA").resize((256, 40))
    for x in range(0, w, 256):
        img.paste(edge, (x, 104))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w - 1, 143], outline=(80, 45, 18, 255), width=5)
    flag_textures = ["paper_yellow.png", "paper_white.png", "paper_blue.png"]
    for i, x in enumerate(range(20, w, 120)):
        tex = tiled_texture(flag_textures[i % 3], (100, 60)).convert("RGBA")
        m = Image.new("L", (100, 60), 0)
        ImageDraw.Draw(m).polygon([(0, 0), (100, 0), (50, 56)], fill=255)
        img.paste(tex, (x, 144), m)
        d.polygon([(x, 144), (x + 100, 144), (x + 50, 200)], outline=(80, 45, 18, 255))
    img.save(os.path.join(OUT, "valance.png"))


# ---------------- カード（読み込み中・ステージ開始で共通） ----------------
def card_background(width=W):
    """明るいクラフト色の段ボール：細かいざらつき・点々・うっすら縦の段、下に横向きの折り返し"""
    import numpy as np
    W = width  # この関数の中だけ、カードの幅
    nrng = np.random.default_rng(2)
    base = np.array([212, 168, 116], np.float32)
    fine = nrng.normal(0, 1, (H, W)).astype(np.float32)
    blot_small = (nrng.random((H // 40, W // 40)) * 255).astype(np.uint8)
    blot = np.asarray(Image.fromarray(blot_small).resize((W, H), Image.BICUBIC)).astype(np.float32) / 255.0
    x = np.arange(W)[None, :].astype(np.float32)
    flute = 0.975 + 0.025 * np.sin(x / 44.0 * 2 * np.pi)
    light = (0.94 + 0.08 * blot) * flute + 0.025 * fine
    a = base[None, None, :] * light[..., None]
    a[nrng.random((H, W)) < 0.0018] *= 0.6  # 紙の繊維の点々
    img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    y = np.arange(170)[:, None].astype(np.float32)
    ridge = 0.93 + 0.05 * np.sin(y / 30.0 * 2 * np.pi)
    flap = np.asarray(img.crop((0, H - 170, W, H))).astype(np.float32) * ridge[..., None] * 0.97
    img.paste(Image.fromarray(np.clip(flap, 0, 255).astype(np.uint8)), (0, H - 170))
    d = ImageDraw.Draw(img, "RGBA")
    d.line([(0, H - 172), (W, H - 172)], fill=(110, 75, 40, 150), width=4)
    d.line([(0, H - 166), (W, H - 166)], fill=(255, 240, 210, 110), width=3)
    return img.convert("RGBA")


def sticker(img, border, scale):
    """切り抜いた絵をシールにする（白いふち）"""
    img = img.resize((max(1, int(img.width * scale)), max(1, int(img.height * scale))), Image.LANCZOS)
    pad = border * 2 + 12
    canvas = Image.new("RGBA", (img.width + pad * 2, img.height + pad * 2), (0, 0, 0, 0))
    alpha = Image.new("L", canvas.size, 0)
    alpha.paste(img.getchannel("A"), (pad, pad))
    white = Image.new("RGBA", canvas.size, (250, 248, 240, 255))
    white.putalpha(alpha.filter(ImageFilter.MaxFilter(border * 2 + 1)).filter(ImageFilter.GaussianBlur(1.2)))
    canvas.alpha_composite(white)
    canvas.alpha_composite(img, (pad, pad))
    return canvas


CARD_MARGIN = 150   # カードの左右の、段ボールの切れ目（ギザギザ）を描く幅。画面に止まった時は画面の外になる
CARD_TOOTH = 120    # ギザギザの山の間隔(px)
CARD_DEPTH = 90     # ギザギザの深さ(px)


CARD_CUT_Y = H - 169  # 切れ目（下の折り目の線）。シーンが始まる時、ここをはさみで切って上と下に分かれる
CARD_CUT_OVERLAP = 3  # 上の部分を切れ目より少し下まで伸ばす（分かれる前に、上下の境目のすき間が見えないように）
CARD_BANNER_Y = 380               # 行き先の名前の帯の中心の高さ（ステージ開始カードとほぼ同じ）
CARD_CLEAR_Y = 805                # 部品の下の端はここより上（切れ目のまわりは、はさみが通るので何も置かない）
CARD_TOP_HEIGHT = CARD_CUT_Y + 12   # 上の部分の画像の高さ（切れ目の波の分だけ余裕を持たせる）
CARD_BOTTOM_TOP = CARD_CUT_Y - 12   # 下の部分の画像の上端


def card_cut_line(width):
    """切れ目の線（手で切ったように、ほんの少し波打つ）"""
    prng = random.Random(11)
    return [(x, CARD_CUT_Y + 2.5 * math.sin(x / 70.0) + prng.uniform(-1.2, 1.2)) for x in range(-20, width + 40, 16)]


def make_card_slit(width):
    """切れ目が開いた線（はさみが進んだところまで、ゲームが左から少しずつ見せる）"""
    top = CARD_CUT_Y - 20
    img = Image.new("RGBA", (width, 40), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    line = [(x, y - top) for x, y in card_cut_line(width)]
    d.line(line, fill=(55, 32, 12, 255), width=7)                                 # 切れ目の奥（暗い）
    d.line([(x, y + 4) for x, y in line], fill=(240, 214, 168, 255), width=2)      # 切り口に見える段ボールの芯
    img.save(os.path.join(OUT, "intro_cut.png"))
    return {"slitTop": top, "slitHeight": img.height}


def make_scissors():
    """紙工作のはさみ（刃先は右。赤いフェルトの持ち手、白いふち）。刃先の位置を返す"""
    k = 4
    w, h = 270 * k, 150 * k
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pivot = (135 * k, 75 * k)
    tips = [(262 * k, 58 * k), (262 * k, 92 * k)]
    # 刃（明るい灰色、ふちは濃い灰色）
    for tip, side in zip(tips, (-1, 1)):
        blade = [(pivot[0] - 30 * k, pivot[1] + side * 4 * k), tip, (pivot[0] + 10 * k, pivot[1] - side * 12 * k)]
        d.polygon(blade, fill=(214, 220, 228), outline=(90, 96, 108))
        d.line([blade[0], tip], fill=(90, 96, 108), width=3 * k)
        d.line([tip, blade[2]], fill=(90, 96, 108), width=3 * k)
    # 持ち手（赤いフェルトの輪）
    handle = tiled_texture("paper_red.png", (w, h), (1.05, 0.95, 0.95)).convert("RGBA")
    mask = Image.new("L", (w, h), 0)
    md = ImageDraw.Draw(mask)
    for cy in (42 * k, 108 * k):
        md.ellipse([22 * k, cy - 34 * k, 96 * k, cy + 34 * k], fill=255)
        md.ellipse([40 * k, cy - 17 * k, 78 * k, cy + 17 * k], fill=0)
        md.polygon([(84 * k, cy - 12 * k), (pivot[0], pivot[1] - 8 * k), (pivot[0], pivot[1] + 8 * k), (84 * k, cy + 12 * k)], fill=255)
    handle.putalpha(mask)
    img.alpha_composite(handle)
    edge = Image.new("RGBA", (w, h), (120, 30, 30, 255))
    edge.putalpha(mask.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.MaxFilter(5)))
    img.alpha_composite(edge)
    d = ImageDraw.Draw(img)
    d.ellipse([pivot[0] - 9 * k, pivot[1] - 9 * k, pivot[0] + 9 * k, pivot[1] + 9 * k], fill=(160, 120, 40), outline=(90, 70, 20), width=2 * k)
    img = img.resize((w // k, h // k), Image.LANCZOS)
    border = 6
    out = sticker(img, border, 1.0)
    pad = border * 2 + 12
    out.save(os.path.join(OUT, "intro_scissors.png"))
    return {"file": "intro_scissors.png", "tipX": 262 + pad, "tipY": 75 + pad, "width": out.width, "height": out.height}


def card_shape(width):
    """左右の辺がギザギザに切れたカードの輪郭（幅 width = 画面 + 左右の切れ目）"""
    left, right = [], []
    for i, y in enumerate(range(0, H + CARD_TOOTH, CARD_TOOTH)):
        y = min(y, H)
        out = (i % 2 == 1)
        left.append((CARD_MARGIN - (CARD_DEPTH if out else 4), y))
        right.append((width - CARD_MARGIN + (CARD_DEPTH if out else 4), y))
    return left, right


def piece_with_shadow(img, shadow=(10, 12)):
    """部品に影を焼き込む（影の分だけ画像を広げる。中心は部品の中心のまま）"""
    pad = 24
    canvas = Image.new("RGBA", (img.width + pad * 2, img.height + pad * 2), (0, 0, 0, 0))
    sh = Image.new("RGBA", img.size, (40, 25, 10, 0))
    sh.putalpha(img.getchannel("A").filter(ImageFilter.GaussianBlur(7)).point(lambda v: v * 0.45))
    canvas.alpha_composite(sh, (pad + shadow[0], pad + shadow[1]))
    canvas.alpha_composite(img, (pad, pad))
    return canvas


# ---------------- ステージ開始カード ----------------
INTRO_DIR = os.path.join(PROJECT, "resources", "stage_intro")  # ステージごとの中身（名前・色・敵・カメラ）
CUTOUT_DIR = os.path.join(HERE, "cutouts")                       # render_cutouts.py が Blender で描き出した切り絵
INTRO_BANNER_SIZE = (1240, 250)  # 名前の帯（布）の大きさ
INTRO_ROLL_HEIGHT = 278          # 巻いた布の高さ（帯より少し高い）
INTRO_ROLL_PERIOD = 120          # 巻いた布の模様の1周期(px)。ゲームはこの幅ずつ模様をずらして転がって見せる
INTRO_LOADING_X, INTRO_LOADING_Y = 1380, 960  # 「ロードちゅう」の左上（カードの座標）


def intro_star_mask(size, r, prng):
    """星の形（角を少し丸めた、ちぎった紙の星）。size×size の L 画像"""
    k = 4
    big = Image.new("L", (size * k, size * k), 0)
    pts = []
    for i in range(10):
        rr = (r if i % 2 == 0 else r * 0.5) * k
        ang = math.pi * i / 5 - math.pi / 2
        pts.append((size * k / 2 + rr * math.cos(ang) + prng.uniform(-3, 3) * k,
                    size * k / 2 + rr * math.sin(ang) + prng.uniform(-3, 3) * k))
    ImageDraw.Draw(big).polygon(pts, fill=255)
    big = big.filter(ImageFilter.GaussianBlur(9 * k)).point(lambda v: 255 if v > 118 else 0)  # 角を丸める
    return big.resize((size, size), Image.LANCZOS)


def crumple(size, seed, base=(246, 244, 238)):
    """紙のしわ（白っぽい紙に、ゆるい明暗と折れ線）。色はゲームが掛けるので白で作る"""
    import numpy as np
    nrng = np.random.default_rng(seed)
    w, h = size
    low = nrng.random((max(2, h // 36), max(2, w // 36)))
    low = np.asarray(Image.fromarray((low * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)).astype(np.float32) / 255.0
    light = 0.88 + 0.12 * low + 0.02 * nrng.normal(0, 1, (h, w))
    img = Image.fromarray(np.clip(np.array(base, np.float32)[None, None, :] * light[..., None], 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img, "RGBA")
    prng = random.Random(seed)
    for _ in range(7):  # 折れ線（明るい線と暗い線を並べる）
        x0, y0 = prng.uniform(0, w), prng.uniform(0, h)
        ang = prng.uniform(0, math.pi)
        length = prng.uniform(w * 0.3, w * 0.8)
        x1, y1 = x0 + math.cos(ang) * length, y0 + math.sin(ang) * length
        d.line([(x0, y0), (x1, y1)], fill=(255, 255, 255, 90), width=2)
        d.line([(x0 + 2, y0 + 2), (x1 + 2, y1 + 2)], fill=(120, 110, 100, 45), width=2)
    return img


def shadow_and_edge(mask, edge=8, shadow=(10, 12)):
    """白いふち（切り抜いた余白）と、台紙に落ちる影（色は付けない層）"""
    from PIL import ImageChops
    w, h = mask.size
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    grown = mask.filter(ImageFilter.MaxFilter(edge * 2 + 1)) if edge else mask
    sh = Image.new("RGBA", (w, h), (40, 25, 10, 0))
    sh.putalpha(ImageChops.offset(grown, *shadow).filter(ImageFilter.GaussianBlur(7)).point(lambda v: v * 0.45))
    out.alpha_composite(sh)
    if edge:
        white = Image.new("RGBA", (w, h), (250, 248, 240, 255))
        white.putalpha(grown)
        out.alpha_composite(white)
    return out


def intro_cutout(name, height, border=12):
    """Blender で描き出した切り絵を、白いふち＋影つきのシールにする（高さ height px）"""
    from PIL import ImageEnhance
    img = Image.open(os.path.join(CUTOUT_DIR, name + ".png")).convert("RGBA")
    img = img.crop(img.getchannel("A").getbbox())
    alpha = img.getchannel("A")
    rgb = ImageEnhance.Contrast(ImageEnhance.Color(img.convert("RGB")).enhance(1.3)).enhance(1.08)  # 描き出しは淡いので少し濃く
    img = rgb.convert("RGBA")
    img.putalpha(alpha)
    scale = height / img.height
    return piece_with_shadow(sticker(img, border, scale))


def make_intro_name(stem, data):
    """ステージ名＋ふりがなの画像（白い丸い太字、ふちは濃い緑）"""
    render_intro_name(data["name"], data.get("ruby", [])).save(os.path.join(OUT, "intro_name_%s.png" % stem))


def render_intro_name(title, ruby):
    """名前＋ふりがなの画像（白い丸い太字、ふちは濃い緑）。ruby＝[(ふりがな, 何文字目から, 何文字)]"""
    font = ImageFont.truetype(FONT, 112)
    ruby_font = ImageFont.truetype(FONT, 38)
    probe = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    width = int(probe.textlength(title, font=font)) + 80
    img = Image.new("RGBA", (width, 230), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    tx, ty = 40, 78
    d.text((tx + 5, ty + 7), title, font=font, fill=(40, 70, 10, 255), stroke_width=12, stroke_fill=(40, 70, 10, 255))  # 影
    d.text((tx, ty), title, font=font, fill=(255, 255, 250, 255), stroke_width=12, stroke_fill=(70, 110, 20, 255))
    d.text((tx, ty), title, font=font, fill=(255, 255, 250, 255))
    for text, start, count in ruby:
        left = tx + probe.textlength(title[:start], font=font)
        span = probe.textlength(title[start:start + count], font=font)
        rx = left + span / 2 - probe.textlength(text, font=ruby_font) / 2
        d.text((rx, ty - 58), text, font=ruby_font, fill=(255, 255, 250, 255), stroke_width=6, stroke_fill=(70, 110, 20, 255))
    return img


def make_intro_banner():
    """名前の帯：フェルトの布（色はゲームが掛けるので白っぽく作る）＋縫い目＋影。左の端は少し巻いてある"""
    import numpy as np
    w, h = INTRO_BANNER_SIZE
    pad = 30
    cw, ch = w + pad * 2, h + pad * 2
    prng = random.Random(21)
    # 布の形（上下のふちを手で切ったように少し波打たせる）
    top = [(pad + x, pad + 4 * math.sin(x / 47.0) + prng.uniform(-2, 2)) for x in range(0, w + 1, 10)]
    bottom = [(pad + x, pad + h + 4 * math.sin(x / 53.0 + 1.0) + prng.uniform(-2, 2)) for x in range(w, -1, -10)]
    mask = Image.new("L", (cw, ch), 0)
    ImageDraw.Draw(mask).polygon(top + bottom, fill=255)
    # フェルトの地（毛羽立ち）。灰色がかった白
    felt = tiled_texture("felt_grass.png", (cw, ch))
    gray = np.asarray(felt.convert("L")).astype(np.float32)
    gray = 0.88 + 0.12 * (gray - gray.mean()) / (gray.std() + 1e-5) * 0.4  # 毛羽立ちは控えめに（文字を読みやすく）
    body = Image.fromarray(np.clip(np.stack([gray * 245, gray * 245, gray * 240], -1), 0, 255).astype(np.uint8)).convert("RGBA")
    body.putalpha(mask)
    body.save(os.path.join(OUT, "intro_banner.png"))
    shadow_and_edge(mask, edge=0, shadow=(12, 14)).save(os.path.join(OUT, "intro_banner_shadow.png"))
    # 縫い目（白い点線）と、左の端の巻き（色は付けない層）
    stitch = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    d = ImageDraw.Draw(stitch)
    for y in (pad + 24, pad + h - 24):
        dashed_line(d, [(pad + 60, y), (pad + w - 30, y)], (255, 255, 248, 235), width=5, dash=20, gap=12)
    curl = Image.new("L", (cw, ch), 0)
    ImageDraw.Draw(curl).rounded_rectangle([pad - 8, pad - 6, pad + 34, pad + h + 6], radius=20, fill=255)
    shade = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shade)
    for i in range(42):  # 巻いた筒の明暗（左が暗く、真ん中が明るい）
        a = int(110 * (1.0 - math.sin(math.pi * i / 41)))
        sd.line([(pad - 8 + i, pad - 6), (pad - 8 + i, pad + h + 6)], fill=(40, 30, 10, a))
    stitch.alpha_composite(Image.composite(shade, Image.new("RGBA", (cw, ch), (0, 0, 0, 0)), curl))
    d.rounded_rectangle([pad - 8, pad - 6, pad + 34, pad + h + 6], radius=20, outline=(60, 50, 30, 120), width=3)
    stitch.save(os.path.join(OUT, "intro_banner_stitch.png"))


def dashed_line(draw, pts, color, width=4, dash=14, gap=10):
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        length = math.hypot(x1 - x0, y1 - y0)
        t = 0.0
        while t < length:
            t1 = min(length, t + dash)
            draw.line([(x0 + (x1 - x0) * t / length, y0 + (y1 - y0) * t / length),
                       (x0 + (x1 - x0) * t1 / length, y0 + (y1 - y0) * t1 / length)], fill=color, width=width)
            t = t1 + gap


def make_intro_roll():
    """転がる巻いた布：模様（横に2周期ぶん。ゲームが少しずつずらして転がして見せる）と、筒の明暗の層"""
    import numpy as np
    p, h = INTRO_ROLL_PERIOD, INTRO_ROLL_HEIGHT
    felt = tiled_texture("felt_grass.png", (p * 2, h))
    gray = np.asarray(felt.convert("L")).astype(np.float32)
    gray = 0.82 + 0.18 * (gray - gray.mean()) / (gray.std() + 1e-5) * 0.5
    img = Image.fromarray(np.clip(np.stack([gray * 242, gray * 242, gray * 236], -1), 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img, "RGBA")
    for k in range(4):  # 巻いた布の重なりの線（斜め。1周期に2本）
        x = k * p / 2
        d.line([(x, 0), (x + 26, h)], fill=(90, 80, 60, 110), width=4)
        d.line([(x + 6, 0), (x + 32, h)], fill=(255, 255, 250, 90), width=2)
    img.save(os.path.join(OUT, "intro_roll.png"))
    # 筒の明暗（左右が暗く、少し左寄りが明るい）＋上の切り口（巻いた渦）
    shade = Image.new("RGBA", (p, h), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shade)
    for i in range(p):
        t = i / (p - 1)
        dark = int(150 * (abs(t - 0.4) / 0.6) ** 1.6)
        sd.line([(i, 0), (i, h)], fill=(30, 22, 8, dark))
    sd.ellipse([2, -16, p - 2, 30], fill=(235, 232, 220, 255), outline=(90, 80, 60, 255), width=3)
    for r in (18, 11, 5):
        sd.arc([p / 2 - r * 2.2, 7 - r * 0.45, p / 2 + r * 2.2, 7 + r * 0.45], 0, 360, fill=(120, 100, 70, 255), width=2)
    sd.rectangle([0, 0, p - 1, h - 1], outline=(70, 60, 40, 140), width=2)
    shade.save(os.path.join(OUT, "intro_roll_shade.png"))


def make_intro_bar():
    """画面の上下の帯：濃い茶色の段ボール。内側（画面の中側）のふちに段ボールの芯の明るい線"""
    img = tiled_texture("kraft.png", (W, 120), (0.46, 0.34, 0.24))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 104, W, 113], fill=(214, 182, 132))
    d.line([(0, 103), (W, 103)], fill=(50, 32, 16), width=3)
    d.line([(0, 114), (W, 114)], fill=(50, 32, 16), width=4)
    img.save(os.path.join(OUT, "intro_bar.png"))


def make_intro_backing():
    """台紙：段ボールの地（左右は段ボールの切れ目、下に紙を重ねた段差）。下の段に「ロードちゅう」。
    シーンが始まる時にはさみで切るので、切れ目（下の折り目の線）で上と下に分けて書き出す"""
    from PIL import ImageChops
    width = W + CARD_MARGIN * 2
    left, right = card_shape(width)
    outline = left + list(reversed(right))
    body = card_background(width)
    mask = Image.new("L", (width, H), 0)
    ImageDraw.Draw(mask).polygon(outline, fill=255)
    card = Image.new("RGBA", (width, H), (0, 0, 0, 0))
    sh = Image.new("RGBA", (width, H), (40, 25, 10, 0))
    sh.putalpha(ImageChops.offset(mask, 14, 10).filter(ImageFilter.GaussianBlur(10)).point(lambda v: v * 0.4))
    card.alpha_composite(sh)
    body.putalpha(mask)
    card.alpha_composite(body)
    d = ImageDraw.Draw(card)
    for edge in (left, right):
        d.line(edge, fill=(236, 206, 160, 255), width=10)
        d.line(edge, fill=(110, 75, 40, 255), width=3)
    # 文字は RGB のうちに描いてから、透明の形を戻す（RGBA へ描くとふちが透けることがある）
    alpha = card.getchannel("A")
    card = card.convert("RGB")
    ImageDraw.Draw(card).text((CARD_MARGIN + INTRO_LOADING_X, INTRO_LOADING_Y), "ロードちゅう",
                              font=ImageFont.truetype(FONT, 58), fill=(255, 255, 250), stroke_width=7, stroke_fill=(70, 45, 20))
    card = card.convert("RGBA")
    card.putalpha(alpha)
    # 切れ目で上と下に分ける（上は切れ目より少し下まで。分かれる前は上が下に重なって、境目が見えない）
    cut = card_cut_line(width)
    top_mask = Image.new("L", (width, H), 0)
    ImageDraw.Draw(top_mask).polygon([(-20, -20)] + [(x, y + CARD_CUT_OVERLAP) for x, y in cut] + [(width + 40, -20)], fill=255)
    bottom_mask = Image.new("L", (width, H), 0)
    ImageDraw.Draw(bottom_mask).polygon([(-20, H + 20)] + cut + [(width + 40, H + 20)], fill=255)
    top = card.copy()
    top.putalpha(ImageChops.multiply(alpha, top_mask))
    bottom = card.copy()
    bottom.putalpha(ImageChops.multiply(alpha, bottom_mask))
    top.crop((0, 0, width, CARD_TOP_HEIGHT)).save(os.path.join(OUT, "intro_backing_top.png"))
    bottom.crop((0, CARD_BOTTOM_TOP, width, H)).save(os.path.join(OUT, "intro_backing_bottom.png"))


def make_stage_intro():
    import json
    import glob
    make_intro_backing()
    make_intro_banner()
    make_intro_roll()
    make_intro_bar()
    # 星：形は1つを使い回す。色はゲームが掛けるので白い紙で作り、ふちと影は別の層（色を掛けない）
    size = 380
    mask = intro_star_mask(size, 150, random.Random(9))
    body = crumple((size, size), 4).convert("RGBA")
    body.putalpha(mask)
    body.save(os.path.join(OUT, "intro_star.png"))
    shadow_and_edge(mask).save(os.path.join(OUT, "intro_star_edge.png"))
    # キャラの切り絵（Blender で描き出した物があれば）
    for name, height in (("dino", 360), ("egg", 200), ("enemy_ground", 250), ("enemy_air", 230), ("enemy_plant", 280)):
        if os.path.exists(os.path.join(CUTOUT_DIR, name + ".png")):
            intro_cutout(name, height).save(os.path.join(OUT, "intro_char_%s.png" % name))
        else:
            print("warning: 切り絵がありません（render_cutouts.py を Blender で実行）:", name)
    # ステージ名（ステージごと）
    for path in glob.glob(os.path.join(INTRO_DIR, "*.json")):
        stem = os.path.splitext(os.path.basename(path))[0]
        with open(path, encoding="utf-8") as f:
            make_intro_name(stem, json.load(f))


def make_intro_layout():
    """カードの部品の置き場所と、切れ目・はさみの寸法を intro_layout.json に書く（ゲームはこれを読む）。
    並べた順に描く（後ろの物から）。角度は度で、正が右回り（画面の見た目）。
    上の部分の部品は、下の端が CARD_CLEAR_Y より上（切れ目のまわりは、はさみが通るので何も置かない）"""
    import json
    width = W + CARD_MARGIN * 2
    stage1 = os.path.join(INTRO_DIR, "stage1.json")  # 色の既定はステージ1と同じ（ステージごとの中身で上書きできる）
    content = json.load(open(stage1, encoding="utf-8")) if os.path.exists(stage1) else {}
    star_colors = content.get("starColors", [[1.0, 1.0, 1.0]] * 6)
    banner_color = content.get("bannerColor", [0.62, 0.84, 0.24])
    elements = []

    def element(id_, layers, x, y, scale=1.0, angle=0.0, sway=2.0, speed=1.6, phase=0.0, **extra):
        data = {"id": id_, "layers": layers, "x": round(x, 1), "y": round(y, 1), "scale": scale, "angle": angle,
                "sway": sway, "swaySpeed": round(speed, 2), "swayPhase": round(phase, 2), "part": "bottom" if y > CARD_CUT_Y else "top"}
        data.update(extra)
        elements.append(data)

    bottom_h = H - CARD_BOTTOM_TOP
    element("backing", [{"file": "intro_backing_top.png"}], 960, CARD_TOP_HEIGHT / 2, sway=0.0, part="top")
    element("backing", [{"file": "intro_backing_bottom.png"}], 960, CARD_BOTTOM_TOP + bottom_h / 2, sway=0.0, part="bottom")
    star_layers = [{"file": "intro_star_edge.png"}, {"file": "intro_star.png", "tinted": True}]
    stars = [(290, 230, 1.0, -12), (545, 115, 0.42, 10), (1490, 575, 0.42, -8),
             (1730, 600, 0.82, 14), (1240, 640, 0.82, -6), (800, 680, 0.52, 18)]
    for i, (x, y, scale, angle) in enumerate(stars):
        element("star%d" % i, star_layers, x, y, scale, angle, sway=2.0, speed=1.3 + 0.17 * i, phase=i * 1.1,
                color=star_colors[i % len(star_colors)])
    element("banner", [{"file": "intro_banner_shadow.png"}, {"file": "intro_banner.png", "tinted": True},
                       {"file": "intro_banner_stitch.png"}], 960, CARD_BANNER_Y, sway=0.5, speed=1.1, color=banner_color)
    element("name", [{"file": "intro_name_stage1.png"}], 960, CARD_BANNER_Y - 8, angle=-2.0, sway=1.0, speed=1.4, phase=0.5)
    element("egg", [{"file": "intro_char_egg.png"}], 560, 665, angle=6.0, sway=4.0, speed=2.4, phase=0.3)
    element("dino", [{"file": "intro_char_dino.png"}], 300, 580, angle=-4.0, sway=1.5, speed=1.9, phase=1.7)
    element("enemy", [{"file": "@enemy"}], 1735, 255, angle=4.0, sway=1.0, speed=2.0, phase=2.4, stamp=True)
    # 「ロードちゅう」の後ろの「…」（1つずつ増える）と、ぴょこぴょこ跳ねる小さな卵（下の段）
    dot = Image.new("RGBA", (28, 28), (0, 0, 0, 0))
    ImageDraw.Draw(dot).ellipse([3, 3, 25, 25], fill=(255, 255, 250, 255), outline=(70, 45, 20, 255), width=5)
    dot.save(os.path.join(OUT, "intro_dot.png"))
    text_end = INTRO_LOADING_X + ImageDraw.Draw(Image.new("RGB", (1, 1))).textlength("ロードちゅう", font=ImageFont.truetype(FONT, 58))
    for k in range(3):
        element("dots", [{"file": "intro_dot.png"}], text_end + 22 + k * 26, INTRO_LOADING_Y + 50, sway=0.0, blink=k)
    element("smallEgg", [{"file": "intro_char_egg.png"}], 1868, 995, scale=0.31, sway=11.0, speed=5.0, bob=10.0)

    # 切れ目のまわりにかかっていないか確かめる
    for e in elements:
        if e["part"] != "top" or e["id"] == "backing":
            continue
        for layer in e["layers"]:
            if layer["file"].startswith("@") or layer["file"].startswith("intro_name"):
                continue
            img = Image.open(os.path.join(OUT, layer["file"]))
            bottom = e["y"] + (img.getchannel("A").getbbox()[3] - img.height / 2) * e["scale"]
            if bottom > CARD_CLEAR_Y:
                print("warning: %s（%s）が切れ目のまわりにかかっています（下の端 %.0f > %d）" % (e["id"], layer["file"], bottom, CARD_CLEAR_Y))
    cut = {"y": CARD_CUT_Y, "topHeight": CARD_TOP_HEIGHT, "bottomTop": CARD_BOTTOM_TOP, "bottomHeight": bottom_h}
    cut.update(make_card_slit(width))
    layout = {"margin": CARD_MARGIN, "width": W, "height": H, "cut": cut, "scissors": make_scissors(), "elements": elements}
    with open(os.path.join(OUT, "intro_layout.json"), "w", encoding="utf-8") as f:
        f.write(json.dumps(layout, ensure_ascii=False, indent=2) + "\n")
    # 前の読み込みカードの画像は使わないので消す
    import glob
    for old in glob.glob(os.path.join(OUT, "card_*")) + [os.path.join(OUT, "intro_backing.png")]:
        if os.path.exists(old):
            os.remove(old)


make_frame()
make_curtain()
make_valance()
make_doors()
make_egg()
make_banner()
make_sounds()
make_stage_intro()
make_intro_layout()  # 部品の画像を使うので後
print("wrote", OUT)
