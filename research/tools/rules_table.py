"""Dump the component rules table (88 records) from pinball.exe: name, handler address, per-state values (points).
Table base = &DAT_01023ef8 - 8 (FUN_0100c01d). Read-only access to the exe."""
import os
GAME=os.environ.get("PINBALL_DIR","Pinball")   # folder containing your own copy of the game files
import struct,json,sys
d=open(GAME+"/pinball.exe","rb").read()
pe=struct.unpack_from("<I",d,0x3c)[0]; nsec=struct.unpack_from("<H",d,pe+6)[0]; opt=struct.unpack_from("<H",d,pe+20)[0]; base=struct.unpack_from("<I",d,pe+24+28)[0]
secs=[]; off=pe+24+opt
for i in range(nsec):
    n,vs,va,rs,ro=struct.unpack_from("<8sIIII",d,off+40*i); secs.append((va,max(vs,rs),ro))
def rd(va,n):
    rva=va-base
    for sva,sz,ro in secs:
        if sva<=rva<sva+sz: return d[ro+rva-sva:ro+rva-sva+n]
def cstr(va):
    b=rd(va,64); return b.split(b'\0')[0].decode('latin1')
out=[]
for i in range(0x58):
    namep,handler,count,valp=struct.unpack("<IIiI",rd(0x1023ef0+16*i,16))
    vals=list(struct.unpack("<%di"%count,rd(valp,4*count))) if 0<count<64 and valp else []
    nm=cstr(struct.unpack('<I',rd(namep,4))[0]) if namep else None
    out.append(dict(i=i,name=nm,handler=hex(handler),count=count,values=vals))
json.dump(out,open("rules_table.json","w"),indent=0)
for o in out: print(o['i'],o['name'],o['handler'],o['count'],o['values'])
