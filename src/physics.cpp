#include "physics.h"
#include "rules_data.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
namespace pb {
static bool lvlHit(int part,int ball){ return (part&ball)!=0; }   // a part meets the ball when they share a layer bit (FUN_0100b3ae)
static float clampf(float v,float a,float b){ return v<a?a:(v>b?b:v); }
static Mat materialFor(const PartFile& dat,const Group& g){
  Mat m; auto chain=[&](int ref){ std::vector<float> f; if(ref>=0&&(size_t)ref<dat.groups.size()){ const Entry* e=dat.groups[ref].find(T_FLOATS); if(e){ const float* p=(const float*)e->data.data(); f.assign(p,p+e->data.size()/4);} } return f; };
  int a=g.intAttr(300);        // material: chain of (id,value): 301 smoothness, 302 elasticity, 304 hit sound
  if(a>=0 && (size_t)a<dat.groups.size()){ for(auto& ent: dat.groups[a].entries) if(ent.type==T_FLOATS){ const float* f=(const float*)ent.data.data(); size_t n=ent.data.size()/4;
      for(size_t i=0;i+1<n;i+=2){ int id=(int)f[i]; if(id==301) m.smooth=f[i+1]; else if(id==302) m.elast=f[i+1]; else if(id==304) m.sndHit=(int)f[i+1]; } } }
  int k=g.intAttr(400);        // kicker: 401 threshold, 402 kick, 404 (3 values), 405, 406 kick sound
  if(k>=0){ auto f=chain(k); for(size_t i=0;i+1<f.size();){ int id=(int)f[i]; if(id==401) m.thresh=f[i+1]; else if(id==402) m.kick=f[i+1]; else if(id==406) m.sndKick=(int)f[i+1]; i+=(id==404)?4:2; } }
  int s304=g.intAttr(304), s406=g.intAttr(406); if(m.sndHit<0 && s304>=0) m.sndHit=s304; if(m.sndKick<0 && s406>=0) m.sndKick=s406;
  return m; }
struct KickRec { float v402=35,v403=5,v405=0; float dir[3]={0,1,0}; int v406=-1; };
static KickRec kickRecFor(const PartFile& dat,const Group& g){ KickRec k; int ref=g.intAttr(400); if(ref<0||(size_t)ref>=dat.groups.size()) return k;
  const Entry* e=dat.groups[ref].find(T_FLOATS); if(!e) return k; const float* f=(const float*)e->data.data(); size_t n=e->data.size()/4;
  for(size_t i=0;i+1<n;){ int id=(int)f[i]; if(id==402) k.v402=f[i+1]; else if(id==403) k.v403=f[i+1]; else if(id==405) k.v405=f[i+1]; else if(id==406) k.v406=(int)f[i+1];
    else if(id==404 && i+3<n){ k.dir[0]=f[i+1]; k.dir[1]=f[i+2]; k.dir[2]=f[i+3]; } i+=(id==404)?4:2; }
  return k; }
static float sideOf(float x0,float y0,float x1,float y1,float x,float y){ return (x1-x0)*(y-y0)-(y1-y0)*(x-x0); }
void World::buildRamp(const PartFile& dat,const Group& g,int gi){
  (void)dat; Ramp R; R.name=g.name(); R.comp=-1; for(size_t i=0;i<scorers.size();i++) if(scorers[i].name==R.name) R.comp=(int)i;
  auto t=g.floats(1300); if(t.size()<2) return; int n=(int)t[0]; R.lvl=2; float gmag=25.f; { auto ga=g.floats(305); (void)ga; }
  for(int k=0;k<n && (size_t)(1+13*(k+1))<=t.size();k++){ const float* b=&t[1+13*k]; Tri T; T.A=b[0]; T.B=b[1]; T.C=b[2]; for(int j=0;j<3;j++){ T.x[j]=b[3+2*j]; T.y[j]=b[4+2*j]; }
    float a=b[9], c=b[10]; T.gx=sinf(a)*cosf(c)*gmag; T.gy=sinf(c)*sinf(a)*gmag; R.tris.push_back(T); }
  if(R.tris.empty()) return; { auto d=g.floats(701); if(!d.empty()) R.drag=d[0]; }
  float cx=0,cy=0; for(auto& T: R.tris) for(int j=0;j<3;j++){ cx+=T.x[j]; cy+=T.y[j]; } cx/=3*R.tris.size(); cy/=3*R.tris.size();
  auto portal=[&](int id,Portal& P){ auto f=g.floats(id); if(f.size()<8) return; P.outLvl=1<<(int)floorf(f[0]); P.on=floorf(f[1])!=0; P.x0=f[3]; P.y0=f[4]; P.x1=f[5]; P.y1=f[6];
    float best=1e9f; int sgn=0; for(auto& T: R.tris) for(int j=0;j<3;j++){ int k=(j+1)%3,o=(j+2)%3; float d1=hypotf(T.x[j]-P.x0,T.y[j]-P.y0)+hypotf(T.x[k]-P.x1,T.y[k]-P.y1), d2=hypotf(T.x[j]-P.x1,T.y[j]-P.y1)+hypotf(T.x[k]-P.x0,T.y[k]-P.y0); float d=std::min(d1,d2);
      if(d<best){ best=d; float s=sideOf(P.x0,P.y0,P.x1,P.y1,T.x[o],T.y[o]); sgn=s>0?1:-1; } }
    P.interior=best<0.5f? sgn : (sideOf(P.x0,P.y0,P.x1,P.y1,cx,cy)>0?1:-1); };
  portal(1301,R.entry); portal(1302,R.exit);
  { auto f=g.floats(1303); if(f.size()>=7){ R.topLvl=1<<(int)floorf(f[0]); R.topX0=f[2]; R.topY0=f[3]; R.topX1=f[4]; R.topY1=f[5]; R.hasTop=true; } }
  // platform height = height of the ramp plane at the exit line
  { float mx=(R.exit.x0+R.exit.x1)/2, my=(R.exit.y0+R.exit.y1)/2; float bestd=1e9f; for(auto& T: R.tris){ float d=0; for(int j=0;j<3;j++) d+=hypotf(T.x[j]-mx,T.y[j]-my); if(d<bestd){ bestd=d; R.topZ=std::max(0.f,T.A*mx+T.B*my+T.C); } } }
  rampGrp.push_back(gi); ramps.push_back(R); }
bool World::build(const PartFile& dat){
  for(auto& r: kRuleScores){ Scorer sc; sc.name=r.name; sc.pts.assign(r.pts,r.pts+r.count); scorers.push_back(sc); }
  for(const char* nm: {"s_onewy1","s_onewy10","s_trip1","s_trip2","s_trip3","s_trip4","s_trip5","a_kick1","a_kick2","v_gate1","v_gate2","v_bloc1","ramp_hole","drain","plunger","v_sink7"}){ bool have=false; for(auto& s: scorers) if(s.name==nm) have=true; if(!have){ Scorer sc; sc.name=nm; scorers.push_back(sc); } }   // components whose rule handler has no point value
  auto compFor=[&](const std::string& n){ for(size_t i=0;i<scorers.size();i++) if(scorers[i].name==n) return (int)i; return -1; };
  for(size_t gi=0;gi<dat.groups.size();gi++){ const Group& g=dat.groups[gi];
    Mat e=materialFor(dat,g); e.comp=compFor(g.name()); e.grp=(int)gi;
    if(g.name().rfind("a_kick",0)==0 && e.thresh>1e9f) e.thresh=0.5f;   // slingshot plates fire whenever hit (data has kick strength but no threshold)
    if(g.name().rfind("a_flag",0)==0){ auto f=g.floats(600); int n=f.empty()?0:(int)f[0]; if(n>=2&&f.size()>=(size_t)(1+2*n)){ Gate q; q.x0=f[1]; q.y0=f[2]; q.x1=f[3]; q.y1=f[4]; q.comp=compFor(g.name()); q.snd=g.intAttr(304); q.lvl=g.layerMask(); gates.push_back(q); } continue; }   // spinner: sensor line, not a wall
    if(g.name().rfind("s_trip",0)==0){ auto f=g.floats(600); int n=f.empty()?0:(int)f[0]; if(n>=2&&f.size()>=(size_t)(1+2*n)){ Gate q; q.x0=f[1]; q.y0=f[2]; q.x1=f[3]; q.y1=f[4]; q.comp=compFor(g.name()); q.snd=-1; q.lvl=g.layerMask(); gates.push_back(q); } continue; }   // skill-shot trip switch: line sensor
    if(g.name()=="table") tiltSnd=g.intAttr(406);
    if(g.name()=="plunger") plungerSnd=g.intAttr(406);
    if(g.name().rfind("a_roll",0)==0){ auto f=g.floats(600); int n=f.empty()?0:(int)f[0];     // rollover sensor polygon (closed; last point repeats the first)
      if(n>=3 && f.size()>=(size_t)(1+2*n)){ Poly p; for(int k=0;k<n;k++){ p.xs.push_back(f[1+2*k]); p.ys.push_back(f[2+2*k]); } p.comp=compFor(g.name()); p.snd=g.intAttr(304); p.grp=(int)gi; p.lvl=g.layerMask(); polys.push_back(p); } }
    if(g.name()=="drain"||g.name().rfind("s_trip",0)==0||g.name().rfind("a_roll",0)==0) continue;   // drain line, trip switches and rollovers are sensors, not walls
    // Collision layers (int attr 602, FUN_0100950a): layer bit = 1<<value (default 1). The ball starts on layer 1 and only meets parts on its own layer;
    // parts on layers 2 and 4 (ramp, upper bumpers, upper rollovers) are not reachable until ramps are ported.
    e.lvl=g.layerMask();   // part layers (int attr 602, possibly several): 1 table, 2 ramp, 4 raised platform
    if(g.name()=="ramp"||g.name()=="s_ramp9"){ buildRamp(dat,g,(int)gi); continue; }
    if(g.name()=="ramp_hole"){ auto c=g.floats(600); if(c.size()>=4){ RampHole h; h.x=c[1]; h.y=c[2]; h.r=c[3]; h.comp=-1; h.hz=0; rampHoles.push_back(h); rampHoleGrp.push_back((int)gi); } continue; }
    { const std::string nm=g.name(); bool kout=nm.rfind("a_kout",0)==0, sink=nm.rfind("v_sink",0)==0;
      if(kout||sink){ Hole H; KickRec k=kickRecFor(dat,g); Mat mm=materialFor(dat,g); H.comp=compFor(nm); H.name=nm; H.active=(nm!="a_kout1"); H.sndHit=mm.sndHit; H.sndKick=k.v406>=0?k.v406:mm.sndKick;
        float l=hypotf(k.dir[0],k.dir[1]); H.dx=l>0?k.dir[0]/l:0; H.dy=l>0?k.dir[1]/l:1; H.speed=k.v402; H.jit=k.v403*0.01f; H.spread=k.v405;
        if(kout){ auto c=g.floats(600); auto p=g.floats(305), q=g.floats(306); if(c.size()>=4&&!p.empty()&&!q.empty()){ H.x=c[1]; H.y=c[2]; H.fieldR=c[3]; H.force=p[0]; H.capR=q[0]*c[3]; holes.push_back(H); } }
        else { auto c=g.floats(601); if(c.size()>=3){ H.x=c[0]; H.y=c[1]; H.capR=c[2]; holes.push_back(H); } } }
      if(kout||nm=="ramp_hole") continue; }          // the kick-out circle is a trigger/field, not a solid bumper
    auto f=g.floats(600); bool ow=g.name().rfind("s_onewy",0)==0;
    if(f.size()>=3){ int n=(int)f[0];
      if(n==1 && f.size()>=4){ if(g.name()=="v_bloc1") postDisc=(int)discs.size(); discs.push_back({f[1],f[2],f[3],e}); }
      else if(n>1 && f.size()>=(size_t)(1+2*n)) for(int k=0;k+1<n;k++) { Seg sg{f[1+2*k],f[2+2*k],f[3+2*k],f[4+2*k],e,ow}; if(ow){ float dx=sg.x1-sg.x0,dy=sg.y1-sg.y0,l=hypotf(dx,dy); if(l>0){ sg.nx=dy/l; sg.ny=-dx/l; } }
          segs.push_back(sg);
          if(ow && k==0){ Gate q; float off=0.8f*radius; q.x0=sg.x0+sg.nx*off; q.y0=sg.y0+sg.ny*off; q.x1=sg.x1+sg.nx*off; q.y1=sg.y1+sg.ny*off; q.comp=compFor(g.name()); q.snd=g.intAttr(304); q.oneDir=true; q.lvl=e.lvl; gates.push_back(q); } } }
  }
  const char* names[2]={"a_flip1","a_flip2"};
  for(int i=0;i<2;i++){ const Group* g=dat.byName(names[i]); if(!g) return false;
    auto p=g->floats(800), a=g->floats(801), b=g->floats(802); if(p.size()<3||a.size()<3||b.size()<3) return false;
    Flipper& F=fl[i]; F.px=p[0]; F.py=p[1]; F.pr=p[2]; F.tr=a[2]; F.left=(p[0]>0);
    float aa=atan2f(a[1]-p[1],a[0]-p[0]), ab=atan2f(b[1]-p[1],b[0]-p[0]);
    F.len=hypotf(a[0]-p[0],a[1]-p[1]);
    // rest position is the tip lower on the table (larger y)
    if(a[1]>b[1]){ F.restA=aa; F.upA=ab; } else { F.restA=ab; F.upA=aa; }
    { float d=F.upA-F.restA; while(d>3.14159265f) d-=6.2831853f; while(d<-3.14159265f) d+=6.2831853f; F.A=d; }
    F.a=F.restA; F.m=materialFor(dat,*g);
    auto t4=g->floats(804), t5=g->floats(805), t3=g->floats(803); if(!t4.empty()) F.tUp=t4[0]; if(!t5.empty()) F.tDown=t5[0]; if(!t3.empty()) F.hit=t3[0];
    F.reach=F.len+F.tr+radius; }
  resetBall(); return true; }
void World::resetBall(){ ball.lvl=1; ball.z=0; ball.ramp=-1; ball.tri=-1; ball.falling=false; ball.lostT=0; ball.held=false; heldBy=-1; ball.x=startX; ball.y=startY; ball.vx=ball.vy=0; ball.inPlay=true; plungerHeld=false; plungerCharge=0; }
void World::releasePlunger(){   // FUN_0101875f / FUN_01017314: kick = pull * (1 + 0.1*rand), then the physics step caps the speed at maxSpeed
  if(atPlunger()){ float pull=std::min(100.f,plungerCharge*40.f); float r=(float)(rand()&0x7fff)/32768.f; float sp=std::min(maxSpeed,pull*(1.f+0.1f*r));
    if(sp>0.5f){ ball.vy=-sp; ball.vx=0; if(plungerSnd>=0) events.push_back(plungerSnd); } }
  plungerHeld=false; plungerCharge=0; }
// Wall response, transcribed from FUN_01017883 in pinball.exe. n = unit normal pointing at the ball. Returns normal speed.
static std::vector<int>* gEvents=nullptr; static std::vector<int>* gHits=nullptr; static std::vector<HitEvent>* gHitEv=nullptr;
static float respond(Ball& b,float nx,float ny,const Mat& m){
  float sp=hypotf(b.vx,b.vy); if(sp<1e-9f) return 0; float dx=b.vx/sp,dy=b.vy/sp;
  float u=-(nx*dx+ny*dy);
  if(u<=0) return 0;                     // moving away: nothing to do
  // Resting/rolling contact: the original resolves each impact once along the ball's ray; with fixed 1 ms steps a ball sliding along a wall
  // would be hit every step, so slow contacts only lose their normal velocity (no bounce, no tangential damping).
  if(u*sp<1.0f && u*sp<m.thresh){ b.vx+=u*sp*nx; b.vy+=u*sp*ny; return u*sp; }
  dx=(u*nx+dx)*m.smooth+u*nx*m.elast; dy=(u*ny+dy)*m.smooth+u*ny*m.elast;
  float l=hypotf(dx,dy); if(l>0){dx/=l;dy/=l;}
  float fn=u*sp; sp=sp-(1.f-m.elast)*fn;
  if(gHits && m.comp>=0 && fn>0.2f) gHits->push_back(m.comp);
  if(gHitEv && m.grp>=0 && fn>0.2f) gHitEv->push_back({m.grp,fn,fn>=m.thresh});   // for visual effects
  if(gEvents){ if(fn>m.thresh){ if(m.sndKick>=0) gEvents->push_back(m.sndKick); } else if(fn>0.2f && m.sndHit>=0) gEvents->push_back(m.sndHit); }   // FUN_01017e85
  if(m.thresh<=fn){ dx=nx*m.kick+sp*dx; dy=ny*m.kick+sp*dy; sp=hypotf(dx,dy); if(sp>0){dx/=sp;dy/=sp;} }
  b.vx=dx*sp; b.vy=dy*sp; return fn; }
static bool hitCapsule(Ball& b,float r,float ax,float ay,float bx,float by,float ra,float rb,const Mat& m,float svx,float svy){
  float dx=bx-ax,dy=by-ay,l2=dx*dx+dy*dy; float t=l2>0? clampf(((b.x-ax)*dx+(b.y-ay)*dy)/l2,0,1):0;
  float cx=ax+dx*t,cy=ay+dy*t,cr=ra+(rb-ra)*t; float nx=b.x-cx,ny=b.y-cy,d=hypotf(nx,ny); float min=r+cr;
  if(d>=min||d<1e-6f) return false; nx/=d; ny/=d; b.x=cx+nx*min; b.y=cy+ny*min;
  if(svx!=0||svy!=0){ b.vx-=svx; b.vy-=svy; respond(b,nx,ny,m); b.vx+=svx; b.vy+=svy; } else respond(b,nx,ny,m);
  return true; }
static const float kNudge[3][2]={{2.f,1.f},{-2.f,1.f},{0.f,1.f}};   // left, right, bottom
void World::nudge(int kind){
  if(tilted||kind<0||kind>2) return; float dx=kNudge[kind][0], dy=kNudge[kind][1];
  if(!ball.held){ ball.vx+=dx*0.5f; ball.vy+=dy*0.5f; float sp=hypotf(ball.vx,ball.vy); if(sp>maxSpeed){ ball.vx*=maxSpeed/sp; ball.vy*=maxSpeed/sp; } }
  shiftX+=(int)floorf(dx+0.5f); shiftY+=(int)floorf(0.5f-dy); pending.push_back({0.4f,-dx,-dy}); }   // every nudge owns its own undo
void World::addScore(int comp){
  if(comp<0||(size_t)comp>=scorers.size()) return; Scorer& sc=scorers[comp]; if(sc.cooldown>0) return;
  if(hitHook){ sc.cooldown=0.1f; hitHook(comp); return; }
  if(sc.pts.empty()) return; int p=sc.pts[std::min<size_t>(sc.state,sc.pts.size()-1)]; sc.cooldown=0.1f; score=std::min(score+p*multiplier,1000000000); }
static bool inPoly(const Poly& p,float x,float y){ bool in=false; size_t n=p.xs.size(); for(size_t i=0,j=n-1;i<n;j=i++) if(((p.ys[i]>y)!=(p.ys[j]>y)) && (x<(p.xs[j]-p.xs[i])*(y-p.ys[i])/(p.ys[j]-p.ys[i])+p.xs[i])) in=!in; return in; }
static bool inTri(const Tri& T,float x,float y,float tol){
  for(int j=0;j<3;j++){ int k=(j+1)%3,o=(j+2)%3; float s=sideOf(T.x[j],T.y[j],T.x[k],T.y[k],x,y), so=sideOf(T.x[j],T.y[j],T.x[k],T.y[k],T.x[o],T.y[o]); float l=hypotf(T.x[k]-T.x[j],T.y[k]-T.y[j]); if(l<1e-6f) continue; if(s*(so>0?1.f:-1.f)/l < -tol) return false; }
  return true; }
// Crossing a portal line (segment p0-p1) between the previous and the new ball position; returns +1 / -1 for the side entered, 0 if not crossed
static int crossed(float x0,float y0,float x1,float y1,float px,float py,float qx,float qy){
  float s0=sideOf(x0,y0,x1,y1,px,py), s1=sideOf(x0,y0,x1,y1,qx,qy); if(s0*s1>=0 && !(s0==0&&s1!=0)) return 0;
  float t=s0/(s0-s1); float cx=px+(qx-px)*t, cy=py+(qy-py)*t; float dx=x1-x0,dy=y1-y0,l2=dx*dx+dy*dy; float u=((cx-x0)*dx+(cy-y0)*dy)/l2; if(u<-0.05f||u>1.05f) return 0; return s1>0?1:-1; }
void World::updateRamps(float px,float py,float dt){
  (void)dt;
  for(size_t ri=0;ri<ramps.size();ri++){ Ramp& R=ramps[ri]; bool on=(ball.ramp==(int)ri);
    auto leave=[&](int lvl){ ball.ramp=-1; ball.tri=-1; ball.lvl=lvl; ball.z= lvl==4? R.topZ : 0.f; ball.lostT=0; };
    auto enter=[&](){ ball.ramp=(int)ri; ball.lvl=R.lvl; ball.lostT=0; ball.tri=-1; };
    Portal* ps[2]={&R.entry,&R.exit};
    for(Portal* P: ps){ if(!P->on) continue; int c=crossed(P->x0,P->y0,P->x1,P->y1,px,py,ball.x,ball.y); if(!c) continue;
      if(on && c!=P->interior){ leave(P->outLvl); on=false; break; }
      if(!on && ball.ramp<0 && ball.lvl==P->outLvl && c==P->interior){ enter(); on=true; break; } }
    if(R.hasTop && ball.lvl==R.topLvl && ball.ramp<0){ int c=crossed(R.topX0,R.topY0,R.topX1,R.topY1,px,py,ball.x,ball.y); if(c){ rampEvents.push_back(rampGrp[ri]); addScore(R.comp); } }
    if(on){ int best=-1; for(size_t k=0;k<R.tris.size();k++) if(inTri(R.tris[k],ball.x,ball.y,0.06f)){ best=(int)k; if((int)k==ball.tri) break; }
      if(best>=0){ ball.tri=best; ball.lostT=0; const Tri& T=R.tris[best]; ball.z=std::max(0.f,T.A*ball.x+T.B*ball.y+T.C); }
      else { if(ball.tri<0){ float bd=1e9f; for(size_t k=0;k<R.tris.size();k++){ float mx=(R.tris[k].x[0]+R.tris[k].x[1]+R.tris[k].x[2])/3, my=(R.tris[k].y[0]+R.tris[k].y[1]+R.tris[k].y[2])/3; float d=hypotf(ball.x-mx,ball.y-my); if(d<bd){ bd=d; ball.tri=(int)k; } } }   // just crossed the portal: use the nearest floor triangle
        const Tri& T=R.tris[ball.tri]; ball.z=std::max(0.f,T.A*ball.x+T.B*ball.y+T.C); ball.lostT+=dt; if(ball.lostT>1.5f) leave(1); } } }
  // raised platform: the ball may fall through the ramp hole
  if(ball.lvl==4 && ball.ramp<0){ for(size_t hi=0;hi<rampHoles.size();hi++){ RampHole& H=rampHoles[hi]; if(hypotf(ball.x-H.x,ball.y-H.y)<=H.r){ ball.falling=true; ball.fallHole=(int)hi; ball.vx=ball.vy=0; ball.x=H.x; ball.y=H.y; ball.vz=0; ball.fallT=0.5f; break; } } }
}
void World::stepFall(float dt){   // ramp_hole (FUN_01018476 / FUN_010169c0): hold 0.5 s, then drop to the table and come out at rest
  RampHole& H=rampHoles[ball.fallHole]; ball.x=H.x; ball.y=H.y; ball.vx=ball.vy=0;
  if(ball.fallT>0){ ball.fallT-=dt; return; }
  ball.vz+=5.f*dt; ball.z-=ball.vz*dt*2.f; if(ball.z<=0){ ball.z=0; ball.vz=0; ball.falling=false; ball.lvl=1; ball.ramp=-1; ball.tri=-1; rampHoleEvents.push_back(rampHoleGrp[ball.fallHole]); } }
void World::step(float dt){
  fl[0].pressed=inL&&!tilted; fl[1].pressed=inR&&!tilted;   // keys are remembered separately, so a held key works again as soon as the tilt ends
  for(size_t k=0;k<pending.size();){ pending[k].t-=dt;
    if(pending[k].t<=0){ float dx=pending[k].dx, dy=pending[k].dy;                // the table swings back: opposite impulse
      if(!ball.held && !tilted){ ball.vx+=dx*0.5f; ball.vy+=dy*0.5f; } shiftX+=(int)floorf(dx+0.5f); shiftY+=(int)floorf(0.5f-dy); pending.erase(pending.begin()+k); }
    else k++; }
  { bool active=!pending.empty(); tiltMeter = active? tiltMeter+4.f*dt : std::max(0.f,tiltMeter-dt);
    if(tiltMeter>0.5f && !warned){ warned=true; }          // "danger" warning (text box not ported)
    if(tiltMeter<=0.f) warned=false;
    if(tiltMeter>1.0f && !tilted){ tilted=true; if(tiltSnd>=0) events.push_back(tiltSnd); } }   // FUN_01017791: flippers go dead until the ball is lost
  gEvents=&events; gHitEv=&hitEvents; std::vector<int> hits; gHits=&hits; for(auto& sc: scorers) if(sc.cooldown>0) sc.cooldown-=dt;
  static const Mat flipMat{0.95f,0.45f,9.0e10f,0.f};   // flipper material is approximate (original uses its own routine)
  // flippers: rotate toward target angle at fixed angular speed
  for(auto& F: fl){ float target=F.pressed? F.restA+F.A:F.restA; float swing=fabsf(F.A); float rate=swing/(F.pressed? F.tUp:F.tDown); float d=target-F.a;
    float s=clampf(d,-rate*dt,rate*dt); F.a+=s; F.w=s/dt; }
  if(plungerHeld) plungerCharge=std::min(2.5f,plungerCharge+dt);
  if(!ball.inPlay) return;
  if(ball.held){ holdT-=dt; if(holdT<=0){ const Hole& H=holes[heldBy]; float u=(float)(rand()&0x7fff)/32768.f, u2=(float)(rand()&0x7fff)/32768.f;
      float ang=(1.f-(u+u))*H.spread, c=cosf(ang), sn=sinf(ang); float ex=H.dx*c-H.dy*sn, ey=H.dx*sn+H.dy*c; float sp=H.speed*H.jit*(1.f-(u2+u2))+H.speed;   // FUN_01015935
      ball.vx=ex*sp; ball.vy=ey*sp; ball.x=H.x+ex*(radius+0.05f); ball.y=H.y+ey*(radius+0.05f); ball.held=false; holes[heldBy].armed=false; heldBy=-1; if(H.sndKick>=0) events.push_back(H.sndKick); }
    return; }
  // table-layer acceleration (FUN_010173e7): gravity minus drag, with a small random sideways jitter refreshed ~60 times/s
  noiseT-=dt; if(noiseT<=0){ noise=0.5f-(float)(rand()&0x7fff)/32768.f;   /* Windows rand() is 0..32767 */ noiseT=1.f/60.f; }
  if(ball.falling){ stepFall(dt); return; }
  if(ball.ramp>=0 && ball.tri>=0){ const Ramp& R=ramps[ball.ramp]; const Tri& T=R.tris[ball.tri]; ball.vx+=(T.gx-ball.vx*R.drag)*dt; ball.vy+=(T.gy-ball.vy*R.drag)*dt; }   // FUN_01016d2e: slope gravity minus drag, no noise
  else { float sp0=hypotf(ball.vx,ball.vy); ball.vx+=(gx-(noise*sp0+ball.vx)*drag)*dt; ball.vy+=(gy-ball.vy*drag)*dt; }
  for(size_t hi=0;hi<holes.size();hi++){ const Hole& H=holes[hi]; if(!H.active||ball.lvl!=1) continue; float ddx=H.x-ball.x, ddy=H.y-ball.y, d2=ddx*ddx+ddy*ddy;
    if(H.fieldR>0 && d2<=H.fieldR*H.fieldR && d2>1e-9f){ float d=sqrtf(d2); ball.vx+=(ddx/d*H.force-ball.vx)*dt; ball.vy+=(ddy/d*H.force-ball.vy)*dt; }   // FUN_0101685b
    if(!H.armed && d2>(H.capR+0.02f)*(H.capR+0.02f)) holes[hi].armed=true;   // capture is an edge crossing from outside, so re-arm only after the ball has left
    if(H.armed && d2<=H.capR*H.capR){ ball.held=true; heldBy=(int)hi; holdT=H.hold; if(capCount.size()<holes.size()) capCount.assign(holes.size(),0); capCount[hi]++; ball.x=H.x; ball.y=H.y; ball.vx=ball.vy=0; addScore(H.comp); if(H.sndHit>=0) events.push_back(H.sndHit); return; } }
  float prevX=ball.x, prevY=ball.y;
  float sp=hypotf(ball.vx,ball.vy); if(sp>maxSpeed){ ball.vx*=maxSpeed/sp; ball.vy*=maxSpeed/sp; }
  ball.x+=ball.vx*dt; ball.y+=ball.vy*dt;
  for(auto& s: segs){ if(!lvlHit(s.m.lvl,ball.lvl)) continue; if(s.oneway){ float side=(ball.x-s.x0)*s.nx+(ball.y-s.y0)*s.ny, vn=ball.vx*s.nx+ball.vy*s.ny; if(!(side>0 && vn<0)) continue; }   // only a ball approaching the front is stopped
      hitCapsule(ball,radius,s.x0,s.y0,s.x1,s.y1,0,0,s.m,0,0); }
  for(size_t di=0;di<discs.size();di++){ auto& d=discs[di]; if((int)di==postDisc && !postUp) continue; if(lvlHit(d.m.lvl,ball.lvl)) hitCapsule(ball,radius,d.x,d.y,d.x,d.y,d.r,d.r,d.m,0,0); }
  for(auto& F: fl){ float tx=F.px+cosf(F.a)*F.len, ty=F.py+sinf(F.a)*F.len;
    float rx=ball.x-F.px, ry=ball.y-F.py; float svx=-F.w*ry, svy=F.w*rx;     // surface velocity at the ball's radius
    // closest point on the flipper capsule, to get the contact normal and distance from the pivot
    float dx=tx-F.px,dy=ty-F.py,l2=dx*dx+dy*dy,t=clampf(((ball.x-F.px)*dx+(ball.y-F.py)*dy)/l2,0,1);
    float cx=F.px+dx*t,cy=F.py+dy*t,cr=F.pr+(F.tr-F.pr)*t,nx=ball.x-cx,ny=ball.y-cy,d=hypotf(nx,ny);
    if(d<radius+cr && d>1e-6f){ nx/=d; ny/=d; ball.x=cx+nx*(radius+cr); ball.y=cy+ny*(radius+cr);
      float kick=0; Mat m=F.m; float sv=hypotf(svx,svy);
      if(sv>1e-3f){ float dot=(svx*nx+svy*ny)/sv;                           // FUN_0101be06: moving flipper kicks the ball
        float omega=fabsf(F.A)/((F.w*F.A>0)? F.tUp:F.tDown);
        float r=hypotf(ball.x-F.px,ball.y-F.py);
        if(dot>=0 && r*r>F.pr*F.pr*1.01f) kick=dot*omega*(r/F.reach)*F.hit; }
      if(kick>0){ m.thresh=-1.f; m.kick=kick; }
      respond(ball,nx,ny,m); } }
  for(auto& q: gates){ if(!(q.lvl&ball.lvl)){ q.side=0; continue; } float dx=q.x1-q.x0,dy=q.y1-q.y0,l2=dx*dx+dy*dy; float t=((ball.x-q.x0)*dx+(ball.y-q.y0)*dy)/l2; float cr=dx*(ball.y-q.y0)-dy*(ball.x-q.x0); int sd=cr>0?1:-1;
    if(q.side!=0 && sd!=q.side && t>=0 && t<=1 && (!q.oneDir || (q.side==1 && sd==-1))){ addScore(q.comp); if(q.snd>=0) events.push_back(q.snd); } q.side=sd; }
  updateRamps(prevX,prevY,dt);
  for(int c: hits) addScore(c);
  for(auto& p: polys){ bool in= (p.lvl&ball.lvl) && inPoly(p,ball.x,ball.y); if(in&&!p.inside){ addScore(p.comp); triggers.push_back(p.grp); if(p.snd>=0) events.push_back(p.snd); } p.inside=in; }
  if(ball.y>drainY){ if(drainHook && drainHook()) return; tilted=false; tiltMeter=0; warned=false; balls--; if(balls<=0){ gameOver=true; resetBall(); } else { ballNumber++; resetBall(); } }
}
}
