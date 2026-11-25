// Reader for PARTOUT(4.0)RESOURCE files (PINBALL.DAT, FONT.DAT).
// Format reconstructed from the loader at 0x10064e2 in pinball.exe.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pb {
enum EntryType : uint8_t { T_SHORT=0, T_BITMAP=1, T_SHORT2=2, T_STRING=3, T_STR9=9, T_INTS=10, T_FLOATS=11, T_VISUAL=12, T_PALETTE=5 };
struct Bitmap { int16_t w=0,h=0,x=0,y=0; uint8_t res=0,flags=0; std::vector<uint8_t> pixels; /* 8-bit, rows bottom-up, stride (w+3)&~3 */ };
struct Entry { uint8_t type=0; std::vector<uint8_t> data; Bitmap bmp; };
struct Group { std::vector<Entry> entries;
  const Entry* find(uint8_t t) const { for (auto& e: entries) if (e.type==t) return &e; return nullptr; }
  // Attribute records: type 10 = int16 [id, payload...], type 11 = float32 [id, payload...]. Returns payload (without id).
  std::vector<float> floats(int id) const;
  std::vector<int16_t> ints(int id) const;
  // Type-10 records are usually flat chains of (id,value) pairs; returns the value for id anywhere in the chain, or -1.
  int intAttr(int id) const;
  int kind() const { auto e=find(T_SHORT); if(!e||e->data.size()<2) return -1; return (int)(int16_t)(e->data[0]|(e->data[1]<<8)); }
  std::string name() const { auto e=find(T_STRING); return e? std::string((const char*)e->data.data()) : std::string(); } };
struct PartFile { std::string description; std::vector<Group> groups;
  bool load(const std::string& path, std::string* err=nullptr);
  const Group* byName(const std::string& n) const { for (auto& g: groups) if (g.name()==n) return &g; return nullptr; } };
}
