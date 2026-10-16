import collections, re, sys, xml.etree.ElementTree as ET

GD = 'D:/MxM/GameServer/gamedata/'
C = GD + 'client/'
sys.stdout.reconfigure(encoding='utf-8')

remotes = {e.get('ID'): e for e in ET.parse(C + 'REMOTE_PC.xml').getroot()}
statuses = {e.get('ID'): e for e in ET.parse(C + 'STATUS_PC.xml').getroot()}
skills = {i.get('ID'): i[0] for i in ET.parse(C + 'SKILL.xml').getroot()}
actions = collections.defaultdict(list)
for a in ET.parse(GD + 'ActionBase.xml').getroot():
    if a.get('Stance') not in (None, '', 'STANCE_TYPE_DEFAULT'):
        continue
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


def status_common(sid):
    e = statuses.get(sid)
    return e.find('ST_COMMONSTATUS') if e is not None else None


def is_buff(sid):
    c = status_common(sid)
    return c is not None and c.get('_EffectType') == 'STATUS_EFFECT_BUFF'


def status_skill(sid):
    c = status_common(sid)
    return c.get('_SkillIndex') if c is not None else None


def remote_multiplier(rid):
    e = remotes.get(rid)
    c = e.find('RemoteComData2') if e is not None else None
    try:
        return float(c.get('_AttackMultiplier') or 0) if c is not None else None
    except ValueError:
        return None


def enemy_statuses(rid, depth=0, seen=None):
    seen = seen if seen is not None else set()
    e = remotes.get(rid)
    if e is None or rid in seen or depth > 5:
        return {}
    seen.add(rid)
    out = {}
    b = e.find('RemoteBoundComData2')
    group = b.get('_DamageGroup') if b is not None else ''
    c = e.find('RemoteComData2')
    if c is not None and group in ('eENEMY', 'eALL'):
        for s in c.findall('_Status'):
            sid = s.get('DATAKEY')
            if sid:
                out.setdefault(sid, -1 if group == 'eALL' and (is_buff(sid) or not status_skill(sid)) else depth)
    sysd = e.find('RemoteSystemComData')
    if sysd is not None:
        for x in sysd.iter():
            ri = x.get('_RemoteIndex')
            if ri and ri.isdigit():
                for sid, d in enemy_statuses(ri, depth + 1, seen).items():
                    out.setdefault(sid, d)
    return out


combo_of = {}
for cid, csk in skills.items():
    if csk.get('_Type') == 'SKILL_TYPE_COMBO' and csk.get('_BaseComboSkill'):
        combo_of.setdefault(csk.get('_BaseComboSkill'), cid)

ENEMY_PRESETS = ('TARGET_CURRENT_FFF', 'TARGET_LAST_CHILD_FFF', 'TARGET_CHILD_INDEX_FFF')


def skill_expectations(sid):
    sk = skills.get(sid)
    if sk is None or sk.get('_Type') == 'SKILL_TYPE_SHIRK':
        return None
    cls = (sk.get('_RequireClass') or '').replace('CLASS_TYPE_', '')
    act = list(actions.get((cls, sk.get('_Action')), []))
    family = {sid}
    combo = skills.get(combo_of.get(sid, ''))
    if combo is not None:
        act += actions.get((cls, combo.get('_Action')), [])
        family.add(combo_of[sid])
    exp = {'statuses': {}, 'allowed': set(), 'move': 0.0, 'mp': float(sk.get('_ConsumeMP') or 0), 'damage': base_damage.get(sid, 0.0), 'family': family}
    for a in act:
        t = a.get('CommandType')
        if t == 'MOVE' and any(k in (a.get('Param1') or '') for k in ('TARGET_POS', 'GOTO_CURRENT_TARGET')):
            exp['move'] = 1e9
        if t in ('REMOTE', 'REMOTE_SKILL_TARGET') and a.get('RemoteIndex'):
            for s, d in enemy_statuses(a.get('RemoteIndex')).items():
                if d < 0:
                    exp['allowed'].add(s)
                else:
                    exp['statuses'].setdefault(s, d)
        if a.get('StatusIndex') and (t == 'STATUS_SKILL_TARGET' or (t == 'STATUS' and a.get('TargetPreset') in ENEMY_PRESETS)):
            exp['allowed'].add(a.get('StatusIndex'))
        if t == 'GRAPH_MOVE_HORZ':
            try:
                exp['move'] = max(exp['move'], abs(float(a.get('Param1') or 0)))
            except ValueError:
                pass
    return exp


def classify_extra(sid, exp):
    if sid in exp['allowed']:
        return 'action or eALL'
    owner = status_skill(sid)
    if owner in exp['family']:
        return 'same-skill'
    if owner in skills and skills[owner].get('_Type') == 'SKILL_TYPE_PASSIVE':
        return 'passive'
    return None


