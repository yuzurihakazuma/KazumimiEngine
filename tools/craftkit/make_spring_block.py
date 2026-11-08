"""ジャンプ台ブロック（種類 4）の見た目：工作のばね。

  python tools/craftkit/make_spring_block.py

  出力：project/resources/block/craft/spring_1x1x1.obj（ブロックのアトラス block_atlas.png を使う）
  ・原点＝底面の真ん中、1m 角・高さ 1m（ほかのブロックと同じ。当たりは今までどおり 1m の箱）
  ・段ボールの土台 → 白い紙を巻いたコイル（4巻き）→ 上に緑のフェルトの板
  ・ゲーム（BlockRenderer）が、跳ねた時に縦に縮めて「びよん」と戻す
"""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "block", "craft", "spring_1x1x1.obj"))

# アトラスの区画（u0, v0, u1, v1）。v は画像の下が 0
UV_BROWN = (0.05, 0.235, 0.95, 0.295)      # 無地の段ボール
UV_CORRUGATED = (0.02, 0.33, 0.98, 0.50)   # 波の段ボール（側面）
UV_WHITE = (0.385, 0.02, 0.51, 0.08)       # 白い紙
UV_GREEN = (0.825, 0.02, 0.875, 0.08)      # 緑のフェルト
UV_YELLOW = (0.05, 0.83, 0.95, 0.97)       # 黄色（板のふち）

verts, uvs, norms, faces = [], [], [], []


def add_vertex(p, uv, n):
    verts.append(p); uvs.append(uv); norms.append(n)
    return len(verts)


def quad(a, b, c, d):
    faces.append((a, b, c)); faces.append((a, c, d))


def box(x0, y0, z0, x1, y1, z1, side_uv, top_uv, bottom_uv=None):
    """箱（面ごとに頂点を分ける＝角がくっきり）。反時計回りが表"""
    bottom_uv = bottom_uv or side_uv
    def face(corners, n, uv):
        u0, v0, u1, v1 = uv
        ids = [add_vertex(c, t, n) for c, t in zip(corners, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)])]
        quad(*ids)
    face([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0, 1, 0), top_uv)      # 上
    face([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0), bottom_uv)  # 下
    face([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1), side_uv)     # 手前 +z
    face([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1), side_uv)    # 奥 -z
    face([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1, 0, 0), side_uv)     # 右 +x
    face([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0), side_uv)    # 左 -x


def coil(y0, y1, turns=4.0, radius=0.30, tube=0.055, steps=160, ring=10):
    """らせんの管（紙を巻いた針金）。始まりと終わりは平らに寝かせて土台・板に付ける"""
    u0, v0, u1, v1 = UV_WHITE
    rows = []
    for k in range(steps + 1):
        t = k / steps
        a = t * turns * 2 * math.pi
        # 両端の 1/8 巻きは高さを止める（ばねの端の座）
        flat = 1.0 / (turns * 8)
        h = min(max((t - flat) / (1 - 2 * flat), 0.0), 1.0)
        center = (math.cos(a) * radius, y0 + (y1 - y0) * h, math.sin(a) * radius)
        tangent = (-math.sin(a) * radius * turns * 2 * math.pi, (y1 - y0), math.cos(a) * radius * turns * 2 * math.pi)
        tl = math.sqrt(sum(c * c for c in tangent)); tangent = tuple(c / tl for c in tangent)
        outward = (math.cos(a), 0.0, math.sin(a))
        # 管の断面の2軸：外向きと、接線×外向き
        binormal = (tangent[1] * outward[2] - tangent[2] * outward[1],
                    tangent[2] * outward[0] - tangent[0] * outward[2],
                    tangent[0] * outward[1] - tangent[1] * outward[0])
        row = []
        for r in range(ring):
            b = r / ring * 2 * math.pi
            n = tuple(outward[j] * math.cos(b) + binormal[j] * math.sin(b) for j in range(3))
            p = tuple(center[j] + n[j] * tube for j in range(3))
            row.append(add_vertex(p, (u0 + (u1 - u0) * r / ring, v0 + (v1 - v0) * (k % 2)), n))
        rows.append(row)
    for k in range(steps):
        for r in range(ring):
            a, b = rows[k][r], rows[k][(r + 1) % ring]
            c, d = rows[k + 1][(r + 1) % ring], rows[k + 1][r]
            quad(a, d, c, b)


# 土台（段ボールの板）
box(-0.48, 0.0, -0.48, 0.48, 0.16, 0.48, UV_CORRUGATED, UV_BROWN)
# コイル
coil(0.16 + 0.055, 0.80 - 0.055)
# 上の板：黄色いふちの段ボールに、緑のフェルトを貼る
box(-0.47, 0.80, -0.47, 0.47, 0.93, 0.47, UV_YELLOW, UV_YELLOW)
box(-0.44, 0.93, -0.44, 0.44, 1.0, 0.44, UV_GREEN, UV_GREEN)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("# spring block (tools/craftkit/make_spring_block.py)\nmtllib material.mtl\no spring_1x1x1\n")
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


# --- ばねの音（びよーん）：project/resources/se/se_spring.wav ---
def make_spring_sound():
    import struct
    import wave
    rate = 44100
    length = 0.55
    out = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "se", "se_spring.wav"))
    samples = []
    phase = 0.0
    for i in range(int(rate * length)):
        t = i / rate
        # 低い所から上へ「びよーん」と上がり、ゆれながら消える
        freq = 180.0 + 420.0 * (1.0 - math.exp(-t * 7.0)) + 35.0 * math.sin(t * 2 * math.pi * 14.0) * math.exp(-t * 3.0)
        phase += 2 * math.pi * freq / rate
        env = min(t / 0.008, 1.0) * math.exp(-t * 5.5)
        value = (math.sin(phase) + 0.35 * math.sin(phase * 2.0) + 0.12 * math.sin(phase * 3.0)) * env * 0.42
        samples.append(int(max(-1.0, min(1.0, value)) * 32767))
    with wave.open(out, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", s) for s in samples))
    print("wrote", out)


make_spring_sound()
