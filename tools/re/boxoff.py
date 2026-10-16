import xml.etree.ElementTree as ET, collections
C='D:/MxM/GameServer/gamedata/client/'
r=ET.parse(C+'REMOTE_PC.xml').getroot()
rows=[]
for e in r:
    b=e.find('RemoteBoundComData2'); c=e.find('RemoteComData2')
    if b is None or c is None or b.get('_Type')!='E_BOUND_BOX' or c.get('_AttackMultiplier') in (None,'0'): continue
    ly=float(b.get('_LengthY') or 0); oy=c.get('_OffsetY') or e.find('.//*[@_OffsetY]') is not None and e.find('.//*[@_OffsetY]').get('_OffsetY')
    rows.append((e.get('ID'),b.get('_LengthX'),ly,oy,c.get('_BehaviorType','').replace('E_REMOTE_BEHAVIOR_TYPE_','')))
cnt=collections.Counter()
for id_,lx,ly,oy,bt in rows:
    try: o=float(oy)
    except: o=None
    k='no offset' if not o else ('offset~LY/2' if abs(o-ly/2)<=ly*0.15 else ('offset~LY' if abs(o-ly)<=ly*0.15 else 'other'))
    cnt[(bt,k)]+=1
for k,v in sorted(cnt.items(),key=lambda x:-x[1])[:14]: print(v,k)
print([x for x in rows if x[4]=='NONETARGET'][:8])
