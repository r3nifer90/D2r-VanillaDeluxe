#pragma once
#include <cstdint>

namespace PLR {
struct Range { uint32_t min, max; };
struct Config {
    uint32_t enabled=1, magic=1, rare=1, unique=1, set=1, crafted=0, runewords=1;
    uint32_t preserve=1, support=0, logRolls=1, salt=1;
    Range difficulty[3]={{10,20},{20,40},{40,60}};
    Range tiers[3]={{10,20},{20,40},{40,60}};
    uint32_t lowMax=35, midMax=65;
};
inline constexpr uint32_t DamageSkills[]={35,38,39,44,45,46,47,48,49,51,53,55,56,59,62,64,67,83,84,92,93,101,112,121,197,225,229,234,240,244,245,256,257,394,398,399};
inline constexpr uint32_t MaxRecords=4096, MaxSkills=1024;
struct Anchor { const char* name; uint16_t lvl, required; };
inline constexpr Anchor UniqueAnchors[]={
    {"Gleamscythe",13,13},{"The Rising Sun",73,65},{"Griffon's Eye",84,76},
    {"Stormlash",86,82},{"Bonehew",72,64}
};
inline constexpr Anchor SetAnchors[]={
    {"Immortal King's Soul Cage",37,76},{"M'avina's Caster",21,70},
    {"Cow King's Hide",20,18},{"Naj's Circlet",43,28},{"Merlin's Amulet",80,73}
};
inline uint64_t Mix(uint64_t x) noexcept {
    x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
    x=(x^(x>>27))*0x94d049bb133111ebULL;
    return x^(x>>31);
}
inline uint32_t RandomLevel(uint64_t key,Range r) noexcept {
    const uint64_t count=r.max-r.min+1;
    const uint64_t reject=(uint64_t{0}-count)%count;
    uint64_t x;
    do {key+=0x9e3779b97f4a7c15ULL;x=Mix(key);} while(x<reject);
    return r.min+static_cast<uint32_t>(x%count);
}
inline Range ItemRange(const Config& c,uint32_t level) noexcept {
    return c.tiers[level<=c.lowMax ? 0 : level<=c.midMax ? 1 : 2];
}
inline Range PreserveFloor(Range r,uint32_t original) noexcept {
    if(original>=r.max) return {original,original};
    if(original>r.min) r.min=original;
    return r;
}
}
