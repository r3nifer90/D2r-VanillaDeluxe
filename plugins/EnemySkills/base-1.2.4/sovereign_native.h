#pragma once
#include "sovereign_model_modes.h"
namespace SovereignNative {using namespace ES;}
#include "sovereign_sequences.h"


namespace SovereignNative {
using namespace ES;
using LearnFn=void*(__fastcall*)(void*,int32_t);
LearnFn learn=nullptr;
bool initialized=false;
Actor* ActorFor(void* game,void* unit) noexcept {
 uint32_t id[3]={};uintptr_t actualGame=0;uint32_t mode=0;uint8_t difficulty=255,gameBank=0,unitBank=0;
 if(!cfg.enabled||!unit||!game||!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1||!SovereignNativeData::Leader(id[1]))return nullptr;
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));if(!epoch)return nullptr;
 auto* a=Find(epoch,game,id[2],id[1],false);
 if(!a||a->thread!=GetCurrentThreadId()||!BossSkills::lookup||BossSkills::lookup(game,1,id[2])!=unit||!Read(reinterpret_cast<uintptr_t>(unit)+0xd8,actualGame)||actualGame!=reinterpret_cast<uintptr_t>(game)||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||!Read(reinterpret_cast<uintptr_t>(game)+0x104,difficulty)||difficulty!=2||!Read(reinterpret_cast<uintptr_t>(game)+0x106,gameBank)||!Read(reinterpret_cast<uintptr_t>(unit)+0x1bd,unitBank)||gameBank<2||gameBank>3||unitBank!=gameBank||alignment(unit)==2||stat(unit,6,0)<256)return nullptr;
 return a;
}
bool Tracked(void* game,void* unit) noexcept {return ActorFor(game,unit)!=nullptr;}
bool Initialize() noexcept {
 if(initialized)return LegendNative::ready;


 const uint8_t entry[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20,0x8b,0xea,0x48,0x8b,0xf9};
 const uint8_t node[]={0x89,0x43,0x10,0xc7,0x43,0x40,0x01,0,0,0,0xc7,0x43,0x4c,0xff,0xff,0xff,0xff};
 if(!LegendNative::Check(0x33a010,entry,sizeof(entry))||!LegendNative::Check(0x33a182,node,sizeof(node))||!LegendNative::Bind()||!BossSkills::InstallTick())return false;
 learn=reinterpret_cast<LearnFn>(ctx->exeBase+0x33a010);LegendNative::Diagnostics();if(!InstallSequence())Log("ES_SOVEREIGN_SEQUENCE_UNAVAILABLE charge_leap_candidates_disabled=1",true);initialized=true;
 Log("ES_SOVEREIGN_NATIVE_READY learned_skill=1 native_ai_turns=1 model_specific_animations=1 all_482_leaders=1 item_proc=0 original_ai_between_spells=1 chance_per_ai_opportunity=1 chance_interval_ms=1000 legacy_hit_flags_unused=1");return true;
}
uint32_t Level(const Actor& a,uint32_t id) noexcept {for(uint32_t i=0;i<a.skillCount;++i)if(Pool::Skills[a.indices[i]].id==id)return a.levels[i];return a.skillLevel;}
bool Levels(Actor& a,void* unit) noexcept {
 if(!learn||!LegendNative::ready)return false;
 const auto& s=Pool::Skills[a.index];const uint32_t configured=Level(a,s.id);const auto* model=SovereignNativeData::Get(a.kind);if(!model)return false;
 void* node=LegendNative::find(unit,s.id,-1);if(!node)node=learn(unit,s.id);
 const uintptr_t n=reinterpret_cast<uintptr_t>(node);uintptr_t row=0;int16_t id=-1;int32_t owner=0;
 if(!node||!Read(n,row)||!row||!Read(row,id)||id!=s.id||!Read(n+0x4c,owner)||owner!=-1||!Writable(n+0x40,4)||!Writable(n+0x10,4))return false;


 uint32_t previous=0;Read(n+0x40,previous);
 if(previous!=configured&&GetTickCount64()>=a.logAt){a.logAt=GetTickCount64()+5000;Text t;t.Add("ES_SOVEREIGN_LEVEL_RESTORE uid=");t.Num(a.uid);t.Add(" skill=");t.Num(s.id);t.Add(" observed_level=");t.Num(previous);t.Add(" configured_level=");t.Num(configured);t.Add(" elite_node_only=1");Log(t.s);}
 *reinterpret_cast<uint32_t*>(n+0x40)=configured;
 const uint32_t mode=(s.id==107||s.id==132)?14:s.id==26?4:model->mode;
 *reinterpret_cast<uint32_t*>(n+0x10)=mode;
 a.mode=mode;return true;
}
}
#include "sovereign_support.h"
#include "sovereign_pacing.h"
#include "sovereign_schedule.h"
namespace SovereignNative {
bool Attach(void* game,void* unit) noexcept {
 auto* a=ActorFor(game,unit);if(!a)return false;if(!a->skillCount){Log("ES_SOVEREIGN_LEARNED skill_count=0 native_combat_only=1");return true;}
 const uint32_t saved=a->index;for(uint32_t i=0;i<a->skillCount;++i){a->index=a->indices[i];if(!Levels(*a,unit)){a->index=saved;return false;}}a->index=saved;if(a->skillCount&&!Levels(*a,unit))return false;
 if(!Root(*a))return false;
 for(uint32_t i=0;i<a->skillCount;++i){Text t;t.Add("ES_SOVEREIGN_SLOT uid=");t.Num(a->uid);t.Add(" variant=");t.Num(a->variant);t.Add(" slot=");t.Num(i);t.Add(" skill=");t.Num(Pool::Skills[a->indices[i]].id);t.Add(" level=");t.Num(a->levels[i]);t.Add(" role=");t.Add(SkillRoles::Name(SkillRoles::Get(Pool::Skills[a->indices[i]])));t.Add(" class=");t.Num(a->kind);t.Add(" name=");t.Add(Pool::Skills[a->indices[i]].name);Log(t.s);}
 if(!a->next){a->next=GetTickCount64()+1000;a->sharedNext=a->next;}
 Text t;t.Add("ES_SOVEREIGN_LEARNED uid=");t.Num(a->uid);t.Add(" class=");t.Num(a->kind);t.Add(" skill=");t.Num(Pool::Skills[a->index].id);t.Add(" level=");t.Num(Level(*a,Pool::Skills[a->index].id));t.Add(" mode=");t.Num(a->mode);t.Add(" node_only=1 mana_reserve=9999 player_rows_unchanged=1");Log(t.s);return true;
}
int32_t GuardLevel(void* game,void* unit,int32_t id,int32_t level) noexcept {
 auto* a=ActorFor(game,unit);if(a)for(uint32_t i=0;i<a->skillCount;++i)if(Pool::Skills[a->indices[i]].id==static_cast<uint32_t>(id))return static_cast<int32_t>(a->levels[i]);return level;
}
void Event(void* game,void* unit,uint32_t id,int32_t result,bool begin) noexcept {
 auto* a=ActorFor(game,unit);if(!a||!a->started||Pool::Skills[a->index].id!=id)return;
 if(begin){++a->handlers;return;}
 if(result){++a->effects;a->failures=0;a->perFailures[a->slot]=0;}else FailSlot(*a,GetTickCount64());
 Text t;t.Add("ES_SOVEREIGN_HANDLER uid=");t.Num(a->uid);t.Add(" skill=");t.Num(id);t.Add(" level=");t.Num(Level(*a,Pool::Skills[a->index].id));t.Add(" returned=");t.Num(static_cast<uint32_t>(result));t.Add(" failures=");t.Num(a->failures);t.Add(" effect_handler_observed=1");Log(t.s,!result);
}
void Missile(void* game,void* unit,void* result) noexcept {
 auto* a=ActorFor(game,unit);if(!a||!a->started||!result||BossSkills::UsedId(unit)!=Pool::Skills[a->index].id)return;
 uint32_t kind=0;Read(reinterpret_cast<uintptr_t>(result)+4,kind);if(kind==195)return;
 ++a->missiles;
}
bool Opponent(Actor& a,void* unit,void* target) noexcept {
 uint32_t type=99,uid=0,mode=0;int32_t x=0,y=0,tx=0,ty=0;
 return target&&target!=unit&&Read(reinterpret_cast<uintptr_t>(target),type)&&(type==0||(type==1&&alignment(target)==2))&&Read(reinterpret_cast<uintptr_t>(target)+8,uid)&&BossSkills::lookup(a.game,type,uid)==target&&Read(reinterpret_cast<uintptr_t>(target)+0xc,mode)&&mode!=0&&mode!=12&&stat(target,6,0)>=256&&BossSkills::Level(unit)==BossSkills::Level(target)&&BossSkills::Position(unit,x,y)&&BossSkills::Position(target,tx,ty)&&BossSkills::Distance(x,y,tx,ty)<=40;
}
bool Decision(void* game,void* unit) noexcept {
 auto* a=ActorFor(game,unit);if(!a)return false;
 if(a->busy){LegendNative::idle(game,unit,3);return true;}
 if(!a->skillCount)return false;
 const auto& activeSkill=Pool::Skills[a->index];const uint64_t now=GetTickCount64();uint32_t mode=0;Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode);
 if(a->started){
  if(!LegendNative::FreeMode(unit)&&mode==a->mode&&BossSkills::UsedId(unit)==activeSkill.id){
   uint32_t frame=0;const bool expired=a->frameValid&&Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)&&static_cast<uint32_t>(frame-a->frame)>=150&&static_cast<uint32_t>(frame-a->frame)<0x80000000u;
   if(!expired){LegendNative::idle(game,unit,3);return true;}
   a->next=now+5000;Log("ES_SOVEREIGN_TIMEOUT native_ai_resume=1 forced_mode_change=0 retry_ms=5000",true);
   LegendNative::CombatCastTimeout(game,unit,activeSkill.id,a->mode,a->frame,a->frameValid);
  }
  Finish(*a,unit,a->interruptMode?"native_interrupt":"animation_exit");
 }
 if(!LegendNative::FreeMode(unit)){Skip(*a,1,"native_animation");return false;}
 if(a->factoryDue){a->factoryDue=false;return false;}
 if(now<a->sharedNext||now<a->recovery||now<a->rollAt)return false;
 a->perNext[a->slot]=a->next;
 void* target=LiveTarget(*a,unit);
 const uint32_t chosen=ChooseSlot(*a,unit,target,now);
 if(chosen==~0u){if(!target)Skip(*a,2,"no_live_opponent_in_range");return false;}
 a->slot=chosen;a->index=a->indices[chosen];a->next=a->perNext[chosen];a->cursor=(chosen+1)%a->skillCount;
 const auto& s=Pool::Skills[a->index];
 if(s.category==Pool::Category::Aura){const bool result=Aura(*a,unit,now);a->perNext[a->slot]=a->next;return result;}
 if(cfg.continuousHammer&&s.id==112){a->next=now+1000;a->perNext[a->slot]=a->next;return false;}
 if(!s.persistent&&!Opponent(*a,unit,target)){Skip(*a,2,"no_live_opponent_in_range");return false;}

 if(mode==2||mode==15){
  if(!a->walkSince)a->walkSince=now;
  int32_t x=0,y=0,tx=0,ty=0;
  const bool distant=BossSkills::Position(unit,x,y)&&BossSkills::Position(target,tx,ty)&&BossSkills::Distance(x,y,tx,ty)>3;
  if(distant&&BossSkills::CastMelee(s.id,s.kind)&&(now-a->walkSince<650||LegendNative::collision(unit,target,4))){Skip(*a,14,"native_committed_walk");return false;}
 }else a->walkSince=0;
 auto* root=PetRoot(game,a->uid,a->kind);
 if(root&&target){Read(reinterpret_cast<uintptr_t>(target),root->targetType);Read(reinterpret_cast<uintptr_t>(target)+8,root->targetId);}
 if(s.persistent)target=unit;
 if(s.kind!=1&&s.kind!=5&&s.kind!=7&&LegendNative::collision(unit,target,4)){Skip(*a,3,"line_of_sight");return false;}
 if(!Levels(*a,unit)){a->next=now+30000;Log("ES_SOVEREIGN_LEARN_REJECT native_node_missing=1 no_item_fallback=1",true);return false;}
 if(s.persistent){
  void* node=LegendNative::find(unit,s.id,-1);uintptr_t row=0;int16_t state=-1;
  if(node&&Read(reinterpret_cast<uintptr_t>(node),row)&&row&&Read(row+0xa0,state)&&state>0&&LegendNative::state(unit,state)){a->next=now+1000;Skip(*a,4,"persistent_state_active");return false;}
 }
 if(s.summon){if(!root){Skip(*a,5,"summon_root_missing");return false;}const uint32_t alive=BossSkills::PetCount(*root,s.id),limit=BossSkills::PetLimit(*root,s.id);if(!limit||alive>=limit||(s.id==88&&alive)||(s.id==62&&alive+3>limit)){a->next=now+1000;Skip(*a,6,"summon_or_trap_limit");return false;}}
 int32_t ux=0,uy=0,ox=0,oy=0;if(!BossSkills::Position(unit,ux,uy)||!BossSkills::Position(target,ox,oy))return false;
 const uint32_t distance=BossSkills::Distance(ux,uy,ox,oy);
 if(!s.persistent&&!BossSkills::CastInRange(game,unit,target,s.id,s.kind,Level(*a,s.id))){Skip(*a,7,"skill_distance");return false;}
 uint32_t rejectedCorpses=0;
 if(s.kind==2||s.kind==6){BossSkills::Boss temporary={};temporary.game=game;temporary.epoch=a->epoch;target=BossSkills::DeadTargetForSkill(root?*root:temporary,unit,s.id,&rejectedCorpses);if(!target){a->next=now+1000;Skip(*a,8,s.id==83?"no_native_eligible_corpse":"no_valid_corpse");return false;}}
 const bool refillPriority=root&&BossSkills::RefillNeeded(*root,s.id);
 a->rollAt=now+1000;if(!cfg.chance||(!refillPriority&&Rand(10000)>=cfg.chance*100))return false;
 if(now/100!=castWindow){castWindow=now/100;castsInWindow=0;}if(castsInWindow>=8)return false;++castsInWindow;
 int32_t tx=0,ty=0;uintptr_t path=0;if(!BossSkills::Position(target,tx,ty)||!Read(reinterpret_cast<uintptr_t>(unit)+0x38,path)||!path)return false;
 LegendNative::face(reinterpret_cast<void*>(path),tx,ty,1);
 if(s.kind==1||s.kind==5||s.kind==7){target=unit;BossSkills::Position(unit,tx,ty);}
 uint32_t gap=s.persistent?SharedGap():CastGap(s);if(root&&BossSkills::RefillSkill(s.id)){gap=BossSkills::RefillGap(cfg.cooldown);BossSkills::RefillLog(*root,s.id,gap);}if(s.category==Pool::Category::Movement&&s.id!=390&&gap<6000)gap=6000;
 a->next=now+gap;a->perNext[a->slot]=a->next;a->sharedNext=now+SharedGap();a->busy=true;a->started=now;a->handlers=a->effects=a->missiles=0;a->factoryDue=true;a->interruptMode=0;a->trapBlocks=0;++a->attempts;a->frameValid=Read(reinterpret_cast<uintptr_t>(game)+0x170,a->frame);


 LegendNative::setStat(unit,9,9999*256,0);LegendNative::setStat(unit,8,9999*256,0);
 Text t;t.Add("ES_SOVEREIGN_CAST_BEGIN uid=");t.Num(a->uid);t.Add(" class=");t.Num(a->kind);t.Add(" skill=");t.Num(s.id);t.Add(" name=");t.Add(s.name);t.Add(" level=");t.Num(Level(*a,Pool::Skills[a->index].id));t.Add(" mode=");t.Num(a->mode);uint32_t targetType=0,targetId=0;Read(reinterpret_cast<uintptr_t>(target),targetType);Read(reinterpret_cast<uintptr_t>(target)+8,targetId);t.Add(" target_type=");t.Num(targetType);t.Add(" target_id=");t.Num(targetId);t.Add(" item_proc=0");Log(t.s);if(s.id==249)ArmageddonProbe::Cast(unit,Level(*a,s.id));
 if(s.id==83||s.id==70||s.id==80){Text c;c.Add("ES_CORPSE_TARGET uid=");c.Num(a->uid);c.Add(" skill=");c.Num(s.id);c.Add(" target_id=");c.Num(targetId);c.Add(" native_rejected_candidates=");c.Num(rejectedCorpses);c.Add(" native_eligible=1 reserved_state=0 corpse_unchanged=1");Log(c.s);}
 if(root){root->nativeStarted=now;root->nativeSkill=s.id;root->nativeMode=a->mode;root->nativePets=root->nativeHandlers=root->nativeMissiles=0;}
 const int32_t result=LegendNative::use(game,unit,static_cast<uint8_t>(a->mode),s.id,target,tx,ty,0);a->busy=false;a->walkSince=0;
 Text end;end.Add(result?"ES_SOVEREIGN_CAST_ACCEPTED uid=":"ES_SOVEREIGN_CAST_REJECT uid=");end.Num(a->uid);end.Add(" skill=");end.Num(s.id);end.Add(" returned=");end.Num(static_cast<uint32_t>(result));end.Add(" acceptance_is_not_effect_confirmation=1");Log(end.s,!result);
 if(!result){if(!a->handlers)FailSlot(*a,now);a->perNext[a->slot]=a->next;if(root)root->nativeStarted=0;a->started=0;a->factoryDue=false;return false;}
 LegendNative::idle(game,unit,3);return true;
}
}
