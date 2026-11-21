#pragma once
#include "record_map.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>

namespace ChronicleFix {
template<class T> T get(const void* p) { T x; std::memcpy(&x,p,sizeof x);return x; }
template<class T> void put(void* p,T x) { std::memcpy(p,&x,sizeof x); }
struct Evidence { uint16_t source=0; uint32_t minutes=0; };
struct Pending { std::array<Evidence,SetCount> sets{};std::array<Evidence,UniqueCount> uniques{}; };
inline unsigned canonical(unsigned kind,unsigned id) {
 return kind==1 && id<UniqueCount ? UniqueCanonical[id] : id;
}
inline bool valid(unsigned kind,unsigned id) { return kind==0?id<SetCount:kind==1?id<UniqueCount:false; }
inline void remember(Pending& p,unsigned kind,unsigned id,Evidence e) {
 if (!valid(kind,id)||!e.source||!e.minutes) return;
 id=canonical(kind,id);
 if(kind==1&&!UniqueVisible[id])return;
 auto& dst=kind==0?p.sets[id]:p.uniques[id];
 if(!dst.minutes||e.minutes<dst.minutes)dst=e;
}
}
