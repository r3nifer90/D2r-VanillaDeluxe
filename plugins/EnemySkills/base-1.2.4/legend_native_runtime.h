#pragma once
namespace BossSkills {
struct Pet {uint64_t epoch=0,created=0,next=0,missingSince=0,alignmentLog=0,handlerNext=0,moveLog=0,supportNext=0,targetLog=0,waitLog=0;void* game=nullptr;uint32_t uid=0,kind=0,owner=0,ownerKind=0,skill=0,thread=0,shots=0,cursor=0,handlerSkill=0,handlerSuppressed=0,roamStep=0,targetType=0,targetId=0,actionFrame=0,actionMode=0,actionSkill=0;bool active=false,retire=false,missing=false,healthApplied=false,actionWatching=false,actionFrameValid=false;};
Pet pets[16384];uint32_t petHigh=0;uint64_t petNextPoll=0;
Boss* NativeBoss(void* game,uint32_t uid,uint32_t kind) noexcept {const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));for(uint32_t i=0;i<count;++i){auto& b=bosses[i];if(b.active&&b.epoch==epoch&&b.game==game&&b.uid==uid&&b.kind==kind&&Legend(kind)&&b.thread==GetCurrentThreadId())return &b;}return SovereignNative::PetRoot(game,uid,kind);}
bool NativeRoot(const Boss& b,void* unit) noexcept {
 uint32_t id[3]={},mode=0;uintptr_t game=0;
 return LegendNative::ready&&lookup&&unit&&b.active&&(Legend(b.kind)||b.sovereign)&&b.thread==GetCurrentThreadId()&&b.epoch==static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0))&&Read(reinterpret_cast<uintptr_t>(unit),id)&&id[0]==1&&id[1]==b.kind&&id[2]==b.uid&&lookup(b.game,1,b.uid)==unit&&Read(reinterpret_cast<uintptr_t>(unit)+0xd8,game)&&game==reinterpret_cast<uintptr_t>(b.game)&&Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)&&mode!=0&&mode!=12&&stat(unit,6,0)>=256;
}


