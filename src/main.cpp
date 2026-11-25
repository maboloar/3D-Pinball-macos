// 3D Pinball (Space Cadet) macOS port - work in progress. Uses only files from the game folder.
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include "partout.h"
#include "camera.h"
#include "physics.h"
#include "audio.h"
#include "music.h"
#include "scene.h"
#include "rules.h"
using namespace pb;
static const int VW=600, VH=416;   // table_size from PINBALL.DAT

static SDL_Surface* makeSurface(const Bitmap& b, const uint8_t* pal){
  SDL_Surface* s=SDL_CreateRGBSurfaceWithFormat(0,b.w,b.h,32,SDL_PIXELFORMAT_ARGB8888); int stride=(b.w+3)&~3;
  for(int y=0;y<b.h;y++){ uint32_t* row=(uint32_t*)((uint8_t*)s->pixels+y*s->pitch); const uint8_t* src=&b.pixels[(b.h-1-y)*stride];
    for(int x=0;x<b.w;x++){ const uint8_t* c=pal+src[x]*4; row[x]=0xFF000000u|(c[2]<<16)|(c[1]<<8)|c[0]; } }
  return s; }

// On-screen number box (score1, ballcount1, player_number1): int record = [first digit group, x, y, w, h]; digits 0-9 are bitmaps in consecutive groups, drawn right-aligned (FUN_01013d2d).
struct NumBox { int x=0,y=0,w=0,h=0; bool ok=false; SDL_Texture* digit[10]={}; int dw=0;
  void load(const PartFile& dat,const char* name,SDL_Renderer* r,const uint8_t* pal){ const Group* g=dat.byName(name); const Entry* e=g?g->find(T_INTS):nullptr; if(!e||e->data.size()<10) return;
    const int16_t* v=(const int16_t*)e->data.data(); int first=v[0]; x=v[1]; y=v[2]; w=v[3]; h=v[4];
    for(int d=0;d<10;d++){ if((size_t)(first+d)>=dat.groups.size()) return; const Entry* b=dat.groups[first+d].find(T_BITMAP); if(!b) return; SDL_Surface* s=makeSurface(b->bmp,pal); digit[d]=SDL_CreateTextureFromSurface(r,s); SDL_FreeSurface(s); dw=b->bmp.w; }
    ok=true; }
  void draw(SDL_Renderer* r,int value) const { if(!ok||value<0) return; std::string s=std::to_string(value); int px=x+w; for(int i=(int)s.size()-1;i>=0;i--){ px-=dw; SDL_Rect d{px,y,dw,h}; SDL_RenderCopy(r,digit[s[i]-'0'],nullptr,&d); } } };

// Multi-frame sprite: the component group plus the following unnamed kind-201 groups hold the animation frames (bitmap x,y are in table-bitmap coordinates).
struct Sprite { std::vector<SDL_Texture*> t; std::vector<SDL_Rect> r;
  void load(const PartFile& dat,const char* name,SDL_Renderer* rr,const uint8_t* pal,int ox,int oy){ size_t i=0; bool found=false; for(;i<dat.groups.size();i++) if(dat.groups[i].name()==name){ found=true; break; } if(!found) return;
    for(size_t j=i;j<dat.groups.size();j++){ const Group& g=dat.groups[j]; if(j>i && (g.kind()!=201||!g.name().empty())) break; const Entry* b=g.find(T_BITMAP); if(!b) continue;
      SDL_Surface* s=makeSurface(b->bmp,pal); SDL_SetColorKey(s,SDL_TRUE,SDL_MapRGB(s->format,0,0,0)); t.push_back(SDL_CreateTextureFromSurface(rr,s)); SDL_FreeSurface(s); r.push_back({b->bmp.x-ox,b->bmp.y-oy,b->bmp.w,b->bmp.h}); } }
  void draw(SDL_Renderer* rr,float frac,int ox=0,int oy=0) const { if(t.empty()) return; int n=(int)t.size(); int k=(int)(frac*(n-1)+0.5f); if(k<0)k=0; if(k>=n)k=n-1; SDL_Rect q=r[k]; q.x+=ox; q.y+=oy; SDL_RenderCopy(rr,t[k],nullptr,&q); } };

static void drawGeometry(SDL_Renderer* r,const PartFile& dat,const Camera& cam){
  auto P=[&](float x,float y,float& sx,float& sy){ cam.project(x,y,0,sx,sy); };
  for(auto& g: dat.groups) for(int id: {600,603}){ auto f=g.floats(id); if(f.size()<3) continue; int n=(int)f[0];
    if(n==1 && f.size()>=4){ SDL_SetRenderDrawColor(r,255,60,60,255); float px=0,py=0;
      for(int k=0;k<=24;k++){ float th=6.2831853f*k/24,sx,sy; P(f[1]+f[3]*cosf(th),f[2]+f[3]*sinf(th),sx,sy); if(k) SDL_RenderDrawLineF(r,px,py,sx,sy); px=sx;py=sy; } }
    else if(n>1 && f.size()>=(size_t)(1+2*n)){ if(id==600) SDL_SetRenderDrawColor(r,0,255,0,255); else SDL_SetRenderDrawColor(r,255,255,0,255);
      float px=0,py=0; for(int k=0;k<n;k++){ float sx,sy; P(f[1+2*k],f[2+2*k],sx,sy); if(k) SDL_RenderDrawLineF(r,px,py,sx,sy); px=sx;py=sy; } } } }