tests = []
cur = None
for path in sys.argv[1:]:
    for line in open(path, encoding='utf-8', errors='replace'):
        m = re.search(r'Showcase :: wall=\d+ master=(\d+) slot=(\w+) skill=(\d+) caster=(\S+) range=(\d+) dist=(\d+) test=(\S+) expect=(-?\d)', line)
        if m:
            cur = {'master': int(m.group(1)), 'slot': m.group(2), 'skill': m.group(3), 'caster': m.group(4), 'dist': int(m.group(6)), 'test': m.group(7),
                   'expect': int(m.group(8)), 'hitsLog': [], 'skillDamage': None, 'dummy': None}
            tests.append(cur)
            continue
        if cur is None:
            continue
        sd = re.search(r'Skill :: (\S+) skill=(\d+) .*damage=(\d+)', line)
        if sd and sd.group(1) == cur['caster'] and cur['skillDamage'] is None:
            cur['skillDamage'] = int(sd.group(3))
        dm = re.search(r'Damage :: (\S+) -> (\S+) skill=(-?\d+) damage=(\d+)( CRIT)? hp=\S+ in=(-?\d+) remote=(\d+) element=(\d+)', line)
        if dm and dm.group(1) == cur['caster'] and dm.group(2) != cur['caster']:
            cur['hitsLog'].append({'skill': dm.group(3), 'damage': int(dm.group(4)), 'crit': bool(dm.group(5)), 'in': int(dm.group(6)), 'remote': dm.group(7),
                                   'element': int(dm.group(8))})
        r = re.search(r'Showcase :: result hit=(\d) expect=(-?\d) (\w+) damage=(-?\d+) hits=(\d+)', line)
        if r:
            cur.update(hit=int(r.group(1)), verdict=r.group(3), damage=int(r.group(4)), hits=int(r.group(5)))
        ms = re.search(r'Showcase :: measure casterMoved=(-?[\d.]+) dummyMoved=(-?[\d.]+) firstHitMs=(-?\d+)(?: firstDamage=(-?\d+))? mana=(-?[\d.]+) stamina=(-?[\d.]+) statuses=(\S*)', line)
        if ms:
            num = lambda v: int(float(v)) if v not in (None, '', '.', '-') else 0
            cur.update(moved=num(ms.group(1)), pushed=num(ms.group(2)), first=num(ms.group(3)), firstDamage=num(ms.group(4)), mana=num(ms.group(5)),
                       stamina=num(ms.group(6)), seen=set(x for x in ms.group(7).split(',') if x))


def damage_flags(t):
    flags, notes = [], []
    for h in t['hitsLog']:
        mul = remote_multiplier(h['remote']) if h['remote'] != '0' else None
        if mul is not None and t['skillDamage'] and h['skill'] == t['skill']:
            want = t['skillDamage'] * mul
            if abs(h['in'] - want) > max(2, want * 0.02):
                flags.append(f'remote {h["remote"]} in={h["in"]} vs skill {t["skillDamage"]} x {mul:g}')
        final = h['in'] * h['element'] / 100.0
        if not h['crit'] and abs(h['damage'] - final) > max(2, final * 0.02):
            flags.append(f'hit {h["damage"]} vs in {h["in"]} x element {h["element"]}%')
        if h['crit']:
            notes.append(f'crit {h["damage"]}')
        if h['element'] != 100:
            notes.append(f'element {h["element"]}%')
    return flags, sorted(set(notes))


by_master = collections.OrderedDict()
for t in tests:
    by_master.setdefault(t['master'], []).append(t)
issues_total = 0
for master, lst in by_master.items():
    print(f'## master {master}')
    for t in lst:
        if t['slot'] == 'TAG' or 'verdict' not in t:
            continue
        flags, notes = [], []
        exp = skill_expectations(t['skill']) if t['skill'] != '0' else None
        if t.get('verdict') == 'MISMATCH':
            flags.append('HIT-EXPECTATION')
        if exp and t.get('hit') == 1:
            seen = t.get('seen', set())
            missing = {s: d for s, d in exp['statuses'].items() if s not in seen}
            direct = sorted(s for s, d in missing.items() if d == 0)
            chained = sorted(s for s, d in missing.items() if d > 0)
            if direct:
                flags.append('statuses missing ' + ','.join(direct))
            if chained:
                notes.append('chained statuses not seen ' + ','.join(chained))
            for s in sorted(seen - set(exp['statuses'])):
                kind = classify_extra(s, exp)
                if kind:
                    notes.append(f'{s} ({kind})')
                else:
                    flags.append(f'status {s} from skill {status_skill(s)}')
        if t['hitsLog']:
            f, n = damage_flags(t)
            flags += f
            notes += n
        elif exp and t.get('hit') == 1 and exp['damage'] > 0 and t.get('firstDamage', 0) > 0:
            if abs(t['firstDamage'] - exp['damage']) > exp['damage'] * 0.25:
                flags.append(f'first hit damage {t["firstDamage"]} vs data {exp["damage"]:.0f}')
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
        print(f'  {status} {t["slot"]:8} {t["skill"]:>9} {t["test"]:16} dist={t["dist"]:5} hit={t.get("hit", "?")} dmg={t.get("damage", "?")} hits={t.get("hits", "?")} {info}'
              + ('  !! ' + ' | '.join(flags) if flags else '') + ('  .. ' + ' | '.join(notes) if notes else ''))
print(f'== {issues_total} tests to check')
