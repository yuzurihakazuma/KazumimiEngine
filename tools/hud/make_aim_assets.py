# -*- coding: utf-8 -*-
"""
卵の照準・食べられる敵の印の素材（卵コンボの手ごたえ 企画書 3）。紙を切り抜いた形。
白地＋濃い茶色のふちで作り、色はコードで塗る（Sprite::SetColor）。

  python tools/hud/make_aim_assets.py

  project/resources/ui/aim_ring.png    照準の輪（太い紙の輪）
  project/resources/ui/aim_notch.png   照準の切れ込み（花びらの形。下＝輪の真ん中を向く。4 枚を回して並べる）
  project/resources/ui/aim_dot.png     紙の点（照準の真ん中・卵の通る道）
  project/resources/ui/tongue_mark.png ベロの印（食べられる敵の頭の上。先が二つに分かれた舌）
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "ui"))
BROWN = (70, 45, 20, 255)
WHITE = (255, 255, 255, 255)
S = 4  # 大きく描いて縮める（ふちをなめらかに）


def finish(img, size, name):
    img = img.resize((size, size), Image.LANCZOS)
    img.save(os.path.join(OUT, name))


def outlined(mask, rim):
    """mask の形：ふち rim(px) を茶色、内側を白"""
    outer = mask.filter(ImageFilter.MaxFilter(rim * 2 + 1)) if rim > 0 else mask
    img = Image.new("RGBA", mask.size, (0, 0, 0, 0))
    img.paste(BROWN, (0, 0), outer)
    img.paste(WHITE, (0, 0), mask)
    return img


def ring():
    size = 256 * S
    c = size / 2
    mask = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(mask)
    r_out, r_in = size * 0.43, size * 0.34
    d.ellipse([c - r_out, c - r_out, c + r_out, c + r_out], fill=255)
    d.ellipse([c - r_in, c - r_in, c + r_in, c + r_in], fill=0)
    finish(outlined(mask, 5 * S), 256, "aim_ring.png")


def notch():
    """花びら：上が丸く、下（真ん中を向く方）がすぼまった形"""
    size = 128 * S
    mask = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(mask)
    w, top, bottom = size * 0.30, size * 0.12, size * 0.88
    d.ellipse([size / 2 - w, top, size / 2 + w, top + w * 2], fill=255)
    d.polygon([(size / 2 - w * 0.98, top + w), (size / 2 + w * 0.98, top + w), (size / 2 + w * 0.22, bottom), (size / 2 - w * 0.22, bottom)], fill=255)
    d.ellipse([size / 2 - w * 0.24, bottom - w * 0.24, size / 2 + w * 0.24, bottom + w * 0.24], fill=255)
    finish(outlined(mask, 6 * S), 128, "aim_notch.png")


def dot():
    size = 64 * S
    mask = Image.new("L", (size, size), 0)
    r = size * 0.36
    ImageDraw.Draw(mask).ellipse([size / 2 - r, size / 2 - r, size / 2 + r, size / 2 + r], fill=255)
    finish(outlined(mask, 5 * S), 64, "aim_dot.png")


def tongue():
    """ベロ：口から出た舌。上は細く、先はふくらんで、先の真ん中が切れ込んで二つに分かれる。真ん中に一本の筋"""
    size = 128 * S
    mask = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(mask)
    c = size / 2
    d.rounded_rectangle([c - size * 0.10, size * 0.06, c + size * 0.10, size * 0.50], radius=size * 0.08, fill=255)  # 根元
    d.ellipse([c - size * 0.25, size * 0.30, c + size * 0.25, size * 0.92], fill=255)                                # 先のふくらみ
    d.polygon([(c - size * 0.09, size * 0.95), (c + size * 0.09, size * 0.95), (c, size * 0.70)], fill=0)            # 先の切れ込み
    img = outlined(mask, 6 * S)
    ImageDraw.Draw(img).line([(c, size * 0.16), (c, size * 0.68)], fill=BROWN, width=5 * S)
    finish(img, 128, "tongue_mark.png")


ring()
notch()
dot()
tongue()
print("wrote", OUT)
