#pragma once
#include <stdint.h>
namespace Forsaken {
struct Config {uint32_t enabled=1,act=20,uber=10,minDistance=100,maxDistance=150;};
inline bool Valid(const Config& c){return c.enabled<=1&&c.act<=100&&c.uber<=100&&c.act+c.uber<=100&&c.minDistance>=16&&c.minDistance<=c.maxDistance&&c.maxDistance<=500;}
inline uint32_t Select(const Config& c,uint32_t roll){return roll<c.act?1:roll<c.act+c.uber?2:0;}
inline uint64_t DistanceSquared(int32_t x,int32_t y,int32_t a,int32_t b){int64_t dx=int64_t(x)-a,dy=int64_t(y)-b;return uint64_t(dx*dx+dy*dy);}
inline bool InRange(const Config& c,int32_t x,int32_t y,int32_t a,int32_t b){auto d=DistanceSquared(x,y,a,b);return d>=uint64_t(c.minDistance)*c.minDistance&&d<=uint64_t(c.maxDistance)*c.maxDistance;}
}
