import xml.etree.ElementTree as ET, re, collections, sys
GD = 'D:/MxM/GameServer/gamedata/'
C = GD + 'client/'
NAMES = {
    'STRIKER': 'Taejin', 'ARTILLERY': 'MBA-07', 'ASSASSIN': 'Sizuka', 'ELECTRO': 'Demenos', 'DEFENDER': 'Koom',
    'SNIPER': 'Innowin', 'DEATHKNIGHT': 'Death Knight', 'DESTROYER': 'Cagnazzo', 'MECHANIC': 'Moro', 'SOULMASTER': 'Arami',
    'DOUBLEGUN': 'Vonak', 'JINSEOYEON': 'Jinsoyun', 'KROMEDE': 'Kromede', 'RODMASTER': 'Jingtai', 'PHOTOG': 'Lorraine',
    'ICEQUEEN': 'Eirna', 'MAGICGIRL': 'Vita', 'POHWARAN': 'Poharan', 'BOOMERANG': 'V-Merang', 'SLIME': 'Bumaro',
    'MONDOZAX': 'Mondo Zax', 'LILU': 'Lilu', 'EFREET': 'Ignuma', 'SHADOWHUNTER': 'Dr. Raoul', 'RYTLOCK': 'Rytlock',
    'CATTHECAT': 'Kat the Cat', 'RNB': 'R&B', 'BATTER': 'Sonid', 'ANDROA': 'Merope', 'ANDROB': 'Maia',
    'ATTACKER': 'Tulam', 'BOXER': 'Pancuga', 'SHUGOTRADER': 'Nanurunerk', 'NAGA': 'Nedien', 'LAUNCHER': 'Lua',
    'STATESMAN': 'Statesman', 'YURI': 'Yuri (name unverified)', 'ESPER': 'Xero', 'PRIEST': 'Titus', 'GHOSTWIDOW': 'Ghost Widow',
}
IMPLEMENTED_PASSIVES = {'STATUS_TYPE_SKILLSTATUS', 'STATUS_TYPE_HIT_PLAGUE', 'STATUS_TYPE_KROMEDE', 'STATUS_TYPE_MANABURN',
                        'STATUS_TYPE_GHOSTWIDOW', 'STATUS_TYPE_ICEQUEEN', 'STATUS_TYPE_FLAG'}
HANDLED_STATUS = {'STATUS_TYPE_STUN', 'STATUS_TYPE_PUSH', 'STATUS_TYPE_KNOCKDOWN', 'STATUS_TYPE_AIRBORNE', 'STATUS_TYPE_FEAR',
                  'STATUS_TYPE_FORCEBACK', 'STATUS_TYPE_DRAGGED', 'STATUS_TYPE_SHOCKED', 'STATUS_TYPE_ACTIONBLOCK', 'STATUS_TYPE_FLY',
                  'STATUS_TYPE_ROOT', 'STATUS_TYPE_SLOW', 'STATUS_TYPE_STAT', 'STATUS_TYPE_HEAL', 'STATUS_TYPE_SHIELD',
                  'STATUS_TYPE_SUPERARMOR', 'STATUS_TYPE_EVADE', 'STATUS_TYPE_GODMODE', 'STATUS_TYPE_NEARLY_IMMORTAL',
                  'STATUS_TYPE_DISPEL', 'STATUS_TYPE_AURA', 'STATUS_TYPE_POISON', 'STATUS_TYPE_BURNING', 'STATUS_TYPE_BLEEDING',
                  'STATUS_TYPE_FLAG', 'STATUS_TYPE_NOCOST', 'STATUS_TYPE_SUICIDE'}

def load(p): return ET.parse(p).getroot()
skills = load(C + 'SKILL.xml')
remotes = {int(i.get('ID')): i for i in load(C + 'REMOTE_PC.xml')}
statuses = {int(i.get('ID')): i for i in load(C + 'STATUS_PC.xml')}
strings = {}
for f in ('skilldesc.xml',):
    for s in load(C + 'string/' + f).iter('StringData'): strings[s.get('DATAKEY')] = s.get('data')
