import collections, re, sys, xml.etree.ElementTree as ET

GD = 'D:/MxM/GameServer/gamedata/'
C = GD + 'client/'
sys.stdout.reconfigure(encoding='utf-8')

remotes = {e.get('ID'): e for e in ET.parse(C + 'REMOTE_PC.xml').getroot()}
statuses = {e.get('ID'): e for e in ET.parse(C + 'STATUS_PC.xml').getroot()}
skills = {i.get('ID'): i[0] for i in ET.parse(C + 'SKILL.xml').getroot()}
actions = collections.defaultdict(list)
for a in ET.parse(GD + 'ActionBase.xml').getroot():
    actions[(a.get('Class', '').upper(), a.get('BehaviorState'))].append(a)
base_damage = {}
for master in ET.parse(C + 'skilltt.xml').getroot():
    for sk in master.findall('data'):
        base = sk.find('data')
        if base is None:
            continue
        for td in base.findall('tableDatas'):
            if td.get('attrType') == 'TTATTR_REMOTE_DAMAGE':
                base_damage[sk.get('DATAKEY')] = float(td.get('value_1') or 0)


def enemy_statuses(rid, depth=0, seen=None):
    seen = seen if seen is not None else set()
    e = remotes.get(rid)
    if e is None or rid in seen or depth > 5:
        return set()
    seen.add(rid)
    out = set()
    b = e.find('RemoteBoundComData2')
    group = b.get('_DamageGroup') if b is not None else ''
    c = e.find('RemoteComData2')
    if c is not None and group in ('eENEMY', 'eALL'):
        out |= {s.get('DATAKEY') for s in c.findall('_Status') if s.get('DATAKEY')}
    sysd = e.find('RemoteSystemComData')
    if sysd is not None:
        for x in sysd.iter():
            ri = x.get('_RemoteIndex')
            if ri and ri.isdigit():
                out |= enemy_statuses(ri, depth + 1, seen)
    return out


def skill_expectations(sid):
    sk = skills.get(sid)
    if sk is None:
        return None
    cls = (sk.get('_RequireClass') or '').replace('CLASS_TYPE_', '')
    act = actions.get((cls, sk.get('_Action')), [])
    exp = {'statuses': set(), 'move': 0.0, 'mp': float(sk.get('_ConsumeMP') or 0), 'damage': base_damage.get(sid, 0.0)}
    for a in act:
        t = a.get('CommandType')
        if t in ('REMOTE', 'REMOTE_SKILL_TARGET') and a.get('RemoteIndex'):
            exp['statuses'] |= enemy_statuses(a.get('RemoteIndex'))
        if t == 'GRAPH_MOVE_HORZ':
            try:
                exp['move'] = max(exp['move'], abs(float(a.get('Param1') or 0)))
            except ValueError:
                pass
    return exp


tests = []
cur = None
for path in sys.argv[1:]:
    for line in open(path, encoding='utf-8', errors='replace'):
        m = re.search(r'Showcase :: wall=\d+ master=(\d+) slot=(\w+) skill=(\d+) caster=\S+ range=(\d+) dist=(\d+) test=(\S+) expect=(-?\d)', line)
        if m:
            cur = {'master': int(m.group(1)), 'slot': m.group(2), 'skill': m.group(3), 'dist': int(m.group(5)), 'test': m.group(6), 'expect': int(m.group(7))}
            tests.append(cur)
            continue
        if cur is None:
            continue
        r = re.search(r'Showcase :: result hit=(\d) expect=(-?\d) (\w+) damage=(-?\d+) hits=(\d+)', line)
        if r:
            cur.update(hit=int(r.group(1)), verdict=r.group(3), damage=int(r.group(4)), hits=int(r.group(5)))
        ms = re.search(r'Showcase :: measure casterMoved=(-?\d+) dummyMoved=(-?\d+) firstHitMs=(-?\d+) mana=(-?\d+) stamina=(-?\d+) statuses=(\S*)', line)
        if ms:
            cur.update(moved=int(ms.group(1)), pushed=int(ms.group(2)), first=int(ms.group(3)), mana=int(ms.group(4)), stamina=int(ms.group(5)),
                       seen=set(x for x in ms.group(6).split(',') if x))

by_master = collections.OrderedDict()
for t in tests:
    by_master.setdefault(t['master'], []).append(t)
issues_total = 0
for master, lst in by_master.items():
    print(f'## master {master}')
    for t in lst:
        if t['slot'] == 'TAG' or 'verdict' not in t:
            continue
        flags = []
        exp = skill_expectations(t['skill']) if t['skill'] != '0' else None
        if t.get('verdict') == 'MISMATCH':
            flags.append('HIT-EXPECTATION')
        if exp and t.get('hit') == 1:
            missing = exp['statuses'] - t.get('seen', set())
            unexpected = t.get('seen', set()) - exp['statuses']
            if missing:
                flags.append('statuses missing ' + ','.join(sorted(missing)))
            if unexpected:
                flags.append('statuses extra ' + ','.join(sorted(unexpected)))
            if exp['damage'] > 0 and t.get('hits', 0) > 0:
                per_hit = t['damage'] / t['hits']
                if abs(per_hit - exp['damage']) > exp['damage'] * 0.25:
                    flags.append(f'damage/hit {per_hit:.0f} vs data {exp["damage"]:.0f}')
        if exp and 'moved' in t:
            if exp['move'] > 0 and t['moved'] > exp['move'] * 1.1 + 50:
                flags.append(f'caster moved {t["moved"]} > data {exp["move"]:.0f}')
            if exp['move'] == 0 and t['moved'] > 150:
                flags.append(f'caster moved {t["moved"]} without move command')
            if exp['mp'] > 0 and t['mana'] > 0 and abs(t['mana'] - exp['mp']) > exp['mp'] * 0.2 + 5:
                flags.append(f'mana spent {t["mana"]} vs data {exp["mp"]:.0f}')
        info = f'moved={t.get("moved", "?")} pushed={t.get("pushed", "?")} firstHit={t.get("first", "?")}ms'
        status = 'OK ' if not flags else 'CHK'
        issues_total += 1 if flags else 0
        print(f'  {status} {t["slot"]:8} {t["skill"]:>9} {t["test"]:16} dist={t["dist"]:5} hit={t.get("hit", "?")} dmg={t.get("damage", "?")} hits={t.get("hits", "?")} {info}' + ('  !! ' + ' | '.join(flags) if flags else ''))
print(f'== {issues_total} tests to check')
