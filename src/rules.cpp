#include "rules.h"
#include <algorithm>
namespace pb {

void Rules::init(const PartFile& dat,World& w){
  world=&w; lights.clear(); groups.clear(); bumperSets.clear(); lightByName.clear(); groupByName.clear(); lightByGrp.clear(); rollLight.clear(); banks.clear(); reboLight.clear();
  const Group* to=dat.byName("table_objects"); const Entry* te=to? to->find(T_INTS):nullptr; if(!te) return;
  const int16_t* v=(const int16_t*)te->data.data(); size_t n=te->data.size()/2;
  for(size_t i=1;i+1<n;i+=2){ int cls=v[i], gi=v[i+1]; if(gi<0||(size_t)gi>=dat.groups.size()) continue; const Group& g=dat.groups[gi];
    if(cls==1002){ Light L; L.group=gi; L.name=g.name(); auto a=g.floats(900), b=g.floats(901); if(!a.empty()) L.offT=a[0]; if(!b.empty()) L.onT=b[0];
      if(!L.name.empty()) lightByName[L.name]=(int)lights.size(); lightByGrp[gi]=(int)lights.size(); lights.push_back(L); } }
  for(size_t i=1;i+1<n;i+=2){ int cls=v[i], gi=v[i+1]; if(gi<0||(size_t)gi>=dat.groups.size()) continue; const Group& g=dat.groups[gi];
    if(cls==1026){ LightGroup G; G.group=gi; G.name=g.name(); for(int16_t m: g.ints(1027)){ int li=lightByGrp.count(m)? lightByGrp[m] : -1; if(li>=0) G.members.push_back(li); }
      groupByName[G.name]=(int)groups.size(); groups.push_back(G); }
    else if(cls==1028){ BumperSet S; S.name=g.name(); for(int16_t m: g.ints(1027)) S.bumpers.push_back(dat.groups[m].name()); bumperSets.push_back(S); } }
  // lane lights of the three top rollovers: a_roll3 -> lite8, a_roll2 -> lite9, a_roll1 -> lite10 (handler 0x100c939)
  struct Map{ const char* roll; const char* lite; } lane[]={{"a_roll3","lite8"},{"a_roll2","lite9"},{"a_roll1","lite10"}};
  // target banks (handlers 0x100d8de, 0x100d9c2, 0x100da94, 0x100db68): each target lights its lamp; three lit lamps blink and reset
  auto gidx=[&](const std::string& nm){ for(size_t gi=0;gi<dat.groups.size();gi++) if(dat.groups[gi].name()==nm) return (int)gi; return -1; };
  struct B{ int t0,l0; const char* g; } bk[]={{10,70,"top_circle_tgt_lights"},{13,101,"ramp_tgt_lights"},{16,104,"lchute_tgt_lights"},{19,107,"bpr_solotgt_lights"}};
  for(auto& b: bk){ Bank k; k.grp=group(b.g); for(int i=0;i<3;i++){ k.targets.push_back(gidx("a_targ"+std::to_string(b.t0+i))); k.lites.push_back(light("lite"+std::to_string(b.l0+i))); } if(k.grp>=0) banks.push_back(k); }
  reboLight[gidx("v_rebo1")]=light("lite84"); reboLight[gidx("v_rebo2")]=light("lite85");
  for(auto& m: lane){ for(size_t gi=0;gi<dat.groups.size();gi++) if(dat.groups[gi].name()==m.roll && lightByName.count(m.lite)) rollLight[(int)gi]=lightByName[m.lite]; }
}

int Rules::bumperLevel(const std::string& set) const { for(auto& s: bumperSets) if(s.name==set) return s.level; return 0; }

void Rules::newGame(){ for(auto& l: lights){ l.on=false; l.flashing=false; l.flashLeft=-1; l.phaseOn=false; l.force=-1; } for(auto& s: bumperSets){ s.level=0; s.timer=-1; setBumperLevel(s,0); } }

// ---- light component (messages from FUN_01009ae3) ----
void Rules::lightMsg(int li,int code,float value){
  if(li<0||(size_t)li>=lights.size()) return; Light& l=lights[li];
  auto stopBlink=[&]{ l.flashing=false; l.flashLeft=-1; };
  switch(code){
    case 0: l.on=false; break;                                           // off (a running blink keeps going)
    case 1: l.on=true; break;                                            // on
    case 8: l.force=0; l.forceLeft=value; break;                         // show dark for `value` seconds
    case 9: l.force=1; l.forceLeft=value; break;                         // show lit for `value` seconds
    case 0xf: l.on=true; l.flashing=true; l.flashLeft=(value>0? value : -1); l.phase=0; l.phaseOn=true; break;   // switch on, blinking for a moment (approximation)
    case 4: l.flashing=true; l.flashLeft=-1; l.phase=0; l.phaseOn=true; break;                 // blink until stopped
    case 7: l.flashing=true; l.flashLeft=(value>0? value : -1); l.phase=0; l.phaseOn=true; break;   // blink for `value` seconds
    case 0xe: stopBlink(); break;                                        // stop blinking, show the logical state
    case 0x13: l.on=true;  stopBlink(); break;                           // on + reset display
    case 0x14: l.on=false; stopBlink(); break;                           // off + reset display
    case 0x15: l.on=!l.on; stopBlink(); break;                           // toggle
    default: break; }
}

// ---- light group (messages from FUN_0100a166) ----
int Rules::groupMsg(int gi,int code,float value){
  if(gi<0||(size_t)gi>=groups.size()) return 0; LightGroup& G=groups[gi]; int n=(int)G.members.size();
  if(code==0x10){ for(int m: G.members){ lightMsg(m,7,value); lights[m].on=false; } return 0; }   // blink every member, then all off
  if(code<0x18){ for(int m: G.members) lightMsg(m,code,value); return 0; }       // low codes are broadcast to every member
  switch(code){
    case 0x18: case 0x19: {                                              // rotate the on/off pattern one step (refused while the end light is blinking)
      if(n<2) return 0; for(int m: G.members) if(lights[m].flashing) return 0;
      std::vector<bool> s; for(int m: G.members) s.push_back(lights[m].on);
      for(int i=0;i<n;i++){ int src= code==0x18? (i-1+n)%n : (i+1)%n; lights[G.members[i]].on=s[src]; }
      return 0; }
    case 0x22: return 0;                                                 // stop animation: not modelled
    case 0x23: if(value>=0&&value<n) lightMsg(G.members[(int)value],1,0); return 0;   // member n on
    case 0x24: if(value>=0&&value<n) lightMsg(G.members[(int)value],0,0); return 0;   // member n off
    case 0x25: { int c=0; for(int m: G.members) if(lights[m].on) c++; return c; }     // number lit
    case 0x26: return n;                                                 // number of members
    case 0x27: return 0;                                                 // current animation id: none
    case 0x2d: { int k=(int)value; for(int i=0;i<n;i++) lightMsg(G.members[i], i<=k? 0x13:0x14,0); return 0; }   // progress level: members 0..k on
    default: return 0; }
}

void Rules::setBumperLevel(BumperSet& s,int level){
  s.level=level;
  for(auto& b: s.bumpers) for(auto& sc: world->scorers) if(sc.name==b) sc.state=level;   // score value per level (rules table)
}

void Rules::targetHit(int grp){
  auto rb=reboLight.find(grp); if(rb!=reboLight.end()){ lightMsg(rb->second,9,0.1f); return; }
  for(auto& b: banks) for(int i=0;i<3;i++) if(b.targets[i]==grp){
    lightMsg(b.lites[i],0xf,2.0f);
    if(groupMsg(b.grp,0x25,0)==3) groupMsg(b.grp,0x10,2.0f);
    return; }
}

void Rules::flipperPressed(int which){                                   // handlers 0x100d53a / 0x100d572: rotate the lane lights
  int g1=group("bmpr_inc_lights"), g2=group("ramp_bmpr_inc_lights"); int code= which==0? 0x18 : 0x19;
  groupMsg(g1,code,0); groupMsg(g2,code,0);
}

// ---- rollover handler for the three top lanes (0x100c939) ----
void Rules::rolloverEntered(int grp){
  auto it=rollLight.find(grp); if(it==rollLight.end()) return; int li=it->second; Light& l=lights[li];
  if(l.flashing) return;
  if(!l.on){
    lightMsg(li,0x13,0);                                                 // light the lane
    int gi=group("bmpr_inc_lights");
    if(groupMsg(gi,0x25,0)==groupMsg(gi,0x26,0)){                        // all three lit: celebrate, upgrade the bumpers
      groupMsg(gi,7,5.0f); groupMsg(gi,0,0);                             // blink 5 s, then end up off
      BumperSet* a=bumperSet("attack_bumpers");
      if(a){ if(a->level<3) setBumperLevel(*a,a->level+1); a->timer=60.0f; }   // level up (max 3), 60 s countdown to decay
    }
  } else lightMsg(li,0x14,0);                                            // a lit lane is switched off again
}

void Rules::update(float dt){
  for(auto& l: lights){ if(l.force>=0){ l.forceLeft-=dt; if(l.forceLeft<=0) l.force=-1; }
    if(!l.flashing) continue; l.phase+=dt; float lim=l.phaseOn? l.onT : l.offT; if(l.phase>=lim){ l.phase-=lim; l.phaseOn=!l.phaseOn; }
    if(l.flashLeft>=0){ l.flashLeft-=dt; if(l.flashLeft<=0){ l.flashing=false; l.flashLeft=-1; } } }
  for(auto& s: bumperSets){ if(s.timer<0) continue; s.timer-=dt; if(s.timer<=0){ if(s.level>0){ setBumperLevel(s,s.level-1); s.timer= s.level>0? 60.0f : -1; } else s.timer=-1; } }   // handler 0x100cabf: decay one level per 60 s
}
}
