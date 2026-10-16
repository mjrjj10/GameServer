import sys, re, struct
BASE=0x400000
m=open(sys.argv[1],'rb').read()
for name in sys.argv[2:]:
    for enc in ('ascii','utf-16le'):
        w=name.encode(enc)
        for h in re.finditer(re.escape(w)+(b'\x00' if enc=='ascii' else b'\x00\x00'),m):
            a=h.start()+BASE
            # require string start (previous byte(s) zero)
            if enc=='ascii' and m[h.start()-1]!=0: continue
            refs=[hex(x.start()+BASE) for x in re.finditer(re.escape(struct.pack('<I',a)),m)]
            print(name,enc,hex(a),refs[:6])
