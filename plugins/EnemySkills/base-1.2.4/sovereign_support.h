#pragma once
namespace SovereignNative {
constexpr uint32_t RootCapacity=2048;
BossSkills::Boss roots[RootCapacity];uint32_t rootHigh=0;
BossSkills::Boss* PetRoot(void* game,uint32_t uid,uint32_t kind) noexcept {
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));if(!epoch)return nullptr;
 auto* a=Find(epoch,game,uid,kind,false);if(!a||a->thread!=GetCurrentThreadId()||a->rootSlot>=rootHigh)return nullptr;
 auto& b=roots[a->rootSlot];return b.active&&b.epoch==epoch&&b.game==game&&b.uid==uid&&b.kind==kind?&b:nullptr;
}
bool Root(Actor& a) noexcept {
 bool needed=false;for(uint32_t i=0;i<a.skillCount;++i)needed=needed||Pool::Skills[a.indices[i]].summon;if(!needed)return true;
 if(PetRoot(a.game,a.uid,a.kind))return true;
 uint32_t slot=0;for(;slot<rootHigh&&roots[slot].active;++slot){}
 if(slot==RootCapacity){Log("ES_SOVEREIGN_PET_ROOT_LIMIT no_untracked_summon=1",true);return false;}
 if(slot==rootHigh)++rootHigh;a.rootSlot=slot;auto& b=roots[slot];b={};b.active=b.sovereign=true;b.epoch=a.epoch;b.game=a.game;b.uid=a.uid;b.kind=a.kind;b.thread=a.thread;
 for(uint32_t i=0;i<a.skillCount;++i){const auto& s=Pool::Skills[a.indices[i]];b.skills[i]={s.id,a.levels[i],s.kind,cfg.cooldown,0};}return true;
}
bool Repair(void* game,void* unit) noexcept {auto* a=ActorFor(game,unit);if(!a)return false;const uint32_t index=a->index;bool ok=true;for(uint32_t i=0;i<a->skillCount;++i){a->index=a->indices[i];ok=Levels(*a,unit)&&ok;}a->index=index;if(a->skillCount)ok=Levels(*a,unit)&&ok;return ok;}
bool AuraConflict(void* unit,uint32_t id) noexcept {
 void* current=LegendNative::right(unit);uintptr_t row=0;uint8_t flags=0;int16_t sid=-1;
 return current&&Read(reinterpret_cast<uintptr_t>(current),row)&&row&&Read(row+0x24,flags)&&(flags&0x20)&&Read(row,sid)&&sid!=static_cast<int32_t>(id);
}
bool Eligible(void* game,void* unit,uint32_t index) noexcept {
 if(index>=Pool::Count)return false;
 uint32_t kind=0;uint8_t bank=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+4,kind)||!Read(reinterpret_cast<uintptr_t>(unit)+0x1bd,bank))return false;
 const auto* m=SovereignNativeData::Get(kind);if(!m)return false;
 const auto& s=Pool::Skills[index];
 if(!Pool::EnemyAssignable(s.id))return false;
 if(s.id==26&&(!m->bow||!(m->mask&(1u<<4))))return false;
 if((s.id==107||s.id==132)&&(!(m->mask&(1u<<4))||!(m->mask&(1u<<2))||!TemplateSequence(bank,s.id)))return false;
 if(s.category==Pool::Category::Aura&&AuraConflict(unit,s.id))return false;
 return true;
}
void Finish(Actor& a,void* unit,const char* reason) noexcept {
 if(!a.started)return;
 if(unit&&Equal(reason,"animation_exit")&&!a.handlers&&!a.effects&&!a.missiles)LegendNative::CompleteCreatureCast(a.game,unit,Pool::Skills[a.index].id,Level(a,Pool::Skills[a.index].id),a.started);
 uint32_t mode=0;const uint32_t used=unit?BossSkills::UsedId(unit):0;if(unit)Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode);
 auto* b=PetRoot(a.game,a.uid,a.kind);const uint32_t pets=b?b->nativePets:0;
 Text t;t.Add("ES_SOVEREIGN_ANIMATION uid=");t.Num(a.uid);t.Add(" skill=");t.Num(Pool::Skills[a.index].id);t.Add(" handlers=");t.Num(a.handlers);t.Add(" missiles=");t.Num(a.missiles);t.Add(" pets=");t.Num(pets);t.Add(" elapsed_ms=");t.Num(GetTickCount64()-a.started);t.Add(" effect_confirmed=");t.Num(a.effects>0||a.missiles>0||pets>0);t.Add(" mode_now=");t.Num(mode);t.Add(" used_now=");t.Num(used);t.Add(" trap_reactions_blocked=");t.Num(LegendNative::TrapCastingSkill(Pool::Skills[a.index].id)?a.trapBlocks:0);t.Add(" cast_reactions_blocked=");t.Num(a.trapBlocks);t.Add(" interruption_mode=");t.Num(a.interruptMode);t.Add(" reason=");t.Add(reason);Log(t.s,!a.effects&&!a.missiles&&!pets);
 const uint64_t now=GetTickCount64();



 const bool retry=unit&&ActorFor(a.game,unit)==&a&&LegendNative::HitReaction(a.interruptMode?a.interruptMode:mode)&&!a.handlers&&!a.effects&&!a.missiles&&!pets;
 if(retry){const uint64_t deadline=now+cfg.interruptedRetry;if(a.next>deadline)a.next=deadline;if(a.sharedNext>deadline)a.sharedNext=deadline;if(a.rollAt>deadline)a.rollAt=deadline;Text t;t.Add("ES_INTERRUPTED_CAST_RETRY uid=");t.Num(a.uid);t.Add(" skill=");t.Num(Pool::Skills[a.index].id);t.Add(" role=sovereign retry_ms=");t.Num(cfg.interruptedRetry);t.Add(" before_handler=1 native_ai_preserved=1");Log(t.s);}
 a.perNext[a.slot]=a.next;a.started=0;a.recovery=now+(retry&&cfg.interruptedRetry<cfg.sovereignRecovery?cfg.interruptedRetry:cfg.sovereignRecovery);if(b)b->nativeStarted=0;
}
bool TrapCastGuard(void* game,void* unit,uint32_t requested) noexcept {
 const bool reaction=LegendNative::HitReaction(requested);
 const bool action=requested==2||requested==15||requested==4||requested==5;
 if(!reaction&&!action)return false;
 auto* a=ActorFor(game,unit);uint32_t mode=0,frame=0;
 if(!reaction){if(!a||a->busy||a->handlers||!cfg.sovereignCastPriority)return false;const auto& s=Pool::Skills[a->index];const auto role=SkillRoles::Get(s);if((s.category==Pool::Category::Movement&&s.id!=390)||role!=SkillRoles::Role::Offensive)return false;}
 if(!a||!a->started||!LegendNative::CastGuardEnabled(Pool::Skills[a->index].id)||!a->frameValid||!Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)||static_cast<uint32_t>(frame-a->frame)>=150||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode!=a->mode||LegendNative::FreeMode(unit)||!LegendNative::PreserveCastSkill(unit,Pool::Skills[a->index].id))return false;
 if(++a->trapBlocks==1){Text t;t.Add(LegendNative::TrapCastingSkill(Pool::Skills[a->index].id)?"ES_TRAP_CAST_PROTECTED uid=":"ES_SKILL_CAST_PROTECTED uid=");t.Num(a->uid);t.Add(" skill=");t.Num(Pool::Skills[a->index].id);t.Add(" requested_mode=");t.Num(requested);t.Add(" role=sovereign damage_unchanged=1 death_allowed=1 active_animation_only=1");Log(t.s);}
 return true;
}
void ModeResult(void* game,void* unit,uint32_t mode,int32_t result) noexcept {
 if(!result)return;auto* a=ActorFor(game,unit);if(!a||!a->started||mode==a->mode)return;

 if(mode==0||mode==3||mode==6||mode==12||mode==13)a->interruptMode=mode;
}
void OnDeath(uint32_t uid,uint32_t kind) noexcept {
 if(!SovereignNativeData::Leader(kind))return;
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));if(!epoch)return;


 for(auto& a:actors)if(a.epoch==epoch&&a.uid==uid&&a.kind==kind&&a.thread==GetCurrentThreadId()){
  Text death;death.Add("ES_SOVEREIGN_DEATH uid=");death.Num(uid);death.Add(" variant=");death.Num(a.variant);death.Add(" skills=");for(uint32_t i=0;i<a.skillCount;++i){if(i)death.Add(",");death.Num(Pool::Skills[a.indices[i]].id);}death.Add(" last_skill=");death.Num(Pool::Skills[a.index].id);death.Add(" cast_attempts=");death.Num(a.attempts);death.Add(" pending_cast=");death.Num(a.started!=0);death.Add(" affixes=");void* unit=BossSkills::lookup?BossSkills::lookup(a.game,1,a.uid):nullptr;uintptr_t data=0;uint8_t mods[9]={};if(unit&&Read(reinterpret_cast<uintptr_t>(unit)+0x10,data)&&data&&Read(data+0x20,mods)){for(uint32_t i=0;i<9&&mods[i];++i){if(i)death.Add(",");death.Num(mods[i]);}}else death.Add("unknown");death.Add(" new_casts_stopped=1 existing_native_effects_may_persist=1");Log(death.s);
  Finish(a,nullptr,"death");auto* b=PetRoot(a.game,a.uid,a.kind);if(b)b->active=false;return;
 }
}
void Reset() noexcept {
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));
 for(auto& a:actors)if(a.epoch==epoch&&a.started&&a.thread==GetCurrentThreadId())Finish(a,nullptr,"session_end");
 rootHigh=0;memset(roots,0,sizeof(roots));
}
void Skip(Actor& a,uint32_t reason,const char* name) noexcept {
 const uint64_t now=GetTickCount64();if(a.diagReason==reason&&now<a.diagNext)return;a.diagReason=reason;a.diagNext=now+10000;
 Text t;t.Add("ES_SOVEREIGN_WAIT uid=");t.Num(a.uid);t.Add(" skill=");t.Num(Pool::Skills[a.index].id);t.Add(" reason=");t.Add(name);t.Add(" attempts=");t.Num(a.attempts);t.Add(" native_ai_allowed=1");Log(t.s);
}
bool Aura(Actor& a,void* unit,uint64_t now) noexcept {
 const auto& s=Pool::Skills[a.index];if(now<a.auraNext)return false;a.auraNext=now+1000;
 if(!Levels(a,unit))return false;
 void* node=LegendNative::find(unit,s.id,-1);uintptr_t row=0;uint8_t flags=0;int16_t state=-1;
 if(!node||!Read(reinterpret_cast<uintptr_t>(node),row)||!row||!Read(row+0x24,flags)||!(flags&0x20)||!Read(row+0xa0,state)||state!=s.state){Skip(a,10,"aura_metadata_mismatch");return false;}
 if(AuraConflict(unit,s.id)){Skip(a,11,"native_affix_aura_preserved");return false;}
 bool selected=LegendNative::right(unit)==node,active=LegendNative::state(unit,state)!=0;
 if((!selected||!active)&&now>=a.next){if(now<a.rollAt)return false;a.rollAt=now+1000;if(Rand(10000)>=cfg.chance*100)return false;LegendNative::setStat(unit,9,9999*256,0);LegendNative::setStat(unit,8,9999*256,0);LegendNative::assign(unit,0,s.id,-1);a.next=now+5000;++a.attempts;selected=LegendNative::right(unit)==node;active=LegendNative::state(unit,state)!=0;}
 if(now>=a.diagNext||a.diagReason!=(active&&selected?12u:13u)){
  a.diagNext=now+10000;a.diagReason=active&&selected?12:13;
  Text t;t.Add("ES_SOVEREIGN_AURA uid=");t.Num(a.uid);t.Add(" skill=");t.Num(s.id);t.Add(" state=");t.Num(state);t.Add(" right_selected=");t.Num(selected);t.Add(" state_active=");t.Num(active);t.Add(" level=");t.Num(Level(a,s.id));t.Add(" attempts=");t.Num(a.attempts);t.Add(" native_periodic_aura=1 original_state_overlays=1");Log(t.s,!selected||!active);
 }
 return false;
}
}
