#pragma once
#include "corrupted_loot_links.h"
#include "sovereign_loot_guard.h"
namespace LegendNative {



uint64_t __fastcall Drop(void* game,void* unit,void* killer,uint16_t tc,int32_t a,int32_t b,int32_t c,void** items,int32_t* countOut,int32_t maximum) noexcept {
 uint32_t id[3]={};uintptr_t owner=0;
 auto* boss=ready&&unit&&Read(reinterpret_cast<uintptr_t>(unit),id)&&id[0]==1&&BossSkills::Legend(id[1])?BossSkills::NativeBoss(game,id[2],id[1]):nullptr;
 auto* elite=ready&&unit&&Read(reinterpret_cast<uintptr_t>(unit),id)&&id[0]==1&&SovereignNativeData::Leader(id[1])?Find(static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0)),game,id[2],id[1],false):nullptr;
 const bool candidate=ready&&unit&&id[0]==1&&SovereignNativeData::Leader(id[1]);
 const bool live=BossSkills::lookup&&BossSkills::lookup(game,1,id[2])==unit&&Read(reinterpret_cast<uintptr_t>(unit)+0xd8,owner)&&owner==reinterpret_cast<uintptr_t>(game);



 bool prepared=false;if(candidate&&live){
  using PrepareFn=bool(__fastcall*)(void*,void*);HMODULE plugin=GetModuleHandleW(L"SovereignPacks.dll");auto prepare=plugin?reinterpret_cast<PrepareFn>(GetProcAddress(plugin,"R3niSovereignPrepareLootV1")):nullptr;if(prepare)prepared=prepare(game,unit);
 }
 const bool sovereign=candidate&&(prepared||(elite&&elite->thread==GetCurrentThreadId()));
 const bool track=(boss||sovereign)&&live;
 uint32_t mode=99;const bool deathRoute=sovereign&&prepared&&live&&!items&&!countOut&&maximum<=0&&a==0&&Read((uintptr_t)unit+0xc,mode)&&(mode==0||mode==12)&&stat(unit,6,0)<=0;


 if(deathRoute&&!SovereignLootGuard::Take(game,id[2],id[1],tc)){
  Text t;t.Add("ES_SOVEREIGN_DUPLICATE_DEATH_DROP_BLOCKED uid=");t.Num(id[2]);t.Add(" class=");t.Num(id[1]);t.Add(" tc_index=");t.Num(tc);t.Add(" epoch=");t.Num((uint64_t)InterlockedCompareExchange64(&generation,0,0));t.Add(" native_death_reward_once=1 explicit_output_calls_preserved=1");Log(t.s);return 0;
 }
 int32_t observed=0;const bool countOnly=track&&!items&&!countOut&&maximum<=0;
 uint8_t bank=0;Read((uintptr_t)unit+0x1bd,bank);
 const bool corrupted=track&&sovereign&&prepared&&bank>=2&&bank<=3&&(!elite||elite->variant==1)&&(static_cast<uint32_t>(stat(unit,bank==3?452:451,0))%2==1);
 CorruptedLootLinks::Scope scope={};CorruptedLootLinks::Enter(scope,corrupted,bank,id[2]);
 const uint64_t result=priorDrop(game,unit,killer,tc,a,b,c,items,countOnly?&observed:countOut,maximum);
 CorruptedLootLinks::Leave(scope);
 if(track){
  int32_t count=-1;if(countOnly)count=observed;else if(countOut)Read(reinterpret_cast<uintptr_t>(countOut),count);
  Text t;t.Add("ES_NATIVE_LOOT_RESULT uid=");t.Num(id[2]);t.Add(" class=");t.Num(id[1]);t.Add(" tc_index=");t.Num(tc);t.Add(" created_items=");if(count<0)t.Add("unknown");else t.Num(count);if(sovereign){uint8_t bank=0;Read(reinterpret_cast<uintptr_t>(unit)+0x1bd,bank);t.Add(" role=sovereign variant=");t.Num(elite?elite->variant:static_cast<uint32_t>(stat(unit,bank==3?452:451,0))%2);t.Add(" prepared=");t.Num(prepared);t.Add(" marker=");t.Num(stat(unit,bank==3?451:450,0));t.Add(" extra=");t.Num(stat(unit,bank==3?452:451,0));}t.Add(" native_call_once=1 reroll=0 source=native_successful_item_creation");Log(t.s,count==0);
 }
 return result;
}



void* __fastcall LootItem(void* game,void* source,int32_t itemClass,int32_t quality,int32_t explicitIndex,int32_t flags) noexcept {
 return priorLootItem(game,source,itemClass,quality,explicitIndex,flags);
}
}
