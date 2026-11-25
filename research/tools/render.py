import os
GAME=os.environ.get("PINBALL_DIR","Pinball")   # folder containing your own copy of the game files
import partout, struct, zlib, sys
def png(path,w,h,rgb):
    raw=b''.join(b'\0'+rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def ch(t,d): c=struct.pack(">I",len(d))+t+d; return c+struct.pack(">I",zlib.crc32(t+d))
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw))+ch(b'IEND',b''))
def bmp8_to_rgb(b,pal,flip=True):
    w,h=b['w'],b['h']; stride=(w+3)&~3; d=b['data']; out=bytearray()
    rows=range(h-1,-1,-1) if flip else range(h)
    for y in rows:
        for x in range(w):
            i=d[y*stride+x]; B,G,R=pal[i*4],pal[i*4+1],pal[i*4+2]; out+=bytes((R,G,B))
    return bytes(out)
if __name__=="__main__":
    r=partout.parse(GAME+"/PINBALL.DAT")
    g=r['groups'][2]; bm=g[0][1]; pal=g[2][1]
    print(bm['w'],bm['h'],bm['flags'],len(pal),pal[:16].hex())
    png("bg.png",bm['w'],bm['h'],bmp8_to_rgb(bm,pal))
