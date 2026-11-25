"""Map the static DAT_0102xxxx pointers used by the rule handlers to component names.
Each is the cache field (descriptor+4) of a {char* name, cache} descriptor in the exe's data section.
usage: PINBALL_DIR=<game folder> python3 -I dat_names.py [hexaddr ...]"""
import os,struct,sys,json
GAME=os.environ.get("PINBALL_DIR","Pinball")
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
    b=rd(va,48)
    if not b: return None
    s=b.split(b'\0')[0]
    if len(s)<3 or not all(32<=c<127 for c in s): return None
    return s.decode()
names={}
for va in range(0x1023600,0x1024500,4):
    b=rd(va,8)
    if not b: continue
    p,c=struct.unpack("<II",b)
    if 0x1000000<p<0x1030000 and c==0:
        s=cstr(p)
        if s and (s[0].isalpha() or s[0]=='_') and len(s)<40: names[va+4]=s      # DAT address = descriptor + 4
if __name__=="__main__":
    if len(sys.argv)>1:
        for a in sys.argv[1:]: print(a, names.get(int(a,16),'?'))
    else:
        json.dump({hex(k):v for k,v in names.items()},open("dat_names.json","w"),indent=0); print(len(names),"names")
