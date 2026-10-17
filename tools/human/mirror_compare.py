import re, sys, collections, xml.etree.ElementTree as ET

sys.stdout.reconfigure(encoding='utf-8')
WALL = re.compile(r'Showcase :: wall=(\d+) master=(\d+) slot=(\w+) skill=(\d+) caster=(\S+) range=(\d+) dist=(\d+) test=(\S+) expect=(-?\d)')
RES = re.compile(r'Showcase :: result hit=(\d) expect=(-?\d) (\w+) damage=(-?\d+) hits=(\d+)')
MEAS = re.compile(r'Showcase :: measure casterMoved=(-?\d+) dummyMoved=(-?\d+) firstHitMs=(-?\d+) firstDamage=(-?\d+) mana=(-?\d+) stamina=(-?\d+) statuses=(\S*)')
SKILL = re.compile(r'Skill :: (\S+) skill=(\d+) anim=(\S+) scheduled=(\d)')
REMOTE = re.compile(r'Remote :: spawn (\d+) ')
CAST = re.compile(r'Cast :: (\S+) skill=(\d+) (start|release|cancelled)')
TURN = re.compile(r'Mirror :: turn=(\d)')
DMG = re.compile(r'Damage :: (\S+) -> (\S+) skill=-?\d+ damage=(\d+)( CRIT)? hp=\S+ in=(\d+) remote=\d+ element=(\d+)')

STYPE = {e.get('ID'): (e.find('ST_COMMONSTATUS').get('_Type') if e.find('ST_COMMONSTATUS') is not None else '') for e in ET.parse('D:/MxM/GameServer/gamedata/client/STATUS_PC.xml').getroot()}
LIMITS = {}
for info in ET.parse('D:/MxM/GameServer/gamedata/client/SKILL.xml').getroot():
    tgt = info[0].find('_Target')
    if tgt is not None:
        LIMITS[info.get('ID')] = (float(tgt.get('_MinDistance') or 0), float(tgt.get('_MaxDistance') or 0))
tests, cur = [], None
for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = WALL.search(line)
    if m:
        cur = dict(master=m.group(2), slot=m.group(3), skill=m.group(4), caster=m.group(5), dist=int(m.group(7)), test=m.group(8),
                   skills=[], remotes=collections.Counter(), casts=[], base=0, crit=False, elements=set())
        tests.append(cur)
        continue
    if cur is None:
        continue
    if (r := DMG.search(line)) and r.group(1) == cur['caster']:
        cur['base'] += int(r.group(5))
        cur['crit'] = cur['crit'] or bool(r.group(4))
        cur['elements'].add(int(r.group(6)))
    if (r := TURN.search(line)):
        cur['turn'] = int(r.group(1))
    elif (r := RES.search(line)):
        cur.update(hit=int(r.group(1)), damage=int(r.group(4)), hits=int(r.group(5)))
    elif (r := MEAS.search(line)):
        cur.update(moved=int(r.group(1)), pushed=int(r.group(2)), first=int(r.group(3)), mana=int(r.group(5)), stamina=int(r.group(6)),
                   statuses=tuple(sorted(x for x in r.group(7).split(',') if x)))
    elif (r := SKILL.search(line)) and r.group(1) == cur['caster']:
        cur['skills'].append(r.group(2))
    elif (r := REMOTE.search(line)):
        cur['remotes'][r.group(1)] += 1
    elif (r := CAST.search(line)) and r.group(1) == cur['caster']:
        cur['casts'].append(r.group(3))

pairs = collections.OrderedDict()
for t in tests:
    key = (t['master'], t['slot'], t['skill'], t['test'])
    side = {0: 'bot', 1: 'human', 2: 'enemy'}.get(t['turn'], 'bot') if 'turn' in t else ('human' if t['caster'] == 'alexis' else 'bot')
    pairs.setdefault(key, {})[side] = t


def tol(a, b, abs_tol, rel):
    return abs(a - b) <= max(abs_tol, max(abs(a), abs(b)) * rel)


