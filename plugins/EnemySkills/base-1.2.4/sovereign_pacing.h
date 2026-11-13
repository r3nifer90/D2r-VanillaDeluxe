#pragma once
namespace SovereignNative {
bool Opponent(Actor&,void*,void*) noexcept;
bool PrioritySkill(const Pool::Skill& s) noexcept {
 const auto role=SkillRoles::Get(s);
 return (s.category!=Pool::Category::Movement||s.id==390)&&
 role==SkillRoles::Role::Offensive&&
 !(s.id==112&&cfg.continuousHammer);
}
uint32_t SharedGap() noexcept {return cfg.cooldown<cfg.sovereignCastGap?cfg.cooldown:cfg.sovereignCastGap;}
uint32_t CastGap(const Pool::Skill& s) noexcept {return PrioritySkill(s)?SharedGap():cfg.cooldown;}
void* LiveTarget(Actor&,void*) noexcept;
bool ReadySlot(Actor&,void*,void*,uint32_t,uint64_t) noexcept;
bool PriorityReady(Actor& a,void* unit,uint64_t now) noexcept {
 if(!cfg.sovereignCastPriority||!cfg.chance||a.busy||a.started||now<a.sharedNext||now<a.recovery||now<a.rollAt)return false;
 void* target=LiveTarget(a,unit);
 for(uint32_t i=0;i<a.skillCount;++i)if(PrioritySkill(Pool::Skills[a.indices[i]])&&ReadySlot(a,unit,target,i,now))return true;
 return false;
}
}
