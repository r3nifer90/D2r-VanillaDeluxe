#pragma once
namespace LegendNative {
void CompleteCreatureCast(void* game,void* unit,uint32_t id,uint32_t level,uint64_t cast) noexcept {
 using namespace BossSkills;uint32_t ident[3]={};if(!ready||!CreatureSummon(id)||!unit||!Read((uintptr_t)unit,ident)||ident[0]!=1)return;
 auto* b=NativeBoss(game,ident[2],ident[1]);if(!b||!NativeRoot(*b,unit)||!b->nativeStarted||b->nativeStarted!=cast||b->nativeSkill!=id||b->nativeHandlers||b->nativePets||!FreeMode(unit)||!cast||GetTickCount64()-cast<100||GetTickCount64()-cast>6000)return;



 void* saved=CastContext::getUsed?CastContext::getUsed(unit):nullptr;const uint32_t used=UsedId(unit);
 if(used&&used!=id)return;void* node=find(unit,id,-1);if(!node||!setUsed)return;
 if(!saved)setUsed(unit,node,-1);if(UsedId(unit)!=id)return;
 uint32_t savedFlags=0;Read((uintptr_t)unit+0x124,savedFlags);
 const uint32_t before=PetCount(*b,id);const int32_t result=SkillHandler(game,unit,id,level,0,0,0);
 if(NativeRoot(*b,unit)&&UsedId(unit)==id&&!saved)setUsed(unit,nullptr,-1);
 uint32_t afterFlags=0;if(NativeRoot(*b,unit)&&Read((uintptr_t)unit+0x124,afterFlags)&&Writable((uintptr_t)unit+0x124,4))*(uint32_t*)((uintptr_t)unit+0x124)=(afterFlags&~0x40u)|(savedFlags&0x40u);
 b->summonCompletedAt=GetTickCount64();b->summonCompletedSkill=id;
 Text t;t.Add("ES_SUMMON_ANIMATION_FALLBACK uid=");t.Num(b->uid);t.Add(" role=");t.Add(b->sovereign?"sovereign":"dungeon");t.Add(" skill=");t.Num(Original(id));t.Add(" native_return=");t.Num(result);t.Add(" alive_before=");t.Num(before);t.Add(" alive_after=");t.Num(PetCount(*b,id));t.Add(" missing_native_execute_event=1 one_completion=1 animation_exit_only=1");Log(t.s,!result);
}
bool LateCreatureHandler(void* game,void* unit,uint32_t id) noexcept {
 using namespace BossSkills;if(!ready||!CreatureSummon(id))return false;uint32_t ident[3]={};if(!unit||!Read((uintptr_t)unit,ident)||ident[0]!=1)return false;
 auto* b=NativeBoss(game,ident[2],ident[1]);return b&&NativeRoot(*b,unit)&&!b->nativeStarted&&b->summonCompletedAt&&b->summonCompletedSkill==id&&GetTickCount64()-b->summonCompletedAt<750;
}
}
