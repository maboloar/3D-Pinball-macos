#include "music.h"
#include <AudioToolbox/AudioToolbox.h>
namespace pb {
bool Music::load(const std::string& path){
  MusicSequence s=nullptr; MusicPlayer p=nullptr;
  if(NewMusicSequence(&s)!=noErr) return false;
  CFURLRef url=CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,(const UInt8*)path.c_str(),(CFIndex)path.size(),false);
  OSStatus st=MusicSequenceFileLoad(s,url,kMusicSequenceFile_MIDIType,kMusicSequenceLoadSMF_ChannelsToTracks); CFRelease(url);
  if(st!=noErr){ DisposeMusicSequence(s); return false; }
  if(NewMusicPlayer(&p)!=noErr){ DisposeMusicSequence(s); return false; }
  if(MusicPlayerSetSequence(p,s)!=noErr){ DisposeMusicPlayer(p); DisposeMusicSequence(s); return false; }
  UInt32 n=0; MusicSequenceGetTrackCount(s,&n); MusicTimeStamp len=0;
  for(UInt32 i=0;i<n;i++){ MusicTrack t=nullptr; if(MusicSequenceGetIndTrack(s,i,&t)!=noErr) continue; MusicTimeStamp tl=0; UInt32 sz=sizeof tl; MusicTrackGetProperty(t,kSequenceTrackProperty_TrackLength,&tl,&sz); if(tl>len) len=tl; }
  for(UInt32 i=0;i<n;i++){ MusicTrack t=nullptr; if(MusicSequenceGetIndTrack(s,i,&t)!=noErr) continue; MusicTrackLoopInfo li{len,0}; MusicTrackSetProperty(t,kSequenceTrackProperty_LoopInfo,&li,sizeof li); }   // loop forever
  Float64 secs=0; MusicSequenceGetSecondsForBeats(s,len,&secs); length=secs;
  if(MusicPlayerPreroll(p)!=noErr){ DisposeMusicPlayer(p); DisposeMusicSequence(s); return false; }
  seq=s; player=p; return true; }
void Music::start(){ if(player && !on){ if(MusicPlayerStart((MusicPlayer)player)==noErr) on=true; } }
void Music::stop(){ if(player && on){ MusicPlayerStop((MusicPlayer)player); on=false; } }
void Music::toggle(){ if(on) stop(); else start(); }
double Music::position() const { if(!player) return -1; MusicTimeStamp t=0; MusicPlayerGetTime((MusicPlayer)player,&t); Float64 secs=0; MusicSequenceGetSecondsForBeats((MusicSequence)seq,t,&secs); return secs; }
Music::~Music(){ if(player){ MusicPlayerStop((MusicPlayer)player); DisposeMusicPlayer((MusicPlayer)player); } if(seq) DisposeMusicSequence((MusicSequence)seq); }
}