// ---- tiny built-in 5x7 text renderer for the info panel (the original used system fonts; this one is drawn with plain rectangles) ----
static const unsigned char kFont5x7[59][5]={   // ASCII 32..90, column bytes, bit 0 = top row
 {0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},
 {0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{0x14,8,0x3E,8,0x14},{8,8,0x3E,8,8},{0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},{0x20,0x10,8,4,2},
 {0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{1,0x71,9,5,3},
 {0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x51,9,6},
 {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x49,0x49,0x7A},
 {0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,2,0x0C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
 {0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7F,1,1},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
 {0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}};
static void drawText(SDL_Renderer* r,int x,int y,int w,int h,const std::string& text,Uint8 cr=210,Uint8 cg=235,Uint8 cb=150){
  SDL_SetRenderDrawColor(r,cr,cg,cb,255); int cx=x, cy=y; const int cw=6, ch=9; auto newline=[&]{ cx=x; cy+=ch; };
  for(size_t i=0;i<text.size();i++){ unsigned char c=(unsigned char)text[i]; if(c=='\n'){ newline(); continue; } if(c>='a'&&c<='z') c-=32; if(c<32||c>90) c='?';
    if(cx+cw>x+w){ newline(); } if(cy+ch>y+h+1) return; const unsigned char* g=kFont5x7[c-32];
    for(int col=0;col<5;col++) for(int row=0;row<7;row++) if(g[col]&(1<<row)){ SDL_Rect p{cx+col,cy+row,1,1}; SDL_RenderFillRect(r,&p); }
    cx+=cw; } }
// ---- high scores (top five) kept in the user's application-support folder ----
static std::string hsPath(const std::string& dir){ return dir+"/highscores.txt"; }
static void loadHighScores(Rules& rules,const std::string& path){ rules.highScores.clear(); FILE* f=fopen(path.c_str(),"r"); if(!f) return; char name[64]; int sc;
  while(rules.highScores.size()<5 && fscanf(f,"%63s %d",name,&sc)==2) rules.highScores.push_back({name,sc}); fclose(f); }
static void saveHighScores(Rules& rules,const std::string& path){ FILE* f=fopen(path.c_str(),"w"); if(!f) return; for(auto& h: rules.highScores) fprintf(f,"%s %d\n",h.name.c_str(),h.score); fclose(f); }
static bool qualifies(const Rules& rules,int score){ return score>0 && (rules.highScores.size()<5 || score>rules.highScores.back().score); }
static void addHighScore(Rules& rules,const std::string& name,int score){ rules.highScores.push_back({name.empty()?"PLAYER":name,score}); std::stable_sort(rules.highScores.begin(),rules.highScores.end(),[](const Rules::HS& a,const Rules::HS& b){ return a.score>b.score; }); if(rules.highScores.size()>5) rules.highScores.resize(5); }
static void tick(Rules& r,float secs){ for(float t=0;t<secs;t+=0.05f) r.update(0.05f); }
static int selfTest(const PartFile& dat,const std::string& dir){
  World w; if(!w.build(dat)){ fprintf(stderr,"world build failed\n"); return 2; }
  printf("walls=%zu circles=%zu flippers: L(%.2f,%.2f len %.2f) R(%.2f,%.2f)\n",w.segs.size(),w.discs.size(),w.fl[0].px,w.fl[0].py,w.fl[0].len,w.fl[1].px,w.fl[1].py);
  { int n=0; for(auto& sg: w.segs) if(sg.m.thresh<1e9f){ if(n++<6) printf(" kick wall (%.2f,%.2f)-(%.2f,%.2f) thresh=%.2f kick=%.2f el=%.2f sm=%.2f\n",sg.x0,sg.y0,sg.x1,sg.y1,sg.m.thresh,sg.m.kick,sg.m.elast,sg.m.smooth); } printf(" walls with kick: %d\n",n); int m=0; for(auto& d: w.discs) if(d.m.thresh<1e9f){ if(m++<8) printf(" kick disc (%.2f,%.2f) r=%.2f thresh=%.2f kick=%.2f el=%.2f\n",d.x,d.y,d.r,d.m.thresh,d.m.kick,d.m.elast); } printf(" discs with kick: %d\n",m); }
  // test 1: drop a ball in the middle of the table, it must reach the drain without NaN
  w.ball={0.f,-6.f,0,0,true}; float t=0; bool ok=true; float maxY=-99;
  for(int i=0;i<20000&&w.ball.inPlay&&w.balls==3;i++,t+=0.001f){ w.step(0.001f); if(!std::isfinite(w.ball.x)||!std::isfinite(w.ball.y)) { ok=false; break; } maxY=std::max(maxY,w.ball.y); if(i%2000==0) printf(" t=%.1f ball (%.2f,%.2f) v(%.1f,%.1f)\n",t,w.ball.x,w.ball.y,w.ball.vx,w.ball.vy); }
  printf("test1 drop: %s, drained=%d, t=%.2fs, maxY=%.2f\n",ok?"finite":"NaN",w.balls<3,t,maxY);
  // test 2: plunger launch at full charge: how high does the ball get?
  World v; v.build(dat); v.plungerHeld=true; for(int i=0;i<1000;i++) v.step(0.001f); v.releasePlunger(); float top=99,tx=0; int steps=0;
  for(int i=0;i<8000&&v.balls==3;i++,steps++){ v.step(0.001f); if(v.ball.y<top){top=v.ball.y;tx=v.ball.x;} }
  printf("test2 plunger: highest y=%.2f at x=%.2f (table top is about -14), balls left=%d\n",top,tx,v.balls);
  { SDL_Init(SDL_INIT_AUDIO); Audio au; bool ok2=au.init(dat,dir); World s; s.build(dat); for(int i=0;i<2000;i++) s.step(0.001f); s.plungerHeld=true; for(int i=0;i<1000;i++) s.step(0.001f); s.releasePlunger();
    for(int i=0;i<6000;i++) s.step(0.001f); std::map<int,int> cnt; for(int e: s.events) cnt[e]++;
    printf("test4 audio: device %s; plunger shot produced %zu sound events from %zu distinct sounds",ok2?"opened":"FAILED",s.events.size(),cnt.size()); for(auto& c: cnt) printf(" [%d x%d]",c.first,c.second); printf("\n"); }
  { World b; b.build(dat); b.ball={0.01f,-5.2f,0,0,true}; int hitsAt=-1; for(int i=0;i<3000&&hitsAt<0;i++){ b.step(0.001f); if(b.score>0) hitsAt=i; }
    printf("test5 scoring: ball dropped on pop bumper a_bump1 scored %d at step %d (expect 500)\n",b.score,hitsAt);
    World r; r.build(dat); const Poly* p=nullptr; for(auto& q: r.polys) if(r.scorers[q.comp<0?0:q.comp].name=="a_roll3") p=&q;
    if(p){ float cx=0,cy=0; for(size_t k=0;k+1<p->xs.size();k++){cx+=p->xs[k];cy+=p->ys[k];} cx/=(p->xs.size()-1); cy/=(p->xs.size()-1); r.ball={cx,cy,0,0,true}; r.step(0.001f); printf("test6 rollover: ball placed in a_roll3 scored %d (expect 2000)\n",r.score); } else printf("test6 rollover: a_roll3 polygon missing\n"); }
  { World k; k.build(dat); const Hole* H3=nullptr; const Hole* H1=nullptr; for(auto& h: k.holes){ if(h.name=="a_kout3") H3=&h; if(h.name=="a_kout1") H1=&h; }
    printf("test7 holes: %zu holes built (gravity well a_kout1 active=%d, expect 0); ",k.holes.size(),H1?H1->active:-1);
    if(H3){ k.ball={H3->x+0.25f,H3->y+0.1f,0,0,true}; float capT=-1,ejT=-1,ejSp=0; bool wasHeld=false; int sc0=k.score;
      for(int i=0;i<4000;i++){ k.step(0.001f); if(k.ball.held&&!wasHeld&&capT<0){ capT=i*0.001f; } if(!k.ball.held&&wasHeld&&ejT<0){ ejT=i*0.001f; ejSp=hypotf(k.ball.vx,k.ball.vy); } wasHeld=k.ball.held; }
      printf("a_kout3: captured at %.2fs, ejected at %.2fs (%.2fs hold, expect 1.5) with speed %.1f, score gained %d (expect >=20000)\n",capT,ejT,ejT-capT,ejSp,k.score-sc0); } else printf("a_kout3 missing\n"); }
  { Music mu; bool ok3=mu.load(dir+"/PINBALL.MID"); if(ok3) mu.start(); SDL_Delay(1200); double p=ok3?mu.position():-1; printf("test8 music: PINBALL.MID %s; playing=%d, position after 1.2s = %.2fs (expect ~1.2)\n",ok3?"loaded":"FAILED",ok3?mu.playing():0,p); mu.stop(); }
  { World n; n.build(dat); n.ball={3.f,-1.f,0,0,true}; for(int i=0;i<10;i++) n.step(0.001f); float vx0=n.ball.vx; n.nudge(0); float dvx=n.ball.vx-vx0; for(int i=0;i<450;i++) n.step(0.001f); int sx=n.shiftX, sy=n.shiftY;
    World t; t.build(dat); t.ball={3.f,-1.f,0,0,true}; t.inL=t.inR=true; for(int i=0;i<3;i++){ t.nudge(i%3); for(int j=0;j<100;j++) t.step(0.001f); } for(int j=0;j<500;j++) t.step(0.001f);
    printf("test9 nudge: left nudge changed vx by %+.2f (expect +1.00); table shift back to (%d,%d) after 0.4s (expect 0,0); 3 quick nudges -> tilted=%d (expect 1), flippers dead=%d\n",dvx,sx,sy,t.tilted,(int)(!t.fl[0].pressed&&!t.fl[1].pressed)); }
  { // test 10: keys held through a tilt must work again once the ball drains (no stuck controls)
    World t; t.build(dat); t.inL=t.inR=true; t.ball={3.f,-1.f,0,0,true}; for(int i=0;i<3;i++){ t.nudge(i%3); for(int j=0;j<100;j++) t.step(0.001f); }
    bool deadWhileTilted=t.tilted && !t.fl[0].pressed && !t.fl[1].pressed; t.ball={0.f,14.5f,0,0,true}; t.step(0.001f); for(int j=0;j<10;j++) t.step(0.001f);   // drain -> tilt ends
    bool backAfter=!t.tilted && t.fl[0].pressed && t.fl[1].pressed;
    printf("test10 input: flippers dead while tilted=%d (expect 1), working again after the drain with keys still held=%d (expect 1)\n",deadWhileTilted,backAfter);
    // test 11: many quick nudges in one direction must leave neither a picture offset nor a velocity imbalance
    World n; n.build(dat); n.ball={3.f,-1.f,0,0,true}; for(int i=0;i<10;i++) n.step(0.001f); float vx0=n.ball.vx; for(int i=0;i<2;i++){ n.nudge(0); for(int j=0;j<50;j++) n.step(0.001f); } for(int j=0;j<900;j++) n.step(0.001f);
    printf("test11 nudge undo: two quick left nudges -> picture shift (%d,%d) after 1 s (expect 0,0), pending=%zu (expect 0)\n",n.shiftX,n.shiftY,n.pending.size()); }
  { World w; w.build(dat); Rules r; r.init(dat,w); r.newGame();
    int l8=r.light("lite8"), l9=r.light("lite9"), l10=r.light("lite10"); bool ok1=l8>=0&&l9>=0&&l10>=0&&r.group("bmpr_inc_lights")>=0&&r.bumperSets.size()==2;
    r.hitName("a_roll3"); bool lane1=r.lights[l8].on&&!r.lights[l9].on;
    r.flipperPressed(0); bool rotated=!r.lights[l8].on&&r.lights[l9].on;               // left flipper shifts the lit lane one place
    r.flipperPressed(1); bool back=r.lights[l8].on&&!r.lights[l9].on;
    r.hitName("a_roll2"); r.hitName("a_roll1"); bool flashing=r.lights[l8].flashing&&r.lights[l9].flashing&&r.lights[l10].flashing;
    int lvl=r.bumperLevel("attack_bumpers"); int st=-1; for(auto& sc: w.scorers) if(sc.name=="a_bump1") st=sc.state;
    tick(r,5.2f); bool allOff=!r.lights[l8].visible()&&!r.lights[l9].visible()&&!r.lights[l10].visible()&&!r.lights[l8].flashing;
    tick(r,61.f); int lvl2=r.bumperLevel("attack_bumpers");
    printf("test12 rules: lights/groups found=%d, lane lights on=%d, flipper rotates=%d/%d, all-lit blinks=%d, bumper level %d (expect 1, scoring state %d), lights off after 5 s=%d, level after 60 s=%d (expect 0)\n",ok1,lane1,rotated,back,flashing,lvl,st,allOff,lvl2); }
  { World w; w.build(dat); Rules r; r.init(dat,w); r.newGame();
    r.hitName("a_targ10"); bool one=r.lights[r.light("lite70")].on; r.hitName("a_targ11"); r.hitName("a_targ12");
    tick(r,2.1f); bool reset=!r.lights[r.light("lite70")].visible()&&!r.lights[r.light("lite71")].visible();
    r.hitName("v_rebo1"); bool reb=r.lights[r.light("lite84")].visible(); tick(r,0.2f); bool rebOff=!r.lights[r.light("lite84")].visible();
    printf("test13 target bank: lamp lit=%d, bank blinks and resets=%d, rebound lamp flashes=%d/%d\n",one,reset,reb,rebOff); }
  { // test 14: ramp ride - a ball shot up the entry portal climbs the ramp, reaches the raised platform and drops through the ramp hole back to the table
    int bestLvl=0; float bestSp=0; bool dropped=false, scoredTop=false;
    { World q; q.build(dat); { int c1=0,c2=0,c4=0; for(auto& s: q.segs){ if(s.m.lvl==1)c1++; else if(s.m.lvl==2)c2++; else c4++; } printf(" segs lvl1=%d lvl2=%d lvl4=%d\n",c1,c2,c4); }
    for(auto& R: q.ramps) printf(" ramp %s tris=%zu entry on=%d interior=%d out=%d (%.2f,%.2f)-(%.2f,%.2f) exit on=%d interior=%d out=%d topZ=%.2f\n",R.name.c_str(),R.tris.size(),R.entry.on,R.entry.interior,R.entry.outLvl,R.entry.x0,R.entry.y0,R.entry.x1,R.entry.y1,R.exit.on,R.exit.interior,R.exit.outLvl,R.topZ); }
    for(float sp: {25.f,30.f,35.f,40.f,45.f,50.f,55.f,60.f}){ World q; q.build(dat); q.ball={2.4f,3.5f,0.f,-sp,true}; q.ball.vx=0; bool reached=false; int lastLvl=1; int sc0=q.score; bool wasUp=false;
      for(int i=0;i<12000 && !q.gameOver && q.balls==3;i++){ q.step(0.001f); if(q.ball.lvl==4) wasUp=true; if(q.ball.lvl>bestLvl){ bestLvl=q.ball.lvl; bestSp=sp; } if(wasUp&&q.ball.lvl==1&&lastLvl==4) reached=true; lastLvl=q.ball.lvl; if(getenv("RAMP_TRACE") && (i%250==0 || (getenv("RAMP_FINE") && i<400 && i%8==0))) printf("   sp %.0f t=%.2f ball(%.2f,%.2f) v(%.1f,%.1f) lvl=%d z=%.2f ramp=%d\n",sp,i*0.001,q.ball.x,q.ball.y,q.ball.vx,q.ball.vy,q.ball.lvl,q.ball.z,q.ball.ramp); }
      if(reached) dropped=true; if(q.score>sc0+4000) scoredTop=true; }
    printf("test14 ramp: highest layer reached=%d (expect 4) at launch speed %.0f; dropped through ramp hole to table=%d; ramp score seen=%d\n",bestLvl,bestSp,dropped,scoredTop); }
  { // test 15: mission machine - choose a mission with the ramp bank, accept it on the ramp, finish it, get rewards and a promotion step
    World w; w.build(dat); Rules r; r.init(dat,w); r.newGame(); bool fuel=r.fuelBars()==6; int rank0=r.rankIndex();
    r.hitName("s_onewy4"); bool sel=r.missionState==1; r.hitName("a_targ13"); bool picked=r.missionSel==3 && r.lit("lite56");
    int s0=w.score; r.hitName("ramp"); bool started=r.missionState==3 && r.missionSel==3 && w.score==s0+10000;
    r.hitName("ramp"); r.hitName("ramp"); bool mid=r.missionState==3 && r.missionSel==1; r.hitName("ramp"); bool done=r.missionState==1 && r.completed==1 && w.score>=s0+10000+500000;
    int og=r.groupMsg(r.group("outer_circle"),0x25,0);
    printf("test15 missions: fuel full=%d rank=%d; select state=%d, mission chosen=%d, accepted (+10000)=%d, countdown=%d, completed (+500000)=%d, outer ring steps=%d (expect 6)\n",fuel,rank0,sel,picked,started,mid,done,og); }
  { // test 16: field multiplier from the top target bank, applied to ordinary scores but not to mission awards
    World w; w.build(dat); Rules r; r.init(dat,w); r.newGame(); int s0=w.score; r.hitName("a_bump1"); int plain=w.score-s0;
    r.hitName("a_targ7"); r.hitName("a_targ8"); r.hitName("a_targ9"); bool two=r.mulIdx==1; int s1=w.score; r.hitName("a_bump1"); int dbl=w.score-s1;
    printf("test16 multiplier: plain bumper %d, after top bank multiplier index %d -> bumper %d (expect %d)\n",plain,r.mulIdx,dbl,plain*2); (void)two; }
  { // test 17: ball flow - ball save, extra ball, normal loss with end-of-ball bonus, game over
    World w; w.build(dat); Rules r; r.init(dat,w); r.newGame(); int b0=w.balls; bool saved=r.drained()&&w.balls==b0&&r.lit("lite200");
    r.hitName("s_onewy1"); tick(r,11.f); bool saveOver=!r.lit("lite200"); if(!saveOver){ auto& L=r.lights[r.light("lite200")]; printf("   lite200 on=%d flashing=%d left=%.2f force=%d offAfter=%d\n",L.on,L.flashing,L.flashLeft,L.force,L.offAfterBlink); } r.hitName("a_roll4"); r.lm_test("lite17",0x13); r.hitName("a_roll4"); bool eb=r.extraBalls==1;
    bool extra=r.drained()&&r.extraBalls==0&&w.balls==b0; r.hitName("s_onewy1"); tick(r,11.f); int sc0=w.score; bool lost=!r.drained(); bool bonusPaid=w.score>=sc0+25000; r.hitName("s_onewy1"); tick(r,11.f);
    w.balls=1; bool last=!r.drained()&&r.missionState==0x20;
    printf("test17 ball flow: ball saved=%d, save window over after 11 s=%d, extra ball earned=%d and used=%d, loss pays bonus=%d/%d, last ball -> game-over state=%d\n",saved,saveOver,eb,extra,lost,bonusPaid,last); }
  { // test 18: hyperspace chute sequence and centre post, skill shot, fuel burn aborting a mission
    World w; w.build(dat); Rules r; r.init(dat,w); r.newGame(); bool postDown=!w.postUp; int s0=w.score; r.hitName("a_kout2"); int first=w.score-s0;
    r.hitName("a_kout2"); r.hitName("a_kout2"); bool post=w.postUp; tick(r,25.f); bool postGone=!w.postUp;
    World v; v.build(dat); Rules q; q.init(dat,v); q.newGame(); tick(q,0.8f); int k=q.groupMsg(q.group("skill_shot_lights"),0x25,0); int s1=v.score; q.hitName("s_onewy4"); int skill=v.score-s1;
    q.hitName("s_onewy4"); q.hitName("a_targ14"); q.hitName("ramp"); bool inMission=q.missionState>=2; tick(q,80.f); bool aborted=q.missionState==1 && q.fuelBars()==0;
    printf("test18 hyperspace/skill/fuel: first chute %d (expect 10000), post up=%d (down at start=%d, drops later=%d), skill shot with %d lit = %d, mission started=%d, aborted when fuel ran out=%d\n",first,post,postDown,postGone,k,skill,inMission,aborted); }
  { // test 19: every mission state can be entered and leaves no stuck state (smoke run through all 17 missions and the maelstrom chain)
    int fails=0,done=0; for(int m=2;m<=18;m++){ World w; w.build(dat); Rules r; r.init(dat,w); r.newGame(); r.missionState=1; r.startState_test(m); if(r.missionState!=m){ fails++; continue; }
      for(int i=0;i<400 && r.missionState!=1;i++){ static const char* seq[]={"a_bump1","a_bump2","a_bump3","a_bump4","a_bump5","a_targ1","a_targ2","a_targ3","a_targ4","a_targ5","a_targ6","a_targ7","a_targ8","a_targ9","a_targ10","a_targ11","a_targ12","a_targ13","a_targ14","a_targ15","a_targ16","a_targ17","a_targ18","a_targ19","a_targ20","a_targ21","a_targ22","ramp","a_kout2","a_kout3","v_sink1","v_sink2","v_sink3","a_roll1","a_roll2","a_roll3","a_roll4","a_roll5","a_roll6","a_roll7","a_roll8","a_roll9","a_roll110","a_roll111","a_roll112","a_flag1","a_flag2","v_rebo1","v_rebo2","v_rebo3","v_rebo4","a_roll184"};
        if(m==10&&i==0){ r.hitName("a_roll3"); r.hitName("a_roll2"); r.hitName("a_roll1"); } if(m==7&&i==0){ r.hitName("a_roll110"); r.hitName("a_roll111"); r.hitName("a_roll112"); } if(m==15&&i<80) for(int z=0;z<2;z++) r.hitName("a_flag1"); const int NS=(int)(sizeof seq/sizeof seq[0]); r.hitName(seq[(i*7)%NS]); r.hitName(seq[i%NS]); tick(r,1.5f); if(m==5||m==9){ for(int t=1;t<=9;t++) r.hitName("a_targ"+std::to_string(t)); } }
      if(r.missionState==1 || r.completed>0) done++; else { fails++; printf("   mission %d (%s) stuck in state %d\n",m,Rules::missionName(m),r.missionState); } }
    printf("test19 mission smoke: %d of 17 missions entered and reached a completion/abort state, failures=%d (expect 0)\n",done,fails); }
  // test 3: ball resting on the left flipper, then flip: it must be thrown up the table
  World f; f.build(dat); f.ball={1.7f,11.7f,0,0,true}; for(int i=0;i<600;i++) f.step(0.001f);
  float restY=f.ball.y; f.inL=true; float minVy=0,minY=99; for(int i=0;i<400;i++){ f.step(0.001f); minVy=std::min(minVy,f.ball.vy); minY=std::min(minY,f.ball.y); }
  printf("test3 flipper: ball rested at y=%.2f, after flip fastest upward speed=%.1f, highest y=%.2f (%s)\n",restY,-minVy,minY,(minVy<-10&&minY<restY-1.0f)?"launched":"NOT launched");
  return ok?0:1; }

// Simulated player: plunges, flips when the ball is near a flipper, and reports stuck positions / NaN / score. Deterministic per seed.
static int soak(const PartFile& dat,double seconds,int seed){
  srand(seed); World w; w.build(dat); Rules rr; rr.init(dat,w); rr.newGame(); int missionsDone=0, maxRank=0, sMax=0; double t=0; int games=0, stuckCount=0, plunges=0; float maxSpeed=0; bool bad=false; double lowT=0; std::map<std::string,int> stuckAt; double heldAbs=0; int holds=0;
  double plungeTimer=0; bool pulling=false; int maxScore=0; double hold[2]={0,0}, cool[2]={0,0};
  while(t<seconds){
    if(w.gameOver){ games++; maxScore=std::max(maxScore,w.score); missionsDone+=rr.completed; w.newGame(); rr.newGame(); }
    if(w.atPlunger()||pulling){ if(!pulling){ pulling=true; w.plungerHeld=true; plungeTimer=0.3+0.7*(rand()%1000)/1000.0; } plungeTimer-=0.001; if(plungeTimer<=0){ w.releasePlunger(); pulling=false; plunges++; } }
    else { for(int i=0;i<2;i++){ const Flipper& F=w.fl[i]; float tx=F.px+cosf(F.restA)*F.len;   // flip when the ball is in front of the flipper (between tip and pivot) and close above it
        float lo=std::min(F.px,tx)-0.3f, hi=std::max(F.px,tx)+0.3f; bool front=w.ball.x>lo && w.ball.x<hi && w.ball.y>F.py-2.2f && w.ball.y<F.py+1.4f;
        bool want=front && (w.ball.vy>-2.f);
        if(hold[i]>0){ hold[i]-=0.001; if(hold[i]<=0){ cool[i]=0.25+0.2*(rand()%100)/100.0; } (i==0?w.inL:w.inR)=true; }       // hold the flip ~0.25 s, then let go
        else { if(cool[i]>0) cool[i]-=0.001; (i==0?w.inL:w.inR)=false; if(want && cool[i]<=0 && !F.pressed){ hold[i]=0.2+0.15*(rand()%100)/100.0; (i==0?w.inL:w.inR)=true; } } } }
    if(w.ball.inPlay && !w.ball.held){ float sp=hypotf(w.ball.vx,w.ball.vy); maxSpeed=std::max(maxSpeed,sp); if(!std::isfinite(sp)||!std::isfinite(w.ball.x)||!std::isfinite(w.ball.y)){ bad=true; break; }
      if(sp<0.6f && w.ball.y<w.fl[0].py-0.8f){ lowT+=0.001; if(lowT>4.0){ char key[64]; snprintf(key,sizeof key,"(%.1f,%.1f)",roundf(w.ball.x*2)/2,roundf(w.ball.y*2)/2); stuckAt[key]++; stuckCount++; lowT=0; w.ball.vx=3; w.ball.vy=-3; } } else lowT=0; }
    if(w.ball.held) holds++;
    if(getenv("SOAK_TRACE") && ((long)(t*1000))%(getenv("SOAK_EVERY")?atoi(getenv("SOAK_EVERY")):20000)==0) printf("  t=%.0f ball(%.2f,%.2f) v(%.1f,%.1f) inPlay=%d held=%d score=%d balls=%d\n",t,w.ball.x,w.ball.y,w.ball.vx,w.ball.vy,w.ball.inPlay,w.ball.held,w.score,w.balls);
    w.step(0.001f); rr.update(0.001f); w.events.clear(); maxRank=std::max(maxRank,rr.rankIndex()); sMax=std::max(sMax,rr.missionState==0x20?0:rr.missionState); t+=0.001; }
  maxScore=std::max(maxScore,w.score); missionsDone+=rr.completed;
  printf("soak seed %d: %.0fs simulated, %d games, %d plunges, best score %d, max speed %.1f, ball-held steps %d, stuck events %d%s\n   rules: missions completed %d, highest mission state %d, best rank %d\n",seed,seconds,games,plunges,maxScore,maxSpeed,holds,stuckCount,bad?"  **NaN**":"",missionsDone,sMax,maxRank);
  for(size_t i=0;i<w.capCount.size();i++) if(w.capCount[i]>0) printf("   captures: %s x%d\n",w.holes[i].name.c_str(),w.capCount[i]);
  for(auto& s: stuckAt) printf("   stuck near %s x%d\n",s.first.c_str(),s.second);
  return bad?1:0; }

// ---- locating the user's own copy of the game files (never bundled in release builds) ----
static bool hasGameData(const std::string& d){ FILE* f=fopen((d+"/PINBALL.DAT").c_str(),"rb"); if(!f) return false; fclose(f); return true; }
static std::string appSupportDir(){ const char* h=getenv("HOME"); return std::string(h?h:".")+"/Library/Application Support/3D-Pinball-macos"; }
static std::string findGameDir(){
  if(const char* e=getenv("PINBALL_DIR")) if(hasGameData(e)) return e;
  char* base=SDL_GetBasePath(); std::string b=base?base:""; SDL_free(base);
  if(hasGameData(b+"../Resources")) return b+"../Resources";            // personal build with data inside the app bundle
  if(hasGameData(appSupportDir())) return appSupportDir();               // release build: files copied here by the user
  if(hasGameData("../Pinball")) return "../Pinball";                     // running from a source checkout next to a Pinball folder
  return appSupportDir(); }
static int reportMissingGameFiles(const std::string& folder){
  std::string cmd="mkdir -p \""+folder+"\""; if(system(cmd.c_str())!=0){}
  std::string msg="3D Pinball could not find the game files.\n\nThis app does not include the game. Copy PINBALL.DAT, FONT.DAT and the WAV / MIDI files from your own copy of 3D Pinball for Windows into:\n\n"+folder+"\n\nthen start the app again.";
  fprintf(stderr,"%s\n",msg.c_str());
  if(getenv("PINBALL_NO_DIALOG")) return 2;                                // headless/testing: no dialog
  if(SDL_Init(SDL_INIT_VIDEO)==0){
    const SDL_MessageBoxButtonData btn[2]={{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,0,"Open Folder"},{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,1,"Quit"}};
    SDL_MessageBoxData box{SDL_MESSAGEBOX_INFORMATION,nullptr,"Game files needed",msg.c_str(),2,btn,nullptr}; int hit=1;
    if(SDL_ShowMessageBox(&box,&hit)==0 && hit==0){ pid_t p=fork(); if(p==0){ execlp("open","open",folder.c_str(),(char*)nullptr); _exit(127); } }
    SDL_Quit(); }
  return 2; }

// ---- how-to-play alert (shown at start; H shows it again) ----
static std::string introMarker(){ return appSupportDir()+"/intro-dismissed"; }
static bool introDismissed(){ FILE* f=fopen(introMarker().c_str(),"rb"); if(!f) return false; fclose(f); return true; }
static void showHowToPlay(SDL_Window* w,bool offerDismiss){
  static const char* text=
    "GOAL\nScore as many points as you can with three balls. Keep the ball in play with the flippers; hit bumpers, targets, lanes and holes to score.\n\n"
    "LAUNCH\nHold SPACE to pull the plunger back (up to 2.5 s for full power), then release to shoot.\n\n"
    "FLIPPERS\nZ or Left arrow = left flipper     /  or Right arrow = right flipper\n\n"
    "NUDGE THE TABLE\nX = left     .  = right     Up arrow = from below\nNudge too often and the table TILTS: the flippers stop until the ball is lost.\n\n"
    "OTHER KEYS\nF2 new game  |  P pause  |  F4 full screen  |  M music  |  H this help  |  Esc quit";
  const SDL_MessageBoxButtonData btn[2]={{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT|SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,"Play"},{0,1,"Don't show again"}};
  SDL_MessageBoxData box{SDL_MESSAGEBOX_INFORMATION,w,"How to play",text,offerDismiss?2:1,btn,nullptr}; int hit=0;
  if(SDL_ShowMessageBox(&box,&hit)==0 && hit==1 && offerDismiss){ std::string cmd="mkdir -p \""+appSupportDir()+"\""; if(system(cmd.c_str())!=0){} FILE* f=fopen(introMarker().c_str(),"wb"); if(f){ fputs("1",f); fclose(f); } } }

int main(int argc,char** argv){
  std::string dir, shotPath; bool dbg=false, selfTest_flag=false, demo=false, startFull=false, ballAt=false, noIntro=false; double soakSecs=0; float ballAtX=0, ballAtY=0;
  for(int i=1;i<argc;i++){ if(!strcmp(argv[i],"--shot")&&i+1<argc) shotPath=argv[++i]; else if(!strcmp(argv[i],"--debug")) dbg=true; else if(!strcmp(argv[i],"--selftest")) selfTest_flag=true; else if(!strcmp(argv[i],"--demo")) demo=true; else if(!strcmp(argv[i],"--no-intro")) noIntro=true; else if(!strcmp(argv[i],"--ballat")&&i+1<argc){ sscanf(argv[++i],"%f,%f",&ballAtX,&ballAtY); ballAt=true; } else if(!strcmp(argv[i],"-fullscreen")||!strcmp(argv[i],"--fullscreen")) startFull=true; else if(!strcmp(argv[i],"--soak")&&i+1<argc) soakSecs=atof(argv[++i]); else if(argv[i][0]!='-' && strncmp(argv[i],"-psn",4)) dir=argv[i]; }
  bool explicitDir=!dir.empty();
  if(dir.empty()) dir=findGameDir();
  PartFile dat; std::string err;
  if(!dat.load(dir+"/PINBALL.DAT",&err)){
    fprintf(stderr,"PINBALL.DAT (%s): %s\n",dir.c_str(),err.c_str());
    if(!explicitDir) return reportMissingGameFiles(appSupportDir());     // first run: tell the user where to put their own copy of the game files
    return 1; }
  const Group* bg=dat.byName("background"); const Group* tb=dat.byName("table"); const Group* cg=dat.byName("camera_info");
  if(!bg||!tb||!cg||!bg->find(T_PALETTE)){ fprintf(stderr,"missing table groups\n"); return 1; }
  const uint8_t* pal=bg->find(T_PALETTE)->data.data();
  Camera cam; { const Entry* e=cg->find(T_FLOATS); const float* f=(const float*)e->data.data(); for(int i=0;i<12;i++) cam.M[i]=f[i]; cam.F=f[12]; }
  { auto c7=tb->floats(700); if(c7.size()>=2){ cam.cx=c7[0]; cam.cy=c7[1]; } }   // projection centre override (FUN_01008eba, table attr 700 = 183,238), in table-picture coordinates
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){ fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
  SDL_Window* w=SDL_CreateWindow("3D Pinball",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,VW*2,VH*2,SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE);
  SDL_Renderer* r=SDL_CreateRenderer(w,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC); SDL_RenderSetLogicalSize(r,VW,VH);
  const Bitmap& tbm=tb->find(T_BITMAP)->bmp; const Bitmap& bgm=bg->find(T_BITMAP)->bmp;
  SDL_Surface* s1=makeSurface(tbm,pal); SDL_Surface* s2=makeSurface(bgm,pal);
  SDL_Texture* tt=SDL_CreateTextureFromSurface(r,s1); SDL_Texture* bt=SDL_CreateTextureFromSurface(r,s2);
  if(selfTest_flag) return selfTest(dat,dir);
  if(getenv("EJECT")){ World w; w.build(dat); int hi=-1; for(size_t i=0;i<w.holes.size();i++) if(w.holes[i].name==getenv("EJECT")) hi=(int)i; if(hi<0){ printf("no such hole\n"); return 1; }
    const Hole& H=w.holes[hi]; w.ball={H.x,H.y,0,0,true}; w.ball.held=true; w.heldBy=hi; w.holdT=0.001f; printf("hole %s at (%.2f,%.2f) dir(%.2f,%.2f) speed %.1f spread %.2f\n",H.name.c_str(),H.x,H.y,H.dx,H.dy,H.speed,H.spread);
    for(int i=0;i<120;i++){ w.step(0.001f); if(i<14||i%10==0) printf("  t=%.3f ball(%.2f,%.2f) v(%.1f,%.1f) held=%d\n",i*0.001,w.ball.x,w.ball.y,w.ball.vx,w.ball.vy,w.ball.held); } return 0; }
  if(getenv("PROBE_X")){ World w; w.build(dat); w.ball={(float)atof(getenv("PROBE_X")),(float)atof(getenv("PROBE_Y")),0,0,true};
    for(int i=0;i<6;i++){ w.step(0.001f); printf("step %d ball(%.4f,%.4f) v(%.4f,%.4f)\n",i,w.ball.x,w.ball.y,w.ball.vx,w.ball.vy); }
    for(auto& s: w.segs){ float dx=s.x1-s.x0,dy=s.y1-s.y0,l2=dx*dx+dy*dy,t=l2>0?std::max(0.f,std::min(1.f,((w.ball.x-s.x0)*dx+(w.ball.y-s.y0)*dy)/l2)):0; float cx=s.x0+dx*t,cy=s.y0+dy*t; float d=hypotf(w.ball.x-cx,w.ball.y-cy); if(d<w.radius+0.02f) printf("  contact seg (%.2f,%.2f)-(%.2f,%.2f) d=%.4f smooth=%.2f elast=%.2f t=%.2f\n",s.x0,s.y0,s.x1,s.y1,d,s.m.smooth,s.m.elast,t); }
    for(auto& d: w.discs){ float dd=hypotf(w.ball.x-d.x,w.ball.y-d.y)-d.r; if(dd<w.radius+0.02f) printf("  contact disc (%.2f,%.2f r%.2f)\n",d.x,d.y,d.r); }
    for(int i=0;i<2;i++){ auto& F=w.fl[i]; float tx=F.px+cosf(F.a)*F.len,ty=F.py+sinf(F.a)*F.len; float dx=tx-F.px,dy=ty-F.py,l2=dx*dx+dy*dy,t=std::max(0.f,std::min(1.f,((w.ball.x-F.px)*dx+(w.ball.y-F.py)*dy)/l2)); float cx=F.px+dx*t,cy=F.py+dy*t; float d=hypotf(w.ball.x-cx,w.ball.y-cy)-(F.pr+(F.tr-F.pr)*t); if(d<w.radius+0.02f) printf("  contact flipper %d d=%.4f t=%.2f\n",i,d,t); }
    return 0; }
  if(soakSecs>0){ int rc=0; int s0=getenv("SOAK_SEED")?atoi(getenv("SOAK_SEED")):1, s1=getenv("SOAK_SEED")?s0:4; for(int sd=s0;sd<=s1;sd++) rc|=soak(dat,soakSecs,sd); return rc; }
  World world; world.build(dat);
  if(ballAt){ world.ball={ballAtX,ballAtY,0,0,true}; world.gy=0; world.drag=0; }   // freeze the ball: no gravity, no drag
  if(demo){ world.score=1234500; world.ballNumber=2; world.inL=world.inR=true; for(int i=0;i<300;i++) world.step(0.001f); }   // screenshot helper
  Audio audio; Music music; if(shotPath.empty()){ audio.init(dat,dir); music.load(dir+"/PINBALL.MID"); }   // original default is music off (registry "Music"=0); M toggles   // no sound when just taking a screenshot
  // The ball has one bitmap per depth (groups "ball" + following): group float 501 = [x, y, z] reference point where that size applies.
  struct BallSprite { SDL_Texture* t; int w,h; float refY; }; std::vector<BallSprite> ballSprites;
  { size_t i0=0; for(;i0<dat.groups.size();i0++) if(dat.groups[i0].name()=="ball") break;
    for(size_t j=i0;j<dat.groups.size();j++){ const Group& g=dat.groups[j]; const Entry* be=g.find(T_BITMAP); auto ref=g.floats(501); if(!be||ref.size()<3) break;
      SDL_Surface* s=makeSurface(be->bmp,pal); SDL_SetColorKey(s,SDL_TRUE,SDL_MapRGB(s->format,0,0,0)); ballSprites.push_back({SDL_CreateTextureFromSurface(r,s),be->bmp.w,be->bmp.h,ref[1]}); SDL_FreeSurface(s); } }
  Sprite sprF[2],sprPlunger; { const Bitmap& tb1=tb->find(T_BITMAP)->bmp; sprF[0].load(dat,"a_flip1",r,pal,tb1.x,tb1.y); sprF[1].load(dat,"a_flip2",r,pal,tb1.x,tb1.y); sprPlunger.load(dat,"plunger",r,pal,tb1.x,tb1.y); }
  Scene scene; scene.load(dat,r,pal);
  bool nameEntry=false, gameOverHandled=false; std::string nameBuf; float hsShowT=0; Rules rules; rules.init(dat,world); auto newGameAll=[&]{ world.newGame(); rules.newGame(); gameOverHandled=false; hsShowT=0; }; rules.newGame(); const std::string hsFile=hsPath(appSupportDir()); if(shotPath.empty()) loadHighScores(rules,hsFile);
  NumBox boxScore,boxBall,boxPlayer; boxScore.load(dat,"score1",r,pal); boxBall.load(dat,"ballcount1",r,pal); boxPlayer.load(dat,"player_number1",r,pal);
  bool paused=false, autoPaused=false, musicWasOn=false, fullscreen=startFull; if(startFull) SDL_SetWindowFullscreen(w,SDL_WINDOW_FULLSCREEN_DESKTOP);
  bool run=true; int frames=0; Uint64 last=SDL_GetPerformanceCounter(); double acc=0;
  if(shotPath.empty() && !noIntro && !getenv("PINBALL_NO_DIALOG") && !introDismissed()){ showHowToPlay(w,true); last=SDL_GetPerformanceCounter(); }   // short how-to-play alert at open
  while(run){ SDL_Event e; while(SDL_PollEvent(&e)){ if(e.type==SDL_QUIT) run=false;
      if(nameEntry){                                                                // typing initials for a new high score
        if(e.type==SDL_TEXTINPUT){ for(const char* p=e.text.text;*p;p++) if(((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='_') && nameBuf.size()<12) nameBuf+=*p; }
        if(e.type==SDL_KEYDOWN){ SDL_Keycode k=e.key.keysym.sym; if(k==SDLK_BACKSPACE&&!nameBuf.empty()) nameBuf.pop_back();
          if(k==SDLK_RETURN||k==SDLK_KP_ENTER||k==SDLK_ESCAPE){ if(k!=SDLK_ESCAPE||true){ std::string nm=nameBuf; for(auto& ch: nm) ch=(char)toupper(ch); addHighScore(rules,nm,world.score); std::string cmd="mkdir -p \""+appSupportDir()+"\""; if(system(cmd.c_str())!=0){} saveHighScores(rules,hsFile); } nameEntry=false; SDL_StopTextInput(); hsShowT=6.f; } }
        continue; }
      if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE) run=false;
      if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_F5&&!e.key.repeat) hsShowT= hsShowT>0? 0.f : 8.f;
      if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_F1) dbg=!dbg;
      if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_m&&!e.key.repeat) music.toggle();
      if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_h&&!e.key.repeat){ world.releaseInput(); showHowToPlay(w,false); last=SDL_GetPerformanceCounter(); acc=0; }   // H = show the help again
      if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_F2&&!e.key.repeat) newGameAll();                       // F2 = new game (as in the original menu)
      if(e.type==SDL_KEYDOWN&&(e.key.keysym.sym==SDLK_p||e.key.keysym.sym==SDLK_F3)&&!e.key.repeat){ paused=!paused; autoPaused=false; if(paused){ musicWasOn=music.playing(); music.stop(); } else if(musicWasOn) music.start(); }
      if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_F4&&!e.key.repeat){ fullscreen=!fullscreen; SDL_SetWindowFullscreen(w,fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0); }   // F4 = full screen (original menu)
      if(e.type==SDL_WINDOWEVENT){ if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST && !paused){ paused=true; autoPaused=true; musicWasOn=music.playing(); music.stop(); world.releaseInput(); }   // the original pauses when deactivated; also drop held keys and pending nudges so nothing sticks
        else if(e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED && autoPaused){ paused=false; autoPaused=false; if(musicWasOn) music.start(); } }
      if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP){ bool dn=e.type==SDL_KEYDOWN; SDL_Keycode k=e.key.keysym.sym;
        if(k==SDLK_z||k==SDLK_LEFT){ if(dn&&!e.key.repeat&&!world.tilted&&!paused) rules.flipperPressed(0); world.inL=dn; } if(k==SDLK_SLASH||k==SDLK_RIGHT){ if(dn&&!e.key.repeat&&!world.tilted&&!paused) rules.flipperPressed(1); world.inR=dn; }
        if(dn&&!e.key.repeat){ if(k==SDLK_x) world.nudge(0); else if(k==SDLK_PERIOD) world.nudge(1); else if(k==SDLK_UP) world.nudge(2); }   // table bumps: left X, right '.', bottom Up
        if(k==SDLK_SPACE&&!e.key.repeat){ if(dn&&world.gameOver) newGameAll(); if(dn&&world.atPlunger()) world.plungerHeld=true; else if(!dn) world.releasePlunger(); } } }
    { Uint64 now=SDL_GetPerformanceCounter(); double frameDt=(double)(now-last)/SDL_GetPerformanceFrequency(); if(frameDt>0.1) frameDt=0.1; if(paused) frameDt=0; acc+=(double)(now-last)/SDL_GetPerformanceFrequency(); last=now; if(acc>0.1) acc=0.1; if(paused) acc=0; while(acc>=0.001){ world.step(0.001f); acc-=0.001; }
      for(int id: world.events) audio.play(id); world.events.clear();
      for(const HitEvent& h: world.hitEvents){ int c=scene.byGroup(h.grp); if(c<0) continue; VisComp& v=scene.at(c); int nf=(int)v.frames.size();
        switch(v.cls){
          case 1005: if(h.kicked) scene.flash(c,std::min(nf-1,v.state*2+1),0.1f,v.state*2); break;   // bumper: lit frame 2*state+1 for a moment (FUN_01015d0f)
          case 1014: if(h.kicked&&nf>1) scene.flash(c,1,0.15f,0); break;                              // slingshot plate pushed out
          case 1000: case 1010: scene.flash(c,0,0.12f,-1); break;                                      // rubber rebounds flash
          case 1006: case 1019: if(nf>1) scene.flash(c,1,0.2f,0); break;                               // standup targets blink when struck
          default: break; } }
      world.hitEvents.clear();
      world.triggers.clear(); rules.update((float)frameDt); if(hsShowT>0) hsShowT-=(float)frameDt;
      if(world.gameOver && !gameOverHandled){ gameOverHandled=true; if(shotPath.empty() && qualifies(rules,world.score)){ nameEntry=true; nameBuf.clear(); SDL_StartTextInput(); world.releaseInput(); } else hsShowT=6.f; }
      for(size_t li=0;li<rules.lights.size();li++){ int c=scene.byGroup(rules.lights[li].group); if(c>=0) scene.show(c,rules.lights[li].visible()?0:-1); }   // lights: frame 0 is the lit picture
      for(auto& s: rules.bumperSets) for(auto& bn: s.bumpers){ int c=scene.find(bn); if(c<0) continue; VisComp& v=scene.at(c); int nf=(int)v.frames.size(); v.state=s.level; v.baseFrame=std::min(nf-1,2*s.level); if(v.timer<=0) v.shown=v.baseFrame; }   // bumper level -> frame pair
      scene.update((float)(frameDt)); }
    SDL_SetRenderDrawColor(r,0,0,0,255); SDL_RenderClear(r);
    SDL_Rect b{bgm.x,bgm.y,bgm.w,bgm.h}; SDL_RenderCopy(r,bt,nullptr,&b);   // info panel first
    SDL_Rect a{world.shiftX,world.shiftY,tbm.w,tbm.h}; SDL_RenderCopy(r,tt,nullptr,&a);   // playfield on top
    scene.draw(r,world.shiftX,world.shiftY);
    for(int i=0;i<2;i++){ const Flipper& F=world.fl[i]; sprF[i].draw(r,F.A!=0? (F.a-F.restA)/F.A : 0.f,world.shiftX,world.shiftY); }
    sprPlunger.draw(r,world.plungerHeld? std::min(1.f,world.plungerCharge/2.5f):0.f,world.shiftX,world.shiftY);
    boxScore.draw(r,world.score); boxBall.draw(r,world.ballNumber); boxPlayer.draw(r,1);
    { std::string it=rules.info.text, mt=rules.mission.text; if(world.tilted) it="TILT!"; else if(world.warned) it="CAREFUL...";
      auto hsList=[&]{ std::string s="HIGH SCORES\n"; for(size_t i=0;i<rules.highScores.size();i++) s+=std::to_string(i+1)+". "+rules.highScores[i].name+" "+std::to_string(rules.highScores[i].score)+"\n"; if(rules.highScores.empty()) s+="NONE YET"; return s; };
      if(nameEntry) mt="NEW HIGH SCORE\nENTER NAME:\n"+nameBuf+"_\nENTER = OK";
      else if(hsShowT>0) mt=hsList();
      else if(world.gameOver) mt="GAME OVER\nSPACE OR F2:\nNEW GAME\nF5: HIGH SCORES";
      else if(paused) mt="GAME PAUSED\nP OR F3 TO RESUME";
      drawText(r,408,241,160,44,it); drawText(r,408,305,160,88,mt); }
    if(!ballSprites.empty() && !world.ball.held){ float sx,sy; cam.project(world.ball.x,world.ball.y,world.radius+world.ball.z,sx,sy);          // ball centre is radius above the table
      size_t k=0; float best=1e9f; for(size_t q=0;q<ballSprites.size();q++){ float dd=fabsf(world.ball.y-ballSprites[q].refY); if(dd<best){best=dd;k=q;} }   // sprite for this depth
      const BallSprite& bs=ballSprites[k]; SDL_Rect br{(int)lroundf(sx-bs.w/2.f)+world.shiftX,(int)lroundf(sy-bs.h/2.f)+world.shiftY,bs.w,bs.h}; SDL_RenderCopy(r,bs.t,nullptr,&br);
      scene.occlude(r,br,cam.depth16(world.ball.x,world.ball.y,world.radius+world.ball.z),world.shiftX,world.shiftY); }   // sprites closer to the camera than the ball are drawn over it
    if(dbg){ drawGeometry(r,dat,cam); SDL_SetRenderDrawColor(r,0,200,255,255); for(auto& F: world.fl){ float ax,ay,bx,by; cam.project(F.px,F.py,0,ax,ay); cam.project(F.px+cosf(F.a)*F.len,F.py+sinf(F.a)*F.len,0,bx,by); SDL_RenderDrawLineF(r,ax,ay,bx,by); } }
    if(!shotPath.empty() && ++frames==3){ int ww,hh; SDL_GetRendererOutputSize(r,&ww,&hh); SDL_Surface* s=SDL_CreateRGBSurfaceWithFormat(0,ww,hh,32,SDL_PIXELFORMAT_ARGB8888);
      SDL_RenderReadPixels(r,nullptr,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch); SDL_SaveBMP(s,shotPath.c_str()); SDL_FreeSurface(s); run=false; }
    SDL_RenderPresent(r); }
  SDL_Quit(); return 0; }
