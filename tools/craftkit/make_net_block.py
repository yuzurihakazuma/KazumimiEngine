"""すり抜け床（種類 6）の見た目：あみあみの床（黄色い段ボールの枠に、白い紙の帯を縦横に編んだ網）。

  python tools/craftkit/make_net_block.py

  出力：project/resources/block/craft/net_1x1x1.obj（ブロックのアトラス block_atlas.png を使う）
  ・原点＝マスの底面の真ん中。網はマスの上の端（高さ 0.86〜1.0m）にだけある（下は空いている＝下と横から通り抜けられるのが見て分かる）
  ・当たりは今までどおり：上にだけ乗れる（上面 1.0m）。卵が当たる板は高さ 0.7〜1.0m
  ・網の目は向こうが見える穴（帯 5 本ずつ・穴は約 9cm 角）。縦の帯と横の帯は、交わる所ごとに上下を入れ替えて編んである
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "block", "craft", "net_1x1x1.obj"))

# アトラスの区画（u0, v0, u1, v1）。v は画像の下が 0
UV_YELLOW = (0.05, 0.83, 0.95, 0.97)       # 黄色（枠の上）
UV_CORRUGATED = (0.02, 0.33, 0.98, 0.50)   # 波の段ボール（枠の横）
UV_WHITE = (0.385, 0.02, 0.51, 0.08)       # 白い紙（帯）
UV_CREAM = (0.56, 0.02, 0.66, 0.08)        # クリーム色の紙（帯）

TOP = 1.0          # マスの上の端（乗る面）
FRAME_W = 0.07     # 枠の太さ
FRAME_H = 0.14     # 枠の高さ
STRIPS = 5         # 帯の本数（縦・横）
STRIP_W = 0.075    # 帯の幅
STRIP_T = 0.022    # 帯の厚さ
WEAVE = 0.022      # 編み目の上下（帯の厚さぶん）

verts, uvs, norms, faces = [], [], [], []


def add_vertex(p, uv, n):
    verts.append(p); uvs.append(uv); norms.append(n)
    return len(verts)


def box(x0, y0, z0, x1, y1, z1, side_uv, top_uv):
    """箱（面ごとに頂点を分ける＝角がくっきり）。反時計回りが表"""
    def face(corners, n, uv):
        u0, v0, u1, v1 = uv
        a, b, c, d = [add_vertex(p, t, n) for p, t in zip(corners, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)])]
        faces.append((a, b, c)); faces.append((a, c, d))
    face([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0, 1, 0), top_uv)     # 上
    face([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0), top_uv)    # 下（下から見上げても帯の色）
    face([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1), side_uv)    # 手前 +z
    face([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1), side_uv)   # 奥 -z
    face([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1, 0, 0), side_uv)    # 右 +x
    face([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0), side_uv)   # 左 -x


# 枠（4 本。上は黄色＝乗れる面の色、横は波の段ボール）
y0 = TOP - FRAME_H
box(-0.5, y0, 0.5 - FRAME_W, 0.5, TOP, 0.5, UV_CORRUGATED, UV_YELLOW)       # 手前
box(-0.5, y0, -0.5, 0.5, TOP, -0.5 + FRAME_W, UV_CORRUGATED, UV_YELLOW)     # 奥
box(-0.5, y0, -0.5 + FRAME_W, -0.5 + FRAME_W, TOP, 0.5 - FRAME_W, UV_CORRUGATED, UV_YELLOW)  # 左
box(0.5 - FRAME_W, y0, -0.5 + FRAME_W, 0.5, TOP, 0.5 - FRAME_W, UV_CORRUGATED, UV_YELLOW)    # 右

# 網：帯を縦横に 5 本ずつ。交わる所ごとに「上を通る・下を通る」を入れ替える（1 本の帯を、交わる所で区切った短い箱で作る）
inner = 0.5 - FRAME_W
pitch = 2 * inner / STRIPS
centers = [-inner + pitch * (i + 0.5) for i in range(STRIPS)]
mid = TOP - 0.03 - WEAVE                      # 網の真ん中の高さ（上を通る帯の上面が、乗る面の少し下）
edges = [-inner] + [(centers[i] + centers[i + 1]) / 2 for i in range(STRIPS - 1)] + [inner]


def strip_height(over):
    base = mid + (WEAVE / 2 if over else -WEAVE / 2)
    return base - STRIP_T / 2, base + STRIP_T / 2


for i, c in enumerate(centers):       # 横の帯（x の向きに延びる。z = c）：白
    for j in range(STRIPS):
        lo, hi = strip_height((i + j) % 2 == 0)
        box(edges[j], lo, c - STRIP_W / 2, edges[j + 1], hi, c + STRIP_W / 2, UV_WHITE, UV_WHITE)
for j, c in enumerate(centers):       # 縦の帯（z の向きに延びる。x = c）：クリーム色。横の帯と上下が逆
    for i in range(STRIPS):
        lo, hi = strip_height((i + j) % 2 == 1)
        box(c - STRIP_W / 2, lo, edges[i], c + STRIP_W / 2, hi, edges[i + 1], UV_CREAM, UV_CREAM)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("# net block (tools/craftkit/make_net_block.py)\nmtllib material.mtl\no net_1x1x1\n")
    for p in verts:
        f.write("v %.5f %.5f %.5f\n" % p)
    for t in uvs:
        f.write("vt %.5f %.5f\n" % t)
    for n in norms:
        f.write("vn %.5f %.5f %.5f\n" % n)
    f.write("usemtl block_craft\n")
    for a, b, c in faces:
        f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c))
print("wrote", OUT, len(verts), "verts", len(faces), "tris")
