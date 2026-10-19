import os
import re
import sys

ANILIST = r'D:\MxM\ClientEUdump\Graphic__Creature__AniList.xml'
DATA = r'D:\MxM\ClientCN\cn\Data'
OUT = r'D:\MxM\GameServer\gamedata\MoveGraph.xml'


def find_file(rel):
    path = os.path.join(DATA, rel.replace('/', os.sep))
    if os.path.exists(path):
        return path
    folder = DATA
    for part in rel.split('/'):
        if not os.path.isdir(folder):
            return None
        match = [e for e in os.listdir(folder) if e.lower() == part.lower()]
        if not match:
            return None
        folder = os.path.join(folder, match[0])
    return folder if os.path.isfile(folder) else None


def curves(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    out = {}
    for m in re.finditer(r'<StatePlay [^>]*>', text):
        tag = m.group(0)
        anim = re.search(r' AnimationIndex="(-?\d+)"', tag)
        if not anim or ' H_Y0="' not in tag:
            continue
        samples = []
        i = 0
        while True:
            v = re.search(r' H_Y%d="([^"]*)"' % i, tag)
            if not v:
                break
            samples.append(float(v.group(1)))
            i += 1
        if not samples or max(abs(x) for x in samples) <= 0:
            continue
        t = re.search(r' H_Time="([^"]*)"', tag)
        out.setdefault(int(anim.group(1)), (float(t.group(1)) if t else 0.0, samples))
    return out


def main():
    lst = open(ANILIST, encoding='utf-8-sig', errors='replace').read()
    lines = ['<?xml version="1.0" encoding="utf-8"?>', '<MoveGraph>']
    done = set()
    for m in re.finditer(r'<Character PC="True" Type="([^"]+)" FileName="([^"]+)"', lst):
        cls, rel = m.group(1), m.group(2)
        if cls.upper() in done:
            continue
        path = find_file(rel)
        if not path:
            print('missing', cls, rel)
            continue
        c = curves(path)
        if not c:
            continue
        done.add(cls.upper())
        lines.append('  <Class Type="%s">' % cls.upper())
        for anim in sorted(c):
            t, s = c[anim]
            lines.append('    <Anim Id="%d" Time="%g" H="%s" />' % (anim, t, ' '.join('%g' % x for x in s)))
        lines.append('  </Class>')
    lines.append('</MoveGraph>')
    open(OUT, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    print(len(done), 'classes ->', OUT)


if __name__ == '__main__':
    sys.exit(main())
