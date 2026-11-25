"""Extract the 16/32/48 px 32-bit icons from pinball.exe (RT_ICON ids 9,8,7; file offsets from inspect_pe_resources) and write an .iconset folder.
Read-only on the exe. usage: python3 -I make_icon.py <out_iconset_dir>"""
import os
GAME=os.environ.get("PINBALL_DIR","Pinball")   # folder containing your own copy of the game files
import struct,sys,zlib,os
d=open(GAME+"/pinball.exe","rb").read()
def png(w,h,rgba):
    raw=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))
    def ch(t,x): c=struct.pack(">I",len(x))+t+x; return c+struct.pack(">I",zlib.crc32(t+x))
    return b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack(">IIBBBBB",w,h,8,6,0,0,0))+ch(b'IDAT',zlib.compress(raw))+ch(b'IEND',b'')
def dib(off,size):
    b=d[off:off+size]; hs,w,h2,pl,bpp=struct.unpack_from("<IiiHH",b,0); h=h2//2; assert bpp==32 and hs==40
    px=b[hs:hs+w*h*4]; out=bytearray()
    for y in range(h-1,-1,-1):
        for x in range(w):
            B,G,R,A=px[(y*w+x)*4:(y*w+x)*4+4]; out+=bytes((R,G,B,A))
    return w,h,bytes(out)
out=sys.argv[1]; os.makedirs(out,exist_ok=True)
for name,(off,size) in {"16":(195608,1128),"32":(191344,4264),"48":(181704,9640)}.items():
    w,h,rgba=dib(off,size); open(os.path.join(out,"src%s.png"%name),"wb").write(png(w,h,rgba)); print(name,w,h)
