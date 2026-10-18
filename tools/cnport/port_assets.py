import os
import re
import sys
import json
import shutil

CN = r'D:\MxM\ClientCN\cn\Data'
EU = r'D:\MxM\Client\Data'
MOD = r'D:\MxM\ClientMod\Data'
TEXTURE_RE = re.compile(rb'[\w\\/ .-]{3,120}\.(?:dds|tga|nif|kf|png)', re.I)


def copy(rel, out):
    dst = os.path.join(MOD, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(os.path.join(CN, rel), dst)
    out.append(rel)


def main():
    name = sys.argv[1].lower()
    manifest = os.path.join(EU, 'cnport_installed.txt')
    installed = set(l.strip().lower() for l in open(manifest, encoding='utf-8-sig')) if os.path.exists(manifest) else set()
    copied = []
    for top in ('Graphic', os.path.join('Design', 'SFX'), 'Ui'):
        for root, _, files in os.walk(os.path.join(CN, top)):
            for f in files:
                rel = os.path.relpath(os.path.join(root, f), CN)
                if name in rel.lower() and (rel.lower() in installed or not os.path.exists(os.path.join(EU, rel))):
                    copy(rel, copied)
    eu_names = set(f.lower() for root, _, fs in os.walk(os.path.join(EU, 'Graphic')) for f in fs
                   if os.path.relpath(os.path.join(root, f), EU).lower() not in installed)
    mod_names = set(os.path.basename(r).lower() for r in copied)
    cn_index = {}
    for root, _, files in os.walk(os.path.join(CN, 'Graphic')):
        for f in files:
            cn_index.setdefault(f.lower(), os.path.relpath(os.path.join(root, f), CN))
    shared = []
    for rel in list(copied):
        if not rel.lower().endswith(('.nif', '.kfm', '.kf')):
            continue
        data = open(os.path.join(CN, rel), 'rb').read()
        for m in TEXTURE_RE.finditer(data):
            n = m.group().decode('latin1').replace('/', chr(92)).split(chr(92))[-1].lower()
            if n in eu_names or n in mod_names or n not in cn_index:
                continue
            mod_names.add(n)
            copy(cn_index[n], shared)
    with open(os.path.join(os.path.dirname(MOD), 'assets_%s.json' % name), 'w', encoding='utf-8') as f:
        json.dump({'own': copied, 'shared': shared}, f, indent=1)
    print('%s: %d own files, %d shared textures' % (name, len(copied), len(shared)))


if __name__ == '__main__':
    main()
