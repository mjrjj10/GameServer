import os
import re
import sys
import json

CN_DATA = r'D:\MxM\ClientCN\cn\Data'
EU_DUMP = r'D:\MxM\ClientEUdump'
EU_COPY = r'D:\MxM\GameServer\build\gamedata\client'
EU_COPY_ROOT = r'D:\MxM\GameServer\build\gamedata'
OUT = r'D:\MxM\ClientMod'
OVERLAY = r'L10N\Locale\USA'
SERVER_DATA = r'D:\MxM\GameServer\gamedata'

INFO_RE = re.compile(r'  <INFO ID="(\d+)".*?\n  </INFO>\r?\n', re.S)
LIST_TYPE_RE = re.compile(r'  <List Type="([^"]+)">.*?\n  </List>\r?\n', re.S)
LIST_ALIAS_RE = re.compile(r'    <List Alias="([^"]+)">.*?\n    </List>\r?\n', re.S)
ACTION_RE = re.compile(r'  <ActionBase [^>]*Class="([^"]+)"[^>]*/>\r?\n')
STRING_RE = re.compile(r'    <StringData DATAKEY="([^"]+)".*?/>\r?\n', re.S)
ENTITY_RE = re.compile(r'(\s*)<Entity _Key="([^"]+)".*?</Entity>\r?\n', re.S)
SKIN_RE = re.compile(r'    <_Skin DATAKEY="([^"]+)".*?\n    </_Skin>\r?\n', re.S)
TT_RE = re.compile(r'\t<data DATAKEY="(\d+)">.*?\n\t</data>\r?\n', re.S)
NUM_RE = re.compile(r'\b(\d{8,10})\b')
NAME_RE = re.compile(r'(?:_ResName|SFXID|_EffectKeyName|_ConstantSFX|_IconAlias|_EffectKeyNames DATA|DATA)="([A-Za-z][A-Za-z0-9_]+)"')
STR_RE = re.compile(r'"(STR_[A-Za-z0-9_]+)"')
BACKREF_RE = re.compile(r'(?:_SkillIndex|_DocID)="(\d+)"')
KFM_RE = re.compile(r'(?:Kfm_ComData KeyName|_BaseKFMKeyName)="([^"]+)"')


def read(p):
    with open(p, encoding='utf-8-sig', errors='strict') as f:
        return f.read()


def eu_sources():
    out = {}
    for root in (EU_COPY_ROOT, EU_COPY):
        for f in os.listdir(root):
            if f.lower().endswith('.xml'):
                out.setdefault(f.lower(), os.path.join(root, f))
    for f in os.listdir(EU_DUMP):
        parts = f.split('__')
        if parts[0].lower() in ('design', 'l10n') and f.lower().endswith('.xml'):
            out[parts[-1].lower()] = os.path.join(EU_DUMP, f)
    for sub in ('string',):
        d = os.path.join(EU_COPY, sub)
        if os.path.isdir(d):
            for f in os.listdir(d):
                out.setdefault(f.lower(), os.path.join(d, f))
    return out


def cn_docs():
    out = {}
    for sub in (r'Design\DOCUMENT', r'Design\GAMEINFO', r'Design\STRING'):
        d = os.path.join(CN_DATA, sub)
        for f in os.listdir(d):
            if f.lower().endswith('.xml'):
                out[f.lower()] = (sub, f, os.path.join(d, f))
    return out


def blocks(text):
    found = []
    for rx, kind in ((INFO_RE, 'id'), (LIST_TYPE_RE, 'name'), (LIST_ALIAS_RE, 'name'), (ACTION_RE, 'class'),
                     (STRING_RE, 'str'), (SKIN_RE, 'skin'), (TT_RE, 'id')):
        for m in rx.finditer(text):
            found.append((kind, m.group(1), m.group(0)))
    for m in ENTITY_RE.finditer(text):
        t = re.findall(r'<EntityType _Type="([^"]+)"', text[:m.start()])
        found.append(('class', m.group(2), (t[-1] if t else 'PC') + '|' + m.group(0).lstrip('\r\n')))
    return found


