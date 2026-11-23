#pragma once
#include <cstdint>
#include <cstddef>
namespace SelectedLoot {
// Runtime record IDs from the supplied 2026-11-23 tables. No item rows are added.
inline constexpr uint32_t Records[] = {241,252,253,260,264,271,294,305,310,353,360,410,425,426,428,453,455,456,766,777,778,785,788,795,817,826,831,870,875,900,915,916,918,942,944,945,987,1221,1232,1233,1240,1243,1250,1272,1280,1285,1324,1329,1365,1380,1381,1383,1407,1409,1410,1453,1481,1485,1489,1492,1493,1501,1505,1524,1531,1534,1536,1539,1540,1544,1547,1548,1606,1607,1609,1611,1690,1694,1698,1701,1702,1710,1714,1733,1740,1743,1745,1748,1749,1753,1756,1757,1815,1816,1818,1820,1846,1852,1858,1859,1888,1890};
inline bool targeted(int id) noexcept {for(auto x:Records)if(id>=0&&x==uint32_t(id))return true;return false;}
inline bool nativeLootFrame(uintptr_t frame,uintptr_t exe) noexcept {
 // 95032 native treasure-class executor and its public drop entry. Item creation,
 // cube recipes, vendors and the unique-quality selector are outside this range.
 return frame>=exe+0x440000 && frame<exe+0x441c00;
}
inline bool nativeLootStack(const void* const* frames,size_t count,uintptr_t exe) noexcept {
 for(size_t i=0;i<count;++i)if(nativeLootFrame(reinterpret_cast<uintptr_t>(frames[i]),exe))return true;
 return false;
}
inline bool reject(bool liveScope,bool mystery,bool nativeLoot,int id,bool entropyOK,uint32_t random) noexcept {
 return liveScope&&!mystery&&nativeLoot&&targeted(id)&&entropyOK&&(random&1u)==0;
}
}
