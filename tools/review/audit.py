"""講評会の項目（Classroom 01・02・03_01）に沿って、自分のコード（project/engine・project/game）を数える。"""
import os
import re
import sys
from collections import Counter, defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "project"))
DIRS = ['engine', 'game']
SKIP = ('externals',)


def files():
    for d in DIRS:
        for base, _, names in os.walk(os.path.join(ROOT, d)):
            if any(s in base for s in SKIP):
                continue
            for n in names:
                if n.endswith(('.h', '.cpp')):
                    yield os.path.join(base, n)


def read(p):
    raw = open(p, 'rb').read()
    for enc in ('utf-8-sig', 'cp932'):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass
    return raw.decode('utf-8', 'replace')


def rel(p):
    return os.path.relpath(p, ROOT).replace('\\', '/')


def strip_comments(t):
    t = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)


all_files = list(files())
texts = {p: read(p) for p in all_files}
print('files', len(all_files), 'lines', sum(t.count('\n') for t in texts.values()))

# 1. 依存の向き：engine → game の include
print('\n== 1. engine から game を include')
for p, t in texts.items():
    if rel(p).startswith('engine/'):
        for m in re.finditer(r'#include\s+"((?:game|Game)/[^"]+)"', t):
            print('  ', rel(p), '->', m.group(1))

# 2. include のパスの大文字小文字が実際のフォルダと違う
print('\n== 2. include の大文字小文字の不一致')
real = {}
for base, dirs, names in os.walk(ROOT):
    if 'externals' in base or '.git' in base:
        continue
    for n in names:
        if n.endswith(('.h', '.hpp')):
            r = os.path.relpath(os.path.join(base, n), ROOT).replace('\\', '/')
            real[r.lower()] = r
bad_case = Counter()
for p, t in texts.items():
    for m in re.finditer(r'#include\s+"([^"]+)"', t):
        inc = m.group(1).replace('\\', '/')
        if inc.lower() in real and real[inc.lower()] != inc:
            bad_case[rel(p)] += 1
print('   files', len(bad_case), 'includes', sum(bad_case.values()), bad_case.most_common(5))

# 3. ヘッダの using namespace
print('\n== 3. ヘッダの using namespace')
for p, t in texts.items():
    if p.endswith('.h'):
        for m in re.finditer(r'^\s*using\s+namespace\s+[\w:]+\s*;', strip_comments(t), flags=re.M):
            print('  ', rel(p), m.group(0).strip())

# 4. new / delete
print('\n== 4. new / delete')
for p, t in texts.items():
    s = strip_comments(t)
    s = re.sub(r'"(?:\\.|[^"\\])*"', '""', s)
    for m in re.finditer(r'(?<![\w.])(new\s+[\w:<]|delete\s*(\[\s*\])?\s+[\w(*])', s):
        line = s.count('\n', 0, m.start()) + 1
        text = s.splitlines()[line - 1].strip()
        if '= delete' in text or 'operator' in text:
            continue
        print('  ', rel(p), line, text[:90])

# 5. friend / protected のメンバ変数 / public のメンバ変数（class）
print('\n== 5. friend（Singleton 以外）')
for p, t in texts.items():
    for m in re.finditer(r'^\s*friend\s+[^\n;]+;', strip_comments(t), flags=re.M):
        if 'Singleton<' in m.group(0):
            continue
        print('  ', rel(p), m.group(0).strip())

# class ブロックを切り出して、アクセス区分ごとのメンバ変数を数える
MEMBER = re.compile(r'^\s*(?:static\s+|mutable\s+|inline\s+|constexpr\s+|const\s+)*[\w:<>,\s\*&]+?\s+(\w+)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*|\{[^;]*\})?;\s*$')


def class_blocks(t):
    s = strip_comments(t)
    for m in re.finditer(r'\b(class|struct)\s+(\w+)\s*(?:final\s*)?(?::[^{;]*)?\{', s):
        depth, i = 1, m.end()
        while i < len(s) and depth:
            if s[i] == '{':
                depth += 1
            elif s[i] == '}':
                depth -= 1
            i += 1
        yield m.group(1), m.group(2), s[m.end():i - 1], s.count('\n', 0, m.start()) + 1


public_vars = []
protected_vars = []
class_count = struct_count = 0
for p, t in texts.items():
    if not p.endswith('.h'):
        continue
    for kind, name, body, line in class_blocks(t):
        if kind == 'struct':
            struct_count += 1
            continue
        class_count += 1
        access = 'private'
        depth = 0
        pubs, prots = [], []
        for ln in body.splitlines():
            st = ln.strip()
            if depth == 0:
                a = re.match(r'(public|protected|private)\s*:', st)
                if a:
                    access = a.group(1)
                    st = st[a.end():].strip()
                if st and '(' not in st and not st.startswith(('using', 'typedef', 'friend', 'enum', 'struct', 'class', 'static constexpr', 'constexpr', 'static const', 'template', '#')):
                    mm = MEMBER.match(st)
                    if mm and not st.startswith('return'):
                        if access == 'public':
                            pubs.append(mm.group(1))
                        elif access == 'protected':
                            prots.append(mm.group(1))
            depth += ln.count('{') - ln.count('}')
        if pubs:
            public_vars.append((rel(p), name, pubs))
        if prots:
            protected_vars.append((rel(p), name, prots))
