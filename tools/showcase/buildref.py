import collections, json, os, re, sys, xml.etree.ElementTree as ET
import cv2, numpy as np

GD = 'D:/MxM/GameServer/gamedata/'
C = GD + 'client/'
DATA = 'D:/MxM/showcase_data/'
OUT = DATA + 'reference/'
STEP = 0.2

remotes = {e.get('ID'): e for e in ET.parse(C + 'REMOTE_PC.xml').getroot()}
statuses = {e.get('ID'): e for e in ET.parse(C + 'STATUS_PC.xml').getroot()}
skills = {i.get('ID'): i[0] for i in ET.parse(C + 'SKILL.xml').getroot()}
strings = {}
for f in ('skilldesc.xml', 'string.xml'):
    for s in ET.parse(C + 'string/' + f).getroot().iter('StringData'):
        strings.setdefault(s.get('DATAKEY'), s.get('data'))
actions = collections.defaultdict(list)
for a in ET.parse(GD + 'ActionBase.xml').getroot():
    actions[(a.get('Class', '').upper(), a.get('BehaviorState'))].append(a)
tt = {}
for master in ET.parse(C + 'skilltt.xml').getroot():
    for sk in master.findall('data'):
        base = sk.find('data')
        if base is None:
            continue
        for td in base.findall('tableDatas'):
            tt.setdefault(sk.get('DATAKEY'), {})[td.get('attrType')] = td.get('value_1')


def clean(t):
    return re.sub(r'\s+', ' ', re.sub(r'\{color:[^}]*\}|\{color:end\}', '', t or '').replace('&#10;', ' ')).strip()


def status_line(sid):
    s = statuses.get(sid)
    if s is None:
        return f'{sid} (?)'
    c = s.find('ST_COMMONSTATUS')
    return f'{sid} {s.get("KEYNAME", "")} {c.get("_Type", "").replace("STATUS_TYPE_", "")} {c.get("_DurationTime", "")}ms'


def remote_lines(rid, depth=0, seen=None):
    seen = seen if seen is not None else set()
    e = remotes.get(rid)
    if e is None or rid in seen or depth > 6:
        return []
    seen.add(rid)
    c = e.find('RemoteComData2')
    b = e.find('RemoteBoundComData2')
    st = e.find('SteeringBehaviorsComData')
    get = lambda x, k: (x.get(k) if x is not None else None)
    parts = [rid, e.get('KEYNAME', ''), (get(c, '_BehaviorType') or '').replace('E_REMOTE_BEHAVIOR_TYPE_', ''),
             (get(b, '_Type') or '').replace('E_BOUND_', ''), f'X={get(b, "_LengthX")} Y={get(b, "_LengthY")}',
             f'offY={get(c, "_OffsetY")}', f'group={get(b, "_DamageGroup")}', f'atk={get(c, "_AttackMultiplier")}',
             f'life={get(c, "_LifeTime")}', f'speed={get(st, "_MaxSpeed")}', f'maxDist={get(st, "_MaxDistance")}',
             f'attach={get(c, "_AttachFireObject")}', f'ignoreGround={get(b, "_IgnoreGround")}']
    out = ['  ' * depth + '- ' + ' '.join(p for p in parts if p and not p.endswith('=None'))]
    if c is not None:
        for s in c.findall('_Status'):
            out.append('  ' * depth + '  - enemy status ' + status_line(s.get('DATAKEY')))
        for s in c.findall('_StatusFriendly'):
            out.append('  ' * depth + '  - ally status ' + status_line(s.get('DATAKEY')))
    sysd = e.find('RemoteSystemComData')
    if sysd is not None:
        for x in sysd.iter():
            ri = x.get('_RemoteIndex')
            if ri and ri.isdigit():
                out.append('  ' * (depth + 1) + f'({x.tag})')
                out += remote_lines(ri, depth + 1, seen)
            if x.tag == '_ChangeCasterActionState':
                out.append('  ' * (depth + 1) + f'(on hit: caster action {x.get("_ActionState")})')
    return out


