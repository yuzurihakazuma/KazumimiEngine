# -*- coding: utf-8 -*-
"""
タイトルの看板・メニューの的に貼る画像を作る（project/resources/title/）。

  python tools/craftkit/make_title_textures.py

  logo_kazumimi.png  仮のロゴ（KAZUMIMI）。横長の面に合わせて 1024x426
                     ※看板に貼られるのは resources/title/logo.png。本番のロゴはそちらに置いてあるので、
                       このスクリプトは logo.png を上書きしない（仮に戻したい時は logo_kazumimi.png をコピーする）
  menu_continue.png  的の面（menu_target の M_menu_face）に貼る項目名。512x512（円の中に収まる）
  menu_start.png
  menu_options.png

どれも CraftKit の紙の下地（logo_placeholder / menu_placeholder）に文字を乗せただけ。
自分で描いた画像に差し替える時は、同じファイル名・同じ縦横比で上書きすればよい。
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", "project"))
KIT_TEX = os.path.join(PROJECT, "resources", "craft", "textures")
OUT = os.path.join(PROJECT, "resources", "title")
FONT = r"C:\Windows\Fonts\UDDigiKyokashoN-B.ttc"

INK = (74, 52, 36, 255)          # こげ茶（段ボールに押したスタンプの色）
PAPER_COLORS = [                 # CraftKit の紙の色（kit_config.json の palette）
    (230, 72, 56), (246, 132, 44), (252, 214, 64), (96, 170, 50),
    (120, 180, 232), (230, 72, 56), (246, 132, 44), (96, 170, 50),
]

os.makedirs(OUT, exist_ok=True)


def draw_centered(draw, box, text, font, fill, stroke=0, stroke_fill=None):
    left, top, right, bottom = draw.textbbox((0, 0), text, font=font, stroke_width=stroke)
    x = box[0] + (box[2] - box[0] - (right - left)) / 2 - left
    y = box[1] + (box[3] - box[1] - (bottom - top)) / 2 - top
    draw.text((x, y), text, font=font, fill=fill, stroke_width=stroke, stroke_fill=stroke_fill)


def make_logo():
    """切り紙を並べたようなロゴ。1文字ずつ色と傾きを変える"""
    width, height = 1024, 426  # 看板の面（3.08m x 1.28m）と同じ縦横比
    base = Image.open(os.path.join(KIT_TEX, "paper_white.png")).convert("RGBA").resize((width, height), Image.LANCZOS)
    text = "KAZUMIMI"
    font = ImageFont.truetype(FONT, 190)
    tilts = [-7, 5, -4, 6, -6, 4, -5, 7]
    cell = (width - 80) / len(text)
    for i, ch in enumerate(text):
        tile = Image.new("RGBA", (260, 300), (0, 0, 0, 0))
        tile_draw = ImageDraw.Draw(tile)
        # 白いふち → 色紙の文字（切り紙を台紙に貼った感じ）
        draw_centered(tile_draw, (0, 0, 260, 300), ch, font, PAPER_COLORS[i], stroke=12, stroke_fill=(255, 255, 255, 255))
        draw_centered(tile_draw, (0, 0, 260, 300), ch, font, PAPER_COLORS[i], stroke=3, stroke_fill=INK)
        tile = tile.rotate(tilts[i], resample=Image.BICUBIC, expand=False)
        x = int(40 + cell * i + cell / 2 - 130)
        y = int(height / 2 - 150 + (10 if i % 2 else -6))
        base.alpha_composite(tile, (x, y))
    base.convert("RGB").save(os.path.join(OUT, "logo_kazumimi.png"))


def make_menu(file_name, text, size):
    base = Image.open(os.path.join(KIT_TEX, "menu_placeholder.png")).convert("RGBA")
    draw = ImageDraw.Draw(base)
    font = ImageFont.truetype(FONT, size)
    draw_centered(draw, (0, 0, base.width, base.height - 10), text, font, INK)
    base.convert("RGB").save(os.path.join(OUT, file_name))


make_logo()
make_menu("menu_continue.png", "つづき", 100)
make_menu("menu_start.png", "スタート", 84)
make_menu("menu_options.png", "せってい", 84)
print("wrote", OUT)