def main():
    seed_doc = int(sys.argv[1]) if len(sys.argv) > 1 else 100000041
    seed_class = sys.argv[2] if len(sys.argv) > 2 else 'SHUTTLE'
    eu = eu_sources()
    tr = translations()
    cn = cn_docs()
    eu_keys = {}
    cn_blocks = {}
    for low, (sub, name, path) in cn.items():
        text = read(path)
        cn_blocks[low] = blocks(text)
        if low in eu:
            eu_keys[low] = set((k, v) for k, v, _ in blocks(read(eu[low])))
    missing_eu = sorted(low for low in cn if low not in eu)

    want_ids = {str(seed_doc)}
    want_names = set()
    want_classes = {seed_class}
    want_strs = set()
    want_skin_class = 'CLASS_TYPE_' + seed_class
    picked = {}
    changed = True
    while changed:
        changed = False
        for low, bl in cn_blocks.items():
            if low not in eu:
                continue
            for kind, key, body in bl:
                ident = (low, kind, key, body if kind == 'class' else '')
                if ident in picked or (kind != 'class' and (kind, key) in eu_keys[low]) or (kind == 'class' and (kind, key) in eu_keys[low] and key != seed_class):
                    continue
                hit = (kind == 'id' and key in want_ids) or (kind == 'name' and key in want_names) or \
                      (kind == 'class' and key in want_classes) or (kind == 'str' and key in want_strs) or \
                      (kind == 'skin' and want_skin_class in body) or \
                      (kind == 'id' and low == 'skill_property.xml' and any(m in want_ids for m in BACKREF_RE.findall(body)))
                if not hit:
                    continue
                picked[ident] = body
                changed = True
                if kind == 'str':
                    continue
                want_ids.update(NUM_RE.findall(body))
                want_names.update(NAME_RE.findall(body))
                want_classes.update(KFM_RE.findall(body))
                want_strs.update(STR_RE.findall(body))

    os.makedirs(OUT, exist_ok=True)
    manifest = {'master': seed_doc, 'class': seed_class, 'files': {}, 'missing_eu_docs': missing_eu}
    by_doc = {}
    for (low, kind, key, _), body in picked.items():
        by_doc.setdefault(low, []).append((kind, key, body))
    for low, items in sorted(by_doc.items()):
        sub, name, _ = cn[low]
        text = read(eu[low])
        for kind, key, body in items:
            text = insert(text, kind, key, body, cn_blocks[low])
        text = translate(text, tr)
        rel = os.path.join('Data', OVERLAY, sub, name)
        dst = os.path.join(OUT, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, 'w', encoding='utf-8-sig', newline='') as f:
            f.write(text)
        manifest['files'][rel] = sorted('%s:%s' % (k, v) for k, v, _ in items)
        print('%-34s +%d (%s)' % (name, len(items), ', '.join(sorted(set(k for k, _, _ in items)))))
    with open(os.path.join(OUT, 'manifest_%d.json' % seed_doc), 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=1, ensure_ascii=False)
    if '--server' in sys.argv:
        for root, _, files in os.walk(SERVER_DATA):
            for name in files:
                low = name.lower()
                if low not in by_doc:
                    continue
                path = os.path.join(root, name)
                raw = open(path, 'rb').read()
                bom = raw.startswith(b'\xef\xbb\xbf')
                text = raw.decode('utf-8-sig')
                have = set((k, v) for k, v, _ in blocks(text))
                have_bodies = set(b for _, _, b in blocks(text))
                added = 0
                for kind, key, body in by_doc[low]:
                    if (kind != 'class' and (kind, key) in have) or (kind == 'class' and body in have_bodies):
                        continue
                    text = insert(text, kind, key, body, cn_blocks[low])
                    added += 1
                translated = translate(text, tr)
                if added or translated != text:
                    text = translated
                    with open(path, 'w', encoding='utf-8-sig' if bom else 'utf-8', newline='') as f:
                        f.write(text)
                    print('server %-60s +%d' % (os.path.relpath(path, SERVER_DATA), added))
    print('classes', sorted(want_classes))
    print('eu docs missing for cn docs:', len(missing_eu))


def translations():
    for a in sys.argv:
        if a.startswith('--tr='):
            with open(a[5:], encoding='utf-8') as f:
                return json.load(f)
    return {}


def translate(text, tr):
    for key, value in tr.items():
        text = re.sub(r'(<StringData DATAKEY="%s"[^>]*?data=")[^"]*(")' % re.escape(key), lambda m: m.group(1) + value + m.group(2), text)
    return text


def insert(text, kind, key, body, cn_list):
    if kind == 'class' and '<Entity ' in body.split('\n', 1)[0] and '|' in body.split('\n', 1)[0]:
        etype, ent = body.split('|', 1)
        start = text.find('<EntityType _Type="%s">' % etype)
        end = text.find('</EntityType>', start)
        line = text.rfind('\n', 0, end)
        return text[:line] + '\n' + ent.rstrip('\r\n') + text[line:]
    if kind == 'str':
        close = text.rfind('  </StringTableData>')
        if close < 0:
            close = text.rfind('</')
        return text[:close] + body + text[close:]
    if kind == 'name' and body.startswith('    <List Alias'):
        close = text.rfind('  </INFO>')
        return text[:close] + body + text[close:]
    if kind == 'skin':
        close = text.rfind('  </')
        return text[:close] + body + text[close:]
    close = text.rfind('</')
    return text[:close] + body + text[close:]


if __name__ == '__main__':
    main()
