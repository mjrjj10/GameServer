import sys, re
BASE=0x400000; CODE_END=0x35d7000
m=open(sys.argv[1],'rb').read()
v=int(sys.argv[2]); lo=v&0xff; hi=(v>>8)&0xff
b2=bytes([lo,hi])
for pat,name in ((b'\x68'+b2+b'\x00\x00','push'),(b'\x66\xc7.{1,2}'+b2,'movw'),(b'[\xb8-\xbf]'+b2+b'\x00\x00','mov r32'),(b'\xc7.{1,2}'+b2+b'\x00\x00','movd'),(b'\x3d'+b2+b'\x00\x00','cmp')):
    hits=[hex(h.start()+BASE) for h in re.finditer(pat,m[:CODE_END],re.S)]
    print(name,len(hits),hits[:20])
