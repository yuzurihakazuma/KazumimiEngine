"""カプセル化（課題 01_01）の確認。project/engine と project/game のヘッダを読んで、だめな例に当たる所を数える。
  python tools/review/audit_encapsulation.py
    1. class の public メンバ変数
    2. protected メンバ変数
    3. メンバを非 const の参照・ポインタでそのまま返す関数（中の vector を書き換えられる形）
    4. friend（Singleton の土台以外）
    5. ファイルの外から見える、const でない変数（グローバル変数）
    6. メンバ関数を 3 個以上持つ struct（参考。データのまとまりとして使っている物）
audit.py の「5b. public のメンバ変数」は関数の既定の引数を数えてしまうので、こちらの数が正しい。
"""
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'project'))
DIRS = ('engine', 'game')
KEYWORD_START = re.compile(r'^(using|typedef|friend|enum|class|struct|union|template|static_assert|namespace|public|private|protected)\b')


def files(exts):
    for d in DIRS:
        for base, _, names in os.walk(os.path.join(ROOT, d)):
            for n in sorted(names):
                if n.endswith(exts):
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


def code_only(t):
    """コメントと文字列を空白にする（長さは変えない）"""
    out = list(t)
    i, n = 0, len(t)
    while i < n:
        c = t[i]
        if t.startswith('//', i):
            j = t.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif t.startswith('/*', i):
            j = t.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif c == '"' or (c == "'" and not (i > 0 and t[i - 1].isdigit())):
            j = i + 1
            while j < n and t[j] != c and t[j] != '\n':
                j += 2 if t[j] == '\\' else 1
            for k in range(i + 1, min(j, n)):
                out[k] = ' '
            i = j + 1
        else:
            i += 1
    return ''.join(out)


def blocks(code):
    """(種類, 名前, 本体の始まり, 本体の終わり) を、入れ子も含めて全部返す"""
    for m in re.finditer(r'\b(class|struct)\s+(?:alignas\s*\([^)]*\)\s*)?([A-Za-z_]\w*(?:::\w+)*)\s*(?:final\s*)?(?::[^;{]*)?\{', code):
        before = code[max(0, m.start() - 12):m.start()]
        if re.search(r'\benum\s*$', before):
            continue
        i = m.end()
        depth = 1
        while i < len(code) and depth:
            depth += (code[i] == '{') - (code[i] == '}')
            i += 1
        yield m.group(1), m.group(2), m.end(), i - 1


def statements(body):
    """本体の深さ 0 の文を (アクセス, 文, 位置) で返す。関数の本体・入れ子の型の中は読まない"""
    out = []
    i, n = 0, len(body)
    start = 0
    depth = 0
    paren = 0
    while i < n:
        c = body[i]
        if c == '(':
            paren += 1
        elif c == ')':
            paren -= 1
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0 and paren == 0:
                # 関数の本体や入れ子の型が終わった。後ろに ; が無ければここで文を切る
                j = i + 1
                while j < n and body[j] in ' \t\r\n':
                    j += 1
                if j >= n or body[j] != ';':
                    out.append((body[start:i + 1], start))
                    start = i + 1
        elif c == ';' and depth == 0 and paren == 0:
            out.append((body[start:i + 1], start))
            start = i + 1
        elif c == ':' and depth == 0 and paren == 0:
            head = body[start:i].strip()
            if head in ('public', 'private', 'protected'):
                out.append((head + ':', start))
                start = i + 1
        i += 1
    return out


def member_variables(kind, body):
    """[(アクセス, 名前)]"""
    access = 'private' if kind == 'class' else 'public'
    found = []
    for text, _ in statements(body):
        s = ' '.join(text.split())
        if s in ('public:', 'private:', 'protected:'):
            access = s[:-1]
            continue
        if not s.endswith(';') or KEYWORD_START.match(s):
            continue
        if re.match(r'^(static|inline static|static inline)\b.*\b(const|constexpr)\b', s) or s.startswith(('constexpr', 'static constexpr', 'inline constexpr')):
            continue
        # 初期値（= … / { … }）を外して、残りにかっこが無ければ変数
        head = re.split(r'=(?!=)', s, 1)[0]
        head = re.sub(r'\{.*$', '', head).rstrip(' ;')
        if '(' in head or ')' in head or not head:
            continue
        if 'operator' in head:
            continue
        for part in head.split(','):
            m = re.search(r'([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*$', part.strip())
            if m:
                found.append((access, m.group(1)))
    return found


