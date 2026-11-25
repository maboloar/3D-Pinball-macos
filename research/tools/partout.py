"""PARTOUT(4.0)RESOURCE reader. Format derived from the loader at 0x10064e2 in pinball.exe (REA/Ghidra)."""
import struct, sys, collections
# int16 per entry type, from pinball.exe table at 0x10235d0 (types 0..13 are real). -1 => u32 length prefix.
SIZES=[2,-1,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0]
def parse(path):
    d=open(path,'rb').read()
    assert d[:20]==b"PARTOUT(4.0)RESOURCE"
    ngroups,=struct.unpack_from("<H",d,175); extra,=struct.unpack_from("<H",d,181)
    pos=183+extra; groups=[]
    for g in range(ngroups):
        n=d[pos]; pos+=1; ents=[]
        for _ in range(n):
            t=d[pos]; pos+=1
            L=SIZES[t]
            if L<0: L,=struct.unpack_from("<I",d,pos); pos+=4
            if t==1:   # bitmap: 14-byte header then pixel data
                res,w,h,x,y,sz,fl=struct.unpack_from("<BhhhhIB",d,pos)
                ents.append((t,dict(res=res,w=w,h=h,x=x,y=y,flags=fl,data=d[pos+14:pos+14+sz])))
                pos+=14+sz
            else:
                ents.append((t,d[pos:pos+L])); pos+=L
        groups.append(ents)
    return dict(ngroups=ngroups,end=pos,filelen=len(d),groups=groups)
if __name__=="__main__":
    r=parse(sys.argv[1]); print({k:r[k] for k in('ngroups','end','filelen')})
    print("entry types:",dict(collections.Counter(t for g in r['groups'] for t,_ in g)))
