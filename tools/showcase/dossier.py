import collections, glob, os, re, sys, xml.etree.ElementTree as ET

GD = 'D:/MxM/GameServer/gamedata/client/'
runs, tag, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
desc = {}
for f in ('skilldesc.xml',):
    for m in re.finditer(r'DATAKEY="STR_SKILL_(NAME|DESC)_(\d+)" data="([^"]*)"', open(GD + 'string/' + f, encoding='utf-8', errors='replace').read()):
        desc.setdefault(m.group(2), {})[m.group(1)] = m.group(3).replace('&#10;', ' / ')
stype = {}
for e in ET.parse(GD + 'STATUS_PC.xml').getroot():
    c = e.find('ST_COMMONSTATUS')
    if c is not None:
        stype[e.get('ID')] = (c.get('_Type', '').replace('STATUS_TYPE_', ''), c.get('_DurationTime'))
WALL = re.compile(r'Showcase :: wall=\d+ master=(\d+) slot=(\w+) skill=(\d+) caster=(\S+) range=\d+ dist=(\d+) test=(\S+)')
for g in range(4):
    log = os.path.join(runs, f'{tag}_g{g}.log')
    if not os.path.exists(log):
        continue
    cur = None
    facts = collections.OrderedDict()
    for line in open(log, encoding='utf-8', errors='replace'):
        m = WALL.search(line)
        if m:
            cur = m.groups()
            facts.setdefault((int(cur[0]), cur[1], cur[2]), collections.OrderedDict())
            continue
        if not cur:
            continue
        key = (int(cur[0]), cur[1], cur[2])
        d = facts[key]
        if (r := re.search(r'Status :: (\S+) -> (\S+) status=(\d+) type=STATUS_TYPE_(\w+) duration=([0-9.]+) shield=(\d+)', line)):
            who = 'self' if r.group(1) == r.group(2) else 'target'
            d.setdefault('status', set()).add(f'{r.group(3)} {r.group(4)} {r.group(5)}s on {who}' + (f' shield {r.group(6)}' if r.group(6) != '0' else ''))
        elif (r := re.search(r'Damage :: \S+ -> \S+ skill=(-?\d+) damage=(\d+)( CRIT)? .* in=(\d+) remote=(\d+)', line)):
            d.setdefault('damage', collections.Counter())[f'{r.group(4)} (remote {r.group(5)})'] += 1
        elif (r := re.search(r'Heal :: \S+ -> \S+ amount=(\d+)', line)):
            d.setdefault('heal', collections.Counter())[r.group(1)] += 1
        elif (r := re.search(r'Showcase :: measure casterMoved=(-?\d+) dummyMoved=(-?\d+)', line)):
            d.setdefault('moved', set()).add(f'caster {r.group(1)} target {r.group(2)} ({cur[5]} at {cur[4]})')
        elif (r := re.search(r'Summon :: \S+ creature=(\d+)', line)):
            d.setdefault('summon', set()).add(r.group(1))
    masters = collections.OrderedDict()
    for (m, slot, skill), d in facts.items():
        masters.setdefault(m, []).append((slot, skill, d))
    for m, slots in masters.items():
        lines = [f'# master {m}', '']
        for slot, skill, d in slots:
            if slot == 'TAG':
                continue
            ds = desc.get(skill, {})
            lines.append(f'## {slot} {skill} {ds.get("NAME", "")}')
            if ds.get('DESC'):
                lines.append(f'DESC: {ds["DESC"]}')
            for k in ('status', 'damage', 'heal', 'moved', 'summon'):
                if k in d:
                    v = d[k]
                    v = ', '.join(f'{a} x{n}' for a, n in v.items()) if isinstance(v, collections.Counter) else '; '.join(sorted(v))
                    lines.append(f'  {k}: {v}')
            lines.append('')
        open(os.path.join(out, f'master_{m:02d}.txt'), 'w', encoding='utf-8').write('\n'.join(lines))
print('ok')
