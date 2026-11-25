#include "rules.h"
#include <algorithm>
#include <cstdio>
namespace pb {

static const int kMult[5]={1,2,3,5,10};
static const char* kRanks[9]={"Cadet","Ensign","Lieutenant","Captain","Lt Commander","Commander","Commodore","Admiral","Fleet Admiral"};
static const char* kMissions[19]={"","","Target Practice","Launch Training","Re-Entry Training","Science","Stray Comet","Black Hole","Space Radiation","Bug Hunt","Alien Menace","Rescue","Satellite Retrieval","Recon","Doomsday Machine","Cosmic Plague","Secret","Time Warp","Maelstrom"};
const char* Rules::rankName(int i){ return kRanks[std::max(0,std::min(8,i))]; }
const char* Rules::missionName(int m){ return (m>=2&&m<=18)? kMissions[m] : ""; }
static std::string fmt(const char* f,int a){ char b[160]; snprintf(b,sizeof b,f,a); return b; }
static std::string fmt2(const char* f,const char* a){ char b[200]; snprintf(b,sizeof b,f,a); return b; }
static bool isTarg(const std::string& n,int a,int b){ if(n.rfind("a_targ",0)!=0) return false; int k=atoi(n.c_str()+6); return k>=a&&k<=b; }

void Rules::init(const PartFile& dat,World& w){
  world=&w; data=&dat; lights.clear(); groups.clear(); bumperSets.clear(); lightByName.clear(); groupByName.clear(); lightByGrp.clear(); soundByName.clear(); fld.clear(); timers.clear();
  const Group* to=dat.byName("table_objects"); const Entry* te=to? to->find(T_INTS):nullptr; if(!te) return;
  const int16_t* v=(const int16_t*)te->data.data(); size_t n=te->data.size()/2;
  for(size_t i=1;i+1<n;i+=2){ int cls=v[i], gi=v[i+1]; if(gi<0||(size_t)gi>=dat.groups.size()) continue; const Group& g=dat.groups[gi];
    if(cls==1002){ Light L; L.group=gi; L.name=g.name(); auto a=g.floats(900), b=g.floats(901); if(!a.empty()) L.offT=a[0]; if(!b.empty()) L.onT=b[0];
      if(!L.name.empty()) lightByName[L.name]=(int)lights.size(); lightByGrp[gi]=(int)lights.size(); lights.push_back(L); } }
  for(size_t i=1;i+1<n;i+=2){ int cls=v[i], gi=v[i+1]; if(gi<0||(size_t)gi>=dat.groups.size()) continue; const Group& g=dat.groups[gi];
    if(cls==1026||cls==1030){ LightGroup G; G.group=gi; G.name=g.name(); for(int16_t m: g.ints(1027)){ int li=lightByGrp.count(m)? lightByGrp[m] : -1; if(li>=0) G.members.push_back(li); }
      auto p=g.floats(903); if(!p.empty()) G.animPeriod=p[0]; groupByName[G.name]=(int)groups.size(); groups.push_back(G); }
    else if(cls==1028){ BumperSet S; S.name=g.name(); for(int16_t m: g.ints(1027)) S.bumpers.push_back(dat.groups[m].name()); bumperSets.push_back(S); } }
  for(size_t gi=0;gi<dat.groups.size();gi++){ std::string nm=dat.groups[gi].name(); if(nm.rfind("soundwave",0)==0) soundByName[nm.substr(9)]=(int)gi; }
  w.hitHook=[this](int c){ hit(c); }; w.drainHook=[this]{ return drained(); };
  highScores.clear();
}

int Rules::bumperLevel(const std::string& set) const { for(auto& s: bumperSets) if(s.name==set) return s.level; return 0; }
int Rules::fuelBars() const { int g=group("fuel_bargraph"); if(g<0) return 0; int c=0; for(int m: groups[g].members) if(lights[m].on) c++; return c; }
int Rules::rankIndex() const { int g=group("middle_circle"); if(g<0) return 0; int c=0; for(int m: groups[g].members) if(lights[m].on) c++; return c; }
int Rules::comp(const std::string& name) const { for(size_t i=0;i<world->scorers.size();i++) if(world->scorers[i].name==name) return (int)i; return -1; }
int Rules::cp(const std::string& name,int state) const { int c=comp(name); if(c<0||world->scorers[c].pts.empty()) return 0; auto& p=world->scorers[c].pts; return p[std::min<size_t>(std::max(0,state),p.size()-1)]; }
void Rules::snd(const char* n){ auto it=soundByName.find(n); if(it!=soundByName.end()){ world->events.push_back(it->second); sndCount++; } }

// ---- scoring (FUN_01017526 / FUN_0100c335) ----
void Rules::addPts(int base){
  if(jackOn) jackpot=std::min(jackpot+base,5000000);
  if(bonusOn) bonusPts=std::min(bonusPts+base,5000000);
  long s=(long)world->score+(long)kMult[mulIdx]*base; world->score=(int)std::min<long>(s,1000000000L); }
void Rules::award(int base){ int mi=mulIdx; bool j=jackOn,b=bonusOn; mulIdx=0; jackOn=bonusOn=false; addPts(base); mulIdx=mi; jackOn=j; bonusOn=b; }

// ---- light component (messages from FUN_01009ae3) ----
void Rules::lightMsg(int li,int code,float value){
  if(li<0||(size_t)li>=lights.size()) return; Light& l=lights[li];
  auto stopBlink=[&]{ l.flashing=false; l.flashLeft=-1; l.offAfterBlink=false; };
  switch(code){
    case 0: l.on=false; break;                                           // off (a running blink keeps going)
    case 1: l.on=true; break;                                            // on
    case 8: l.force=0; l.forceLeft=value; break;                         // show dark for `value` seconds
    case 9: l.force=1; l.forceLeft=value; break;                         // show lit for `value` seconds
    case 0xf: l.on=true; l.flashing=true; l.flashLeft=(value>0? value : -1); l.phase=0; l.phaseOn=true; break;   // switch on, blinking for a moment (approximation)
    case 4: l.flashing=true; l.flashLeft=-1; l.phase=0; l.phaseOn=true; break;                 // blink until stopped
    case 7: l.flashing=true; l.flashLeft=(value>0? value : -1); l.phase=0; l.phaseOn=true; break;   // blink for `value` seconds
    case 0x10: l.flashing=true; l.flashLeft=(value>0? value : 2.f); l.phase=0; l.phaseOn=true; l.offAfterBlink=true; break;   // blink, then go dark
    case 0xe: stopBlink(); break;                                        // stop blinking, show the logical state
    case 0x13: l.on=true;  stopBlink(); l.force=-1; break;               // on + reset display
    case 0x14: l.on=false; stopBlink(); l.force=-1; break;               // off + reset display
    case 0x15: l.on=!l.on; stopBlink(); break;                           // toggle
    default: break; }
}

// ---- light group (messages from FUN_0100a166) ----
int Rules::groupMsg(int gi,int code,float value){
  if(gi<0||(size_t)gi>=groups.size()) return 0; LightGroup& G=groups[gi]; int n=(int)G.members.size();
  auto litCount=[&]{ int c=0; for(int m: G.members) if(lights[m].on) c++; return c; };
  if(code==0x10){ for(int m: G.members) lightMsg(m,0x10,value); return 0; }   // blink every member, then all off
  if(code<0x18){ for(int m: G.members) lightMsg(m,code,value); return 0; }       // low codes are broadcast to every member
  switch(code){
    case 0x18: case 0x19: {                                              // rotate the on/off pattern one step (refused while the end light is blinking)
      if(n<2) return 0; for(int m: G.members) if(lights[m].flashing) return 0;
      std::vector<bool> s; for(int m: G.members) s.push_back(lights[m].on);
      for(int i=0;i<n;i++){ int src= code==0x18? (i-1+n)%n : (i+1)%n; lights[G.members[i]].on=s[src]; }
      return 0; }
    case 0x1a: G.anim=1; G.animPeriod= value>0? value : 0.25f; G.animT=0; G.animStep=0; return 0;        // fill sequence (cosmetic chase)
    case 0x20: G.anim=2; G.animPeriod= value>0? value : 0.25f; G.animT=0; G.animStep=0; return 0;        // drain sequence (cosmetic chase)
    case 0x1b: return 0;                                                 // warning flash of the rank ring (cosmetic)
    case 0x21: for(int i=n-1;i>=0;i--) if(lights[G.members[i]].on){ lightMsg(G.members[i],0x14,0); break; } return 0;   // switch the last lit member off
    case 0x22: G.anim=0; return 0;                                       // stop the running animation
    case 0x23: if(value>=0&&value<n) lightMsg(G.members[(int)value],1,0); return 0;   // member n on
    case 0x24: if(value>=0&&value<n) lightMsg(G.members[(int)value],0,0); return 0;   // member n off
    case 0x25: return litCount();                                        // number lit
    case 0x26: return n;                                                 // number of members
    case 0x27: return G.anim;                                            // running animation id (0 = none)
    case 0x29: for(int i=0;i<n;i++) if(!lights[G.members[i]].on){ lightMsg(G.members[i],0x13,0); break; } return 0;   // light the next member
    case 0x2b: G.timer= value>0? value : -1; G.timerPeriod=value; return 0;   // periodic timer (tick = message 0x3d), negative stops it
    case 0x2c: return 0;
    case 0x2d: { int k=(int)value; for(int i=0;i<n;i++) lightMsg(G.members[i], i<=k? 0x13:0x14,0); return 0; }   // progress level: members 0..k on
    default: return 0; }
}

void Rules::setBumperLevel(BumperSet& s,int level){
  level=std::max(0,std::min(3,level)); bool changed=s.level!=level; s.level=level;
  for(auto& b: s.bumpers) for(auto& sc: world->scorers) if(sc.name==b) sc.state=level;   // score value per level (rules table)
  if(changed && !s.bumpers.empty()){ missionMsg(0xb,s.bumpers[0]); }
}

void Rules::flipperPressed(int which){                                   // handlers 0x100d53a / 0x100d572: rotate the lane lights
  int code= which==0? 0x18 : 0x19; gm("bmpr_inc_lights",code); gm("ramp_bmpr_inc_lights",code);
}

void Rules::update(float dt){
  for(auto& l: lights){ if(l.force>=0){ l.forceLeft-=dt; if(l.forceLeft<=0) l.force=-1; }
    if(!l.flashing) continue; l.phase+=dt; float lim=l.phaseOn? l.onT : l.offT; if(l.phase>=lim){ l.phase-=lim; l.phaseOn=!l.phaseOn; }
    if(l.flashLeft>=0){ l.flashLeft-=dt; if(l.flashLeft<=0){ l.flashing=false; l.flashLeft=-1; if(l.offAfterBlink){ l.on=false; l.offAfterBlink=false; } } } }
  for(auto& s: bumperSets){ if(s.timer<0) continue; s.timer-=dt; if(s.timer<=0){ if(s.level>0){ setBumperLevel(s,s.level-1); s.timer= s.level>0? 60.0f : -1; } else s.timer=-1; } }   // handler 0x100cabf: decay one level per 60 s
  // light group animations and periodic timers
  for(size_t gi=0;gi<groups.size();gi++){ LightGroup& G=groups[gi]; int n=(int)G.members.size();
    if(G.anim){ G.animT+=dt; while(G.animT>=G.animPeriod && G.anim){ G.animT-=G.animPeriod; G.animStep=(G.animStep+1)%(n+1);
        for(int i=0;i<n;i++){ bool on= G.anim==1? i<G.animStep : i>=G.animStep; lights[G.members[i]].on=on; } } }
    if(G.timer>0){ G.timer-=dt; if(G.timer<=0){ G.timer=G.timerPeriod; if(G.name=="top_target_lights"){ if(mulIdx>0){ mulIdx--; groupMsg((int)gi,0x21,0); } if(groupMsg((int)gi,0x25,0)==0) G.timer=-1; }
        else if(G.name=="bumper_target_lights"){ groupMsg((int)gi,0x21,0); if(groupMsg((int)gi,0x25,0)==0) G.timer=-1; }
        else if(G.name=="hyperspace_lights"){ groupMsg((int)gi,0x21,0); if(groupMsg((int)gi,0x25,0)==0) G.timer=-1; } } } }
  // generic one-shot timers
  for(size_t i=0;i<timers.size();){ timers[i].t-=dt; if(timers[i].t<=0){ auto fn=timers[i].fn; timers.erase(timers.begin()+i); fn(); } else i++; }
  // text boxes: the mission box falls back to the persistent mission line
  if(info.left>0){ info.left-=dt; if(info.left<=0){ info.text.clear(); info.left=0; } }
  if(mission.left>0){ mission.left-=dt; if(mission.left<=0){ mission.left=0; mission.text.clear(); missionMsg(0x43,""); } }
  // centre post: up for a while, then flashes before it drops (v_bloc1 handler 0x100c764)
  if(postPhase){ postT-=dt; if(postT<=0){ if(postPhase==1){ postPhase=2; postT=5.f; lm("lite1",7,5.f); } else { postPhase=0; world->postUp=false; lm("lite1",0x14); } } }
  // fuel burns while a mission runs
  if(missionState>=2 && !world->gameOver){ fuelTimer+=dt; int bars=fuelBars(); float period=14.f-2.f*(6-std::max(1,std::min(6,bars)));
    if(fuelTimer>=period){ fuelTimer=0; if(bars>0){ int g=group("fuel_bargraph"); for(int i=(int)groups[g].members.size()-1;i>=0;i--) if(lights[groups[g].members[i]].on){ lightMsg(groups[g].members[i],0x14,0); break; }
        bars=fuelBars(); if(bars==0) fuelAbort(); else if(bars==1) say(mission,"Warning: fuel low",4.f); } } }
  for(size_t i=0;i<world->rampHoleEvents.size();i++){ lm("lite54",7,5.f); note("ramp-hole"); } world->rampHoleEvents.clear(); world->rampEvents.clear();   // message 0x3a of ramp_hole (0x100c8d7)
  if(shootAgainT>0){ shootAgainT-=dt; }
}
}