print('\n== 5b. class', class_count, 'struct', struct_count)
print('   public のメンバ変数を持つ class', len(public_vars), '合計', sum(len(v) for _, _, v in public_vars))
for f, n, v in sorted(public_vars, key=lambda x: -len(x[2]))[:25]:
    print('    %3d %s (%s) %s' % (len(v), n, f, ', '.join(v[:6])))
print('   protected のメンバ変数を持つ class', len(protected_vars), '合計', sum(len(v) for _, _, v in protected_vars))
for f, n, v in sorted(protected_vars, key=lambda x: -len(x[2]))[:15]:
    print('    %3d %s (%s) %s' % (len(v), n, f, ', '.join(v[:6])))

# 6. 64bit を超える引数の値渡し
print('\n== 6. 大きい型の値渡しの引数')
BIG = r'(?:Vector3|Vector4|Vector2|Matrix4x4|Matrix3x3|Quaternion|Transform|EulerTransform|QuaternionTransform|std::string|std::wstring|std::vector<[^()]*?>|std::map<[^()]*?>|std::unordered_map<[^()]*?>|std::function<[^()]*?(?:\([^()]*\))?[^()]*?>|nlohmann::json|json|AABB|Sphere|OBB|Segment|Ray|Plane|Triangle|BackdropItem|RailCamera|RailTrain|RailSkin|SplineRail)'
PARAM = re.compile(r'(?<![\w:<&*])(?:const\s+)?(' + BIG + r')\s+(\w+)\s*(?:=[^,)]*)?(?=[,)])')
by_value = Counter()
samples = defaultdict(list)
for p, t in texts.items():
    s = strip_comments(t)
    for m in re.finditer(r'[\w~>:]+\s*\(([^(){};]*(?:\([^(){};]*\)[^(){};]*)*)\)\s*(?:const)?\s*(?:override|noexcept|final)?\s*(?:\{|;|:)', s):
        args = m.group(1)
        if not args.strip():
            continue
        line_start = s.rfind('\n', 0, m.start()) + 1
        head = s[line_start:m.start()].strip()
        if head.startswith(('return', 'if', 'for', 'while', 'else', '}', '=')) or '=' in head and 'operator' not in head:
            continue
        for a in PARAM.finditer(args):
            typ = a.group(1)
            if typ == 'Vector2':
                continue  # 64bit ちょうど（float 2 個）＝値渡しでよい
            by_value[rel(p)] += 1
            if len(samples[rel(p)]) < 2:
                samples[rel(p)].append((s.count('\n', 0, m.start()) + 1, a.group(0)))
print('   files', len(by_value), 'params', sum(by_value.values()))
for f, c in by_value.most_common(25):
    print('    %3d %s %s' % (c, f, samples[f]))

# 7. クラスコメント：class / struct の定義の直前にコメントがあるか
print('\n== 7. クラスコメントの無い class / struct')
missing = []
total = 0
for p, t in texts.items():
    lines = t.splitlines()
    for i, ln in enumerate(lines):
        m = re.match(r'\s*(?:template\s*<[^>]*>\s*)?(class|struct)\s+(\w+)\s*(?:final\s*)?(?::[^;{]*)?\{?\s*$', ln)
        if not m or ln.strip().endswith(';'):
            continue
        # 前方宣言を除く（{ が同じ行か次の行にある）
        nxt = lines[i + 1].strip() if i + 1 < len(lines) else ''
        if '{' not in ln and not nxt.startswith('{') and not nxt.startswith(':') and not nxt.startswith(','):
            continue
        total += 1
        j = i - 1
        while j >= 0 and (lines[j].strip() == '' or lines[j].strip().startswith('template')):
            j -= 1
        prev = lines[j].strip() if j >= 0 else ''
        if not (prev.startswith('//') or prev.endswith('*/')):
            missing.append((rel(p), i + 1, m.group(1), m.group(2)))
print('   total', total, 'missing', len(missing))
by_file = Counter(f for f, *_ in missing)
for f, c in by_file.most_common(30):
    print('    %3d %s %s' % (c, f, [n for ff, _, _, n in missing if ff == f][:6]))

if '--dump' in sys.argv:
    import json
    json.dump({'missing': missing, 'public': public_vars, 'by_value': dict(by_value)}, open(os.path.join(os.path.dirname(__file__), 'audit.json'), 'w', encoding='utf-8'), ensure_ascii=False)

# 8. エンジン層の namespace（02_07）
print('\n== 8. エンジン層の namespace（KazumimiEngine）')
outside = [rel(p) for p in files() if rel(p).replace(os.sep, '/').startswith('engine/') and 'namespace KazumimiEngine {' not in read(p)]
print('   engine のファイルで namespace に入っていない物', len(outside))
for f in outside[:20]:
    print('    ', f)
