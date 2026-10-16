import re, sys, collections
log = sys.argv[1]
NAMES = {1:'Taejin',2:'MBA-07',3:'Sizuka',4:'Demenos',5:'Koom',6:'Innowin',7:'Death Knight',8:'Cagnazzo',9:'Moro',10:'Arami',11:'Vonak',12:'Jinsoyun',13:'Kromede',14:'Jingtai',15:'Lorraine',16:'Eirna',17:'Vita',18:'Poharan',19:'V-Merang',20:'Bumaro',21:'Mondo Zax',22:'Lilu',23:'Ignuma',24:'Dr. Raoul',25:'Rytlock',26:'Kat',27:'R&B',28:'Sonid',29:'Merope',30:'Maia',31:'Tulam',32:'Pancuga',33:'Nanurunerk',34:'Nedien',35:'Lua',36:'Statesman',37:'Yuri',38:'Xero',39:'Titus',40:'Ghost Widow'}
rows = collections.OrderedDict(); last = None
for line in open(log, encoding='utf-8', errors='replace'):
    m = re.search(r'Showcase :: wall=\d+ master=(\d+) slot=(\w+) skill=(\d+) caster=\S+ range=(\d+) dist=(\d+) test=(\S+) expect=(-?\d)', line)
    if m: last = m.groups(); continue
    r = re.search(r'Showcase :: result hit=(\d) expect=(-?\d) (\w+) damage=(-?\d+)(?: hits=(\d+))?(?: dummyZ=(-?\d+) groundZ=(-?\d+))?', line)
    if r and last:
        rows.setdefault(int(last[0]), []).append((last[1], last[2], last[3], last[4], last[5], r.group(1), r.group(3), r.group(4), r.group(6), r.group(7), r.group(5) or '?'))
        last = None
for master, lst in rows.items():
    bad = [x for x in lst if x[6] == 'MISMATCH']
    print(f"## {NAMES.get(master, master)} : {len(lst)} tests, {len(bad)} écarts")
    for slot, skill, rng, dist, test, hit, verdict, dmg, dz, gz, nh in lst:
        flag = '**ÉCART**' if verdict == 'MISMATCH' else verdict
        if dz and gz and abs(int(dz) - int(gz)) > 20: flag += f' **PAS À PLAT (z {dz} vs sol {gz})**'
        print(f"- {slot} {skill} {test} (portée {rng}, distance {dist}) : touché={hit} coups={nh} dégâts={dmg} {flag}")
