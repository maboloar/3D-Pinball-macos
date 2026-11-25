#include "partout.h"
#include <cstdio>
#include <cstring>
namespace pb {
static const int16_t kSizes[14] = {2,-1,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0};
static uint32_t rd32(const uint8_t* p){ uint32_t v; memcpy(&v,p,4); return v; }
static uint16_t rd16(const uint8_t* p){ uint16_t v; memcpy(&v,p,2); return v; }
std::vector<float> Group::floats(int id) const {
  for(auto& e: entries) if(e.type==T_FLOATS && e.data.size()>=4){ const float* f=(const float*)e.data.data(); size_t n=e.data.size()/4;
    if((int)f[0]==id) return std::vector<float>(f+1,f+n); }
  return {}; }
std::vector<int16_t> Group::ints(int id) const {
  for(auto& e: entries) if(e.type==T_INTS && e.data.size()>=2){ const int16_t* v=(const int16_t*)e.data.data(); size_t n=e.data.size()/2;
    if(v[0]==id) return std::vector<int16_t>(v+1,v+n); }
  return {}; }
int Group::intAttr(int id) const {
  for(auto& e: entries) if(e.type==T_INTS && e.data.size()>=4){ const int16_t* v=(const int16_t*)e.data.data(); size_t n=e.data.size()/2;
    if(v[0]==1025) continue;                      // table_objects list, not a chain
    for(size_t i=0;i+1<n;){ int a=v[i]; if(a==id) return v[i+1]; i+= (a==1500)? 9 : 2; } }
  return -1; }
int Group::layerMask() const {
  int m=0; for(auto& e: entries) if(e.type==T_INTS && e.data.size()>=4){ const int16_t* v=(const int16_t*)e.data.data(); size_t n=e.data.size()/2; if(v[0]==1025) continue;
    for(size_t i=0;i+1<n;){ int a=v[i]; if(a==602 && v[i+1]>=0 && v[i+1]<8) m|=1<<v[i+1]; i+= (a==1500)? 9 : 2; } }
  return m? m : 1; }
bool PartFile::load(const std::string& path, std::string* err){
  auto fail=[&](const char* m){ if(err) *err=m; return false; };
  FILE* f=fopen(path.c_str(),"rb"); if(!f) return fail("cannot open file");
  std::vector<uint8_t> d; uint8_t buf[65536]; size_t n; while((n=fread(buf,1,sizeof buf,f))>0) d.insert(d.end(),buf,buf+n); fclose(f);
  if(d.size()<183 || memcmp(d.data(),"PARTOUT(4.0)RESOURCE",20)!=0) return fail("bad header");
  description=(const char*)&d[71];
  uint16_t ngroups=rd16(&d[175]); size_t pos=183+rd16(&d[181]);
  auto need=[&](size_t k){ return pos+k<=d.size(); };
  groups.assign(ngroups,Group());
  for(auto& g: groups){
    if(!need(1)) return fail("truncated"); uint8_t cnt=d[pos++]; g.entries.resize(cnt);
    for(auto& e: g.entries){
      if(!need(1)) return fail("truncated"); e.type=d[pos++]; if(e.type>13) return fail("unknown entry type");
      int32_t len=kSizes[e.type];
      if(len<0){ if(!need(4)) return fail("truncated"); len=(int32_t)rd32(&d[pos]); pos+=4; }
      if(e.type==T_BITMAP){
        if(!need(14)) return fail("truncated"); const uint8_t* h=&d[pos];
        e.bmp.res=h[0]; e.bmp.w=(int16_t)rd16(h+1); e.bmp.h=(int16_t)rd16(h+3); e.bmp.x=(int16_t)rd16(h+5); e.bmp.y=(int16_t)rd16(h+7);
        uint32_t sz=rd32(h+9); e.bmp.flags=h[13]; pos+=14; if(!need(sz)) return fail("truncated bitmap");
        e.bmp.pixels.assign(d.begin()+pos,d.begin()+pos+sz); pos+=sz;
      } else { if(!need(len)) return fail("truncated"); e.data.assign(d.begin()+pos,d.begin()+pos+len); pos+=len; }
    }
  }
  if(pos!=d.size()) return fail("trailing data");
  return true;
}
}