issues = 0
def sides(p):
    if 'human' in p:
        yield 'human', p.get('bot'), p['human']
    if 'enemy' in p:
        yield 'enemy', p.get('bot'), p['enemy']


flat = collections.OrderedDict()
for key, p in pairs.items():
    if 'human' not in p and 'enemy' not in p:
        flat[(key, 'human')] = (p.get('bot'), None)
    for name, b, h in sides(p):
        flat[(key, name)] = (b, h)

for (key, side), (b, h) in flat.items():
    label = f'{key[0]:>2} {key[1]:8} {key[2]:>9} {key[3]:16} {side:5}'
    if not b or not h:
        print(f'-- {label} only {"bot" if b else "human"}')
        continue
    diffs = []
    if side == 'human' and key[3] == 'skip_beyond_flat':
        print(f'ok {label}  target off screen for a human (bench only)')
        continue
    lo, hi = LIMITS.get(key[2], (0, 0))
    if side == 'human' and not h['skills'] and key[1] not in ('WEAPON', 'TAG') and ((lo > 0 and h['dist'] < lo) or (hi > 0 and h['dist'] > hi)):
        print(f'ok {label}  client refused outside [{lo:.0f}, {hi:.0f}] (bench bot casts anyway)')
        continue
    if side == 'human' and not h['skills'] and key[1] not in ('WEAPON', 'TAG'):
        diffs.append('human cast not received')
    if b.get('hit') != h.get('hit'):
        diffs.append(f'hit bot={b.get("hit")} human={h.get("hit")}')
    if b['crit'] or h['crit']:
        if not tol(b['base'], h['base'], 30, 0.15):
            diffs.append(f'damage before crit bot={b["base"]} human={h["base"]}')
    elif not tol(b.get('damage', 0), h.get('damage', 0), 30, 0.15):
        diffs.append(f'damage bot={b.get("damage")} human={h.get("damage")}')
    if b['elements'] and h['elements'] and b['elements'] != h['elements']:
        diffs.append(f'element bot={sorted(b["elements"])} human={sorted(h["elements"])}')
    if b.get('hits') != h.get('hits'):
        diffs.append(f'hits bot={b.get("hits")} human={h.get("hits")}')
    if not tol(b.get('moved', 0), h.get('moved', 0), 80, 0.2):
        diffs.append(f'casterMoved bot={b.get("moved")} human={h.get("moved")}')
    feared = any(STYPE.get(x) == 'STATUS_TYPE_FEAR' for x in tuple(b.get('statuses', ())) + tuple(h.get('statuses', ())))
    if not feared and not tol(b.get('pushed', 0), h.get('pushed', 0), 80, 0.25):
        diffs.append(f'dummyMoved bot={b.get("pushed")} human={h.get("pushed")}')
    if b.get('statuses') != h.get('statuses'):
        diffs.append(f'statuses bot={",".join(b.get("statuses", ()))} human={",".join(h.get("statuses", ()))}')
    if 'release' not in h['casts'] and b.get('first', -1) >= 0 and h.get('first', -1) >= 0 and not tol(b['first'], h['first'], 250, 0.3):
        diffs.append(f'firstHitMs bot={b["first"]} human={h["first"]}')
    if b.get('mana', 0) != 0 and not tol(b.get('mana', 0), h.get('mana', 0), 5, 0.1):
        diffs.append(f'mana bot={b.get("mana")} human={h.get("mana")}')
    if set(b['remotes']) != set(h['remotes']):
        diffs.append(f'remotes bot-only={sorted(set(b["remotes"]) - set(h["remotes"]))} human-only={sorted(set(h["remotes"]) - set(b["remotes"]))}')
    if b['casts'] != h['casts']:
        diffs.append(f'cast flow bot={b["casts"]} human={h["casts"]}')
    issues += 1 if diffs else 0
    print(('!! ' if diffs else 'OK ') + label + ('  ' + ' | '.join(diffs) if diffs else ''))
print(f'== {len(flat)} comparisons, {issues} with differences')
