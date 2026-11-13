#pragma once

#include <cstdint>
namespace SPAffixPool {
constexpr uint32_t Slots=4,MaxCandidates=13;
constexpr uint16_t NativeIds[]={5,6,7,8,9,17,18,25,26,27,28,29,30};


template<class Random>
bool Draw(uint16_t* pool,uint32_t count,uint32_t wanted,uint16_t* out,Random random) noexcept {
 if(count>MaxCandidates||wanted>Slots||wanted>count)return false;
 for(uint32_t i=0;i<count;++i){
  if(!pool[i]||pool[i]>=32)return false;
  for(uint32_t j=0;j<i;++j)if(pool[j]==pool[i])return false;
 }
 for(uint32_t i=0;i<wanted;++i){const uint32_t j=i+random(count-i);if(j>=count)return false;const uint16_t t=pool[i];pool[i]=pool[j];pool[j]=t;out[i]=pool[i];}
 return true;
}
}
