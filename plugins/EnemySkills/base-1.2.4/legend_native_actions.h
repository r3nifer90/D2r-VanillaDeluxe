#pragma once
namespace BossSkills {
void AttackObserved(Boss&,void*) noexcept {}
void NativeActionWatch(Boss& b,void* unit) noexcept {LegendNative::CombatWatch(b.game,unit);}

}
namespace LegendNative {
bool TrapCastingSkill(uint32_t id) noexcept {
 const uint32_t original=Original(id);
 return original==256||original==257||original==262||original==271||original==276;
}
bool CastGuardEnabled(uint32_t id) noexcept {return TrapCastingSkill(id)?cfg.trapCastGuard!=0:cfg.skillCastGuard!=0;}
bool PreserveCastSkill(void* unit,uint32_t id) noexcept {
 const uint32_t used=BossSkills::UsedId(unit);if(used==id)return true;



 uintptr_t list=0,current=0;
 if(used||!setUsed||!find||!Read(reinterpret_cast<uintptr_t>(unit)+0x100,list)||!list||!Read(list+0x18,current)||current)return false;
 void* node=find(unit,id,-1);uintptr_t row=0;int16_t skill=-1;int32_t owner=0;
 if(!node||!Read(reinterpret_cast<uintptr_t>(node),row)||!row||!Read(row,skill)||skill!=id||!Read(reinterpret_cast<uintptr_t>(node)+0x4c,owner)||owner!=-1)return false;
 setUsed(unit,node,-1);return BossSkills::UsedId(unit)==id;
}
bool TrapCastGuard(void* game,void* unit,uint32_t requested) noexcept {
 using namespace BossSkills;
 uint32_t id[3]={},mode=0,frame=0;
 if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1)return false;
 if(SovereignNativeData::Leader(id[1]))return SovereignNative::TrapCastGuard(game,unit,requested);
 if(!HitReaction(requested))return false;
 if(!BossSkills::Legend(id[1]))return false;
 auto* b=NativeBoss(game,id[2],id[1]);
 if(!b||!NativeRoot(*b,unit)||!b->nativeStarted||!CastGuardEnabled(b->nativeSkill)||!b->castFrameValid||!Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)||static_cast<uint32_t>(frame-b->castFrame)>=150||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode!=b->nativeMode||FreeMode(unit)||!PreserveCastSkill(unit,b->nativeSkill))return false;
 if(++b->trapBlocks==1){Text t;t.Add(TrapCastingSkill(b->nativeSkill)?"ES_TRAP_CAST_PROTECTED uid=":"ES_SKILL_CAST_PROTECTED uid=");t.Num(b->uid);t.Add(" skill=");t.Num(b->nativeSkill);t.Add(" requested_mode=");t.Num(requested);t.Add(" role=dungeon_legend damage_unchanged=1 death_allowed=1 active_animation_only=1");Log(t.s);}
 return true;
}
int32_t __fastcall ChangeMode(void* game,void* request,int32_t force) noexcept {
 using namespace BossSkills;
 uint32_t incoming=0;void* caster=nullptr;
 if(ready&&request&&Read(reinterpret_cast<uintptr_t>(request),incoming)&&Read(reinterpret_cast<uintptr_t>(request)+8,caster)&&LeapMode(game,caster,incoming))return 0;
 if(ready&&request&&Read(reinterpret_cast<uintptr_t>(request),incoming)&&(incoming==3||incoming==6||incoming==13||incoming==2||incoming==15||incoming==4||incoming==5)&&Read(reinterpret_cast<uintptr_t>(request)+8,caster)&&TrapCastGuard(game,caster,incoming))return 0;
 bool handled=false;const int32_t adapted=CombatMode(game,request,force,handled);if(handled)return adapted;
 const int32_t result=priorMode(game,request,force);
 void* unit=nullptr;uint32_t mode=0;
 if(ready&&request&&Read(reinterpret_cast<uintptr_t>(request),mode)&&Read(reinterpret_cast<uintptr_t>(request)+8,unit)&&unit)SovereignNative::ModeResult(game,unit,mode,result);
 return result;
}
}
