// Table camera: 3x4 matrix + focal length from group "camera_info" (verified against the table art).
#pragma once
#include <array>
namespace pb {
struct Camera { float M[12]={}, F=-400.f, cx=300.f, cy=208.f;
  void project(float x,float y,float z,float& sx,float& sy) const {
    float xp=M[0]*x+M[1]*y+M[2]*z+M[3], yp=M[4]*x+M[5]*y+M[6]*z+M[7], zp=M[8]*x+M[9]*y+M[10]*z+M[11];
    sx=cx+F*xp/zp; sy=cy+F*yp/zp; }
  // 16-bit depth used by sprite depth maps (FUN_01013705: centre 19.5013, scale 4303.97); smaller = closer
  int depth16(float x,float y,float z) const { float zp=M[8]*x+M[9]*y+M[10]*z+M[11]; float d=(zp-19.5013f)*4303.97f; return d<0?0:(d>65534?65534:(int)d); } };
}
