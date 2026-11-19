"""Visual Studio のプロジェクトファイルを、フォルダの中身に合わせる。

  python tools/vcxproj/sync_project_files.py          … 書き換える
  python tools/vcxproj/sync_project_files.py --check  … 何が変わるかを出すだけ

やる事：
  1. CG2_00_01.vcxproj
     ・もう無いファイルの行を消す
     ・engine/ と game/ にあるのに書いていない .cpp / .h を足す
     ・resources/shaders の .hlsl / .hlsli を足す（ビルドはしない。エディタで開けるように並べるだけ。
       シェーダーのコンパイルは、ゲームの起動時に DXC が行う）
  2. CG2_00_01.vcxproj.filters
     ・ソリューション エクスプローラーの見え方（フィルター）を、フォルダと同じ形に作り直す
       （engine\\2d、game\\clear、resources\\shaders\\Sprite …）

新しいファイルを作った時・フォルダを動かした時は、これを 1 回動かせばよい（手で vcxproj を書かなくてよい）。
何度動かしても同じ結果になる。
"""
import re
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / 'project'
PROJECT = 'CG2_00_01'
CR, LF = chr(13), chr(10)
EOL = CR + LF
BS = chr(92)
CHECK = '--check' in sys.argv

ITEM = re.compile(r'[ \t]*<(ClCompile|ClInclude|FxCompile|None|Text|Image)\s+Include="([^"]+)"\s*(/>|>.*?</\1>)[ \t]*\r?\n', re.S)


def read(path):
    raw = path.read_bytes()
    bom = raw.startswith(b'\xef\xbb\xbf')
    return raw.decode('utf-8-sig' if bom else 'utf-8'), bom


def write(path, text, bom):
    if not CHECK:
        path.write_bytes((b'\xef\xbb\xbf' if bom else b'') + text.encode('utf-8'))


def relative(path):
    return str(path.relative_to(ROOT)).replace('/', BS)


def main():
    vcx = ROOT / (PROJECT + '.vcxproj')
    text, bom = read(vcx)
    changes = []

    # --- もう無いファイル
    def keep(match):
        if (ROOT / match.group(2)).exists():
            return match.group(0)
        changes.append('removed  ' + match.group(2))
        return ''

    text = ITEM.sub(keep, text)
    text = re.sub(r'[ \t]*<ItemGroup>\s*</ItemGroup>[ \t]*\r?\n', '', text)
    listed = {m.group(2).lower() for m in ITEM.finditer(text)}

    # --- 書いていないファイル
    def add(kind, paths):
        nonlocal text
        paths = [p for p in paths if p.lower() not in listed]
        if not paths:
            return
        same_kind = list(re.finditer(r'[ \t]*<%s Include="[^"]+" />\r?\n' % kind, text))
        if not same_kind:
            raise SystemExit('no <%s> item to add after' % kind)
        at = same_kind[-1].end()
        text = text[:at] + ''.join('    <%s Include="%s" />%s' % (kind, p, EOL) for p in paths) + text[at:]
        for p in paths:
            listed.add(p.lower())
            changes.append('added    ' + p)

    for top in ('engine', 'game'):
        add('ClCompile', [relative(f) for f in sorted((ROOT / top).rglob('*.cpp'))])
        add('ClInclude', [relative(f) for f in sorted((ROOT / top).rglob('*.h'))])
    shader_root = ROOT / 'resources' / 'shaders'
    add('None', [relative(f) for f in sorted(shader_root.rglob('*')) if f.suffix.lower() in ('.hlsl', '.hlsli')])
    write(vcx, text, bom)

    # --- フィルター：フォルダと同じ形
    items = [(m.group(1), m.group(2)) for m in ITEM.finditer(text)]
    folders = set()
    for _, path in items:
        parts = path.split(BS)[:-1]
        for depth in range(1, len(parts) + 1):
            folders.add(BS.join(parts[:depth]))
    out = ['<?xml version="1.0" encoding="utf-8"?>',
           '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">',
           '  <ItemGroup>']
    for name in sorted(folders, key=str.lower):
        # フィルターの番号は、名前から決める（動かすたびに変わらないように）
        guid = uuid.uuid5(uuid.NAMESPACE_URL, PROJECT + '/filter/' + name.lower())
        out += ['    <Filter Include="%s">' % name, '      <UniqueIdentifier>{%s}</UniqueIdentifier>' % guid, '    </Filter>']
    out.append('  </ItemGroup>')
    for kind in ('ClCompile', 'ClInclude', 'FxCompile', 'None', 'Text', 'Image'):
        group = sorted((p for k, p in items if k == kind), key=str.lower)
        if not group:
            continue
        out.append('  <ItemGroup>')
        for path in group:
            folder = BS.join(path.split(BS)[:-1])
            if folder:
                out += ['    <%s Include="%s">' % (kind, path), '      <Filter>%s</Filter>' % folder, '    </%s>' % kind]
            else:
                out.append('    <%s Include="%s" />' % (kind, path))
        out.append('  </ItemGroup>')
    out.append('</Project>')
    filters = ROOT / (PROJECT + '.vcxproj.filters')
    old, filter_bom = read(filters)
    new = EOL.join(out)
    if new != old:
        changes.append('filters  rewritten (%d folders, %d items)' % (len(folders), len(items)))
    write(filters, new, filter_bom)

    for line in changes:
        print(line)
    print('%s: %d change(s)' % ('check only' if CHECK else 'done', len(changes)))


if __name__ == '__main__':
    main()
