// Table rules: lights, light groups and the component handlers that drive them. Independent of SDL so it can be tested headless;
// the app copies the resulting visibility/levels into the sprite scene each frame.
// Light messages follow the original component (class 1002); group messages follow class 1026 (members come from int attr 1027).
#pragma once
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
  bool visible() const { return force>=0? force==1 : (flashing? phaseOn : on); }
};
struct LightGroup { int group=-1; std::string name; std::vector<int> members; };      // members = light indices
struct BumperSet { std::string name; std::vector<std::string> bumpers; int level=0; float timer=-1; };   // attack_bumpers / launch_bumpers

class Rules {
public:
  void init(const PartFile& dat,World& w);
  void update(float dt);
  // table messages
  void rolloverEntered(int grp);          // a_roll* sensor entered (message 0x3f to the rollover handler)
  void targetHit(int grp);                // a target / rebound was struck (message 0x3f to its handler)
  void flipperPressed(int which);         // 0 = left (rotate lights one way), 1 = right (the other way)
  void newGame();
  // light API (original message codes)
  void lightMsg(int light,int code,float value);
  int  groupMsg(int g,int code,float value);   // returns the query result for 0x25 (lit count) / 0x26 (size) / 0x27 (animation id)
  int  light(const std::string& name) const { auto it=lightByName.find(name); return it==lightByName.end()? -1 : it->second; }
  int  group(const std::string& name) const { auto it=groupByName.find(name); return it==groupByName.end()? -1 : it->second; }
  int  lightByGroup(int g) const { auto it=lightByGrp.find(g); return it==lightByGrp.end()? -1 : it->second; }
  int  bumperLevel(const std::string& set) const;
  std::vector<Light> lights; std::vector<LightGroup> groups; std::vector<BumperSet> bumperSets;
private:
  World* world=nullptr; std::map<std::string,int> lightByName, groupByName; std::map<int,int> lightByGrp;
  struct Bank{ std::vector<int> targets; std::vector<int> lites; int grp; };
  std::vector<Bank> banks; std::map<int,int> reboLight;
  std::map<int,int> rollLight;            // rollover group -> lane light
  void setBumperLevel(BumperSet& s,int level);
  BumperSet* bumperSet(const std::string& n){ for(auto& s: bumperSets) if(s.name==n) return &s; return nullptr; }
};
}
