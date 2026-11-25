// Ball physics prototype. Geometry comes from PINBALL.DAT (attribute 600 walls/circles, 800-series flippers).
// Surface physics follows the original: gravity 25*sin(0.5) down the table, drag 0.2 with random x jitter, max speed 200*radius,
// and the wall response (smoothness/elasticity/kick) decoded from pinball.exe (FUN_01017883). Flipper motion and plunger power are still approximations.
#pragma once
#include <vector>
#include <functional>
#include <cmath>
#include "partout.h"
namespace pb {
struct HitEvent { int grp; float fn; bool kicked; };   // an impact on a component (group index), for sprites/lights
struct Mat { float smooth=0.95f, elast=0.6f, thresh=9.0e10f, kick=0.f; int sndHit=-1, sndKick=-1; int comp=-1, grp=-1; int lvl=1; };   // lvl = collision layer bit of the part (int attr 602): 1 table, 2 ramp, 4 raised platform   // comp = scoring component index or -1   // defaults from FUN_01008fe0
struct Seg { float x0,y0,x1,y1; Mat m; bool oneway=false; float nx=0,ny=0; };  // oneway gate: blocks only a ball approaching the front of its p0->p1 edge, normal (dy,-dx) (FUN_01008beb, FUN_01019204)      // wall segment, e = restitution
struct Disc { float x,y,r; Mat m; };           // round obstacle
struct Flipper { float px,py,pr, tr; float restA, upA, len, A=0;   /* A = signed swing (shortest way round) */ float a=0, w=0; bool left=false, pressed=false;
  float tUp=0.04f, tDown=0.08f, hit=0.9f, reach=1.f; Mat m; };   // times = attrs 804/805, hit = attr 803 (FUN_0101c2ba / FUN_0101be06)
struct Poly { std::vector<float> xs,ys; int comp=-1; int snd=-1; bool inside=false; int grp=-1; int lvl=1; };   // rollover sensor
struct Gate { float x0,y0,x1,y1; int comp=-1, snd=-1; int side=0; bool oneDir=false; int lvl=1; };   // spinner flag: line sensor, scores when the ball crosses it
struct Scorer { std::string name; std::vector<int> pts; int state=0; float cooldown=0; };
struct Hole { float x=0,y=0, fieldR=0, force=0, capR=0; float dx=0,dy=1, spread=0, speed=35, jit=0.05f, hold=1.5f; int comp=-1, sndHit=-1, sndKick=-1; bool active=true, armed=true; std::string name; int lvl=1; };   // kick-out / sink; the gravity well a_kout1 is built inactive until a mission rule enables it (FUN_01019884, FUN_010167dd)
struct Ball { float x=0,y=0,vx=0,vy=0; bool inPlay=false; bool held=false;
  int lvl=1; float z=0; int ramp=-1, tri=-1; float lostT=0; bool falling=false; float vz=0; int fallHole=-1; float fallT=0; };   // lvl = collision layer; z = height of the ball centre above the table minus its radius; ramp = ramp the ball rides on, falling = dropping through the ramp hole
// Ramps (classes 1021/1022, ctor FUN_01019bb8): floor triangles with a plane z = A*x+B*y+C and a slope gravity; portal lines move the ball between layers.
struct Tri { float A,B,C, x[3],y[3], gx,gy; };
struct Portal { float x0,y0,x1,y1; int interior=1; int outLvl=1; bool on=false; };   // crossing it into the ramp side puts the ball on the ramp; crossing out puts it on layer outLvl
struct Ramp { std::string name; std::vector<Tri> tris; Portal entry, exit; float topX0=0,topY0=0,topX1=0,topY1=0; int topLvl=4; bool hasTop=false; int topSide=0; int comp=-1; float drag=0.2f; int lvl=2; float topZ=0; };
struct RampHole { float x=0,y=0,r=0.33f, zEnd=0.3f, grav=0.5f; int comp=-1; int lvl=4; bool seen=false; float hz=0; };
struct World {
  std::vector<Seg> segs; std::vector<Disc> discs; Flipper fl[2]; Ball ball; float radius=0.3f;
  float gx=0.f, gy=11.9856f, drag=0.2f, maxSpeed=60.f, noise=0, noiseT=0;
  float plungerPower=0; /* plungerCharge = seconds held; pull = min(100, 40*seconds): +1 per 25 ms tick (FUN_01017314, step ftol(100/(8 frames*8))=1) */ bool plungerHeld=false; float plungerCharge=0; float startX=-7.021f, startY=10.085f, drainY=14.2f;
  std::vector<Gate> gates; std::vector<Hole> holes; std::vector<int> capCount; int heldBy=-1; float holdT=0;
  std::vector<Ramp> ramps; std::vector<RampHole> rampHoles; std::vector<int> rampEvents; std::vector<int> rampHoleEvents; std::vector<Scorer> scorers; std::vector<Poly> polys; bool gameOver=false; int ballNumber=1;
  int balls=3, score=0; std::vector<HitEvent> hitEvents; std::vector<int> triggers; std::vector<int> events;   // triggers = group indices of sensors that fired (rollovers) for the rules
     // sound groups to play (drained by the app each frame)
  int plungerSnd=-1;
  std::vector<int> rampGrp, rampHoleGrp; void buildRamp(const PartFile& dat,const Group& g,int gi);
  bool build(const PartFile& dat);
  void resetBall();
  bool atPlunger() const { return !ball.held && fabsf(ball.x-startX)<0.9f && ball.y>11.0f && ball.y<12.0f && fabsf(ball.vy)<3.f; }   // ball resting on the plunger face
  void releasePlunger();
  void step(float dt);
  void updateRamps(float px,float py,float dt); void stepFall(float dt);
  // Table nudges (FUN_01014fa4/F52/1015024 -> FUN_01014dcf): impulse (dx,dy)*0.5 on the live ball, reversed 0.4 s later; tilt meter +4/s while a nudge is active, -1/s otherwise
  struct Pending{ float t,dx,dy; }; std::vector<Pending> pending; bool inL=false, inR=false; float tiltMeter=0; bool tilted=false, warned=false; int tiltSnd=-1; int shiftX=0, shiftY=0;   // shift = table picture offset in pixels
  void nudge(int kind);
  void clearNudges(){ pending.clear(); shiftX=shiftY=0; }          // forget undo timers and straighten the picture
  void releaseInput(){ inL=inR=false; plungerHeld=false; plungerCharge=0; clearNudges(); }   // e.g. when the window loses focus
  void newGame(){ clearNudges(); tilted=false; tiltMeter=0; warned=false; score=0; balls=3; ballNumber=1; gameOver=false; resetBall(); }
  void addScore(int comp);
  std::function<void(int)> hitHook;       // when set, every component hit goes to the table rules instead of the plain point table
  std::function<bool()> drainHook;        // called when the ball is lost; returns true when the rules took over (ball saved, extra ball, game over)
  bool postUp=false; int postDisc=-1;      // v_bloc1: the centre post only exists while a rule raises it
  int multiplier=1;
};
}