void DungeonSpeed(Boss& b,void* unit) noexcept {
 if(b.sovereign||!Legend(b.kind)||!NativeRoot(b,unit)||!LegendNative::baseStat||!LegendNative::setStat)return;
 uintptr_t list=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+0x88,list))return;
 const bool changed=b.speedUnit!=reinterpret_cast<uintptr_t>(unit)||b.speedList!=list;
 const uint64_t now=GetTickCount64();if(!changed&&now<b.speedRetry)return;b.speedRetry=now+1000;
 const uint32_t stats[]={68,67},bonus[]={cfg.legendAttackSpeed,cfg.legendMoveSpeed};
 for(uint32_t i=0;i<2;++i){
  const int32_t current=static_cast<int32_t>(LegendNative::baseStat(unit,stats[i],0));


  if(changed&&(!b.speedApplied[i]||current!=b.speedAfter[i]))b.speedApplied[i]=false;
  if(b.speedApplied[i]&&(current!=b.speedBefore[i]||!bonus[i]))continue;
  int64_t target=static_cast<int64_t>(current)+bonus[i];if(target>2047)target=2047;if(target< -2048)target=-2048;
  b.speedBefore[i]=current;b.speedAfter[i]=static_cast<int32_t>(target);b.speedApplied[i]=true;
  if(target!=current)LegendNative::setStat(unit,stats[i],static_cast<int32_t>(target),0);
  const int32_t actual=static_cast<int32_t>(LegendNative::baseStat(unit,stats[i],0));
  Text t;t.Add(actual==target?"ES_DUNGEON_SPEED uid=":"ES_DUNGEON_SPEED_PARTIAL uid=");t.Num(b.uid);t.Add(" stat=");t.Num(stats[i]);t.Add(" bonus_percent=");t.Num(bonus[i]);t.Add(" applied_base=");t.Num(actual>=0?static_cast<uint32_t>(actual):0);t.Add(" negative_base=");t.Num(actual<0);t.Add(" native_animation_update=1 owner_only=1 no_ai_mode_override=1 player_sovereign_unchanged=1");Log(t.s,actual!=target);
 }
 b.speedUnit=reinterpret_cast<uintptr_t>(unit);Read(reinterpret_cast<uintptr_t>(unit)+0x88,b.speedList);
}
bool NativeLevels(Boss& b,void* unit) noexcept {
 if(!NativeRoot(b,unit))return false;
 if(!b.sovereign)DungeonSpeed(b,unit);
 if(b.sovereign)return SovereignNative::Repair(b.game,unit);
 bool valid=true;const uint64_t now=GetTickCount64();
 for(uint32_t i=0;i<3;++i){const auto& s=b.skills[i];
  if(SharedOffensive::Supported(s.id)&&!LegendNative::PrepareShared(b.game,unit,s)){valid=false;continue;}
  void* learned=LegendNative::find(unit,s.id,-1);uintptr_t row=0;int16_t id=-1;int32_t owner=0;uint32_t level=0;
  const uintptr_t node=reinterpret_cast<uintptr_t>(learned);
  if(!learned||!Read(node,row)||!row||!Read(row,id)||id!=s.id||!Read(node+0x4c,owner)||owner!=-1||!Read(node+0x40,level)||!Writable(node+0x40,4)){valid=false;continue;}
  if(level==s.level)continue;


  *reinterpret_cast<uint32_t*>(node+0x40)=s.level;
  if(now>=b.levelLog[i]){b.levelLog[i]=now+5000;Text t;t.Add("ES_NATIVE_LEVEL_RESTORE uid=");t.Num(b.uid);t.Add(" skill=");t.Num(s.id);t.Add(" observed_level=");t.Num(level);t.Add(" configured_level=");t.Num(s.level);t.Add(" learned_level=");t.Num(s.level);t.Add(" boss_node_only=1 player_skills_untouched=1");Log(t.s);}
 }
 return valid;
}
Pet* NativePet(void* game,void* unit) noexcept {uint32_t id[3]={};if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1||Legend(id[1])||SovereignNativeData::Leader(id[1]))return nullptr;const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));for(uint32_t i=0;i<petHigh;++i){auto& p=pets[i];if(p.active&&p.epoch==epoch&&p.game==game&&p.uid==id[2]&&p.kind==id[1]&&p.thread==GetCurrentThreadId())return &p;}return nullptr;}
bool NativePetHostile(Pet& p,void* unit) noexcept {
 auto* b=NativeBoss(p.game,p.owner,p.ownerKind);void* owner=b?lookup(p.game,1,p.owner):nullptr;uintptr_t game=0;uint32_t mode=0;
 if(!b||b->ownerDied||(owner&&!NativeRoot(*b,owner))||NativePet(p.game,unit)!=&p||lookup(p.game,1,p.uid)!=unit||!Read(reinterpret_cast<uintptr_t>(unit)+0xd8,game)||game!=reinterpret_cast<uintptr_t>(p.game)||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||stat(unit,6,0)<256)return false;
 const int32_t before=alignment(unit);if(before==0)return true;
 const uintptr_t at=reinterpret_cast<uintptr_t>(unit)+0x124;uint32_t flags=0;
 if(!Read(at,flags)||!Writable(at,4))return false;



 constexpr uint32_t gate=0x80000000u;
 *reinterpret_cast<uint32_t*>(at)=flags&~gate;
 LegendNative::align(unit,0,1);
 *reinterpret_cast<uint32_t*>(at)=(*reinterpret_cast<uint32_t*>(at)&~gate)|(flags&gate);
 const int32_t after=alignment(unit);const uint64_t now=GetTickCount64();
 if(now>=p.alignmentLog){p.alignmentLog=now+5000;Text t;t.Add(after==0?"ES_NATIVE_PET_HOSTILE uid=":"ES_NATIVE_PET_ALIGNMENT_REJECT uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" alignment_before=");t.Num(before);t.Add(" alignment=");t.Num(after);t.Add(" pet_flag_restored=1 native_transition=1 player_pets_untouched=1");Log(t.s,after!=0);}
 return after==0;
}
#include "sovereign_summon_health.h"
void PetHealth(Pet& p,void* unit) noexcept {
 if(p.healthApplied||!LegendNative::ready)return;
 auto* root=NativeBoss(p.game,p.owner,p.ownerKind);if(root&&root->sovereign){SovereignPetHealth(p,unit,*root);return;}

 if(p.skill==441||p.skill==494||p.skill==496||cfg.legendSummonHealth==100){p.healthApplied=true;return;}
 auto* b=NativeBoss(p.game,p.owner,p.ownerKind);void* owner=b?lookup(p.game,1,p.owner):nullptr;uintptr_t game=0;uint32_t mode=0;
 if(!b||b->healthSpawnDepth||!NativeRoot(*b,owner)||NativePet(p.game,unit)!=&p||lookup(p.game,1,p.uid)!=unit||!Read(reinterpret_cast<uintptr_t>(unit)+0xd8,game)||game!=reinterpret_cast<uintptr_t>(p.game)||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||p.retire||!LegendNative::baseStat||!LegendNative::setStat)return;
 const uint32_t life=stat(unit,6,0),maximum=stat(unit,7,0);
 if(life<256||maximum<256)return;
 constexpr uint32_t cap=0x7fffffffu;
 const uint32_t baseMax=LegendNative::baseStat(unit,7,0),baseLife=LegendNative::baseStat(unit,6,0);
 p.healthApplied=true;
 const uint64_t desired=static_cast<uint64_t>(maximum)*cfg.legendSummonHealth/100;
 const uint32_t target=desired>cap?cap:static_cast<uint32_t>(desired);
 uint32_t actualMax=maximum,actualLife=life;bool applied=false;
 if(life<=cap&&maximum<=cap&&baseMax<=cap&&baseLife<=cap){

  const uint64_t newBase=static_cast<uint64_t>(baseMax)+target-maximum;
  LegendNative::setStat(unit,7,static_cast<int32_t>(newBase>cap?cap:newBase),0);
  actualMax=stat(unit,7,0);
  if(actualMax>=maximum&&actualMax<=cap){

   const uint32_t current=life>maximum?maximum:life;
   const uint32_t targetLife=static_cast<uint64_t>(current)*actualMax/maximum;
   const uint64_t newLife=static_cast<uint64_t>(baseLife)+(targetLife>life?targetLife-life:0);
   if(targetLife>=life)LegendNative::setStat(unit,6,static_cast<int32_t>(newLife>cap?cap:newLife),0);
   actualLife=stat(unit,6,0);applied=actualMax==target&&actualLife==targetLife;
  }
 }
 Text t;t.Add(applied?"ES_NATIVE_SUMMON_HEALTH uid=":"ES_NATIVE_SUMMON_HEALTH_PARTIAL uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" skill=");t.Num(p.skill);t.Add(" percent=");t.Num(cfg.legendSummonHealth);t.Add(" old_max_hp=");t.Num(maximum/256);t.Add(" actual_max_hp=");t.Num(actualMax/256);t.Add(" old_hp=");t.Num(life/256);t.Add(" actual_hp=");t.Num(actualLife/256);t.Add(" capped=");t.Num(desired>cap);t.Add(" apply_once=1 health_fraction_preserved=1");Log(t.s,!applied);
}
uint32_t UsedId(void* unit) noexcept {if(!CastContext::getUsed)return 0;void* selected=CastContext::getUsed(unit);uintptr_t row=0;int16_t id=-1;return selected&&Read(reinterpret_cast<uintptr_t>(selected),row)&&row&&Read(row,id)&&id>=0?static_cast<uint16_t>(id):0;}
bool TrapSkill(uint32_t skill) noexcept {const auto id=LegendNative::Original(skill);return id==257||id==271||id==262||id==276;}
bool PetGroup(uint32_t a,uint32_t b) noexcept {return a==b||(TrapSkill(a)&&TrapSkill(b));}



bool CreatureSummon(uint32_t skill) noexcept {
 const auto* meta=LegendNative::Meta(skill);const uint32_t id=LegendNative::Original(skill);
 return meta&&meta->summon&&id!=88&&id!=62&&!TrapSkill(skill);
}
uint32_t NativePetLimit(const Boss& b,uint32_t skill) noexcept {
 void* unit=lookup(b.game,1,b.uid);void* learned=unit?LegendNative::find(unit,skill,-1):nullptr;
 uintptr_t row=0;uint32_t expression=0,level=0;uint8_t bank=0;
 if(!learned||!LegendNative::formula||!Read(reinterpret_cast<uintptr_t>(learned),row)||!row||!Read(row+0x114,expression)||!Read(reinterpret_cast<uintptr_t>(learned)+0x40,level)||!level||!Read(reinterpret_cast<uintptr_t>(b.game)+0x106,bank))return 0;
 for(const auto& s:b.skills)if(s.id==skill){level=s.level;break;}
 const auto raw=LegendNative::priorFormula?LegendNative::priorFormula:LegendNative::formula;
 const int32_t n=raw(bank,unit,expression,skill,level);
 return n<=0?0:n>32?32:static_cast<uint32_t>(n);
}
uint32_t PetLimit(const Boss& b,uint32_t skill) noexcept {
 if(LegendNative::Original(skill)==88)return 32;



 if(b.sovereign&&TrapSkill(skill))return cfg.trapMax;
 if(CreatureSummon(skill)){
  void* unit=lookup(b.game,1,b.uid);void* learned=unit?LegendNative::find(unit,skill,-1):nullptr;uintptr_t row=0;uint32_t level=0;
  if(!learned||!Read(reinterpret_cast<uintptr_t>(learned),row)||!row||!Read(reinterpret_cast<uintptr_t>(learned)+0x40,level)||!level)return 0;
  return SummonTuning::Limit(skill,b.sovereign?cfg.summonMax:cfg.legendSummonMax);
 }
 uint32_t limit=NativePetLimit(b,skill);if(!limit)return 0;
 if(b.sovereign){if(skill==62)return 3*(limit<cfg.hydraMax?limit:cfg.hydraMax);if(TrapSkill(skill)&&limit>cfg.trapMax)limit=cfg.trapMax;}
 return limit;
}


uint32_t NativeGroupLimit(const Boss& b,uint32_t skill) noexcept {
 const uint32_t own=PetLimit(b,skill);void* unit=lookup(b.game,1,b.uid);void* node=unit?LegendNative::find(unit,skill,-1):nullptr;uintptr_t row=0;int8_t type=-1;
 if(!node||!Read((uintptr_t)node,row)||!row||!Read(row+0x112,type)||type<0)return own;
 uint32_t sum=0,seen[3]={},n=0;
 for(const auto& s:b.skills){if(!CreatureSummon(s.id))continue;bool duplicate=false;for(uint32_t j=0;j<n;++j)duplicate|=seen[j]==s.id;if(duplicate)continue;seen[n++]=s.id;
  void* other=LegendNative::find(unit,s.id,-1);uintptr_t data=0;int8_t otherType=-1;
  if(other&&Read((uintptr_t)other,data)&&data&&Read(data+0x112,otherType)&&otherType==type)sum+=PetLimit(b,s.id);
 }
 return sum>own?(sum>32?32:sum):own;
}
uint32_t PetCount(const Boss& b,uint32_t skill) noexcept {uint32_t n=0;for(uint32_t i=0;i<petHigh;++i){const auto& p=pets[i];if(p.active&&!p.retire&&!p.missing&&p.game==b.game&&p.epoch==b.epoch&&p.owner==b.uid&&p.ownerKind==b.kind&&PetGroup(p.skill,skill))++n;}return n;}


bool RefillSkill(uint32_t id) noexcept {
 return cfg.summonRefill&&CreatureSummon(id);
}
bool RefillNeeded(const Boss& b,uint32_t id) noexcept {
 if(!RefillSkill(id))return false;const uint32_t limit=PetLimit(b,id);return limit&&PetCount(b,id)<limit;
}
uint32_t RefillGap(uint32_t minimum) noexcept {return cfg.summonRefillGap>minimum?cfg.summonRefillGap:minimum;}
void RefillLog(const Boss& b,uint32_t id,uint32_t gap) noexcept {
 return;
}
void PetLost(Pet& p,const char* reason,bool inactive) noexcept {
 if(inactive)p.active=false;
 Text t;t.Add("ES_NATIVE_PET_GONE uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" skill=");t.Num(p.skill);t.Add(" reason=");t.Add(reason);t.Add(" replacement_allowed=1");Log(t.s);
}
void PetRetire(Pet& p,void* unit,const char* reason) noexcept {
 p.active=false;
 if(unit&&(LegendNative::Legend(unit)||LegendNative::SovereignLeader(unit))){Log("ES_NATIVE_PET_GUARD legend_retirement_blocked=1 boss_drop_untouched=1",true);return;}
 if(unit){LegendNative::noLoot(unit,1);LegendNative::kill(p.game,unit,nullptr,0);}
 Text t;t.Add("ES_NATIVE_PET_REMOVED uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" reason=");t.Add(reason);Log(t.s);
}
void NativeReset() noexcept {petHigh=0;petNextPoll=0;}
void PetPoll(void* game) noexcept {
 const uint64_t now=GetTickCount64();if(now<petNextPoll)return;petNextPoll=now+200;
 for(uint32_t i=0;i<petHigh;++i){auto& p=pets[i];if(!p.active||p.game!=game||p.thread!=GetCurrentThreadId())continue;
  void* unit=lookup(game,1,p.uid);if(!unit){
   if(!p.missingSince)p.missingSince=now;
   if(!p.missing&&now-p.missingSince>=2000){p.missing=true;PetLost(p,"not_found_after_grace",false);}continue;
  }
  const bool returned=p.missing;p.missing=false;p.missingSince=0;
  uint32_t id[3]={},mode=0;if(!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1||id[1]!=p.kind||id[2]!=p.uid){PetLost(p,"identity_changed",true);continue;}
  if(!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||stat(unit,6,0)<256){PetLost(p,"dead_or_expired",true);continue;}
  Boss* b=NativeBoss(game,p.owner,p.ownerKind);void* owner=b?lookup(game,1,p.owner):nullptr;
  if(returned&&b){const uint32_t limit=PetLimit(*b,p.skill);if(PetCount(*b,p.skill)>limit)p.retire=true;Text t;t.Add("ES_NATIVE_PET_RETURNED uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" over_cap=");t.Num(p.retire);Log(t.s);}
  if(p.retire||!b||b->ownerDied||(owner&&stat(owner,6,0)<256))PetRetire(p,unit,p.retire?"capacity":"owner_dead_or_unregistered");
  else {NativePetHostile(p,unit);if(!p.healthApplied)PetHealth(p,unit);}
 }
}
uint32_t ApproachRange(const Boss& b) noexcept {if(b.kind>=2051&&b.kind<=2066)return 3;bool ranged=false;for(uint32_t i=0;i<3;++i){const auto& s=b.skills[i];if(s.kind==4)return 3;if(s.kind==0&&!b.failedEffects[i])ranged=true;}return ranged?9:3;}
void NativePoll(Boss& b,void* unit,void* target,uint64_t now) noexcept {
 uint32_t mode=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode))return;
 if(b.nativeStarted&&mode!=b.nativeMode){
  if(LegendNative::FreeMode(unit)&&!b.nativeHandlers&&!b.nativePets)for(const auto& s:b.skills)if(s.id==b.nativeSkill)LegendNative::CompleteCreatureCast(b.game,unit,s.id,s.level,b.nativeStarted);
  const bool retry=LegendNative::HitReaction(mode)&&NativeRoot(b,unit)&&!b.nativeMissiles&&!b.nativePets&&!b.nativeHandlers;
  const uint64_t resume=now+(retry&&cfg.interruptedRetry<cfg.legendRecovery?cfg.interruptedRetry:cfg.legendRecovery);if(b.nextAny<resume)b.nextAny=resume;
  if(retry){const uint64_t deadline=now+cfg.interruptedRetry;for(uint32_t i=0;i<3;++i)if(b.skills[i].id==b.nativeSkill){if(b.next[i]>deadline)b.next[i]=deadline;b.cursor=i;}if(b.nextAny>deadline)b.nextAny=deadline;Text t;t.Add("ES_INTERRUPTED_CAST_RETRY uid=");t.Num(b.uid);t.Add(" skill=");t.Num(b.nativeSkill);t.Add(" role=dungeon_legend retry_ms=");t.Num(cfg.interruptedRetry);t.Add(" before_handler=1 native_ai_preserved=1");Log(t.s);}
  Text wait;wait.Add("ES_NATIVE_RECOVERY uid=");wait.Num(b.uid);wait.Add(" skill=");wait.Num(b.nativeSkill);wait.Add(" time_ms=");wait.Num(now);wait.Add(" observed_mode=");wait.Num(mode);wait.Add(" wait_ms=");wait.Num(b.nextAny-now);wait.Add(" next_skill_at_ms=");wait.Num(b.nextAny);wait.Add(" movement_allowed=1");Log(wait.s);
  Text t;t.Add("ES_NATIVE_ANIMATION uid=");t.Num(b.uid);t.Add(" skill=");t.Num(b.nativeSkill);t.Add(" mode_now=");t.Num(mode);t.Add(" used_now=");t.Num(UsedId(unit));t.Add(" missiles=");t.Num(b.nativeMissiles);t.Add(" pets=");t.Num(b.nativePets);t.Add(" handlers=");t.Num(b.nativeHandlers);t.Add(" trap_reactions_blocked=");t.Num(LegendNative::TrapCastingSkill(b.nativeSkill)?b.trapBlocks:0);t.Add(" cast_reactions_blocked=");t.Num(b.trapBlocks);t.Add(" elapsed_ms=");t.Num(now-b.nativeStarted);t.Add(" lightning_enchanted_excluded=1 effect_requires_handler_or_creation_evidence=1");Log(t.s);
  if(!b.nativeMissiles&&!b.nativePets&&!b.nativeHandlers){Text missing;missing.Add("ES_NATIVE_EFFECT_UNOBSERVED uid=");missing.Num(b.uid);missing.Add(" skill=");missing.Num(b.nativeSkill);missing.Add(" requested_mode=");missing.Num(b.nativeMode);missing.Add(" no_handler_or_creation_observed=1 not_effect_confirmation=1");Log(missing.s,true);}b.nativeStarted=0;
 }
 int32_t x=0,y=0,tx=0,ty=0;const bool positioned=Position(unit,x,y);
 if(positioned&&(!b.progressSeen||x!=b.lastX||y!=b.lastY)){b.progressAt=now;b.lastX=x;b.lastY=y;b.progressSeen=true;}b.lastMode=mode;
 if(now<b.nextState)return;b.nextState=now+3000;
 const bool located=target&&positioned&&Position(target,tx,ty);
 Text t;t.Add("ES_NATIVE_STATE uid=");t.Num(b.uid);t.Add(" time_ms=");t.Num(now);t.Add(" mode=");t.Num(mode);t.Add(" used=");t.Num(UsedId(unit));t.Add(" target_found=");t.Num(target!=nullptr);t.Add(" target_type=");t.Num(b.targetType);t.Add(" target_id=");t.Num(b.targetId);t.Add(" distance=");t.Num(located?Distance(x,y,tx,ty):0);t.Add(" distance_valid=");t.Num(located);t.Add(" approach_range=");t.Num(ApproachRange(b));t.Add(" cooldown_ms=");t.Num(now<b.nextAny?b.nextAny-now:0);t.Add(" native_elapsed_ms=");t.Num(b.nativeStarted?now-b.nativeStarted:0);t.Add(" stagnant_ms=");t.Num(b.progressSeen?now-b.progressAt:0);uint32_t flags=0;Read(reinterpret_cast<uintptr_t>(unit)+0x124,flags);t.Add(" notc=");t.Num((flags&0x20000)!=0);t.Add(" action=");t.Add(!target?"no_target":!LegendNative::FreeMode(unit)?"animation":mode==2||mode==15?"walking":now<b.nextAny?"cooldown":"select");Log(t.s);
}
void NativeCast(Boss& b,void* unit,void* opponent,uint32_t index,uint64_t now) noexcept {
 const auto& s=b.skills[index];const auto* meta=LegendNative::Meta(s.id);
 const uint32_t castMode=SharedOffensive::Supported(s.id)?LegendNative::SharedMode(b.kind):(meta?meta->mode:0);
 if(cfg.continuousHammer&&LegendNative::Original(s.id)==112)return;
 if(s.kind==7&&LegendNative::AuraState(s.id)>=0)return;
 if(!meta||!castMode||!LegendNative::ready||b.busy||now<b.next[index]||now<b.nextAny||!LegendNative::FreeMode(unit)||!opponent||stat(opponent,6,0)<256)return;
 int32_t x=0,y=0,tx=0,ty=0;if(!Position(unit,x,y)||!Position(opponent,tx,ty))return;const uint32_t distance=Distance(x,y,tx,ty);
 if(Level(unit)!=Level(opponent)||!CastInRange(b.game,unit,opponent,s.id,s.kind,s.level))return;


 uint32_t mode=0;Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode);
 if((mode==2||mode==15)&&now<b.walkCommit&&distance>3&&CastMelee(s.id,s.kind))return;


 if(LegendNative::collision&&s.kind!=1&&s.kind!=5&&s.kind!=7&&LegendNative::collision(unit,opponent,4))return;

 if(meta->summon){const uint32_t n=PetCount(b,s.id),limit=PetLimit(b,s.id);
  if(n!=b.summonCount[index]||limit!=b.summonLimit[index]||now>=b.summonLog[index]){b.summonCount[index]=n;b.summonLimit[index]=limit;b.summonLog[index]=now+10000;Text t;t.Add("ES_NATIVE_SUMMON_STATE uid=");t.Num(b.uid);t.Add(" skill=");t.Num(s.id);t.Add(" alive=");t.Num(n);t.Add(" limit=");t.Num(limit);t.Add(" shared_trap_limit=");t.Num(TrapSkill(s.id));t.Add(" cast_needed=");t.Num(limit&&n<limit);t.Add(CreatureSummon(s.id)?" limit_source=ini":" native_petmax=1");Log(t.s);}
  if(!limit||n>=limit){b.next[index]=now+500;b.cursor=(index+1)%3;return;}
 }
 void* target=opponent;
 if(s.kind==1||s.kind==5||s.kind==7)target=unit;
 if(s.kind==2||s.kind==6)target=DeadTarget(b,unit);
 if(!target){b.next[index]=now+750;b.cursor=(index+1)%3;return;}
 if(!NativeLevels(b,unit))return;
 void* learned=LegendNative::find(unit,s.id,-1);uint32_t learnedLevel=0;
 if(!learned||!Read(reinterpret_cast<uintptr_t>(learned)+0x40,learnedLevel)||learnedLevel<1){Text t;t.Add("ES_NATIVE_SKILL_MISSING uid=");t.Num(b.uid);t.Add(" skill=");t.Num(s.id);t.Add(" compile_and_restart=1");Log(t.s,true);b.next[index]=now+30000;return;}
 if(s.kind==1&&LegendNative::state){uintptr_t row=0;int16_t state=-1;if(Read(reinterpret_cast<uintptr_t>(learned),row)&&row&&Read(row+0xa0,state)&&state>0&&LegendNative::state(unit,state)){b.next[index]=now+1000;b.cursor=(index+1)%3;return;}}
 if(!Position(target,tx,ty))return;


 int32_t aimX=tx,aimY=ty;if(target==unit&&!Position(opponent,aimX,aimY))return;
 uintptr_t path=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+0x38,path)||!path)return;
 if(LegendNative::face)LegendNative::face(reinterpret_cast<void*>(path),aimX,aimY,1);
 uint32_t gap=s.refresh?s.refresh:s.cooldown;if(RefillSkill(s.id)){gap=RefillGap(cfg.legendGap);RefillLog(b,s.id,gap);}if(!b.sovereign&&CreatureSummon(s.id)&&gap<cfg.legendSummonInterval)gap=cfg.legendSummonInterval;if(gap<cfg.legendGap)gap=cfg.legendGap;if(s.id==466&&gap<BladeWarpGapMs)gap=BladeWarpGapMs;
 b.next[index]=now+gap;b.nextAny=now+cfg.legendGap;b.cursor=(index+1)%3;b.busy=true;
 LegendNative::Mana(unit);b.nativeSkill=s.id;b.trapBlocks=0;b.nativeStarted=now;b.factoryDue=true;b.castFrameValid=Read(reinterpret_cast<uintptr_t>(b.game)+0x170,b.castFrame);b.nativeMissiles=b.nativePets=b.nativeHandlers=0;
 Text aim;aim.Add("ES_NATIVE_AIM uid=");aim.Num(b.uid);aim.Add(" skill=");aim.Num(s.id);aim.Add(" target_type=");aim.Num(b.targetType);aim.Add(" target_id=");aim.Num(b.targetId);aim.Add(" x=");aim.Num(x);aim.Add(" y=");aim.Num(y);aim.Add(" aim_x=");aim.Num(aimX);aim.Add(" aim_y=");aim.Num(aimY);aim.Add(" native_face_before_cast=1");Log(aim.s);
 Text t;t.Add("ES_NATIVE_CAST_BEGIN uid=");t.Num(b.uid);t.Add(" class=");t.Num(b.kind);t.Add(" skill=");t.Num(s.id);t.Add(" original=");t.Num(meta->original);t.Add(" learned_level=");t.Num(learnedLevel);t.Add(" mode=");t.Num(castMode);t.Add(" target_id=");uint32_t tid=0;Read(reinterpret_cast<uintptr_t>(target)+8,tid);t.Num(tid);t.Add(" time_ms=");t.Num(now);t.Add(" native_ai=1 item_proc=0");Log(t.s);
 const int32_t result=LegendNative::use(b.game,unit,static_cast<uint8_t>(castMode),s.id,target,tx,ty,0);
 b.busy=false;Read(reinterpret_cast<uintptr_t>(unit)+0xc,b.nativeMode);
 Text done;done.Add(result?"ES_NATIVE_CAST_ACCEPTED uid=":"ES_NATIVE_CAST_REJECT uid=");done.Num(b.uid);done.Add(" skill=");done.Num(s.id);done.Add(" mode_after=");done.Num(b.nativeMode);done.Add(" used_after=");done.Num(UsedId(unit));done.Add(" returned=");done.Num(result);done.Add(" accepted_mode_is_not_effect_confirmation=1");Log(done.s,!result);
 if(!result){b.nativeStarted=0;if(RefillSkill(s.id)){uint32_t backoff=s.refresh?s.refresh:s.cooldown;if(backoff<5000)backoff=5000;b.next[index]=now+backoff;}}
}
void PetMove(Pet& p,void* unit,void* target,uint64_t now,uint32_t distance) noexcept {


 const int32_t result=LegendNative::walk?LegendNative::walk(p.game,unit,target,0):0;
 p.next=now+(result?600:300);
 if(now>=p.moveLog){p.moveLog=now+3000;Text t;t.Add("ES_NATIVE_PET_MOVE uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" distance=");t.Num(distance);t.Add(" returned=");t.Num(result);t.Add(" native_pathfinder=1 attack_before_melee_range=0");Log(t.s,!result);}
}
bool PetOpponent(const Boss& b,void* unit,void* target) noexcept {
 uint32_t type=99,mode=0,id=0;int32_t x=0,y=0,tx=0,ty=0;
 return target&&target!=unit&&Read((uintptr_t)target,type)&&Ally(target,type)&&Read((uintptr_t)target+8,id)&&lookup(b.game,type,id)==target&&Read((uintptr_t)target+0xc,mode)&&mode!=0&&mode!=12&&stat(target,6,0)>=256&&Level(unit)==Level(target)&&Position(unit,x,y)&&Position(target,tx,ty)&&Distance(x,y,tx,ty)<=40;
}
void* PetTarget(Pet& p,void* unit,const Boss& b) noexcept {
 void* target=p.targetId?lookup(p.game,p.targetType,p.targetId):nullptr;
 if(!PetOpponent(b,unit,target))target=LegendNative::currentTarget?LegendNative::currentTarget(p.game,unit):nullptr;
 if(!PetOpponent(b,unit,target)){
  uintptr_t data=0,control=0;int32_t distance=0,combat=0;
  if(Read((uintptr_t)unit+0x10,data)&&data&&Read(data+0x30,control)&&control&&Readable(control,0x3a))target=LegendNative::chooseTarget(p.game,unit,(void*)control,&distance,&combat,0);
 }


 if(!PetOpponent(b,unit,target)&&each){
  Snapshot s={};each(p.game,Capture,&s,0);uint32_t best=41;int32_t x=0,y=0;
  if(Position(unit,x,y))for(uint32_t i=0;i<s.count;++i){void* candidate=lookup(p.game,0,s.players[i].uid);if(!PetOpponent(b,unit,candidate))continue;const uint32_t d=Distance(x,y,s.players[i].x,s.players[i].y);if(d<best){best=d;target=candidate;}}
 }
 if(!PetOpponent(b,unit,target)){p.targetId=0;return nullptr;}
 uint32_t type=0,id=0;Read((uintptr_t)target,type);Read((uintptr_t)target+8,id);
 const uint64_t now=GetTickCount64();
 if(type!=p.targetType||id!=p.targetId||now>=p.targetLog){p.targetType=type;p.targetId=id;p.targetLog=now+5000;void* owner=lookup(p.game,1,p.owner);int32_t x=0,y=0,ox=0,oy=0;const bool located=owner&&Position(unit,x,y)&&Position(owner,ox,oy);Text t;t.Add("ES_PET_INDEPENDENT_TARGET uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" owner_present=");t.Num(owner!=nullptr);t.Add(" owner_distance_valid=");t.Num(located);t.Add(" owner_distance=");t.Num(located?Distance(x,y,ox,oy):0);t.Add(" target_type=");t.Num(type);t.Add(" target_id=");t.Num(id);t.Add(" pet_radius=40 owner_distance_gate=0");Log(t.s);}
 return target;
}
void SupportSpiritMove(Pet& p,void* unit,void* owner,const Boss& b,uint64_t now) noexcept {
 int32_t x=0,y=0,ox=0,oy=0;if(!Position(unit,x,y))return;const bool hasOwner=owner&&Position(owner,ox,oy);if(!hasOwner){ox=x;oy=y;}
 void* threat=PetTarget(p,unit,b);
 int32_t tx=0,ty=0;const bool close=PetOpponent(b,unit,threat)&&Position(threat,tx,ty)&&Distance(x,y,tx,ty)<=5;
 const uint32_t ownerDistance=Distance(x,y,ox,oy);
 if(!close&&hasOwner&&ownerDistance>8){PetMove(p,unit,owner,now,ownerDistance);return;}
 int32_t aimX=0,aimY=0;const char* action="roam";
 if(close){
  int32_t dx=(x>tx)-(x<tx),dy=(y>ty)-(y<ty);if(!dx&&!dy)dx=(p.uid&1)?1:-1;
  aimX=x+dx*4;aimY=y+dy*4;

  action="evade";
 }else{
  constexpr int32_t dx[]={1,1,0,-1,-1,-1,0,1},dy[]={0,1,1,1,0,-1,-1,-1};
  const uint32_t step=(p.uid+p.roamStep++)%8;aimX=ox+dx[step]*3;aimY=oy+dy[step]*3;
  if(Distance(x,y,aimX,aimY)<2){p.next=now+1000;return;}
 }
 if(aimX<=0||aimY<=0||aimX>65535||aimY>65535)return;
 const int32_t result=LegendNative::walkXY?LegendNative::walkXY(p.game,unit,aimX,aimY):0;
 p.next=now+(result?(close?1000:1800):500);
 if(now>=p.moveLog){p.moveLog=now+3000;Text t;t.Add("ES_SUPPORT_PET_MOVE uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" action=");t.Add(action);t.Add(" returned=");t.Num(result);t.Add(" native_collision_and_path=1 owner_required=0 local_evade=1 teleport=0");Log(t.s,!result);}
}
void PetActionStarted(Pet& p,void* unit,uint32_t skill,int32_t accepted) noexcept;
void DefilerThink(Pet& p,void* unit,void* owner,const Boss& b,uint64_t now) noexcept {
 void* threat=PetTarget(p,unit,b);int32_t x=0,y=0,tx=0,ty=0;if(!Position(unit,x,y))return;
 void* node=LegendNative::find(unit,405,-1);uintptr_t row=0;int16_t state=-1;
 const bool linked=node&&Read((uintptr_t)node,row)&&row&&Read(row+0xa0,state)&&state>0&&LegendNative::state(unit,state);
 if(node&&!linked&&now>=p.supportNext){
  void* ally=nullptr;uint32_t best=21;
  auto consider=[&](void* candidate){uint32_t type=99,mode=0;int32_t ax=0,ay=0;if(!candidate||candidate==unit||!Read((uintptr_t)candidate,type)||type!=1||alignment(candidate)!=0||!Read((uintptr_t)candidate+0xc,mode)||mode==0||mode==12||stat(candidate,6,0)<256||Level(unit)!=Level(candidate)||!Position(candidate,ax,ay))return;uint32_t d=Distance(x,y,ax,ay);if(d<best&&(!LegendNative::collision||!LegendNative::collision(unit,candidate,4))){best=d;ally=candidate;}};
  consider(owner);
  for(uint32_t i=0;i<petHigh;++i){const auto& peer=pets[i];if(peer.active&&!peer.retire&&!peer.missing&&peer.game==p.game&&peer.epoch==p.epoch&&peer.owner==p.owner&&peer.ownerKind==p.ownerKind)consider(lookup(p.game,1,peer.uid));}
  if(ally){Position(ally,tx,ty);const int32_t result=LegendNative::use(p.game,unit,8,405,ally,tx,ty,0);PetActionStarted(p,unit,405,result);p.next=now+(result?1000:500);p.supportNext=now+(result?5000:1000);return;}
 }



 if(!PetOpponent(b,unit,threat)||!Position(threat,tx,ty)){SupportSpiritMove(p,unit,owner,b,now);return;}
 const uint32_t distance=Distance(x,y,tx,ty);
 if(distance>3||(LegendNative::collision&&LegendNative::collision(unit,threat,4))){PetMove(p,unit,threat,now,distance);return;}
 uintptr_t path=0;if(Read((uintptr_t)unit+0x38,path)&&path&&LegendNative::face)LegendNative::face((void*)path,tx,ty,1);
 const int32_t result=LegendNative::use(p.game,unit,4,0,threat,tx,ty,0);
 PetActionStarted(p,unit,0,result);p.next=now+(result?800:300);
}
void PetActionStarted(Pet& p,void* unit,uint32_t skill,int32_t accepted) noexcept {
 p.actionWatching=accepted!=0;p.actionSkill=skill;
 p.actionFrameValid=p.actionWatching&&Read((uintptr_t)p.game+0x170,p.actionFrame)&&Read((uintptr_t)unit+0xc,p.actionMode);
}
bool PetActionWait(Pet& p,void* unit) noexcept {
 if(LegendNative::FreeMode(unit)){p.actionWatching=false;return false;}
 uint32_t mode=0,frame=0;Read((uintptr_t)unit+0xc,mode);const uint32_t used=UsedId(unit);const uint64_t now=GetTickCount64();
 if(p.actionWatching&&(mode!=p.actionMode||used!=p.actionSkill))p.actionWatching=false;


 const bool expired=p.actionWatching&&p.actionFrameValid&&Read((uintptr_t)p.game+0x170,frame)&&uint32_t(frame-p.actionFrame)>=250&&uint32_t(frame-p.actionFrame)<0x80000000u;
 if(expired){p.actionWatching=false;p.next=now+1200;const int32_t result=LegendNative::move(p.game,unit,1,nullptr);Text t;t.Add("ES_PET_ACTION_TIMEOUT uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" skill=");t.Num(used);t.Add(" mode=");t.Num(mode);t.Add(" elapsed_frames=");t.Num(uint32_t(frame-p.actionFrame));t.Add(" native_neutral_result=");t.Num((uint32_t)result);t.Add(" own_action_only=1 extra_damage=0");Log(t.s,true);return true;}
 if(now>=p.waitLog){p.waitLog=now+5000;Text t;t.Add("ES_PET_ACTION_WAIT uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" used=");t.Num(used);t.Add(" mode=");t.Num(mode);t.Add(" own_action=");t.Num(p.actionWatching);t.Add(" frame_valid=");t.Num(p.actionFrameValid);t.Add(" observation_only=1");Log(t.s);}
 return true;
}
void PetThink(Pet& p,void* unit) noexcept {
 Boss* b=NativeBoss(p.game,p.owner,p.ownerKind);void* owner=b?lookup(p.game,1,p.owner):nullptr;
 if(p.retire||!b||b->ownerDied||(owner&&stat(owner,6,0)<256)){PetRetire(p,unit,"owner_or_capacity");return;}
 if(!p.healthApplied)PetHealth(p,unit);

 LegendNative::idle(p.game,unit,5);
 if(PetActionWait(p,unit)||!NativePetHostile(p,unit))return;
 const uint64_t now=GetTickCount64();if(now<p.next)return;
 LegendNative::Mana(unit);const uint32_t original=LegendNative::Original(p.skill);
 if(original==88){p.next=now+1000;return;}
 if(original==226||original==246){SupportSpiritMove(p,unit,owner,*b,now);return;}
 if(original==377){DefilerThink(p,unit,owner,*b,now);return;}

 void* target=original==231?DeadTargetForSkill(*b,unit,231):PetTarget(p,unit,*b);
 if(original!=231&&!PetOpponent(*b,unit,target)||!target){if(now>=p.waitLog){p.waitLog=now+5000;Text t;t.Add("ES_PET_NO_TARGET uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" radius=40 observation_only=1");Log(t.s);}return;}
 int32_t x=0,y=0,tx=0,ty=0;if(!Position(unit,x,y)||!Position(target,tx,ty))return;const uint32_t distance=Distance(x,y,tx,ty);


 const uint32_t attackRange=original==80||original==376?12:3;
 if(distance>attackRange||(attackRange>3&&LegendNative::collision&&LegendNative::collision(unit,target,4))){PetMove(p,unit,target,now,distance);return;}
 uint32_t chosen=0,mode=4;uintptr_t list=0,entry=0;
 if(Read((uintptr_t)unit+0x100,list)&&list&&Read(list,entry)){
  uint32_t candidates[8]={},modes[8]={},n=0;uint8_t bank=0;Read((uintptr_t)p.game+0x106,bank);
  using MonFn=const void*(__fastcall*)(uint8_t,int32_t);auto mon=(MonFn)(ctx->exeBase+0x976e0);const uintptr_t row=(uintptr_t)mon(bank,p.kind);
  for(uint32_t k=0;k<8&&row;++k){int16_t sid=-1;uint8_t m=0;Read(row+0x1c4+2*k,sid);Read(row+0x1d4+k,m);
   if(sid<=0||m>=16||sid==221||sid==54||sid==292||sid==356||!LegendNative::find(unit,sid,-1))continue;

   if(sid==420){if(LegendNative::right(unit)!=LegendNative::find(unit,sid,-1))LegendNative::assign(unit,0,sid,-1);continue;}
   candidates[n]=sid;modes[n++]=m;
  }
  if(n){const uint32_t pick=p.cursor++%n;chosen=candidates[pick];mode=modes[pick];}
 }
 if(chosen==405){target=owner;Position(target,tx,ty);}
 if(chosen&&LegendNative::find(unit,chosen,-1)){
  if(original==231&&now>=p.targetLog){
   p.targetLog=now+5000;uint32_t corpse=0;uintptr_t row=0;int16_t execute=-1;
   Read((uintptr_t)target+8,corpse);void* node=LegendNative::find(unit,chosen,-1);
   if(node&&Read((uintptr_t)node,row)&&row)Read(row+0x4e,execute);
   Text t;t.Add("ES_CARRION_TARGET uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" corpse=");t.Num(corpse);t.Add(" skill=");t.Num(chosen);t.Add(" execute=");t.Num((uint16_t)execute);t.Add(" distance=");t.Num(distance);t.Add(" native_eligible=1 reserved_state=0 corpse_unchanged=1");Log(t.s);
  }
  uintptr_t path=0;if(Read((uintptr_t)unit+0x38,path)&&path&&LegendNative::face)LegendNative::face((void*)path,tx,ty,1);
  const int32_t result=LegendNative::use(p.game,unit,uint8_t(mode),chosen,target,tx,ty,0);PetActionStarted(p,unit,chosen,result);p.next=now+(result?1000:300);++p.shots;
  Text t;t.Add("ES_NATIVE_PET_CAST uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" skill=");t.Num(chosen);t.Add(" mode=");t.Num(mode);t.Add(" distance=");t.Num(distance);t.Add(" returned=");t.Num(result);Log(t.s,!result);
 }else {const int32_t result=LegendNative::use(p.game,unit,4,0,target,tx,ty,0);PetActionStarted(p,unit,0,result);p.next=now+(result?800:300);}
}
}
namespace LegendNative {
void* PetHealthStart(void* game,void* unit,int32_t id) noexcept {
 using namespace BossSkills;const auto* meta=Meta(id);if(!meta||!meta->summon)return nullptr;
 uint32_t ident[3]={};if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),ident)||ident[0]!=1)return nullptr;
 auto* b=NativeBoss(game,ident[2],ident[1]);if(!b||!NativeRoot(*b,unit))return nullptr;
 for(const auto& s:b->skills)if(s.id==id){++b->healthSpawnDepth;return b;}
 return nullptr;
}
void PetHealthEnd(void* scope) noexcept {
 using namespace BossSkills;auto& b=*static_cast<Boss*>(scope);
 if(b.healthSpawnDepth)--b.healthSpawnDepth;
 if(b.healthSpawnDepth||!NativeRoot(b,lookup(b.game,1,b.uid)))return;


 for(uint32_t i=0;i<petHigh;++i){auto& p=pets[i];if(p.active&&!p.healthApplied&&p.game==b.game&&p.epoch==b.epoch&&p.owner==b.uid&&p.ownerKind==b.kind&&p.thread==GetCurrentThreadId())PetHealth(p,lookup(p.game,1,p.uid));}
}
void InitLevels(void* game,void* unit) noexcept {
 uint32_t id[3]={};if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1)return;
 auto* b=BossSkills::NativeBoss(game,id[2],id[1]);if(b)BossSkills::NativeLevels(*b,unit);
}
int32_t GuardLevel(void* game,void* unit,int32_t id,int32_t level) noexcept {
 uint32_t ident[3]={};if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),ident)||ident[0]!=1)return level;
 auto* b=BossSkills::NativeBoss(game,ident[2],ident[1]);if(!b||!BossSkills::NativeRoot(*b,unit))return level;
 for(const auto& s:b->skills)if(s.id==id){
  BossSkills::NativeLevels(*b,unit);
  if(level!=s.level){Text t;t.Add("ES_NATIVE_HANDLER_LEVEL uid=");t.Num(b->uid);t.Add(" skill=");t.Num(id);t.Add(" incoming_level=");t.Num(static_cast<uint32_t>(level));t.Add(" configured_level=");t.Num(s.level);t.Add(" execution_level=");t.Num(s.level);t.Add(" registered_boss_only=1");Log(t.s);}
  return static_cast<int32_t>(s.level);
 }
 return level;
}
bool Owned(void* game,void* unit) noexcept {return BossSkills::NativePet(game,unit)!=nullptr;}
bool PanelOwned(void* unit) noexcept {
 using namespace BossSkills;uint32_t id[3]={};if(!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1)return false;
 for(uint32_t i=0;i<petHigh;++i){const auto& p=pets[i];if(p.active&&p.uid==id[2]&&p.kind==id[1]&&p.thread==GetCurrentThreadId()&&NativeBoss(p.game,p.owner,p.ownerKind)&&lookup(p.game,1,p.uid)==unit&&NativePet(p.game,unit)==&p)return true;}return false;
}
bool CustomPetAi(void* game,void* unit) noexcept {auto* p=BossSkills::NativePet(game,unit);return p&&!BossSkills::TrapSkill(p->skill)&&Original(p->skill)!=62;}
bool DispatchAi(void* game,void* unit) noexcept {
 using namespace BossSkills;uint32_t id[3]={};if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1)return false;
 if(Legend(unit)){auto* b=NativeBoss(game,id[2],id[1]);return b?NativeDecision(*b,unit):false;}
 if(id[1]>=815&&id[1]<1779&&(id[1]-815)%2==0)return SovereignNative::Decision(game,unit);
 auto* pet=NativePet(game,unit);if(!pet)return false;



 if(TrapSkill(pet->skill)||Original(pet->skill)==62){if(!NativePetHostile(*pet,unit)){idle(game,unit,5);return true;}return false;}
 PetThink(*pet,unit);return true;
}
void OwnerChanged(void* game,void* unit,int32_t ownerId,int32_t type) noexcept {
 using namespace BossSkills;if(type!=1||ownerId<0||!lookup||!unit)return;
 uint32_t identity[3]={};if(!Read(reinterpret_cast<uintptr_t>(unit),identity)||identity[0]!=1)return;
 if(Legend(unit)||SovereignLeader(unit)){Text t;t.Add("ES_NATIVE_OWNER_IGNORED uid=");t.Num(identity[2]);t.Add(" reason=legend_is_not_pet boss_drop_untouched=1");Log(t.s);return;}
 void* owner=lookup(game,1,ownerId);uint32_t id[3]={};if(!owner||!Read(reinterpret_cast<uintptr_t>(owner),id)||id[0]!=1)return;
 if(owner==unit)return;
 Boss* b=NativeBoss(game,id[2],id[1]);uint32_t source=0;
 if(!b){auto* parent=NativePet(game,owner);if(!parent)return;b=NativeBoss(game,parent->owner,parent->ownerKind);source=parent->skill;}
 if(!b)return;if(auto* existing=NativePet(game,unit)){existing->owner=b->uid;existing->ownerKind=b->kind;NativePetHostile(*existing,unit);return;}
 if(!source)source=UsedId(owner);
 bool assigned=false;for(const auto& sk:b->skills)if(sk.id==source&&source)assigned=true;
 if(!assigned){const uint64_t now=GetTickCount64();if(!b->nativeStarted||now-b->nativeStarted>5000)return;source=b->nativeSkill;}
 assigned=false;for(const auto& sk:b->skills)if(sk.id==source&&source)assigned=true;
 const auto* meta=Meta(source);if(!assigned||!meta||!meta->summon)return;
 uint32_t slot=0;for(;slot<petHigh&&pets[slot].active;++slot){}if(slot==16384){Log("ES_NATIVE_PET_RECORD_LIMIT",true);return;}
 if(slot==petHigh)++petHigh;auto& p=pets[slot];p={};p.epoch=b->epoch;p.created=GetTickCount64();p.game=game;p.uid=identity[2];p.kind=identity[1];p.owner=b->uid;p.ownerKind=b->kind;p.skill=source;p.thread=GetCurrentThreadId();p.active=true;
 p.retire=PetCount(*b,source)>PetLimit(*b,source);++b->nativePets;
 noLoot(unit,1);NativePetHostile(p,unit);
 Text t;t.Add("ES_NATIVE_PET_CREATED uid=");t.Num(p.uid);t.Add(" class=");t.Num(p.kind);t.Add(" owner=");t.Num(b->uid);t.Add(" owner_type=1 skill=");t.Num(source);t.Add(" alignment=");t.Num(alignment(unit));t.Add(" over_cap=");t.Num(p.retire);Log(t.s);
}
void SkillEvent(void* game,void* unit,uint32_t id,int32_t result,bool begin) noexcept {


 uint32_t suppressed=0;auto* pet=BossSkills::NativePet(game,unit);
 if(pet&&!cfg.logCasts){
  if(begin)return;
  const uint64_t now=GetTickCount64();
  if(result&&pet->handlerSkill==id&&now<pet->handlerNext){++pet->handlerSuppressed;return;}
  suppressed=pet->handlerSuppressed;pet->handlerSuppressed=0;pet->handlerSkill=id;pet->handlerNext=now+10000;
 }
 uint32_t uid=0;Read(reinterpret_cast<uintptr_t>(unit)+8,uid);Text t;t.Add(begin?"ES_NATIVE_HANDLER_ENTER uid=":"ES_NATIVE_HANDLER_RESULT uid=");t.Num(uid);t.Add(" skill=");t.Num(id);if(!begin){t.Add(" returned=");t.Num(static_cast<uint32_t>(result));if(pet&&!cfg.logCasts){t.Add(" repeated_successes_suppressed=");t.Num(suppressed);}}Log(t.s,!begin&&!result);


 uint32_t kind=0;Read(reinterpret_cast<uintptr_t>(unit)+4,kind);
 auto* b=BossSkills::NativeBoss(game,uid,kind);const auto* meta=Meta(id);
 if(begin){if(b&&b->nativeStarted&&b->nativeSkill==id)++b->nativeHandlers;return;}
 if(!b||!meta||!meta->summon||b->nativeSkill!=id||!b->nativeStarted)return;
 for(uint32_t i=0;i<3;++i)if(b->skills[i].id==id){
  if(result){b->failedEffects[i]=0;return;}
  if(b->failedCast[i]==b->nativeStarted)return;
  b->failedCast[i]=b->nativeStarted;
  if(b->failedEffects[i]<4)++b->failedEffects[i];
  const uint64_t delay=b->failedEffects[i]>=4?30000:5000ULL<<(b->failedEffects[i]-1);
  const uint64_t retry=GetTickCount64()+delay;if(b->next[i]<retry)b->next[i]=retry;
  Text d;d.Add("ES_NATIVE_EFFECT_BACKOFF uid=");d.Num(uid);d.Add(" skill=");d.Num(id);d.Add(" original=");d.Num(meta->original);d.Add(" failures=");d.Num(b->failedEffects[i]);d.Add(" retry_ms=");d.Num(delay);d.Add(" other_skills_and_movement_allowed=1");Log(d.s,true);return;
 }
}
void MissileEvent(void* game,void* owner,void* result) noexcept {
 using namespace BossSkills;uint32_t id[3]={};if(!Read(reinterpret_cast<uintptr_t>(owner),id))return;auto* b=NativeBoss(game,id[2],id[1]);if(!b){auto* p=NativePet(game,owner);if(p)b=NativeBoss(game,p->owner,p->ownerKind);}if(!b)return;
 uint32_t missileClass=0;if(result)Read(reinterpret_cast<uintptr_t>(result)+4,missileClass);


 if(result&&missileClass==195){if(++b->affixMissiles<=3||b->affixMissiles%20==0){Text a;a.Add("ES_NATIVE_AFFIX_MISSILE owner=");a.Num(id[2]);a.Add(" boss=");a.Num(b->uid);a.Add(" missile_class=195 name=lightunique excluded_from_skill_evidence=1");Log(a.s);}return;}
 if(result&&id[2]==b->uid&&b->nativeStarted&&UsedId(owner)==b->nativeSkill)++b->nativeMissiles;
 if(++b->actorMissiles>3&&b->actorMissiles%20)return;
 Text t;t.Add(result?"ES_NATIVE_MISSILE_CREATED owner=":"ES_NATIVE_MISSILE_REJECT owner=");t.Num(id[2]);t.Add(" boss=");t.Num(b->uid);t.Add(" skill=");t.Num(UsedId(owner));uint32_t uid=0,kind=0;if(result){Read(reinterpret_cast<uintptr_t>(result)+8,uid);Read(reinterpret_cast<uintptr_t>(result)+4,kind);}t.Add(" missile_uid=");t.Num(uid);t.Add(" missile_class=");t.Num(kind);Log(t.s,!result);
}
}
