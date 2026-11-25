// Table rules: lights, light groups, component handlers, the mission state machine, bonuses, multipliers and ball flow.
// Independent of SDL so it can be tested headless; the app copies the resulting lamp states and text boxes onto the screen each frame.
// Light messages follow the original component (class 1002); group messages follow class 1026 (members come from int attr 1027).
// Handler behaviour was recovered from pinball.exe (component rule handlers 0x100c44b..0x100e0ed, 0x101240d, 0x1012570, 0x1011f1a,
// 0x10126ba, mission dispatcher 0x1011bb5 and its 33 state handlers); all on-screen wording is this project's own.
#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "partout.h"
#include "physics.h"
namespace pb {

struct Light {
  int group=-1; std::string name;
  bool on=false;             // logical state
  bool flashing=false;       // blinking overrides the logical state while true
  float flashLeft=-1;        // seconds of blinking left (<0 = until stopped)
  float phase=0; bool phaseOn=false; float onT=0.1f, offT=0.1f;   // blink timing (float attrs 900/901)
  int force=-1; float forceLeft=0;   // temporary override: 1 = shown, 0 = hidden (messages 9 / 8)
  bool offAfterBlink=false;          // message 0x10: blink, then go dark
  bool visible() const { return force>=0? force==1 : (flashing? phaseOn : on); }
  bool lit() const { return on || flashing || force==1; }          // FUN_0100c092
};
struct LightGroup { int group=-1; std::string name; std::vector<int> members; int anim=0; float animPeriod=0.2f, animT=0; int animStep=0; float timer=-1, timerPeriod=0; };      // members = light indices; anim 1 = fill sequence, 2 = drain sequence
struct BumperSet { std::string name; std::vector<std::string> bumpers; int level=0; float timer=-1; };   // attack_bumpers / launch_bumpers
struct TextBox { std::string text; float left=0; };                                                       // info / mission panels (left < 0 = stays)

class Rules {
public:
  void init(const PartFile& dat,World& w);
  void update(float dt);
  void flipperPressed(int which);         // 0 = left (rotate lights one way), 1 = right (the other way)
  void newGame();
  void launchReady();                     // a new ball sits on the plunger (message 0x3f8)
  void plungerReleased();                 // the plunger was let go (message 0x3f7)
  // component hit (message 0x3f to the component's handler, then to the mission machine)
  void hit(int comp);
  void hitName(const std::string& n){ hit(comp(n)); }
  bool drained();                         // ball lost (message 0x3f to `drain`); returns true when the rules took over the ball
  // light API (original message codes)
  void lightMsg(int light,int code,float value);
  int  groupMsg(int g,int code,float value);   // returns the query result for 0x25 (lit count) / 0x26 (size) / 0x27 (animation id)
  int  light(const std::string& name) const { auto it=lightByName.find(name); return it==lightByName.end()? -1 : it->second; }
  int  group(const std::string& name) const { auto it=groupByName.find(name); return it==groupByName.end()? -1 : it->second; }
  int  lightByGroup(int g) const { auto it=lightByGrp.find(g); return it==lightByGrp.end()? -1 : it->second; }
  int  bumperLevel(const std::string& set) const;
  std::vector<Light> lights; std::vector<LightGroup> groups; std::vector<BumperSet> bumperSets;
  TextBox info, mission;                  // text boxes on the info panel
  // table state (exposed for the UI and the tests)
  int missionState=0, missionSel=0;       // mission machine state (0 idle, 1 selecting, 2..18 missions, 20..31 sub-states, 32 game over) and the selected mission
  int mulIdx=0;                           // field multiplier index: x1 x2 x3 x5 x10
  int rampBonus=25000, jackpot=20000, bonusPts=25000; bool jackOn=false, bonusOn=false;
  int extraBalls=0, extraCounter=0, completed=0; bool replayGiven=false;
  float fuelTimer=0;
  int sndCount=0;                         // number of rule-triggered sounds (for tests)
  std::vector<std::string> log;           // short event names, newest last (for tests; trimmed)
  // high scores (top five, kept by the app in the user's folder)
  struct HS { std::string name; int score; }; std::vector<HS> highScores;
  void lm_test(const char* n,int code,float v=0){ lm(n,code,v); }   // tests only
  void startState_test(int s){ startState(s); }
  bool lit(const std::string& lightName) const { int i=light(lightName); return i>=0 && lights[i].lit(); }
  int fuelBars() const;
  int rankIndex() const;                  // middle_circle pips lit = rank
  static const char* rankName(int i);
  static const char* missionName(int m);
private:
  World* world=nullptr; const PartFile* data=nullptr;
  std::map<std::string,int> lightByName, groupByName, soundByName; std::map<int,int> lightByGrp;
  std::map<std::string,int> fld;          // the per-component "+6" field used by several handlers
  struct Timer{ float t; std::function<void()> fn; }; std::vector<Timer> timers;
  float postT=0; int postPhase=0;
  float shootAgainT=-1; bool skillActive=false;
  int ballsLeftOnNewGame=3;
  void lm(const char* n,int code,float v=0){ lightMsg(light(n),code,v); }
  int  gm(const char* n,int code,float v=0){ return groupMsg(group(n),code,v); }
  void snd(const char* n);                // plays soundwave<n>
  void say(TextBox& b,const std::string& s,float secs){ b.text=s; b.left=secs; }
  void addPts(int base);                  // FUN_01017526: score with multiplier, track bonus/jackpot
  void award(int base);                   // FUN_0100c335: plain award, no multiplier, not tracked
  int  cp(const std::string& name,int state=0) const;
  int  comp(const std::string& name) const;
  void setBumperLevel(BumperSet& s,int level);
  BumperSet* bumperSet(const std::string& n){ for(auto& s: bumperSets) if(s.name==n) return &s; return nullptr; }
  void componentRule(const std::string& n);
  void missionMsg(int code,const std::string& comp);
  void stateMsg(int code,const std::string& comp);      // handler of the current mission state
  void startState(int s);                 // set the state and run its start handler (message 0x42)
  void finish(int pts,const char* title,int rankSteps);
  bool promote(int steps);                // FUN_0100e50b
  void extraBallCounter(float secs);      // FUN_0100c2f3 / FUN_0100c281
  void raisePost(float secs);
  void fuelAbort();
  void wormholeLights(int hole);
  void relaunch(const char* why);
  void resetLamps();
  void after(float secs,std::function<void()> fn){ timers.push_back({secs,fn}); }
  int  f(const char* n){ return fld[n]; }
  void note(const std::string& s){ log.push_back(s); if(log.size()>200) log.erase(log.begin(),log.begin()+100); }
};
}
