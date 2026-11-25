import os
GAME=os.environ.get("PINBALL_DIR","Pinball")   # folder containing your own copy of the game files
import partout,render,struct,sys
S="./"
r=partout.parse(GAME+"/PINBALL.DAT"); G=r['groups']
pal=G[2][2][1]; tb=[v for t,v in G[107] if t==1][0]
W,H=600,416; img=bytearray(W*H*3)
rgb=render.bmp8_to_rgb(tb,pal)
for y in range(tb['h']):
    for x in range(tb['w']):
        X,Y=x,y
        if 0<=X<W and 0<=Y<H: img[(Y*W+X)*3:(Y*W+X)*3+3]=rgb[(y*tb['w']+x)*3:(y*tb['w']+x)*3+3]
f=struct.unpack("<15f",[v for t,v in G[3] if t==11][0]); M=f[:12]; F=f[12]
def proj(x,y,z=0.0):
    xp=M[0]*x+M[1]*y+M[2]*z+M[3]; yp=M[4]*x+M[5]*y+M[6]*z+M[7]; zp=M[8]*x+M[9]*y+M[10]*z+M[11]
    return 183+F*xp/zp, 238+F*yp/zp   # centre (183,238) = table attribute 700 (FUN_01008eba)
def line(a,b,c):
    (x0,y0),(x1,y1)=a,b; n=int(max(abs(x1-x0),abs(y1-y0)))+1
    for i in range(n+1):
        X=int(x0+(x1-x0)*i/n); Y=int(y0+(y1-y0)*i/n)
        if 0<=X<W and 0<=Y<H: img[(Y*W+X)*3:(Y*W+X)*3+3]=bytes(c)
cnt=0
for g in G:
    for t,v in g:
        if t==11:
            a=struct.unpack("<%df"%(len(v)//4),v)
            if int(a[0]) in (600,603):
                n=int(a[1])
                if n==1:
                    import math
                    cx,cy,rad=a[2],a[3],a[4]; prev=None
                    for k in range(25):
                        th=2*math.pi*k/24; p=proj(cx+rad*math.cos(th),cy+rad*math.sin(th))
                        if prev: line(prev,p,(255,60,60))
                        prev=p
                else:
                    pts=[(a[2+2*i],a[3+2*i]) for i in range(n)]
                    P=[proj(*p) for p in pts]
                    for i in range(len(P)-1): line(P[i],P[i+1],(0,255,0) if int(a[0])==600 else (255,255,0))
                cnt+=1
print("polylines drawn:",cnt)
render.png(S+"overlay.png",W,H,bytes(img))
