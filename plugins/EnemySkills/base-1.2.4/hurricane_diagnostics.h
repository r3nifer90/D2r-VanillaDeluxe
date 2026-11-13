#pragma once
namespace SovereignNative {ES::Actor* ActorFor(void*,void*) noexcept;}
namespace LegendNative {
struct DamageEvidence {uint64_t epoch=0,next=0;void* game=nullptr;uint32_t uid=0,thread=0,skill=0,targetType=0,targetUid=0;uintptr_t source=0;};
DamageEvidence damageEvidence[1024];
BossSkills::Boss* HurricaneRoot(void* game,void* unit,uint32_t& skill) noexcept {
 uint32_t id[3]={};if(!ready||!unit||!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1)return nullptr;
 auto* b=BossSkills::NativeBoss(game,id[2],id[1]);if(!b||!BossSkills::NativeRoot(*b,unit))return nullptr;
 for(const auto& s:b->skills)if(Original(s.id)==250){skill=s.id;return b;}
 return nullptr;
}
bool HurricaneState(void* unit,uint32_t skill) noexcept {
 void* node=find?find(unit,skill,-1):nullptr;uintptr_t row=0;int16_t sid=-1;
 return node&&Read(reinterpret_cast<uintptr_t>(node),row)&&row&&Read(row+0xa0,sid)&&sid>0&&state&&state(unit,sid);
}
void HurricaneCastEnd(void* game,void* unit,uint32_t skill,int32_t level,int32_t result) noexcept {
 return;
}


uint32_t DamagePacketSkill(void* game,void* attacker,void* damage) noexcept {
 uint8_t tagged=0;uint32_t sid=0;
 if(!Read((uintptr_t)damage+0xc,tagged)||tagged!=1||!Read((uintptr_t)damage+8,sid)||!sid)return 0;
 if(auto* a=SovereignNative::ActorFor(game,attacker)){
  for(uint32_t i=0;i<a->skillCount;++i)if(Pool::Skills[a->indices[i]].id==sid)return sid;
  return 0;
 }
 uint32_t id[3]={};if(!attacker||!Read((uintptr_t)attacker,id)||id[0]!=1)return 0;
 auto* b=BossSkills::NativeBoss(game,id[2],id[1]);if(!b||!BossSkills::NativeRoot(*b,attacker))return 0;
 for(const auto& skill:b->skills)if(skill.id==sid)return sid;return 0;
}
uint64_t __fastcall Damage(void* game,void* attacker,void* target,int32_t hit,void* damage) noexcept {
 return priorDamage(game,attacker,target,hit,damage);
}
}
