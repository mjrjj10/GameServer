import xml.etree.ElementTree as ET, re, collections
GD = 'D:/MxM/GameServer/gamedata/client/'
st = {int(i.get('ID')): i for i in ET.parse(GD + 'STATUS_PC.xml').getroot()}
strings = {}
for f in ('skilldesc.xml', 'StatusPcdesc.xml'):
    for s in ET.parse(GD + 'string/' + f).getroot().iter('StringData'): strings[s.get('DATAKEY')] = s.get('data')
types = collections.Counter()
for info in ET.parse(GD + 'SKILL.xml').getroot():
    sk = info[0]
    if sk.get('_Type') != 'SKILL_TYPE_PASSIVE': continue
    sid = sk.get('_Status')
    s = st.get(int(sid)) if sid else None
    c = s.find('ST_COMMONSTATUS') if s is not None else None
    t = c.get('_Type') if c is not None else '-'
    types[t] += 1
    attrs = ' '.join(f'{k[1:]}={v}' for k, v in (c.attrib.items() if c is not None else []) if k not in ('_Description','_Name','_CanDispel','_EffectType','_Passive','_ReAction','_SaveDB','_SkillIndex','_Type','_IconAlias','_Tooltip','_EffectKeyName','_EffectSlot'))
    kids = ' '.join(f'<{e.tag} ' + ' '.join(f'{k}={v}' for k, v in e.attrib.items()) + '>' for e in (c if c is not None else []))
    print(f"{sk.get('_RequireClass','')[11:]:12} {info.get('ID')} {strings.get(sk.get('_Name'),'')!s:28} {s.get('KEYNAME','') if s is not None else '-'} [{t}] {attrs} {kids}")
    print('      ' + re.sub(r'\s+', ' ', strings.get(sk.get('_Description'), '') or '')[:300])
print(types)