def main():
    public_vars = defaultdict(list)
    protected_vars = defaultdict(list)
    leaks = []
    friends = []
    busy_structs = []
    for p in files(('.h',)):
        code = code_only(read(p))
        for kind, name, a, b in blocks(code):
            body = code[a:b]
            variables = member_variables(kind, body)
            names = {v for _, v in variables}
            if kind == 'class':
                for access, v in variables:
                    if access == 'public':
                        public_vars[(rel(p), name)].append(v)
            for access, v in variables:
                if access == 'protected':
                    protected_vars[(rel(p), name)].append(v)
            # メンバをそのまま、非 const の参照・ポインタで返す関数
            for text, pos in statements(body):
                s = ' '.join(text.split())
                m = re.match(r'^(?:\[\[nodiscard\]\] )?(?:virtual |inline |static )*(.+?)([&*])\s*(\w+)\s*\(([^)]*)\)\s*(const)?\s*(?:override)?\s*\{\s*return\s+(&?)\s*([A-Za-z_]\w*)\b([^;]*);\s*\}$', s)
                if not m:
                    continue
                ret, sigil, func, _, _, addr, target, tail = m.groups()
                if re.match(r'^(const\b|static const\b)', ret.strip()) or 'operator' in func:
                    continue
                if target not in names or '(' in tail:
                    continue
                if sigil == '*' and not addr and not tail.strip().startswith('.data'):
                    continue  # 持っているポインタそのもの（借り物・外部の物）を返すだけ
                line = code.count('\n', 0, a + pos + len(text) - len(text.lstrip())) + 1
                leaks.append((rel(p), line, name, s[:110]))
            for m in re.finditer(r'\bfriend\s+(?:class|struct)\s+([\w:<>]+)', body):
                if 'Singleton' not in m.group(1):
                    friends.append((rel(p), name, m.group(1)))
            if kind == 'struct':
                funcs = [t for t, _ in statements(body) if re.search(r'\b[A-Z]\w*\s*\([^;]*\)\s*(?:const)?\s*(?:\{|;)', ' '.join(t.split())) and not KEYWORD_START.match(' '.join(t.split()))]
                if len(funcs) >= 3 and variables:
                    busy_structs.append((len(funcs), name, rel(p)))

    print('== 1. class の public メンバ変数')
    print('   class %d 個・変数 %d 個' % (len(public_vars), sum(len(v) for v in public_vars.values())))
    for (f, name), vs in sorted(public_vars.items(), key=lambda kv: -len(kv[1])):
        print('   %3d %s (%s) %s' % (len(vs), name, f, ', '.join(vs[:8])))
    print('\n== 2. protected メンバ変数')
    print('   %d 個' % sum(len(v) for v in protected_vars.values()))
    for (f, name), vs in protected_vars.items():
        print('       %s (%s) %s' % (name, f, ', '.join(vs)))
    print('\n== 3. メンバを非 const の参照・ポインタでそのまま返す関数')
    signals = [x for x in leaks if 'Signal<' in x[3]]
    services = [x for x in leaks if x not in signals and x[2] in ('DirectXCommon',)]
    rest = [x for x in leaks if x not in signals and x not in services]
    print('   %d 個（ほかに、知らせをつなぐ口 Signal %d 個・土台のサービス %d 個）' % (len(rest), len(signals), len(services)))
    for f, line, name, s in rest:
        print('       %s:%d %s' % (f, line, s))
    print('\n== 4. friend（Singleton の土台以外）')
    print('   %d 個' % len(friends))
    for f, name, who in friends:
        print('       %s: %s が %s を friend にしている' % (f, name, who))
    print('\n== 5. ファイルの外から見える、const でない変数')
    globals_found = []
    for p in files(('.h', '.cpp')):
        code = code_only(read(p))
        for m in re.finditer(r'(?m)^extern\s+(?!"C")(?!IMGUI_IMPL_API)([^;(]+);', code):
            globals_found.append((rel(p), ' '.join(m.group(0).split())[:90]))
    print('   %d 個' % len(globals_found))
    for f, s in globals_found:
        print('       %s: %s' % (f, s))
    print('\n== 6. メンバ関数を 3 個以上持つ struct（参考）')
    for n, name, f in sorted(busy_structs, reverse=True):
        print('   %3d 関数  %s (%s)' % (n, name, f))
    return 1 if (public_vars or rest or globals_found) else 0


if __name__ == '__main__':
    sys.exit(main())
