"""カメラ反転（裏から見ると夕方）の素材を作る。

出力: project/resources/flip/sky_evening.png
  夕焼けの紙の空（上は藍色 → 橙 → 地平線の近くは明るい黄色）。紙の繊維のざらつきと、ぼんやりした夕日。
  カメラのまわりを囲む筒（ふた無しの円柱）の内側に貼る（game/flip/WorldFlip）。横の真ん中が裏のカメラの正面。
"""
import os
import random

from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'project', 'resources', 'flip'))
SIZE = 512
SKY_ASPECT = 2.0 * 3.14159 * 170.0 / 240.0  # 空の筒の 周りの長さ÷高さ（WorldFlip::ApplyEvening の大きさと合わせる）

# 上から下への色（位置 0〜1, sRGB）
STOPS = [
    (0.00, (62, 52, 128)),
    (0.30, (146, 82, 138)),
    (0.55, (236, 120, 86)),
    (0.75, (252, 176, 102)),
    (1.00, (255, 214, 148)),
]


def gradient(t):
    for (t0, c0), (t1, c1) in zip(STOPS, STOPS[1:]):
        if t <= t1:
            k = (t - t0) / (t1 - t0)
            return tuple(round(a + (b - a) * k) for a, b in zip(c0, c1))
    return STOPS[-1][1]


def main():
    random.seed(7)
    img = Image.new('RGB', (SIZE, SIZE))
    px = img.load()
    # 夕日は横の真ん中：空はカメラのまわりを囲む筒に貼り、横の真ん中が裏のカメラの正面（-Z）に来る
    sun_x, sun_y, sun_r = 0.5 * SIZE, 0.70 * SIZE, 0.05 * SIZE
    for y in range(SIZE):
        base = gradient(y / (SIZE - 1))
        for x in range(SIZE):
            r, g, b = base
            # ぼんやりした夕日（まわりも少し明るい）。筒は横に長い（周り 2π×170m ÷ 高さ 240m ≒ 4.45 倍）ので、
            #   画像の横は縮めて描く（そのままだと横長の楕円に伸びて見える）
            d = (((x - sun_x) * SKY_ASPECT) ** 2 + (y - sun_y) ** 2) ** 0.5
            glow = max(0.0, 1.0 - d / (sun_r * 4.0)) ** 2 * 0.35
            if d < sun_r:
                glow = 1.0
            r = r + (255 - r) * glow
            g = g + (236 - g) * glow
            b = b + (170 - b) * glow
            # 紙のざらつき
            n = random.uniform(-7, 7)
            px[x, y] = (max(0, min(255, int(r + n))), max(0, min(255, int(g + n))), max(0, min(255, int(b + n))))
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, 'sky_evening.png')
    img.save(path)
    print('saved', path)


if __name__ == '__main__':
    main()
