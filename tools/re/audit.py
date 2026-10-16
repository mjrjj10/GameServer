"""Per-master skill table: skill name/desc -> remote tree with gameplay attributes."""
import re, sys, collections
import xml.etree.ElementTree as ET
GD = r'D:\MxM\GameServer\gamedata'
def load(p): return ET.parse(p).getroot()
skills = load(GD + r'\client\SKILL.xml')
remotes = {int(i.get('ID')): i for i in load(GD + r'\client\REMOTE_PC.xml')}
statuses = {int(i.get('ID')): i for i in load(GD + r'\client\STATUS_PC.xml')}
strings = {}
for f in ('skilldesc.xml', 'StatusPcdesc.xml'):
    for s in load(GD + '\\client\\string\\' + f).iter('StringData'): strings[s.get('DATAKEY')] = s.get('data')
actions = collections.defaultdict(list)
for a in load(GD + r'\ActionBase.xml'):
    actions[(a.get('Class').upper(), a.get('BehaviorState'))].append(a)

KEEP = ['_BehaviorType','_ActivateCount','_PenetrationCount','_AttackMultiplier','_LifeTime','_DirectExplosion','_Extensive','_ActivateGround','_DestroyWhenHitTarget','_StatusTypeToMultiplyDamage','_StatusTypeMultiplier','_HitDamageMultiplier','_MaxHitDamageMultiplier','_DamageDistStart','_DamageOnBound','_MultiHitTimeOut','_DistPenaltyRate','_PenaltyApplyDistance','_OnHitOwnerStatusIndex','_CanReflect']
def stname(sid):
    s = statuses.get(sid)
    if s is None: return str(sid)
    return s.get('KEYNAME', str(sid)).replace('STATUS_', '')
def remote_line(rid, depth, seen):
    r = remotes.get(rid)
    pad = '    ' * depth
    if r is None: return [pad + f'R{rid} (not in REMOTE_PC)']
    if rid in seen: return [pad + f'R{rid} (loop)']
    seen = seen | {rid}
    c = r.find('RemoteComData2'); b = r.find('RemoteBoundComData2'); st = r.find('SteeringBehaviorsComData')
    attrs = ' '.join(f"{k[1:]}={c.get(k)}" for k in KEEP if c is not None and c.get(k) not in (None, '0', 'false', 'False'))
    bound = f"{b.get('_Type','').replace('E_BOUND_','')}{'/'+b.get('_DamageGroup') if b.get('_DamageGroup') else ''}" if b is not None else ''
    for k in ('_LengthX','_LengthY','_Radius','_Angle'):
        if b is not None and b.get(k): bound += f" {k[1:]}={b.get(k)}"
    spd = f" speed={st.get('_MaxSpeed')} dist={st.get('_MaxDistance')}" if st is not None else ''
    out = [pad + f"R{rid} {r.get('KEYNAME','')} [{bound}]{spd} {attrs}"]
    for tag in ('_Status', '_StatusFriendly'):
        for s in r.iter(tag): out.append(pad + f"   {tag[1:]} {stname(int(s.get('DATAKEY')) if s.get('DATAKEY','').isdigit() else 0) if s.get('DATAKEY','').isdigit() else s.get('DATAKEY')} rate={s.get('_Rate')} val={s.get('_Value')}")
    sysd = r.find('RemoteSystemComData')
    if sysd is not None:
        for ev in sysd:
            for act in ev.iter():
                ri = act.get('_RemoteIndex')
                if act is ev: continue
                extra = ' '.join(f'{k}={v}' for k, v in act.attrib.items() if k != '_RemoteIndex')
                if ri and ri.isdigit():
                    out.append(pad + f"   on{ev.tag} {act.tag} {extra}")
                    out += remote_line(int(ri), depth + 1, seen)
                elif act.tag not in ('_Radiate','_Default'):
                    out.append(pad + f"   on{ev.tag} {act.tag} {extra}")
    return out

cls_filter = sys.argv[1].upper() if len(sys.argv) > 1 else None
by_class = collections.defaultdict(list)
for info in skills:
    sk = info[0]
    rc = sk.get('_RequireClass')
    if not rc: continue
    by_class[rc.replace('CLASS_TYPE_', '')].append((int(info.get('ID')), sk))
for cls in sorted(by_class):
    if cls_filter and cls != cls_filter: continue
    print(f"\n######## {cls}")
    for sid, sk in by_class[cls]:
        name = strings.get(sk.get('_Name'), sk.get('_Name'))
        desc = strings.get(sk.get('_Description'), '') or ''
        print(f"\n== {sid} {name} [{sk.get('_Type','').replace('SKILL_TYPE_','')} {sk.get('_Action')} cd={sk.get('_CoolTime')}]")
        print('   ' + re.sub(r'\s+', ' ', desc)[:400])
        for a in actions.get((cls, sk.get('_Action')), []):
            ct = a.get('CommandType')
            if ct == 'REMOTE' and a.get('RemoteIndex'):
                print(f"  anim{a.get('AnimationIndex')} {a.get('TargetPreset')}")
                print('\n'.join(remote_line(int(a.get('RemoteIndex')), 1, set())))
            elif ct:
                print(f"  anim{a.get('AnimationIndex')} {ct} " + ' '.join(f'{k}={v}' for k, v in a.attrib.items() if k not in ('Class','BehaviorState','AnimationIndex','AnimationId','CommandType')))
