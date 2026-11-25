#include "scene.h"
#include <algorithm>
#include <cstring>
namespace pb {

static SDL_Surface* makeSprite(const Bitmap& b,const uint8_t* pal){
  SDL_Surface* s=SDL_CreateRGBSurfaceWithFormat(0,b.w,b.h,32,SDL_PIXELFORMAT_ARGB8888); int stride=(b.w+3)&~3;
  for(int y=0;y<b.h;y++){ uint32_t* row=(uint32_t*)((uint8_t*)s->pixels+y*s->pitch); const uint8_t* src=&b.pixels[(b.h-1-y)*stride];
    for(int x=0;x<b.w;x++){ int p=src[x]; const uint8_t* c=pal+p*4; row[x]= p==0? 0u : (0xFF000000u|(c[2]<<16)|(c[1]<<8)|c[0]); } }   // palette index 0 is transparent
  return s; }

// Depth map entry (type 12): u16 w,h,stride, 8 more header bytes, then h rows of `stride` u16 values.
static std::vector<uint16_t> readDepth(const Group& g,const Bitmap& b,SDL_Surface* surf){
  std::vector<uint16_t> out(b.w*b.h,0xffff); const Entry* e=g.find(T_VISUAL); if(!e||e->data.size()<14) return out;
  const uint8_t* d=e->data.data(); uint16_t w,h,st; memcpy(&w,d,2); memcpy(&h,d+2,2); memcpy(&st,d+4,2);
  if(w!=b.w||h!=b.h||e->data.size()<14u+(size_t)st*h*2) return out;
  auto val=[&](int row,int x){ uint16_t v; memcpy(&v,d+14+((size_t)row*st+x)*2,2); return v; };
  // the rows may be stored bottom-up like the bitmap or top-down: pick the orientation whose "has depth" mask matches the opaque pixels
  int same=0,flip=0;
  for(int y=0;y<h;y++) for(int x=0;x<w;x++){ uint32_t px=((uint32_t*)((uint8_t*)surf->pixels+y*surf->pitch))[x]; bool op=(px>>24)!=0;
    if((val(y,x)!=0xffff)==op) same++; if((val(h-1-y,x)!=0xffff)==op) flip++; }
  bool flipped=flip>same;
  for(int y=0;y<h;y++) for(int x=0;x<w;x++) out[y*w+x]=val(flipped? h-1-y : y,x);
  return out; }

void Scene::clear(){ for(auto& c: comps) for(auto& f: c.frames){ if(f.tex) SDL_DestroyTexture(f.tex); if(f.surf) SDL_FreeSurface(f.surf); } comps.clear(); byName.clear(); byGrp.clear(); }

void Scene::load(const PartFile& dat,SDL_Renderer* rr,const uint8_t* pal){
  clear();
  const Group* to=dat.byName("table_objects"); const Entry* te=to? to->find(T_INTS):nullptr; if(!te) return;
  const int16_t* v=(const int16_t*)te->data.data(); size_t n=te->data.size()/2;
  const Group* tg=dat.byName("table"); int tx=137, ty=2; if(tg&&tg->find(T_BITMAP)){ tx=tg->find(T_BITMAP)->bmp.x; ty=tg->find(T_BITMAP)->bmp.y; }   // sprite positions are relative to the table bitmap
  for(size_t i=1;i+1<n;i+=2){ int cls=v[i], gi=v[i+1];
    if(gi<0||(size_t)gi>=dat.groups.size()) continue;
    if(cls==1001||cls==1003||cls==1004||cls==1007||cls==1031||cls==1032) continue;       // plunger, flippers, drain and sound players are handled elsewhere / have no picture
    const Group& g0=dat.groups[gi]; if(!g0.find(T_BITMAP)) continue;
    VisComp c; c.name=g0.name(); c.group=gi; c.cls=cls;
    for(size_t j=gi;j<dat.groups.size();j++){ const Group& g=dat.groups[j]; if(j>(size_t)gi && (g.kind()!=201||!g.name().empty())) break;
      const Entry* be=g.find(T_BITMAP); if(!be) continue; Frame f; f.surf=makeSprite(be->bmp,pal); f.tex=SDL_CreateTextureFromSurface(rr,f.surf);
      f.r={be->bmp.x-tx,be->bmp.y-ty,be->bmp.w,be->bmp.h}; f.depth=readDepth(g,be->bmp,f.surf); c.frames.push_back(std::move(f)); }
    if(c.frames.empty()) continue;
    // Frame shown at start (decided by each class's constructor): bumper caps, targets, gates, plates, rollovers and spinners show their first frame; the rest start hidden.
    switch(cls){ case 1005: case 1006: case 1019: case 1013: case 1014: case 1015: case 1018: c.shown=0; c.baseFrame=0; break; default: c.shown=-1; c.baseFrame=-1; }
    int idx=(int)comps.size(); if(!c.name.empty()) byName[c.name]=idx; byGrp[gi]=idx; comps.push_back(std::move(c)); }
}

void Scene::show(int i,int frame){ if(i<0||(size_t)i>=comps.size()) return; VisComp& c=comps[i]; if(frame>=(int)c.frames.size()) frame=(int)c.frames.size()-1; c.shown=frame; c.timer=0; }
void Scene::flash(int i,int frame,float seconds,int back){ if(i<0||(size_t)i>=comps.size()) return; VisComp& c=comps[i]; if(frame>=(int)c.frames.size()) frame=(int)c.frames.size()-1; c.shown=frame; c.timer=seconds; c.baseFrame=back; }
void Scene::update(float dt){ for(auto& c: comps) if(c.timer>0){ c.timer-=dt; if(c.timer<=0){ c.timer=0; c.shown=c.baseFrame; } } }

void Scene::draw(SDL_Renderer* r,int ox,int oy) const {
  for(const auto& c: comps){ if(c.shown<0||c.shown>=(int)c.frames.size()) continue; const Frame& f=c.frames[c.shown]; SDL_Rect q=f.r; q.x+=ox; q.y+=oy; SDL_RenderCopy(r,f.tex,nullptr,&q); } }

void Scene::occlude(SDL_Renderer* r,const SDL_Rect& ball,int ballDepth,int ox,int oy) const {
  for(const auto& c: comps){ if(c.shown<0||c.shown>=(int)c.frames.size()) continue; const Frame& f=c.frames[c.shown]; SDL_Rect q=f.r; q.x+=ox; q.y+=oy;
    int x0=std::max(q.x,ball.x), y0=std::max(q.y,ball.y), x1=std::min(q.x+q.w,ball.x+ball.w), y1=std::min(q.y+q.h,ball.y+ball.h); if(x0>=x1||y0>=y1) continue;
    for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++){ int sx=x-q.x, sy=y-q.y; uint16_t d=f.depth[sy*f.r.w+sx]; if(d==0xffff||d>=ballDepth) continue;
      uint32_t px=((uint32_t*)((uint8_t*)f.surf->pixels+sy*f.surf->pitch))[sx]; if((px>>24)==0) continue;
      SDL_SetRenderDrawColor(r,(px>>16)&255,(px>>8)&255,px&255,255); SDL_RenderDrawPoint(r,x,y); } } }
}
