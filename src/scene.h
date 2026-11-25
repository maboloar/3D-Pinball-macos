// Component sprites drawn over the table picture: bumper caps, targets, gates, spinner flags, lane lights, plates, lights.
// Each component has one bitmap per state/frame (its own group plus the following unnamed groups) and a per-pixel depth map
// (depth = (z' - 19.5013) * 4303.97, smaller = closer to the camera) used to let the ball pass behind sprites.
#pragma once
#include <SDL.h>
#include <map>
#include <string>
#include <vector>
#include "partout.h"
namespace pb {
struct Frame { SDL_Texture* tex=nullptr; SDL_Surface* surf=nullptr; SDL_Rect r{0,0,0,0}; std::vector<uint16_t> depth; };   // depth: w*h, top row first, 0xffff = no depth
struct VisComp {
  std::string name; int group=-1, cls=0; std::vector<Frame> frames;
  int shown=-1;                 // frame currently drawn, -1 = hidden
  int baseFrame=-1;             // frame to return to after a timed effect
  float timer=0;                // seconds left of a timed effect (0 = none)
  int state=0;                  // logical state index (e.g. bumper level)
};
class Scene {
public:
  void load(const PartFile& dat,SDL_Renderer* r,const uint8_t* pal);
  int find(const std::string& name) const { auto it=byName.find(name); return it==byName.end()? -1 : it->second; }
  int byGroup(int group) const { auto it=byGrp.find(group); return it==byGrp.end()? -1 : it->second; }
  VisComp& at(int i){ return comps[i]; }
  size_t size() const { return comps.size(); }
  void show(int i,int frame);                          // frame -1 hides
  void flash(int i,int frame,float seconds,int back);  // show `frame` for `seconds`, then show `back`
  void update(float dt);
  void draw(SDL_Renderer* r,int ox,int oy) const;
  void occlude(SDL_Renderer* r,const SDL_Rect& ballRect,int ballDepth,int ox,int oy) const;   // redraw sprite pixels that are closer than the ball
  void clear();
private:
  std::vector<VisComp> comps; std::map<std::string,int> byName; std::map<int,int> byGrp;
};
}
