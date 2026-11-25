"""Print vtable entries from pinball.exe (read-only). usage: python3 -I vtable.py 0x1002508 [count]"""
import os
GAME=os.environ.get("PINBALL_DIR","Pinball")   # folder containing your own copy of the game files
import struct,sys
d=open(GAME+"/pinball.exe","rb").read()
pe=struct.unpack_from("<I",d,0x3c)[0]; nsec=struct.unpack_from("<H",d,pe+6)[0]; opt=struct.unpack_from("<H",d,pe+20)[0]; base=struct.unpack_from("<I",d,pe+24+28)[0]
secs=[]; off=pe+24+opt
for i in range(nsec):
    n,vs,va,rs,ro=struct.unpack_from("<8sIIII",d,off+40*i); secs.append((va,max(vs,rs),ro))
def rd(va,n):
    rva=va-base
    for sva,sz,ro in secs:
        if sva<=rva<sva+sz: return d[ro+rva-sva:ro+rva-sva+n]
va=int(sys.argv[1],0); n=int(sys.argv[2]) if len(sys.argv)>2 else 16
print([hex(x) for x in struct.unpack("<%dI"%n,rd(va,4*n))])
