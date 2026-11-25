#include "audio.h"
#include <algorithm>
#include <cctype>
namespace pb {
static bool loadClip(const std::string& path,std::vector<Uint8>& out){
  SDL_AudioSpec spec; Uint8* buf=nullptr; Uint32 len=0;
  if(!SDL_LoadWAV(path.c_str(),&spec,&buf,&len)) return false;
  SDL_AudioCVT cvt; if(SDL_BuildAudioCVT(&cvt,spec.format,spec.channels,spec.freq,AUDIO_U8,1,11025)<0){ SDL_FreeWAV(buf); return false; }
  if(cvt.needed){ cvt.len=(int)len; std::vector<Uint8> tmp(len*cvt.len_mult); std::copy(buf,buf+len,tmp.begin()); cvt.buf=tmp.data(); SDL_ConvertAudio(&cvt); out.assign(tmp.begin(),tmp.begin()+cvt.len_cvt); }
  else out.assign(buf,buf+len);
  SDL_FreeWAV(buf); return true; }
bool Audio::init(const PartFile& dat,const std::string& dir){
  for(size_t i=0;i<dat.groups.size();i++){ const Group& g=dat.groups[i]; auto k=g.kind(); const Entry* s=g.find(T_STR9);
    if(k!=0xca || !s || s->data.size()<5) continue; std::string name((const char*)s->data.data());
    if(name.size()<5 || name.compare(name.size()-4,4,".wav")!=0) continue;
    std::vector<Uint8> d; std::string up=name; std::transform(up.begin(),up.end(),up.begin(),[](unsigned char c){return std::toupper(c);}); up.replace(up.size()-4,4,".WAV");
    if(loadClip(dir+"/"+name,d)||loadClip(dir+"/"+up,d)) clips[(int)i].data=std::move(d); }
  SDL_AudioSpec want{}; want.freq=11025; want.format=AUDIO_U8; want.channels=1; want.samples=512; want.callback=&Audio::callback; want.userdata=this;
  SDL_AudioSpec got; dev=SDL_OpenAudioDevice(nullptr,0,&want,&got,0); if(!dev) return false; SDL_PauseAudioDevice(dev,0); return true; }
void Audio::play(int group){
  auto it=clips.find(group); if(it==clips.end()||!dev) return; Uint32 now=SDL_GetTicks(); if(now-it->second.lastStart<60) return;   // rate limit per sound
  SDL_LockAudioDevice(dev); it->second.lastStart=now; if(voices.size()<16) voices.push_back({&it->second,0}); SDL_UnlockAudioDevice(dev); }
void Audio::callback(void* u,Uint8* out,int len){
  Audio* a=(Audio*)u; for(int i=0;i<len;i++){ int acc=0; for(auto& v: a->voices) if(v.pos<v.c->data.size()) acc+=(int)v.c->data[v.pos+0]-128; out[i]=(Uint8)std::max(0,std::min(255,128+acc)); (void)acc; 
    // advance voices once per sample
    for(auto& v: a->voices) v.pos++; }
  a->voices.erase(std::remove_if(a->voices.begin(),a->voices.end(),[](const Voice& v){ return v.pos>=v.c->data.size(); }),a->voices.end()); }
void Audio::shutdown(){ if(dev){ SDL_CloseAudioDevice(dev); dev=0; } voices.clear(); }
}
