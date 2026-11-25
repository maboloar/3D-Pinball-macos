// Component rule handlers, the mission state machine and the ball flow (see rules.h). Messages: 0x3f hit, 0x42 state entered, 0x43 refresh text,
// 0xb bumper level changed, 0x2f fuel changed. Wording of every on-screen text is this project's own.
#include "rules.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
namespace pb {

static std::string fmt(const char* f,int a){ char b[200]; snprintf(b,sizeof b,f,a); return b; }
static std::string fmts(const char* f,const char* a){ char b[200]; snprintf(b,sizeof b,f,a); return b; }
static bool starts(const std::string& s,const char* p){ return s.rfind(p,0)==0; }
static bool isTarg(const std::string& n,int a,int b){ if(!starts(n,"a_targ")) return false; int k=atoi(n.c_str()+6); return k>=a&&k<=b; }
static bool isBump(const std::string& n,int a,int b){ if(!starts(n,"a_bump")) return false; int k=atoi(n.c_str()+6); return k>=a&&k<=b; }
static bool isSink(const std::string& n){ return n=="v_sink1"||n=="v_sink2"||n=="v_sink3"; }
static bool isLane(const std::string& n){ return n=="a_roll1"||n=="a_roll2"||n=="a_roll3"||n=="a_roll110"||n=="a_roll111"||n=="a_roll112"||n=="a_roll4"||n=="a_roll8"||n=="a_roll6"||n=="a_roll7"||n=="a_roll5"; }

void Rules::extraBallCounter(float secs){   // FUN_0100c2f3: an extra ball is granted
  extraBalls++; snd("28"); say(info,"Extra ball!",secs); note("extra-ball"); }
void Rules::raisePost(float secs){ world->postUp=true; postPhase=1; postT=secs; lm("lite1",0x13); note("post-up"); }
void Rules::fuelAbort(){                  // fuel ran out during a mission (FUN_01011bb5 message 0x2f)
  if(missionState<2) return; gm("l_trek_lights",0x22); gm("l_trek_lights",0x14); gm("r_trek_lights",0x22); gm("r_trek_lights",0x14);
  say(mission,"Mission aborted",4.f); note("mission-aborted"); startState(1); }

bool Rules::promote(int steps){           // FUN_0100e50b: mission rewards fill the outer ring; a full ring promotes the player one rank
  lm("lite16",0x13); int og=group("outer_circle"), mg=group("middle_circle"); for(int i=0;i<steps;i++) groupMsg(og,0x29,2.f);
  int lit=groupMsg(og,0x25,0), size=groupMsg(og,0x26,0);
  if(lit==size){ groupMsg(og,0x10,5.f); groupMsg(mg,0x22,0); int r=groupMsg(mg,0x25,0);
    if(r<9){ groupMsg(mg,0x29,5.f); say(mission,fmts("Promotion to\n%s",rankName(r)),8.f); snd("10"); note("promotion"); return true; } }
  return false; }

void Rules::finish(int pts,const char* title,int steps){   // common tail of a completed mission
  startState(1); say(mission,title,4.f); award(pts); completed++; note(std::string("mission-complete:")+title);
  if(!promote(steps)){ say(mission,fmt("Mission complete\n%d",pts),8.f); snd("9"); } }

void Rules::startState(int s){ missionState=s; stateMsg(0x42,""); }

// ===================== component rule handlers =====================
void Rules::hit(int c){
  if(c<0||(size_t)c>=world->scorers.size()) return; const std::string n=world->scorers[c].name;
  note("hit:"+n); componentRule(n); missionMsg(0x3f,n); }

void Rules::componentRule(const std::string& n){
  auto lite=[&](const char* l){ return lit(l); };
  auto bankDone=[&](const char* grp,int snd14){ gm(grp,0x10,2.f); if(snd14) snd("14"); };
  if(starts(n,"a_bump")){ int c=comp(n); addPts(cp(n,world->scorers[c].state)); return; }
  if(n=="v_rebo1"){ lm("lite84",9,0.1f); addPts(cp(n)); return; }
  if(n=="v_rebo2"){ lm("lite85",9,0.1f); addPts(cp(n)); return; }
  if(n=="v_rebo3"||n=="v_rebo4"){ addPts(cp(n)); return; }
  if(n=="a_roll1"||n=="a_roll2"||n=="a_roll3"){                                      // top lanes (0x100c939)
    if(!lite("lite56") && gm("l_trek_lights",0x27)!=0){ gm("l_trek_lights",0x22); gm("l_trek_lights",0x14); gm("r_trek_lights",0x22); gm("r_trek_lights",0x14); }
    const char* lt= n=="a_roll3"?"lite8": n=="a_roll2"?"lite9":"lite10"; int li=light(lt);
    if(li>=0 && !lights[li].flashing){
      if(!lights[li].on){ lm(lt,0x13);
        if(gm("bmpr_inc_lights",0x25)==gm("bmpr_inc_lights",0x26)){ gm("bmpr_inc_lights",7,5.f); gm("bmpr_inc_lights",0);
          BumperSet* a=bumperSet("attack_bumpers"); if(a && a->level<3){ setBumperLevel(*a,a->level+1); say(info,"Attack bumpers upgraded",2.f); a->timer=60.f; } } }
      else lm(lt,0x14); }
    addPts(cp(n)); return; }
  if(n=="a_roll110"||n=="a_roll111"||n=="a_roll112"){                               // launch lanes (0x100cafe)
    const char* lt= n=="a_roll112"?"lite171": n=="a_roll111"?"lite170":"lite169"; int li=light(lt);
    if(li>=0 && !lights[li].flashing){
      if(!lights[li].on){ lm(lt,0x13);
        if(gm("ramp_bmpr_inc_lights",0x25)==gm("ramp_bmpr_inc_lights",0x26)){ gm("ramp_bmpr_inc_lights",7,5.f); gm("ramp_bmpr_inc_lights",0);
          BumperSet* a=bumperSet("launch_bumpers"); if(a){ if(a->level<3){ setBumperLevel(*a,a->level+1); say(info,"Weapons upgraded",2.f); } a->timer=60.f; } } }
      else lm(lt,0x14); }
    addPts(cp(n)); return; }
  if(n=="a_roll4"||n=="a_roll8"){                                                   // outlane rollovers (0x100cc20)
    if(lite("lite17")||lite("lite18")){ extraBallCounter(2.f); lm("lite17",0x14); lm("lite18",0x14); } else snd("26");
    if(n=="a_roll4"){ if(lite("lite30")){ lm("lite30",4); lm("lite196",4); } } else { if(lite("lite29")){ lm("lite29",4); lm("lite195",4); } }
    addPts(cp(n)); return; }
  if(n=="a_roll6"||n=="a_roll7"){                                                   // bonus lane rollovers (0x100cda0)
    const char* lt= n=="a_roll6"?"lite27":"lite28"; int idx=0; if(lite(lt)){ lm("lite59",0x14); lm(lt,0x14); idx=1; } addPts(cp(n,idx)); return; }
  if(n=="a_roll5"){                                                                  // refuel lane (0x100ce40)
    if(!lite("lite16")){ addPts(cp(n)); snd("25"); say(info,"Ship refuelled",2.f); }
    else { say(info,fmt("Bonus awarded\n%d",bonusPts),2.f); award(bonusPts); lm("lite16",0x14); snd("50"); }
    gm("fuel_bargraph",0x2d,11.f); return; }
  if(n=="a_roll9"){ lm("lite27",0x13); lm("lite28",0x13); return; }
  if(starts(n,"a_roll") && atoi(n.c_str()+6)>=179 && atoi(n.c_str()+6)<=184){      // fuel chute lanes (0x100cf36 ..)
    int i=atoi(n.c_str()+6)-179; static const float lvl[6]={1,3,5,7,9,11}; int thr=2*(i+1);
    if(gm("fuel_bargraph",0x25)<thr){ gm("fuel_bargraph",0x2d,lvl[i]); say(info,"Ship refuelled",2.f); } else lm((std::string("literoll")+std::to_string(179+i)).c_str(),8,0.05f);
    addPts(cp(n)); return; }
  if(n=="a_flag1"||n=="a_flag2"){ addPts(cp(n,lite("lite20")?1:0)); return; }       // spinners: doubled while the flags are upgraded
  if(n=="s_onewy4"){                                                                 // skill shot gate (0x100c63a)
    int k=gm("skill_shot_lights",0x25); if(k!=0){ snd("3"); int p=cp(n,k-1); addPts(p); say(info,fmt("Skill shot\n%d",p*(mulIdx?1:1)*1),2.f);
      if(!lite("lite56")){ gm("l_trek_lights",0x22); gm("l_trek_lights",0x14); gm("r_trek_lights",0x22); gm("r_trek_lights",0x14); }
      gm("skill_shot_lights",0x22); } return; }
  if(n=="s_onewy10"){ gm("skill_shot_lights",0x22); gm("skill_shot_lights",0x14); return; }
  if(n=="s_onewy1"){                                                                 // top of the launch lane (0x100dea5)
    lm("lite200",9,5.f); after(5.f,[this]{ lm("lite200",0x10,5.f); });
    if(lite("lite67")){ gm("skill_shot_lights",0x22); gm("skill_shot_lights",0x14); lm("lite67",0x13); lm("lite54",7,5.f); lm("lite25",7,5.f); gm("fuel_bargraph",0x2d,11.f); snd("14"); } return; }
  if(n=="s_trip1"||n=="s_trip2"||n=="s_trip3"||n=="s_trip4"||n=="s_trip5"){
    if(lite("lite67")){ static const char* t[5]={"lite68","lite69","lite131","lite132","lite133"}; lm(t[n[6]-'1'],0x13); snd("14"); } return; }
  if(n=="ramp"){                                                                     // ramp shot (0x100c7e9)
    int bits=0; if(lite("lite54")){ bits|=1; say(info,fmt("Reflex shot award\n%d",rampBonus),2.f); award(rampBonus); } if(lite("lite55")) bits|=2; if(lite("lite56")) bits|=4;
    if(bits==0){ addPts(cp(n)); snd("30"); } else if(bits==1) snd("21"); else if(bits<4) snd("23"); else snd("24"); return; }
  if(n=="a_kout3"){ int p=cp(n); addPts(p); say(info,fmt("Black hole\n%d",p*1),2.f); return; }   // black hole (0x100dcb0)
  if(n=="a_kout1"){                                                                  // gravity well (0x100dd7a)
    int p=cp(n); addPts(p); say(info,fmt("Gravity normalised\n%d",p),2.f); lm("lite62",0x14); for(auto& h: world->holes) if(h.name=="a_kout1") h.active=false; snd("7"); return; }
  if(n=="a_kout2"){                                                                  // hyperspace chute (0x1011f1a)
    int k=gm("hyperspace_lights",0x25); gm("hyperspace_lights",0x29,2.f); int p=0;
    if(k==0){ p=cp(n,0); addPts(p); say(info,fmt("Hyperspace bonus\n%d",p),2.f); }
    else if(k==2){ raisePost(15.f); p=cp(n,2); addPts(p); say(info,fmt("Centre post\n%d",p),2.f); }
    else if(k==3){ lm("lite17",9,55.f); lm("lite18",9,55.f); after(55.f,[this]{ lm("lite17",7,5.f); lm("lite18",7,5.f); }); p=cp(n,3); addPts(p); say(info,fmt("Extra ball lit\n%d",p),2.f); }
    else if(k==4){ gm("hyperspace_lights",0); p=cp(n,4); addPts(p);
      say(info,fmt("Gravity well\n%d",p),2.f); lm("lite62",4); for(auto& h: world->holes) if(h.name=="a_kout1") h.active=true; }
    if(k==4){                                                                          // all four hyperspace lamps: the full set of bonuses is lit
      if(lite("lite25")){ say(info,fmt("Reflex shot award\n%d",rampBonus),2.f); award(rampBonus); }
      lm("lite130",0x14); mulIdx=4; gm("top_target_lights",0x13); gm("top_target_lights",0x2b,30.f); gm("bumper_target_lights",0x13);
      jackOn=true; lm("lite60",9,60.f); after(60.f,[this]{ jackOn=false; }); bonusOn=true; lm("lite59",9,60.f); after(60.f,[this]{ bonusOn=false; });
      lm("lite20",9,60.f); lm("lite19",9,60.f); lm("lite61",9,60.f); lm("lite58",0x13); lm("lite27",0x13); lm("lite28",0x13);
      lm("lite17",9,55.f); lm("lite18",9,55.f); raisePost(15.f); jackpot=std::max(jackpot,100000); bonusPts=std::max(bonusPts,100000); }
    snd(k==1?"36":k==2?"35":k==3?"38":k==4?"39":"35"); lm("lite25",7,5.f); return; }
  if(isSink(n)){                                                                     // wormholes (0x100d303)
    int idx= n=="v_sink1"?0: n=="v_sink2"?1:2; int tgt=fld["lite4"];
    if(tgt==0){ addPts(cp(n,0)); gm("worm_hole_lights",0x10,2.f); }
    else { fld["lite4"]=0; gm("worm_hole_lights",0x14); gm("bsink_arrow_lights",0x14); lm("lite110",0x14);
      if(tgt==idx+1){ if(!replayGiven){ replayGiven=true; say(info,"Replay awarded",4.f); lm("lite199",0x13); } addPts(cp(n,1)); extraBallCounter(2.f); addPts(10000); snd("14"); }
      else { addPts(cp(n,2)); } }
    return; }
  if(n=="v_sink7"){ return; }
  if(isTarg(n,1,3)||isTarg(n,4,6)||isTarg(n,7,9)){                                   // drop-target banks
    int k=atoi(n.c_str()+6); int bank=(k-1)/3; int& fl=fld[n];
    if(fl!=0){ addPts(cp(n,0)); return; } fl=1;
    int f0=bank*3+1; bool all=fld["a_targ"+std::to_string(f0)]&&fld["a_targ"+std::to_string(f0+1)]&&fld["a_targ"+std::to_string(f0+2)];
    auto reset=[&]{ for(int i=0;i<3;i++) fld["a_targ"+std::to_string(f0+i)]=0; };
    if(bank==0){                                                                    // flags / jackpot / bonus / bonus hold in turn (0x100d5ea)
      if(!all){ addPts(cp(n,0)); return; }
      if(!lite("lite61")){ if(missionState!=0xf && missionState!=0x1d){ lm("lite20",9,60.f); lm("lite19",9,60.f); lm("lite61",9,60.f); say(info,"Flags upgraded",2.f); } snd("47"); }
      else if(!lite("lite60")){ jackOn=true; lm("lite60",9,60.f); after(60.f,[this]{ jackOn=false; }); say(info,"Jackpot lit",2.f); snd("45"); }
      else if(!lite("lite59")){ bonusOn=true; lm("lite59",9,60.f); after(60.f,[this]{ bonusOn=false; }); say(info,"Bonus lit",2.f); snd("46"); }
      else if(!lite("lite58")){ lm("lite58",0x13); say(info,"Bonus hold",2.f); }
      else { addPts(cp(n,1)); snd("48"); }
      if(!lite("lite61")||true) addPts(cp(n,all?1:0)); reset(); return; }
    if(bank==1){                                                                    // bumper target lights (0x101240d)
      if(!all){ addPts(cp(n,0)); return; }
      addPts(cp(n,0)); gm("bumper_target_lights",0x29,2.f); gm("bumper_target_lights",0x2b,30.f); int c=gm("bumper_target_lights",0x25);
      if(c==1){ addPts(cp(n,1)); say(info,"Bumpers upgraded",2.f); } else if(c==2){ addPts(cp(n,2)); say(info,"Bumpers upgraded",2.f); } else { say(info,"Lights full",2.f); extraBallCounter(4.f); }
      reset(); return; }
    if(!all){ addPts(cp(n,0)); return; }                                           // top targets: field multiplier (0x1012570)
    addPts(cp(n,1)); gm("top_target_lights",0x29,2.f); gm("top_target_lights",0x2b,30.f); int c=gm("top_target_lights",0x25); mulIdx=std::max(1,std::min(4,c));
    static const char* mt[5]={"","Field multiplier 2x","Field multiplier 3x","Field multiplier 5x","Field multiplier 10x"}; say(info,mt[mulIdx],2.f); reset(); return; }
  if(isTarg(n,10,22)){
    int k=atoi(n.c_str()+6); if(k==22){ if(!lite("lite110")){ lm("lite110",0xf,3.f); say(info,"Wormhole opened",2.f); addPts(cp(n)); wormholeLights(1); } return; }
    struct B{ int t0; const char* l0; const char* g; const char* fl; } bk[4]={{10,"lite70","top_circle_tgt_lights",nullptr},{13,"lite101","ramp_tgt_lights","lite101"},{16,"lite104","lchute_tgt_lights","lite104"},{19,"lite107","bpr_solotgt_lights","lite107"}};
    int b=(k-10)/3; int i=k-bk[b].t0; std::string lamp=std::string("lite")+std::to_string(atoi(bk[b].l0+4)+i);
    lm(lamp.c_str(),0xf,2.f); if(bk[b].fl) fld[bk[b].fl]|=1<<i; addPts(cp(n));
    if(gm(bk[b].g,0x25)==3){ gm(bk[b].g,0x10,2.f);
      if(b==0){ gm("fuel_bargraph",0x2d,11.f); snd("25"); say(info,"Ship refuelled",2.f); } else if(b==3){ lm("v_gate2",0x35); snd("14"); } else snd("14"); }
    else snd("49D");
    return; }
}

void Rules::wormholeLights(int hole){      // FUN_0100c397: point the wormhole arrows at the next hole
  (void)hole; if(missionState==0x10||missionState==0x16||missionState==0x17) return;
  int t=fld["lite4"]+1; if(t>=4) t=1; fld["lite4"]=t; gm("worm_hole_lights",0x13); gm("bsink_arrow_lights",0x13); }

// ===================== mission machine =====================
void Rules::missionMsg(int code,const std::string& c){
  if(code==0x3f && (c=="plunger")) return;
  if(code==0x2f && missionState>1){ fuelAbort(); return; }
  stateMsg(code,c); }

void Rules::stateMsg(int code,const std::string& c){
  int& cnt=missionSel; int st=missionState;
  auto lit_=[&](const char* l){ return lit(l); };
  auto blink=[&](std::initializer_list<const char*> ls){ for(auto l: ls) lm(l,7,0); };
  auto offl=[&](std::initializer_list<const char*> ls){ for(auto l: ls) lm(l,0x14); };
  auto txt=[&](const std::string& s){ say(mission,s,-1.f); };
  auto trekReset=[&]{ gm("l_trek_lights",0x22); gm("l_trek_lights",0x14); gm("r_trek_lights",0x22); gm("r_trek_lights",0x14); };
  auto trekAnim=[&]{ gm("l_trek_lights",0x14); gm("l_trek_lights",0x20,0.2f); gm("r_trek_lights",0x14); gm("r_trek_lights",0x20,0.2f); };
  auto resetTargs=[&]{ for(int i=1;i<=9;i++) fld["a_targ"+std::to_string(i)]=0; };
  // generic "hit N things" mission: returns after handling 0x3f/0x42/0x43
  auto countdown=[&](bool matches,int start,const char* fmtText,std::function<void()> begin,std::function<void()> end,int pts,const char* title,int steps){
    if(code==0x3f){ if(matches){ if(--cnt<=0){ end(); finish(pts,title,steps); } else stateMsg(0x43,c); } return; }
    if(code==0x42){ cnt=start; begin(); }
    if(code==0x42||code==0x43) txt(fmt(fmtText,cnt)); };
  auto chain=[&](int next,std::initializer_list<const char*> off){ offl(off); startState(next); };   // finish a stage and begin the next one
  if(code==0x43 && st==0){ txt("Awaiting deployment"); return; }
  switch(st){
    case 0: if(code==0x3f && (c=="s_onewy4"||c=="s_onewy10")) startState(1); return;
    case 1: {                                                                        // mission select (0x100e6d2)
      if(code==0x42){ lm("lite198",0x14); gm("outer_circle",0x22); gm("ramp_tgt_lights",0x14); cnt=0; fld["lite101"]=0; trekReset(); gm("goal_lights",0x14); code=0x43; }
      if(code==0x3f){
        int cv=0; if(c=="a_targ13") cv=1; if(c=="a_targ14") cv=2; if(c=="a_targ15") cv=3;
        if(cv==0){
          if(c=="ramp" && lit_("lite56") && fuelBars()!=0){                          // launch ramp accepts the selected mission
            int m=cnt; lm("lite56",0x14); lm("lite198",0x13); offl({"lite317","lite318","lite319"}); award(m<=5? 10000 : (m<=14? 20000 : 30000));
            startState(m); say(mission,fmt("Mission accepted\n%d",m<=5? 10000 : (m<=14? 20000 : 30000)),4.f); note(std::string("mission-start:")+missionName(m)); }
          return; }
        if(fld["lite101"]==7){ fld["lite101"]=0; cv=4; }
        static const int tab[5][5]={{0,0,0,0,0},{0,3,4,2,5},{0,9,0xb,10,0x10},{0,6,8,7,0xf},{0,0xc,0xd,0xe,0x11}}; int rk=rankIndex(); int m=0;
        if(rk==1) m=tab[1][cv]; else if(rk==2||rk==3) m=tab[2][cv]; else if(rk==4||rk==5) m=tab[3][cv]; else if(rk==6||rk==7) m=tab[4][cv];
        else if(rk>=8){ static const int t5[5]={0,0xf,0x10,0x11,0x12}; m=t5[cv]; }
        if(m){ cnt=m; lm("lite56",0xf,2.f); lm("lite198",4); }
        code=0x43; }
      if(code==0x43){
        if(fuelBars()==0){ txt("Refuel the ship"); offl({"lite317","lite319"}); if(!lit_("lite318")) lm("lite318",7); }
        else if(!lit_("lite56")){ txt("Hit mission targets\nto choose a mission"); offl({"lite317","lite318"}); if(!lit_("lite319")) lm("lite319",7); }
        else { txt(fmts("Launch ramp to accept\n%s",missionName(cnt))); offl({"lite318","lite319"}); if(!lit_("lite317")) lm("lite317",7); } }
      return; }
    case 2: countdown(isBump(c,1,4),8,"Attack bumper hits\nleft: %d",[&]{ blink({"lite308","lite311"}); },[&]{ offl({"lite308","lite311"}); },500000,"Target practice passed",6); return;
    case 3: countdown(c=="ramp",3,"Launches left: %d",[&]{ blink({"lite317"}); },[&]{ offl({"lite317"}); },500000,"Launch training passed",6); return;
    case 4: countdown(c=="a_roll1"||c=="a_roll2"||c=="a_roll3",3,"Re-entries left: %d",[&]{ trekAnim(); blink({"lite307"}); },[&]{ offl({"lite307"}); },500000,"Re-entry training passed",6); return;
    case 5: countdown(isTarg(c,1,9),9,"Drop targets\nleft: %d",[&]{ resetTargs(); blink({"lite303","lite309","lite315"}); },[&]{ offl({"lite303","lite309","lite315"}); },750000,"Science mission complete",9); return;
    case 6: case 8: {                                                                // fill a hazard bank, then use the hole
      const char* fl= st==6? "lite107":"lite104"; int bLo= st==6? 19:16;
      if(code==0x3f){
        if(isTarg(c,bLo,bLo+2)){ if(fld[fl]==7){ if(st==6){ lm("lite306",0x14); lm("lite304",7); } else { gm("bsink_arrow_lights",7); lm("lite313",0x14); wormholeLights(1); } fld[fl]=0xf; stateMsg(0x43,c); } }
        else if(fld[fl]==0xf && ((st==6 && c=="a_kout2")||(st==8 && isSink(c)))){ if(st==6) lm("lite304",0x14); finish(1000000,st==6?"Comet destroyed":"Radiation eliminated",8); }
        return; }
      if(code==0x42){ gm(st==6?"bpr_solotgt_lights":"lchute_tgt_lights",0x14); fld[fl]=0; lm(st==6?"lite306":"lite313",7); }
      if(code==0x42||code==0x43) txt(fld[fl]==0xf? (st==6? "Hyperspace launch":"Enter the wormhole") : (st==6? "Fill right hazard banks":"Fill left hazard banks"));
      return; }
    case 7:                                                                          // black hole: upgrade the launch bumpers, then enter the hole
      if(code==0xb && c=="a_bump5"){ stateMsg(0x43,c); return; }
      if(code==0x3f){ if(c=="a_kout3" && bumperLevel("launch_bumpers")!=0){ offl({"lite316","lite314"}); finish(1000000,"Black hole eliminated",8); } return; }
      if(code==0x42){ BumperSet* b=bumperSet("launch_bumpers"); if(b) setBumperLevel(*b,0); }
      if(code==0x42||code==0x43){ if(bumperLevel("launch_bumpers")==0){ txt("Upgrade launch bumpers"); if(lit_("lite314")) lm("lite314",0x14); if(!lit_("lite316")) lm("lite316",7); }
        else { txt("Enter the black hole"); if(lit_("lite316")) lm("lite316",0x14); if(!lit_("lite314")) lm("lite314",7); } }
      return;
    case 9: countdown(isTarg(c,1,22),15,"Targets\nleft: %d",[&]{ resetTargs(); for(const char* g: {"top_circle_tgt_lights","ramp_tgt_lights","lchute_tgt_lights","bpr_solotgt_lights"}) gm(g,0x14); blink({"lite306","lite308","lite310","lite313","lite319"}); },
                       [&]{ offl({"lite306","lite308","lite310","lite313","lite319"}); },750000,"Bugs destroyed",7); return;
    case 10:                                                                         // alien menace: upgrade the attack bumpers first
      if(code==0xb && c=="a_bump1" && bumperLevel("attack_bumpers")!=0){ lm("lite307",0x14); startState(0x14); return; }
      if(code==0x42){ BumperSet* b=bumperSet("attack_bumpers"); if(b) setBumperLevel(*b,0); trekAnim(); blink({"lite307"}); }
      if(code==0x42||code==0x43) txt("Upgrade attack bumpers");
      return;
    case 0x14: countdown(isBump(c,1,4),8,"Attack bumper hits\nleft: %d",[&]{ trekReset(); blink({"lite308","lite311"}); },[&]{ offl({"lite308","lite311"}); },750000,"Aliens repelled",7); return;
    case 11:                                                                         // rescue: upgrade the flags, then use the hyperspace chute
      if(code==0x3f){
        if(isTarg(c,1,3)){ stateMsg(0x43,c); }
        else if(c=="a_kout2" && lit_("lite20")){ if(--cnt<=0){ offl({"lite303","lite304"}); finish(750000,"Survivors rescued",7); } else stateMsg(0x43,c); }
        return; }
      if(code==0x42){ offl({"lite20","lite19"}); cnt=1; }
      if(code==0x42||code==0x43){ if(!lit_("lite20")){ txt("Upgrade the flags"); if(lit_("lite304")) lm("lite304",0x14); if(!lit_("lite303")) lm("lite303",7); }
        else { txt("Hyperspace launch"); if(lit_("lite303")) lm("lite303",0x14); if(!lit_("lite304")) lm("lite304",7); } }
      return;
    case 12: countdown(c=="a_bump4",3,"Remote attack bumper\nhits left: %d",[&]{ blink({"lite308"}); },[&]{ offl({"lite308"}); },1250000,"Satellite repaired",9); return;
    case 13: countdown(isLane(c),15,"Lane passes\nleft: %d",[&]{ blink({"lite301","lite302","lite307","lite316","lite320","lite321"}); },[&]{ offl({"lite301","lite302","lite307","lite316","lite320","lite321"}); },1250000,"Survey complete",9); return;
    case 14: countdown(c=="a_roll4"||c=="a_roll8",3,"Out lane passes\nleft: %d",[&]{ blink({"lite301","lite320"}); },[&]{ offl({"lite301","lite320"}); },1250000,"Doomsday machine destroyed",9); return;
    case 15:                                                                         // cosmic plague: roll the flags, then the space warp rollover
      if(code==0x3f){ if(c=="a_flag1"||c=="a_flag2"){ if(--cnt<=0){ offl({"lite305","lite312"}); startState(0x15); offl({"lite20","lite19"}); } else stateMsg(0x43,c); } return; }
      if(code==0x42){ cnt=75; lm("lite20",0x13); lm("lite19",0x13); blink({"lite305","lite312"}); }
      if(code==0x42||code==0x43) txt(fmt("Roll flags: %d",cnt));
      return;
    case 0x15: if(code==0x3f){ if(c=="a_roll9"){ offl({"lite310"}); finish(1750000,"Plague eliminated",0xb); } return; }
      if(code==0x42) blink({"lite310"}); if(code==0x42||code==0x43) txt("Hit the space warp rollover"); return;
    case 16: if(code==0x3f){ if(c=="v_sink3") startState(0x16); return; }               // secret: three wormholes in order
      if(code==0x42){ gm("worm_hole_lights",0x14); gm("bsink_arrow_lights",0x14); lm("lite110",0x14); lm("lite7",0x13); blink({"lite3"}); } if(code==0x42||code==0x43) txt("Hit the yellow wormhole"); return;
    case 0x16: if(code==0x3f){ if(c=="v_sink1") startState(0x17); return; }
      if(code==0x42){ lm("lite5",0x13); blink({"lite4"}); } if(code==0x42||code==0x43) txt("Hit the red wormhole"); return;
    case 0x17: if(code==0x3f){ if(c=="v_sink2"){ offl({"lite2","lite3","lite4","lite5","lite6","lite7"}); finish(1500000,"Plans recovered",10); } return; }
      if(code==0x42){ lm("lite6",0x13); blink({"lite2"}); } if(code==0x42||code==0x43) txt("Hit the green wormhole"); return;
    case 17: if(code==0x3f){ if(c.rfind("v_rebo",0)==0){ if(--cnt<=0){ offl({"lite300","lite322"}); startState(0x18); } else stateMsg(0x43,c); } return; }   // time warp
      if(code==0x42){ cnt=25; blink({"lite300","lite322"}); } if(code==0x42||code==0x43) txt(fmt("Rebound hits\nleft: %d",cnt)); return;
    case 0x18:
      if(code==0x3f){
        if(c=="a_kout2"){ int r=rankIndex(); if(r>1){ gm("middle_circle",0x21); say(mission,fmts("Demotion to\n%s",rankName(r-2)),8.f); snd("10"); } else say(mission,"Time warp backward",4.f);
          offl({"lite55","lite26","lite304","lite317"}); startState(1); }
        else if(c=="ramp"){ int r=rankIndex(); say(mission,"Time warp forward",4.f); award(2000000); if(r<9){ gm("middle_circle",0x29,5.f); say(mission,fmts("Promotion to\n%s",rankName(r)),8.f); snd("10"); } offl({"lite55","lite26","lite304","lite317"}); startState(1); }
        return; }
      if(code==0x42){ lm("lite55",7,-1); lm("lite26",7,-1); blink({"lite304","lite317"}); }
      if(code==0x42||code==0x43) txt("Hit the hyperspace chute\nor the launch ramp"); return;
    case 18: if(code==0x3f){ if(isTarg(c,1,9)){ if(--cnt<=0) chain(0x19,{"lite303","lite309","lite315"}); else stateMsg(0x43,c); } return; }       // maelstrom, stage 1
      if(code==0x42){ cnt=3; blink({"lite303","lite309","lite315"}); } if(code==0x42||code==0x43) txt(fmt("Drop target hits\nleft: %d",cnt)); return;
    case 0x19: if(code==0x3f){ if(isTarg(c,10,22)){ if(--cnt<=0) chain(0x1a,{"lite306","lite308","lite310","lite313","lite319"}); else stateMsg(0x43,c); } return; }
      if(code==0x42){ cnt=3; blink({"lite306","lite308","lite310","lite313","lite319"}); } if(code==0x42||code==0x43) txt(fmt("Spot target hits\nleft: %d",cnt)); return;
    case 0x1a: if(code==0x3f){ if(isLane(c)){ if(--cnt<=0) chain(0x1b,{"lite301","lite302","lite307","lite316","lite320","lite321"}); else stateMsg(0x43,c); } return; }
      if(code==0x42){ cnt=5; blink({"lite301","lite302","lite307","lite316","lite320","lite321"}); } if(code==0x42||code==0x43) txt(fmt("Lane passes\nleft: %d",cnt)); return;
    case 0x1b: if(code==0x3f){ if(c=="a_roll184") chain(0x1c,{"lite318"}); return; } if(code==0x42){ cnt=0; blink({"lite318"}); } if(code==0x42||code==0x43) txt("Shoot the ball up the fuel chute"); return;
    case 0x1c: if(code==0x3f){ if(c=="ramp") chain(0x1d,{"lite317"}); return; } if(code==0x42) blink({"lite317"}); if(code==0x42||code==0x43) txt("Hit the launch ramp"); return;
    case 0x1d: if(code==0x3f){ if(c=="a_flag1"||c=="a_flag2"){ offl({"lite305","lite312"}); startState(0x1e); offl({"lite20","lite19"}); } return; }
      if(code==0x42){ lm("lite20",0x13); lm("lite19",0x13); blink({"lite305","lite312"}); } if(code==0x42||code==0x43) txt("Hit the flags"); return;
    case 0x1e: if(code==0x3f){ if(isSink(c)) startState(0x1f); return; } if(code==0x42) wormholeLights(1); if(code==0x42||code==0x43) txt("Hit a wormhole"); return;
    case 0x1f: if(code==0x3f){ if(c=="a_kout2"){ offl({"lite304","lite130"}); startState(1); award(5000000); say(info,"Final battle won\n5000000",4.f); completed++; note("maelstrom-complete");
          if(!promote(0x12)){ say(mission,"Mission complete\n5000000",8.f); snd("9"); } } return; }
      if(code==0x42){ lm("lite130",0x13); blink({"lite304"}); } if(code==0x42||code==0x43) txt("Hyperspace chute\nto end the maelstrom"); return;
    case 0x20: if(code==0x42) gm("goal_lights",0x14); if(code==0x42||code==0x43) txt("Game over"); return;
    default: return; }
}

// ===================== ball flow =====================
void Rules::resetLamps(){
  int og=group("outer_circle"), mg=group("middle_circle"); std::vector<bool> keep(lights.size(),false);
  for(int g: {og,mg}) if(g>=0) for(int m: groups[g].members) keep[m]=true;
  for(size_t i=0;i<lights.size();i++) if(!keep[i]){ Light& l=lights[i]; l.on=false; l.flashing=false; l.flashLeft=-1; l.force=-1; l.offAfterBlink=false; }
  for(auto& G: groups){ G.anim=0; G.timer=-1; }
  for(auto& s: bumperSets){ s.timer=-1; setBumperLevel(s,0); }
  for(auto& kv: fld) kv.second=0; timers.clear(); mulIdx=0; jackOn=false; bonusOn=false; world->postUp=false; postPhase=0; postT=0;
  for(auto& h: world->holes) if(h.name=="a_kout1") h.active=false;
  fuelTimer=0; missionSel=0; }

void Rules::launchReady(){                  // message 0x3f8: a new ball is on the plunger
  if(rankIndex()==0) gm("middle_circle",0x29);
  if(!lit("lite200")){                      // fresh ball (not a saved one): arm the skill shot and refill
    gm("skill_shot_lights",0x14); lm("lite67",0x13); gm("skill_shot_lights",0x1a,0.25f); gm("l_trek_lights",0x14); gm("l_trek_lights",0x20,0.2f); gm("r_trek_lights",0x14); gm("r_trek_lights",0x20,0.2f);
    rampBonus=25000; mulIdx=0; gm("top_target_lights",0x14); gm("top_target_lights",0x2b,-1.f); gm("fuel_bargraph",0x13); lm("lite200",0x13); lm("v_gate1",0x35); lm("v_gate2",0x35); }
  missionMsg(0x43,""); }

void Rules::plungerReleased(){ missionMsg(0x43,""); }

void Rules::relaunch(const char* why){ note(std::string("relaunch:")+why); world->resetBall(); world->clearNudges(); world->tilted=false; world->tiltMeter=0; world->warned=false; launchReady(); }

bool Rules::drained(){
  note("drain"); bool tilt=world->tilted; fuelTimer=0;
  if(!tilt){
    if(lit("lite200")){ snd("27"); lm("lite200",0x13); say(info,"Ball saved",2.f); relaunch("ball-save"); return true; }
    if(lit("lite199")){ snd("27"); lm("lite199",0x14); say(info,"Replay ball",2.f); relaunch("replay"); return true; }
    if(extraBalls>0){ extraBalls--; snd("59"); say(info,"Shoot again",-1.f); relaunch("extra-ball"); return true; } }
  if(!tilt && bonusPts>0){ say(info,fmt("Bonus\n%d",bonusPts),2.f); award(bonusPts); }
  bool last= world->balls<=1;
  if(!lit("lite58")) bonusPts=25000;
  resetLamps(); if(last){ missionState=0x20; startState(0x20); } else { missionState=0; stateMsg(0x43,""); }
  if(!last) after(0.01f,[this]{ launchReady(); });
  return false; }

void Rules::newGame(){
  for(auto& l: lights){ l.on=false; l.flashing=false; l.flashLeft=-1; l.phaseOn=false; l.force=-1; l.offAfterBlink=false; }
  for(auto& G: groups){ G.anim=0; G.timer=-1; } resetLamps();
  mulIdx=0; rampBonus=25000; jackpot=20000; bonusPts=25000; extraBalls=0; extraCounter=0; completed=0; replayGiven=false; missionState=0; missionSel=0;
  info=TextBox(); mission=TextBox(); world->hitHook=[this](int c){ hit(c); }; world->drainHook=[this]{ return drained(); };
  launchReady(); }
}
