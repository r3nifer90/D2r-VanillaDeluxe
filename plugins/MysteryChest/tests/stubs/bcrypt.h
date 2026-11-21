#pragma once
#include <cstdint>
#include <cstring>
using PUCHAR=unsigned char*;
constexpr unsigned BCRYPT_USE_SYSTEM_PREFERRED_RNG=2;
inline uint32_t mockRandom=0xffffffff;
inline bool mockRngFails=false;
inline int BCryptGenRandom(void*,PUCHAR out,unsigned size,unsigned){if(mockRngFails)return -1;std::memcpy(out,&mockRandom,size);++mockRandom;return 0;}