actions = collections.defaultdict(list)
for a in load(GD + 'ActionBase.xml'):
    actions[(a.get('Class').upper(), a.get('BehaviorState'))].append(a)
tt = {}
for master in load(C + 'skilltt.xml'):
    for sk in master.findall('data'):
        base = sk.find('data')
        if base is None: continue
        for td in base.findall('tableDatas'):
            if td.get('attrType') == 'TTATTR_REMOTE_DAMAGE': tt[sk.get('DATAKEY')] = td.get('value_1')

def clean(t):
    t = re.sub(r'\{color:[^}]*\}|\{color:end\}', '', t or '')
    return re.sub(r'\s+', ' ', t.replace('&#10;', ' ')).strip()

def status_desc(sid):
    s = statuses.get(sid)
    if s is None: return f'{sid}(?)'
    c = s.find('ST_COMMONSTATUS')
    t = c.get('_Type', '').replace('STATUS_TYPE_', '')
    d = c.get('_DurationTime')
    flag = '' if c.get('_Type') in HANDLED_STATUS else ' **[type non géré]**'
    return f"{t}{' ' + str(int(float(d))/1000) + 's' if d and float(d) > 0 else ''}{flag}"

def remote_summary(rid, depth=0, seen=None):
    seen = seen or set()
    r = remotes.get(rid)
    if r is None or rid in seen: return []
    seen.add(rid)
    c = r.find('RemoteComData2'); b = r.find('RemoteBoundComData2'); st = r.find('SteeringBehaviorsComData')
    beh = (c.get('_BehaviorType') or '').replace('E_REMOTE_BEHAVIOR_TYPE_', '')
    bound = (b.get('_Type') or '').replace('E_BOUND_', '') if b is not None else ''
    size = ' '.join(f"{k[1:]}={b.get(k)}" for k in ('_LengthX', '_LengthY') if b is not None and b.get(k))
    rng = ''
    if st is not None and st.get('_MaxDistance'): rng = f" portée={st.get('_MaxDistance')}"
    elif st is not None and st.get('_MaxSpeed') and c.get('_LifeTime'):
        try: rng = f" portée≈{float(st.get('_MaxSpeed'))*float(c.get('_LifeTime')):.0f}"
        except: pass
    dmg = 'dégâts' if c.get('_AttackMultiplier') not in (None, '0') else 'sans dégâts'
    ac = c.get('_ActivateCount')
    pierce = 's’arrête au 1er' if ac == '1' else ('traverse' if ac and int(ac) > 1 else '')
    st_list = [status_desc(int(s.get('DATAKEY'))) for s in r.iter('_Status') if s.get('DATAKEY', '').isdigit()]
    fr_list = [status_desc(int(s.get('DATAKEY'))) for s in r.iter('_StatusFriendly') if s.get('DATAKEY', '').isdigit()]
    if c.get('_AttackMultiplier') in (None, '0') and not st_list and not fr_list and bound in ('NONE', ''):
        line = None
    else:
        line = f"{'  ' * depth}- `{rid}` {beh} {bound} {size}{rng}, {dmg}{', ' + pierce if pierce else ''}" + \
               (f", statuts: {', '.join(st_list)}" if st_list else '') + (f", alliés: {', '.join(fr_list)}" if fr_list else '')
    out = [line] if line else []
    sysd = r.find('RemoteSystemComData')
    if sysd is not None:
        for act in sysd.iter():
            ri = act.get('_RemoteIndex')
            if ri and ri.isdigit(): out += remote_summary(int(ri), depth + (1 if line else 0), seen)
    return out

guide = open(GD + 'reference/master_guides.md', encoding='utf-8').read()
def guide_block(name):
    m = re.search(r'^## ' + re.escape(name.split(' (')[0]) + r'\b.*?(?=^## |\Z)', guide, re.M | re.S)
    return m.group(0).strip() if m else None

