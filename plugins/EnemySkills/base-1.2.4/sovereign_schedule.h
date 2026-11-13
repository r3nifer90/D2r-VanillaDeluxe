#pragma once
namespace SovereignNative {
int32_t EffectState(void* unit,const Pool::Skill& s,bool targetState) noexcept {
 void* node=LegendNative::find?LegendNative::find(unit,s.id,-1):nullptr;uintptr_t row=0;int16_t id=-1,state=-1;
 if(!node||!Read(reinterpret_cast<uintptr_t>(node),row)||!row||!Read(row,id)||id!=static_cast<int32_t>(s.id)||!Read(row+(targetState?0xa2:0xa0),state)||state<=0)return -1;
 return state;
}
void EffectWait(Actor& a,const Pool::Skill& s,void* target,uint32_t slot,int32_t state,uint64_t now) noexcept {
 if(now<a.supportDiag[slot])return;a.supportDiag[slot]=now+5000;uint32_t type=99,uid=0;
 if(target){Read(reinterpret_cast<uintptr_t>(target),type);Read(reinterpret_cast<uintptr_t>(target)+8,uid);}
 Text t;t.Add("ES_SOVEREIGN_EFFECT_WAIT uid=");t.Num(a.uid);t.Add(" skill=");t.Num(s.id);t.Add(" target_type=");t.Num(type);t.Add(" target_id=");t.Num(uid);t.Add(" state=");t.Num(state);t.Add(" active=1 native_state=1 other_slots_allowed=1");Log(t.s);
}
void SlotWait(Actor& a,const Pool::Skill& s,uint32_t slot,const char* reason,uint64_t now) noexcept {
 if(now<a.supportDiag[slot])return;a.supportDiag[slot]=now+10000;
 Text t;t.Add("ES_SOVEREIGN_SLOT_WAIT uid=");t.Num(a.uid);t.Add(" skill=");t.Num(s.id);t.Add(" reason=");t.Add(reason);t.Add(" other_slots_allowed=1 native_ai_allowed=1");Log(t.s);
}
void* LiveTarget(Actor& a,void* unit) noexcept {
 void* target=LegendNative::currentTarget?LegendNative::currentTarget(a.game,unit):nullptr;
 if(Opponent(a,unit,target))return target;
 uintptr_t monster=0,control=0;int32_t distance=0,combat=0;
 if(!Read(reinterpret_cast<uintptr_t>(unit)+0x10,monster)||!monster||!Read(monster+0x30,control)||!control||!Readable(control,0x3a))return nullptr;
 target=LegendNative::chooseTarget(a.game,unit,reinterpret_cast<void*>(control),&distance,&combat,0);
 return Opponent(a,unit,target)?target:nullptr;
}
bool ReadySlot(Actor& a,void* unit,void* target,uint32_t slot,uint64_t now) noexcept {
 const auto& s=Pool::Skills[a.indices[slot]];if(!Pool::EnemyAssignable(s.id))return false;
 const uint64_t next=slot==a.slot?a.next:a.perNext[slot];if(now<next)return false;
 if(cfg.continuousHammer&&s.id==112)return false;
 if(s.category==Pool::Category::Aura){
  const int32_t state=EffectState(unit,s,false);void* node=LegendNative::find(unit,s.id,-1);
  return state>0&&(!LegendNative::state(unit,state)||LegendNative::right(unit)!=node);
 }
 if(s.persistent){const int32_t state=EffectState(unit,s,false);if(state>0&&LegendNative::state(unit,state)){EffectWait(a,s,unit,slot,state,now);return false;}return true;}
 if(!Opponent(a,unit,target))return false;
 if(!BossSkills::CastInRange(a.game,unit,target,s.id,s.kind,Level(a,s.id)))return false;
 if(s.kind!=1&&s.kind!=5&&s.kind!=7&&LegendNative::collision(unit,target,4))return false;
 if(SkillRoles::IsCurse(s)){const int32_t state=EffectState(unit,s,true);if(state>0&&LegendNative::state(target,state)){EffectWait(a,s,target,slot,state,now);return false;}}
 if(s.summon){auto* b=PetRoot(a.game,a.uid,a.kind);if(!b)return false;const uint32_t alive=BossSkills::PetCount(*b,s.id),limit=BossSkills::PetLimit(*b,s.id);if(!limit||alive>=limit||(s.id==88&&alive)||(s.id==62&&alive+3>limit))return false;}
 if(s.kind==2||s.kind==6){auto* root=PetRoot(a.game,a.uid,a.kind);BossSkills::Boss temporary={};temporary.game=a.game;temporary.epoch=a.epoch;temporary.sovereign=true;
  if(!BossSkills::DeadTargetForSkill(root?*root:temporary,unit,s.id)){SlotWait(a,s,slot,"no_native_eligible_corpse",now);const uint64_t retry=now+1000;a.perNext[slot]=retry;if(slot==a.slot)a.next=retry;return false;}
 }

 return true;
}
uint32_t ChooseSlot(Actor& a,void* unit,void* target,uint64_t now) noexcept {
 uint32_t best=~0u,rank=~0u;auto* root=PetRoot(a.game,a.uid,a.kind);
 for(uint32_t n=0;n<a.skillCount;++n){const uint32_t i=(a.cursor+n)%a.skillCount;if(!ReadySlot(a,unit,target,i,now))continue;
  const auto& s=Pool::Skills[a.indices[i]];uint32_t candidate=3;
  if(s.category==Pool::Category::Aura||(s.persistent&&SkillRoles::Get(s)==SkillRoles::Role::Offensive))candidate=0;
  else if(SkillRoles::IsCurse(s)&&EffectState(unit,s,true)>0)candidate=1;
  else if(root&&BossSkills::RefillNeeded(*root,s.id))candidate=2;
  if(candidate<rank){best=i;rank=candidate;}
 }
 return best;
}
void FailSlot(Actor& a,uint64_t now) noexcept {
 auto& failures=a.perFailures[a.slot];if(failures<4)++failures;a.failures=failures;
 const uint32_t delay=failures>=3?30000:5000;const uint64_t retry=now+delay;if(a.next<retry)a.next=retry;a.perNext[a.slot]=a.next;
 Text t;t.Add("ES_SOVEREIGN_EFFECT_BACKOFF uid=");t.Num(a.uid);t.Add(" skill=");t.Num(Pool::Skills[a.index].id);t.Add(" slot=");t.Num(a.slot);t.Add(" failures=");t.Num(failures);t.Add(" retry_ms=");t.Num(delay);t.Add(" other_skill_success_does_not_reset=1 other_slots_allowed=1");Log(t.s,true);
}
}
