// Tiny 8-bit mixer for the game's WAV effects (all are 8-bit mono 11025 Hz). Sounds are addressed by PINBALL.DAT group index.
#pragma once
#include <SDL.h>
#include <map>
#include <string>
#include <vector>
#include "partout.h"
namespace pb {
class Audio {
public:
  bool init(const PartFile& dat,const std::string& dir);
  void play(int group);            // group index of a "soundN.wav" group; unknown ids are ignored
  void shutdown();
  ~Audio(){ shutdown(); }
private:
  struct Clip { std::vector<Uint8> data; Uint32 lastStart=0; };
  struct Voice { const Clip* c; size_t pos; };
  std::map<int,Clip> clips; std::vector<Voice> voices; SDL_AudioDeviceID dev=0;
  static void callback(void* u,Uint8* out,int len);
};
}