by_class = collections.defaultdict(list)
for info in skills:
    sk = info[0]
    rc = sk.get('_RequireClass')
    if rc: by_class[rc.replace('CLASS_TYPE_', '')].append((int(info.get('ID')), sk))

out = ['# Plan de test des masters', '',
       'Généré depuis les données du client (SKILL, ActionBase, REMOTE_PC, STATUS_PC, skilltt) et les guides.',
       '`[type non géré]` = type de statut que le serveur n’applique pas encore. Cases à cocher pour le test final.', '']
for cls in NAMES:
    lst = by_class.get(cls, [])
    out.append(f'## {NAMES[cls]} ({cls})')
    out.append('')
    def order(e):
        sk = e[1]; t = sk.get('_Type', ''); a = sk.get('_Action') or ''
        if t == 'SKILL_TYPE_PASSIVE': return 0
        if t == 'SKILL_TYPE_SHIRK': return 9
        m = re.search(r'SKILL_(\d+)_', a)
        return int(m.group(1)) if m else 8
    for sid, sk in sorted(lst, key=order):
        t = sk.get('_Type', '').replace('SKILL_TYPE_', '')
        if t not in ('PASSIVE', 'NORMAL', 'SHIRK'): continue
        name = strings.get(sk.get('_Name'), sk.get('_Name'))
        if '[Not Shown]' in (name or ''): continue
        action = sk.get('_Action') or ''
        slot = {'PASSIVE': 'Passif', 'SHIRK': 'Esquive'}.get(t) or action.replace('_BEHAVIORSTATE', '').replace('SKILL_', 'Sort ')
        if action == 'SKILL_5_BEHAVIORSTATE': slot = 'Ultime'
        meta = []
        if sk.get('_CoolTime'): meta.append(f"recharge {sk.get('_CoolTime')}s")
        if tt.get(str(sid)) not in (None, '0'): meta.append(f"dégâts de base {tt[str(sid)]}")
        out.append(f"- [ ] **{slot} — {name}** {'(' + ', '.join(meta) + ')' if meta else ''}")
        out.append(f"  - {clean(strings.get(sk.get('_Description'), ''))[:300]}")
        if t == 'PASSIVE':
            st = sk.get('_Status')
            s = statuses.get(int(st)) if st else None
            ptype = s.find('ST_COMMONSTATUS').get('_Type') if s is not None else None
            ok = ptype in IMPLEMENTED_PASSIVES
            out.append(f"  - serveur : {'géré' if ok else '**non géré**'} ({(ptype or 'aucune donnée').replace('STATUS_TYPE_', '')})")
            continue
        lines = []
        for a in actions.get((cls, action), []):
            if a.get('CommandType') == 'REMOTE' and a.get('RemoteIndex'):
                lines += remote_summary(int(a.get('RemoteIndex')), 2)
            elif a.get('CommandType') == 'STATUS' and a.get('StatusIndex', '').isdigit() and a.get('Param2') != 'Remove':
                lines.append(f"    - statut sur soi/cible : {status_desc(int(a.get('StatusIndex')))}")
            elif a.get('CommandType') in ('ENTITY', 'CALL_CHILD_SKILL', 'MOVE', 'GRAPH_MOVE_HORZ'):
                lines.append(f"    - {a.get('CommandType')} {a.get('EntityIndex') or a.get('Param1') or ''}")
        seen = set()
        for l in lines:
            if l in seen: continue
            seen.add(l); out.append('  ' + l if not l.startswith('    ') else l)
    g = guide_block(NAMES[cls])
    if g:
        out.append('')
        out.append('<details><summary>Guide</summary>')
        out.append('')
        out.append('\n'.join(g.splitlines()[1:]))
        out.append('</details>')
    out.append('')
open(GD + 'reference/test_plan.md', 'w', encoding='utf-8').write('\n'.join(out))
print(len(out), 'lines')
