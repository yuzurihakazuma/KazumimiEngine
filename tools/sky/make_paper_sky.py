"""スカイドーム用のキューブマップ：無地の紙の空（ステージの空の板と同じ「紙の空」）だけ。

  python tools/sky/make_paper_sky.py            … project/resources/sky/paper_sky.dds（6 面・512px・sRGB）
  python tools/sky/make_paper_sky.py --preview  … tools/sky/preview_sky.png（ぐるりと 1 周を横に広げた絵）も描く

雲・丘・地面は描かない。空の絵は、どこから見ても同じ大きさに見えるので、大きさの分かる物を描くと
カメラを引いた時に「巨大な絵の前の模型」に見えてしまった。景色は、段ボールの書き割りを本当の距離に立てて見せる
（tools/stage_intro/make_backdrop_ring.py）。スカイドームは、書き割りで埋めきれない真上・遠くのすき間を埋めるだけ
キューブマップの向きは D3D と同じ（左手系・y が上）
"""
import math
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TEX = os.path.join(ROOT, "project", "resources", "craft", "textures")
OUT = os.path.join(ROOT, "project", "resources", "sky", "paper_sky.dds")
SIZE = 512
DEG = math.pi / 180.0


def load(name):
    return np.asarray(Image.open(os.path.join(TEX, name + ".png")).convert("RGB")).astype(np.float64) / 255.0


SKY = load("paper_sky")


def sample(tex, u, v):
    """くり返しで引く（u・v は何周でもよい）。近い画素"""
    h, w = tex.shape[:2]
    x = np.mod(np.floor(u * w), w).astype(int)
    y = np.mod(np.floor(v * h), h).astype(int)
    return tex[y, x]


def face_directions(face, size):
    """D3D のキューブマップの面ごとに、各画素の向き（長さ 1）"""
    t = (np.arange(size) + 0.5) / size * 2.0 - 1.0
    u, v = np.meshgrid(t, t)  # u：右へ、v：下へ
    one = np.ones_like(u)
    d = {
        0: (one, -v, -u),   # +X
        1: (-one, -v, u),   # -X
        2: (u, one, v),     # +Y
        3: (u, -one, -v),   # -Y
        4: (u, -v, one),    # +Z
        5: (-u, -v, -one),  # -Z
    }[face]
    x, y, z = (np.asarray(c, dtype=np.float64) for c in d)
    n = np.sqrt(x * x + y * y + z * z)
    return x / n, y / n, z / n


def shade(x, y, z):
    az = np.arctan2(x, z)
    el = np.arcsin(np.clip(y, -1, 1))
    u_az = az / (2 * math.pi)                      # ぐるり 1 周で 0〜1

    # --- 空：紙の空をそのまま（1 周に 8 枚。上は少しだけ濃く）
    color = sample(SKY, u_az * 8.0, el / (45 * DEG) * 1.0 + 0.3)
    color = color * (1.0 - 0.12 * np.clip(el / (80 * DEG), 0, 1))[..., None]

    return np.clip(color, 0, 1)


def write_dds(path, faces):
    """6 面の RGBA8（sRGB）を、DX10 拡張ヘッダ付きのキューブマップ DDS で書く"""
    h, w = faces[0].shape[:2]
    flags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000  # CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
    pixel_format = struct.pack("<II4sIIIII", 32, 0x4, b"DX10", 0, 0, 0, 0, 0)
    header = struct.pack("<IIIIIII", 124, flags, h, w, w * 4, 0, 1) + b"\0" * 44 + pixel_format
    header += struct.pack("<IIIII", 0x1000 | 0x8, 0x200 | 0xFC00, 0, 0, 0)  # TEXTURE | COMPLEX、CUBEMAP ＋ 6 面
    dx10 = struct.pack("<IIIII", 29, 3, 0x4, 1, 0)  # R8G8B8A8_UNORM_SRGB・2D・キューブ・配列 1
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"DDS " + header + dx10)
        for face in faces:
            f.write(face.tobytes())


def main():
    faces = []
    for face in range(6):
        x, y, z = face_directions(face, SIZE)
        rgb = shade(x, y, z)
        rgba = np.concatenate([rgb, np.ones(rgb.shape[:2] + (1,))], axis=2)
        faces.append((rgba * 255 + 0.5).astype(np.uint8))
    write_dds(OUT, faces)
    print("wrote", os.path.relpath(OUT, ROOT), "%.1f MB" % (os.path.getsize(OUT) / 1e6))
    if "--preview" in sys.argv:
        W, H = 2000, 700
        az = (np.arange(W) + 0.5) / W * 2 * math.pi - math.pi
        el = (60 - (np.arange(H) + 0.5) / H * 90) * DEG   # 上 60° 〜 下 -30°
        A, E = np.meshgrid(az, el)
        x, y, z = np.cos(E) * np.sin(A), np.sin(E), np.cos(E) * np.cos(A)
        Image.fromarray((shade(x, y, z) * 255).astype(np.uint8)).save(os.path.join(HERE, "preview_sky.png"))
        print("wrote tools/sky/preview_sky.png")


if __name__ == "__main__":
    main()
