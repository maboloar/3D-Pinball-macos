// Background music: plays PINBALL.MID in a loop through macOS's built-in MIDI synthesizer (AudioToolbox MusicPlayer). Toggle with start()/stop().
#pragma once
#include <string>
namespace pb {
class Music {
public:
  bool load(const std::string& path);   // false if the file or synth is unavailable
  void start(); void stop(); void toggle(); bool playing() const { return on; }
  double position() const;              // seconds into the sequence (for tests)
  ~Music();
private:
  void* seq=nullptr; void* player=nullptr; bool on=false; double length=0;
};
}