def extract_frames(video, folder):
    cap = cv2.VideoCapture(video)
    fps = cap.get(cv2.CAP_PROP_FPS) or 30
    n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    thumbs = []
    t = 0.0
    while t * fps < n:
        cap.set(cv2.CAP_PROP_POS_FRAMES, int(t * fps))
        ok, fr = cap.read()
        if not ok:
            break
        cv2.imwrite(os.path.join(folder, f'frame_{t:05.1f}s.jpg'), fr, [cv2.IMWRITE_JPEG_QUALITY, 85])
        th = cv2.resize(fr, (480, 270))
        cv2.putText(th, f'{t:.1f}s', (6, 26), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        thumbs.append(th)
        t += STEP
    cols = 6
    while len(thumbs) % cols:
        thumbs.append(np.zeros((270, 480, 3), np.uint8))
    if thumbs:
        rows = [np.hstack(thumbs[i:i + cols]) for i in range(0, len(thumbs), cols)]
        cv2.imwrite(os.path.join(folder, 'sheet.jpg'), np.vstack(rows), [cv2.IMWRITE_JPEG_QUALITY, 70])
    return n / fps if fps else 0


only = set(sys.argv[1:])
mapping = {sk: (n, y) for sk, n, y in json.load(open(DATA + 'skill_to_youtube.json'))}
for sid, sk in skills.items():
    cls = (sk.get('_RequireClass') or '').replace('CLASS_TYPE_', '')
    if not cls or sk.get('_Type') not in ('SKILL_TYPE_NORMAL', 'SKILL_TYPE_PASSIVE', 'SKILL_TYPE_SHIRK', 'SKILL_TYPE_COMBO'):
        continue
    if only and cls not in only and sid not in only:
        continue
    num = int(sid[3:5]) if sid.startswith('180') else 0
    if num == 0:
        continue
    folder = os.path.join(OUT, f'{num:02d}_{cls}', sid)
    os.makedirs(folder, exist_ok=True)
    video = DATA + f'previews/{sid}.mp4'
    length = extract_frames(video, folder) if os.path.exists(video) and not os.path.exists(os.path.join(folder, 'sheet.jpg')) else None
    notes = os.path.join(folder, 'notes.md')
    observations = ''
    if os.path.exists(notes):
        old = open(notes, encoding='utf-8').read()
        i = old.find('## Observations from the preview')
        observations = old[i:] if i >= 0 else ''
    lines = [f'# {sid} {clean(strings.get(sk.get("_Name"), ""))} ({cls}, {sk.get("_Type", "").replace("SKILL_TYPE_", "")})', '',
             clean(strings.get(sk.get('_Description'), '')), '',
             f'- action: {sk.get("_Action")}  cooldown: {sk.get("_CoolTime")}s  mp: {sk.get("_ConsumeMP")}  stamina: {sk.get("_ConsumeStamina")}',
             f'- range indicator: {sk.get("_SkillRangeSFX")} X={sk.get("_SkillRangeLengthX")} Y={sk.get("_SkillRangeLengthY")}  target zone X={sk.get("_TargetRangeLengthX")}',
             f'- base damage (skilltt): {tt.get(sid, {}).get("TTATTR_REMOTE_DAMAGE")}',
             f'- preview: {"youtube " + mapping[sid][1] if sid in mapping else "none"}' + (f', {length:.1f}s, frames every {STEP}s in this folder, contact sheet sheet.jpg' if length else ''),
             '', '## Action commands', '']
    for a in actions.get((cls, sk.get('_Action')), []):
        attrs = {k: v for k, v in a.attrib.items() if k not in ('Class', 'BehaviorState', 'AnimationId')}
        lines.append('- ' + ' '.join(f'{k}={v}' for k, v in attrs.items()))
        if a.get('CommandType') in ('REMOTE', 'REMOTE_SKILL_TARGET') and a.get('RemoteIndex'):
            lines += ['  ' + l for l in remote_lines(a.get('RemoteIndex'))]
        if a.get('CommandType') == 'STATUS' and a.get('StatusIndex'):
            lines.append('  - ' + status_line(a.get('StatusIndex')))
    if sk.get('_Status'):
        lines += ['', '## Passive status', '', '- ' + status_line(sk.get('_Status'))]
    lines += ['', observations if observations else '## Observations from the preview\n\n- origin:\n- direction:\n- shape and size:\n- sequence and timing:\n- caster movement:\n- target reaction:\n']
    open(notes, 'w', encoding='utf-8').write('\n'.join(lines))
print('done')
