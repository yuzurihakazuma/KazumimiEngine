"""railsim mesh の OBJ を、決めたカメラから絵にする（つなぎ目の形を、ゲームを起動せずに見るため）。

  python render.py in.obj out.png  ex ey ez  tx ty tz  [fov=45] [radius=8] [w=960] [h=600]
     e＝カメラの位置 / t＝見る点 / radius＝見る点からこの距離より遠い三角形は描かない
  色：土台＝段ボール色 / 継ぎ板(patch)＝緑 / つなぎ(conn)＝水色 / 広場(plaza)＝紫 / 布＝レールごとに違うオレンジ系
      裏から見えている面は暗い青（面が裏返っている・中が見えている）
"""
import sys
import numpy as np
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
eye = np.array([float(v) for v in sys.argv[3:6]])
target = np.array([float(v) for v in sys.argv[6:9]])
fov = float(sys.argv[9]) if len(sys.argv) > 9 else 45.0
radius = float(sys.argv[10]) if len(sys.argv) > 10 else 8.0
W = int(sys.argv[11]) if len(sys.argv) > 11 else 960
H = int(sys.argv[12]) if len(sys.argv) > 12 else 600

CLOTH = [(0.95, 0.55, 0.28), (0.95, 0.35, 0.30), (0.98, 0.75, 0.25), (0.80, 0.45, 0.65)]


def color_of(name):
    kind = name.split("_")[0].rstrip("0123456789")
    rail = int(name.split("_r")[-1]) if "_r" in name else 0
    if kind == "cloth": return CLOTH[rail % len(CLOTH)]
    if kind == "base": return (0.72, 0.60, 0.42)
    if kind == "patch": return (0.45, 0.80, 0.40)
    if kind == "conn": return (0.40, 0.75, 0.90)
    if kind == "plaza": return (0.65, 0.50, 0.85)
    return (0.8, 0.8, 0.8)


verts, tris, tri_color = [], [], []
current = (0.8, 0.8, 0.8)
for line in open(src, encoding="utf-8"):
    p = line.split()
    if not p: continue
    if p[0] == "o": current = color_of(p[1])
    elif p[0] == "v": verts.append([float(p[1]), float(p[2]), float(p[3])])
    elif p[0] == "f":
        tris.append([int(q.split("/")[0]) - 1 for q in p[1:4]])
        tri_color.append(current)
V = np.array(verts); T = np.array(tris); C = np.array(tri_color)
centers = V[T].mean(1)
keep = np.linalg.norm(centers - target, axis=1) < radius
T, C = T[keep], C[keep]

# カメラ（左手系：+Z が奥。ゲームと同じ向きに見える）
forward = target - eye; forward /= np.linalg.norm(forward)
right = np.cross([0.0, 1.0, 0.0], forward); right /= np.linalg.norm(right)
up = np.cross(forward, right)
rel = V - eye
cam = np.stack([rel @ right, rel @ up, rel @ forward], axis=1)
f = 1.0 / np.tan(np.radians(fov) / 2)
aspect = W / H
depth = np.full((H, W), np.inf)
img = np.zeros((H, W, 3)); img[:] = (0.55, 0.75, 0.55)
light = np.array([0.35, 0.85, -0.4]); light /= np.linalg.norm(light)
for tri, col in zip(T, C):
    p = cam[tri]
    if (p[:, 2] < 0.05).any(): continue
    sx = (p[:, 0] / p[:, 2] * f / aspect * 0.5 + 0.5) * W
    sy = (0.5 - p[:, 1] / p[:, 2] * f * 0.5) * H
    x0, x1 = int(max(np.floor(sx.min()), 0)), int(min(np.ceil(sx.max()), W - 1))
    y0, y1 = int(max(np.floor(sy.min()), 0)), int(min(np.ceil(sy.max()), H - 1))
    if x0 > x1 or y0 > y1: continue
    world = V[tri]
    normal = np.cross(world[1] - world[0], world[2] - world[0])
    length = np.linalg.norm(normal)
    if length < 1e-12: continue
    normal /= length
    # 表か裏か：このエンジンは時計回りが表（左手系）。カメラから見て法線が手前を向いていれば表
    facing = np.dot(normal, eye - world.mean(0)) > 0
    shade = 0.45 + 0.55 * abs(np.dot(normal, light))
    rgb = np.array(col) * shade if facing else np.array((0.10, 0.12, 0.35)) * (0.6 + 0.4 * shade)
    xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    d = (sy[1] - sy[2]) * (sx[0] - sx[2]) + (sx[2] - sx[1]) * (sy[0] - sy[2])
    if abs(d) < 1e-9: continue
    a = ((sy[1] - sy[2]) * (xs - sx[2]) + (sx[2] - sx[1]) * (ys - sy[2])) / d
    b = ((sy[2] - sy[0]) * (xs - sx[2]) + (sx[0] - sx[2]) * (ys - sy[2])) / d
    c = 1 - a - b
    inside = (a >= 0) & (b >= 0) & (c >= 0)
    if not inside.any(): continue
    z = 1.0 / (a / p[0, 2] + b / p[1, 2] + c / p[2, 2])
    region = depth[y0:y1 + 1, x0:x1 + 1]
    hit = inside & (z < region)
    region[hit] = z[hit]
    img[y0:y1 + 1, x0:x1 + 1][hit] = rgb
Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8)).save(dst)
print("wrote", dst, len(T), "triangles")
