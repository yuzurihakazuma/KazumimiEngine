"""マップ JSON のレールを、railsim が読む簡単なテキストにする。

  python tools/railsim/export_rails.py [マップ.json] [出力.txt]
  既定：project/resources/map/stage1.json → tools/railsim/rails.txt
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "project", "resources", "map", "stage1.json")
dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "rails.txt")
d = json.load(open(src, encoding="utf-8"))


def at(key, i, fallback):
    values = d.get(key) or []
    return values[i] if i < len(values) else fallback


lines = d["railLines"]
with open(dst, "w", encoding="utf-8", newline="\n") as f:
    f.write("%d\n" % len(lines))
    for i, nodes in enumerate(lines):
        skin = at("railSkins", i, {}) or {}
        motion = at("railMotions", i, [0, 0, 0, 2])
        holes = at("railNodeHoles", i, [])
        f.write("rail %d %d %d %d %d %d %d\n" % (len(nodes), at("railLineModes", i, 0), at("railTypes", i, -1), at("railVisible", i, 1),
                                                at("railRoadModes", i, 0), at("railEndPlazas", i, 0), at("railGroundTypes", i, 0)))
        f.write("skin %d %.6f %.6f %.6f %d\n" % (skin.get("style", 0), skin.get("width", 1.8), skin.get("lift", 0.012), skin.get("curl", 0.035),
                                              skin.get("layer", 0)))
        f.write("motion %.6f %.6f %.6f %d %d\n" % (motion[0], motion[1], motion[2], at("railMotionTypes", i, 0), at("railGuideRails", i, -1)))
        for n in nodes:
            f.write("%.9g %.9g %.9g\n" % (n[0], n[1], n[2]))
        f.write("holes %d %s\n" % (len(holes), " ".join(str(int(h)) for h in holes)))
print("wrote", os.path.normpath(dst), len(lines), "rails")
