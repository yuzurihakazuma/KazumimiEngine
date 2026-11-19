"""UML クラス図（設計の一部）を PNG に描く。
  python tools/review/make_class_diagram.py   →  docs/class_diagram.png
載せている範囲：エンジンとゲームのつなぎ（Framework / シーン）と、プレイ中の敵・ゲームオーバー演出。
記号：白い三角＝継承、黒いひし形＝コンポジション（寿命を一緒に管理する）、白いひし形＝集約（借りているだけ）、点線の矢印＝依存。
同じ内容の Mermaid 版は docs/class_diagram.md（図を直す時は両方を直す）。
"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'class_diagram.png')
W, H = 2760, 2420
INK = (30, 30, 36)
ENGINE_FILL = (232, 240, 252)
GAME_FILL = (255, 247, 226)
PAPER = (255, 255, 255)
FONT_PATH = 'C:/Windows/Fonts/YuGothM.ttc'
BOLD_PATH = 'C:/Windows/Fonts/YuGothB.ttc'
font = ImageFont.truetype(FONT_PATH, 22)
small = ImageFont.truetype(FONT_PATH, 19)
bold = ImageFont.truetype(BOLD_PATH, 24)
title_font = ImageFont.truetype(BOLD_PATH, 34)
LINE = 30      # 1 行の高さ
PAD = 10

img = Image.new('RGB', (W, H), PAPER)
d = ImageDraw.Draw(img)
boxes = {}


def box(key, x, y, w, name, ops, stereotype=None, engine=False):
    """クラスの箱（名前の段＋操作の段）。boxes[key] に (左, 上, 右, 下) を残す"""
    head = (2 if stereotype else 1) * LINE + PAD * 2
    body = max(1, len(ops)) * LINE + PAD * 2
    fill = ENGINE_FILL if engine else GAME_FILL
    d.rectangle([x, y, x + w, y + head + body], fill=fill, outline=INK, width=3)
    d.line([x, y + head, x + w, y + head], fill=INK, width=3)
    ty = y + PAD
    if stereotype:
        tw = d.textlength(stereotype, font=small)
        d.text((x + (w - tw) / 2, ty + 2), stereotype, font=small, fill=INK)
        ty += LINE
    tw = d.textlength(name, font=bold)
    d.text((x + (w - tw) / 2, ty), name, font=bold, fill=INK)
    for i, op in enumerate(ops):
        d.text((x + PAD + 4, y + head + PAD + i * LINE), op, font=font, fill=INK)
    boxes[key] = (x, y, x + w, y + head + body)


def poly(points, dashed=False):
    for a, b in zip(points, points[1:]):
        if not dashed:
            d.line([a, b], fill=INK, width=3)
            continue
        (x0, y0), (x1, y1) = a, b
        length = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
        steps = int(length // 16)
        for i in range(steps + 1):
            t0 = i * 16 / length
            t1 = min(1.0, (i * 16 + 9) / length)
            d.line([(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0), (x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1)], fill=INK, width=3)


def unit(a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = (dx * dx + dy * dy) ** 0.5
    return dx / n, dy / n


def triangle(tip, toward):
    """継承の白い三角：tip＝親の箱に付く頂点、toward＝線が来る側の点。返り値＝三角の底の中点（線はここで止める）"""
    ux, uy = unit(tip, toward)
    size, half = 26, 15
    bx, by = tip[0] + ux * size, tip[1] + uy * size
    d.polygon([tip, (bx - uy * half, by + ux * half), (bx + uy * half, by - ux * half)], fill=PAPER, outline=INK, width=3)
    return (bx, by)


def diamond(tip, toward, filled):
    """ひし形（持ち主の箱に付く）。返り値＝ひし形の反対側の頂点"""
    ux, uy = unit(tip, toward)
    length, half = 34, 11
    mx, my = tip[0] + ux * length / 2, tip[1] + uy * length / 2
    end = (tip[0] + ux * length, tip[1] + uy * length)
    d.polygon([tip, (mx - uy * half, my + ux * half), end, (mx + uy * half, my - ux * half)], fill=INK if filled else PAPER, outline=INK, width=3)
    return end


def arrow(tip, toward):
    """開いた矢印（関連・依存の向き）"""
    ux, uy = unit(tip, toward)
    size, half = 20, 9
    bx, by = tip[0] + ux * size, tip[1] + uy * size
    d.line([tip, (bx - uy * half, by + ux * half)], fill=INK, width=3)
    d.line([tip, (bx + uy * half, by - ux * half)], fill=INK, width=3)


def label(pos, text):
    tw = d.textlength(text, font=small)
    d.rectangle([pos[0] - 3, pos[1], pos[0] + tw + 3, pos[1] + 24], fill=PAPER)
    d.text(pos, text, font=small, fill=INK)


def mid_x(key):
    b = boxes[key]
    return (b[0] + b[2]) / 2


def inherit(children, parent, bus_y):
    """子の箱（複数）から、横の線でまとめて親の下へ"""
    px = mid_x(parent)
    tip = (px, boxes[parent][3])
    if len(children) == 1 and abs(mid_x(children[0]) - px) < 1:
        base = triangle(tip, (px, boxes[children[0]][1]))
        poly([base, (px, boxes[children[0]][1])])
        return
    base = triangle(tip, (px, bus_y))
    poly([base, (px, bus_y)])
    xs = [mid_x(c) for c in children] + [px]
    poly([(min(xs), bus_y), (max(xs), bus_y)])
    for c in children:
        poly([(mid_x(c), bus_y), (mid_x(c), boxes[c][1])])


def own(points, filled, mult=None, mult_pos=None, name=None, name_pos=None):
    """持ち主（points[0]）→ 持たれる側（points[-1]）。持ち主側にひし形、相手側に矢印"""
    start = diamond(points[0], points[1], filled)
    poly([start] + points[1:])
    arrow(points[-1], points[-2])
    if mult:
        label(mult_pos, mult)
    if name:
        label(name_pos, name)


# ---- 枠：エンジン層（namespace）----
d.text((60, 22), 'たまごザウルスの大冒険　クラス図（設計の一部：シーンのつなぎ・敵のふるまい・ゲームオーバーの状態）', font=title_font, fill=INK)
d.rectangle([40, 86, W - 40, 470], outline=(110, 130, 170), width=3)
d.rectangle([40, 86, 640, 124], fill=(110, 130, 170))
d.text((52, 92), 'namespace KazumimiEngine（エンジン層：project/engine）', font=small, fill=PAPER)
d.text((760, 486), 'ゲーム層（project/game）：エンジンを使う側。エンジンからゲームへの #include は 0', font=small, fill=(120, 90, 20))

# ---- 箱 ----
box('Framework', 80, 190, 330, 'Framework', ['+ Run()', '+ Initialize()', '+ Update()', '+ DrawScene() {abstract}'], '«abstract»', engine=True)
box('ASF', 500, 190, 400, 'AbstractSceneFactory', ['+ CreateScene(name) {abstract}'], '«interface»', engine=True)
box('SceneManager', 1010, 190, 420, 'SceneManager', ['+ ChangeScene(next)', '+ SetSceneFactory(factory)', '+ SetTransition(transition)', '+ Update() / Draw()'], '«Singleton»', engine=True)
box('IScene', 1620, 190, 380, 'IScene', ['+ Initialize() {abstract}', '+ Update() {abstract}', '+ Draw() {abstract}', '+ Finalize() {abstract}'], '«interface»', engine=True)
box('IST', 2080, 190, 440, 'ISceneTransition', ['+ Update(dt) {abstract}', '+ IsActive() {abstract}', '+ DrawInto(target) {abstract}'], '«interface»', engine=True)

box('Game', 80, 560, 330, 'Game', ['+ Initialize()', '+ Update()', '+ DrawScene()'])
box('SceneFactory', 500, 560, 400, 'SceneFactory', ['+ CreateScene(name)'])
box('BaseScene', 1620, 560, 380, 'BaseScene', ['+ Initialize() / Update() / Draw()', '# OnInitialize()', '# OnUpdate()', '# OnDrawOpaque() ほか'], '«abstract»')
box('Kamishibai', 2080, 560, 440, 'KamishibaiTransition', ['+ Play(timeline)', '+ Update(dt)', '+ DrawInto(target)'], '«Singleton»')

box('TitleScene', 1200, 930, 320, 'TitleScene', ['# OnInitialize()', '# OnUpdate()'])
box('StageSelect', 1620, 930, 380, 'StageSelectScene', ['# OnInitialize()', '# OnUpdate()'])
box('GamePlay', 2040, 930, 400, 'GamePlayScene', ['# OnInitialize()', '# OnUpdate()'])

box('EnemyManager', 1560, 1250, 380, 'EnemyManager', ['+ Spawn(spawns, rails)', '+ Update(rails, playerPos, dt)'])
box('GameOverFlow', 2040, 1250, 400, 'GameOverFlow', ['+ UpdateBeforeGame()', '+ UpdateAfterCombat()'])

box('Enemy', 1560, 1560, 380, 'Enemy', ['+ Initialize(spawn)', '+ Update()', '+ Draw()'])
box('GameOverSeq', 2040, 1560, 400, 'GameOverSequence', ['+ Start()', '+ Update()', '- Enter(phase)'])
box('EnemyBehavior', 900, 1560, 460, 'EnemyBehavior', ['+ Create(type) {static}', '+ StartClip(patrols) {abstract}', '+ Update(model, frame) {abstract}', '+ Reset()'], '«abstract»')

box('Walker', 620, 1900, 300, 'WalkerBehavior', ['+ Update(model, frame)'])
box('Biter', 980, 1900, 300, 'BiterBehavior', ['+ Update(model, frame)', '+ Reset()'])
box('Flyer', 1340, 1900, 300, 'FlyerBehavior', ['+ Update(model, frame)'])
box('PhaseState', 2040, 1900, 400, 'PhaseState', ['+ Length(sequence)', '+ Enter(sequence)', '+ Update(sequence, t, dt)'], '«abstract»')

box('Freeze', 1500, 2230, 250, 'FreezeState', ['+ Update(...)'])
box('Crumple', 1790, 2230, 250, 'CrumpleState', ['+ Update(...)'])
box('Choose', 2080, 2230, 250, 'ChooseState', ['+ Update(...)'])
box('Leave', 2370, 2230, 250, 'LeaveState', ['+ Update(...)'])

# ---- 継承 ----
inherit(['Game'], 'Framework', 0)
inherit(['SceneFactory'], 'ASF', 0)
inherit(['BaseScene'], 'IScene', 0)
inherit(['Kamishibai'], 'IST', 0)
inherit(['TitleScene', 'StageSelect', 'GamePlay'], 'BaseScene', 880)
inherit(['Walker', 'Biter', 'Flyer'], 'EnemyBehavior', 1850)
inherit(['Freeze', 'Crumple', 'Choose', 'Leave'], 'PhaseState', 2170)

# ---- コンポジション（黒いひし形）：unique_ptr・値のメンバ ----
b = boxes
own([(b['SceneManager'][2], 290), (b['IScene'][0], 290)], True, '0..1', (b['IScene'][0] - 62, 256), 'currentScene_', (b['SceneManager'][2] + 40, 300))
own([(b['Game'][2], 620), (b['SceneFactory'][0], 620)], True, '1', (b['SceneFactory'][0] - 30, 586))
own([(2260, b['GamePlay'][3]), (2260, b['GameOverFlow'][1])], True, '1', (2274, b['GameOverFlow'][1] - 34))
own([(2110, b['GamePlay'][3]), (2110, 1196), (mid_x('EnemyManager'), 1196), (mid_x('EnemyManager'), b['EnemyManager'][1])], True, '1', (mid_x('EnemyManager') + 14, b['EnemyManager'][1] - 34))
own([(mid_x('EnemyManager'), b['EnemyManager'][3]), (mid_x('EnemyManager'), b['Enemy'][1])], True, '0..*', (mid_x('EnemyManager') + 14, b['Enemy'][1] - 34), 'enemies_', (mid_x('EnemyManager') + 14, b['EnemyManager'][3] + 40))
own([(mid_x('GameOverFlow'), b['GameOverFlow'][3]), (mid_x('GameOverFlow'), b['GameOverSeq'][1])], True, '1', (mid_x('GameOverFlow') + 14, b['GameOverSeq'][1] - 34), 'sequence_', (mid_x('GameOverFlow') + 14, b['GameOverFlow'][3] + 40))
own([(b['Enemy'][0], 1640), (b['EnemyBehavior'][2], 1640)], True, '1', (b['EnemyBehavior'][2] + 12, 1606), 'behavior_', (b['Enemy'][0] - 150, 1648))
own([(mid_x('GameOverSeq'), b['GameOverSeq'][3]), (mid_x('GameOverSeq'), b['PhaseState'][1])], True, '12', (mid_x('GameOverSeq') + 14, b['PhaseState'][1] - 34), 'states_', (mid_x('GameOverSeq') + 14, b['GameOverSeq'][3] + 40))

# ---- 集約（白いひし形）：ポインタで借りているだけ（持ち主は別）----
own([(b['SceneManager'][0], 290), (b['ASF'][2], 290)], False, '0..1', (b['ASF'][2] + 8, 256))
label((b['ASF'][2] - 30, 342), 'sceneFactory_')
own([(mid_x('SceneManager') + 120, b['SceneManager'][1]), (mid_x('SceneManager') + 120, 146), (mid_x('IST'), 146), (mid_x('IST'), b['IST'][1])], False, '0..1', (mid_x('IST') + 12, 152), 'transition_', (mid_x('SceneManager') + 190, 134))

# ---- 依存（点線の矢印）：SceneFactory がシーンを作る ----
poly([(b['SceneFactory'][2], 660), (b['BaseScene'][0], 660)], dashed=True)
arrow((b['BaseScene'][0], 660), (b['SceneFactory'][2], 660))
label((1010, 626), '«create»　名前から各シーン（BaseScene の派生）を作る')

# ---- 注釈 ----
d.rectangle([900, 2230, 1440, 2380], fill=(250, 250, 250), outline=INK, width=2)
for i, t in enumerate(['PhaseState の派生は 12 個（図は 4 個だけ）', 'ほか：Reveal / Capture / Open / Screen /', 'ResumeShot / Unscreen / Fold / Rebuild', '次の状態は、各状態が自分で決める（State）']):
    d.text((914, 2242 + i * 33), t, font=small, fill=INK)
d.rectangle([60, 1900, 560, 2014], fill=(250, 250, 250), outline=INK, width=2)
for i, t in enumerate(['EnemyBehavior＝Strategy。敵の種類ごとの', '動きを派生に分け、Enemy は種類で分岐', 'しない（派生を選ぶのは Create だけ）']):
    d.text((74, 1910 + i * 33), t, font=small, fill=INK)

# ---- 凡例 ----
lx, ly = 80, 940
d.rectangle([lx - 20, ly - 20, lx + 900, ly + 400], outline=INK, width=2)
d.text((lx, ly - 8), '凡例', font=bold, fill=INK)
rows = [
    ('inherit', '継承（汎化）：白い三角が親の側'),
    ('comp', 'コンポジション：黒いひし形が持ち主の側。unique_ptr や値のメンバ'),
    ('aggr', '集約：白いひし形が持ち主の側。ポインタで借りているだけ'),
    ('dep', '依存：点線の矢印。使う・作るだけで、メンバには持たない'),
]
for i, row in enumerate(rows):
    y = ly + 62 + i * 62
    a, c = (lx + 10, y), (lx + 170, y)
    if row[0] == 'inherit':
        base = triangle(c, a)
        poly([a, base])
    elif row[0] == 'comp':
        s = diamond(a, c, True)
        poly([s, c])
        arrow(c, a)
    elif row[0] == 'aggr':
        s = diamond(a, c, False)
        poly([s, c])
        arrow(c, a)
    else:
        poly([a, c], dashed=True)
        arrow(c, a)
    d.text((lx + 196, y - 14), row[1], font=small, fill=INK)
d.rectangle([lx + 10, ly + 302, lx + 70, ly + 332], fill=ENGINE_FILL, outline=INK, width=2)
d.text((lx + 84, ly + 304), 'エンジン層のクラス', font=small, fill=INK)
d.rectangle([lx + 330, ly + 302, lx + 390, ly + 332], fill=GAME_FILL, outline=INK, width=2)
d.text((lx + 404, ly + 304), 'ゲーム層のクラス', font=small, fill=INK)
d.text((lx + 10, ly + 348), '+ public　# protected　- private　／　数字＝多重度（持つ数）', font=small, fill=INK)

img.save(OUT)
print('saved', os.path.normpath(OUT), img.size)
