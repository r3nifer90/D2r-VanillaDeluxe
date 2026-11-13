#pragma once
namespace LegendNative::SovereignLootGuard {
using namespace ES;



struct Claim {uint64_t epoch=0;void* game=nullptr;uint32_t uid=0,kind=0,tc=0;};
constexpr uint32_t Capacity=16384;
Claim claims[Capacity];
SRWLOCK claimLock=SRWLOCK_INIT;
bool Take(void* game,uint32_t uid,uint32_t kind,uint32_t tc) noexcept {
 const uint64_t epoch=(uint64_t)InterlockedCompareExchange64(&generation,0,0);
 if(!epoch)return true;
 const uint32_t hash=(uint32_t)(((uintptr_t)game>>4)^uid*2654435761u^kind*2246822519u^tc*3266489917u)&(Capacity-1);
 AcquireSRWLockExclusive(&claimLock);
 for(uint32_t n=0;n<Capacity;++n){auto& c=claims[(hash+n)&(Capacity-1)];
  if(c.epoch!=epoch){c={epoch,game,uid,kind,tc};ReleaseSRWLockExclusive(&claimLock);return true;}
  if(c.game==game&&c.uid==uid&&c.kind==kind&&c.tc==tc){ReleaseSRWLockExclusive(&claimLock);return false;}
 }
 ReleaseSRWLockExclusive(&claimLock);Log("ES_SOVEREIGN_LOOT_GUARD_FULL native_drop_preserved=1",true);return true;
}
}
