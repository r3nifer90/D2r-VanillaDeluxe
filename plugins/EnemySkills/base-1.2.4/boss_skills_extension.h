#pragma once


#include "boss_skills_api.h"
#include <D2RLPlugin/plugin_communication.h>
#include "nova_route_trace.h"
#include "legend_native.h"
#include "shared_offensive_native.h"
namespace BossSkills {
using namespace ES;
bool Andariel(uint32_t kind) noexcept {return kind==793||kind==794;}
bool Duriel(uint32_t kind) noexcept {return kind==795||kind==796;}
bool CombatBoss(uint32_t kind) noexcept {return Andariel(kind)||Duriel(kind);}
bool Allowed(uint32_t kind) noexcept {return CombatBoss(kind)||((kind>=1779&&kind<2099)||(kind>=2102&&kind<2278));}
using LookupFn=void*(__fastcall*)(void*,uint32_t,uint32_t);
using TickFn=void(__fastcall*)(void*);
using EachFn=void(__fastcall*)(void*,void(__fastcall*)(void*,void*),void*,uint32_t);
LookupFn lookup=nullptr;TickFn priorTick=nullptr;void** tickSlot=nullptr;EachFn each=nullptr;
const D2RL::PluginCommunicationService* communication=nullptr;
constexpr uint32_t LegendRange=12,LegendGapMs=1000,BladeWarpRange=8,BladeWarpGapMs=6000;
bool Legend(uint32_t kind) noexcept {return ((kind>=1779&&kind<2099)||(kind>=2102&&kind<2278));}
struct Boss {uint64_t epoch=0;void* game=nullptr;uint32_t uid=0,kind=0,thread=0,targetType=0,targetId=0;R3niBossSkills::Skill skills[3];uint64_t next[3]={};bool active=false,busy=false,sovereign=false,ownerDied=false,logged[3]={};uint64_t logAfter[3]={};uint64_t nextAny=0;uint32_t cursor=0,nativeSkill=0,nativeMode=0,nativeMissiles=0,nativePets=0,nativeHandlers=0,affixMissiles=0,actorMissiles=0;uint64_t nativeStarted=0;
 uint32_t healthSpawnDepth=0,trapBlocks=0,executingSkill=0;
 uintptr_t speedUnit=0,speedList=0;int32_t speedBefore[2]={},speedAfter[2]={};bool speedApplied[2]={};uint64_t speedRetry=0;
 uint64_t summonCompletedAt=0;uint32_t summonCompletedSkill=0;
 uint32_t summonBatchDepth=0,summonBatchSkill=0;uint64_t summonBatchCast=0;
 uint32_t attackWatchFrame=0,attackWatchMode=0,aiFrame=0,modeAdjustments=0;bool attackWatching=false,aiFrameValid=false;uint64_t modeLog[2]={};
 uint64_t levelLog[3]={},moveModeLog=0,hurricaneDamageLog=0;uint32_t hurricaneDamageEvents=0;uintptr_t hurricaneDamageSource=0;
 uint64_t summonLog[3]={};uint32_t summonCount[3]={~0u,~0u,~0u},summonLimit[3]={};
 uint64_t aiLogAt=0,nativeWalkAt=0;uint32_t aiReason=0,nativeAiTurns=0,castFrame=0;bool factoryDue=false,castFrameValid=false;
 uint64_t progressAt=0,moveAt=0,nextMove=0,nextState=0,nextTarget=0,targetHold=0,targetLostAt=0,walkCommit=0,failedCast[3]={},nextAura=0,nextAuraRetry=0,nextAuraLog=0;int32_t lastX=0,lastY=0;uint32_t lastMode=0,failedEffects[3]={},auraAttempts=0;bool progressSeen=false,auraSeen=false,auraHealthy=false;CombatActions::State combat;HammerState hammer;CastVisualState visual;};
Boss bosses[256];uint32_t count=0;uint64_t nextFrame=0;
struct Corpse {uint64_t epoch;uint32_t uid,kind,level;};Corpse corpses[256];uint32_t corpseCursor=0;
struct Player {uint32_t uid;int32_t x,y,level;};struct Snapshot {Player players[64];uint32_t count=0;};
bool Position(void* unit,int32_t& x,int32_t& y) noexcept {
 uintptr_t path=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+0x38,path)||!path)return false;
 using CoordFn=uint16_t(__fastcall*)(const void*);
 auto fx=reinterpret_cast<CoordFn>(RipSlot(coreBase+0x41163f)),fy=reinterpret_cast<CoordFn>(RipSlot(coreBase+0x411650));if(!fx||!fy)return false;
 x=fx(reinterpret_cast<void*>(path));y=fy(reinterpret_cast<void*>(path));return true;
}
uint32_t Distance(int32_t x,int32_t y,int32_t xx,int32_t yy) noexcept {uint32_t dx=x>xx?x-xx:xx-x,dy=y>yy?y-yy:yy-y;return dx>dy?dx:dy;}
}
#include "enemy_cast_range.h"
namespace BossSkills {
int32_t Level(void* u) noexcept {using Fn=int32_t(__fastcall*)(const void*);auto f=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"D2RCore.dll"),MAKEINTRESOURCEA(2000)));return f?f(u):-1;}
void __fastcall Capture(void* client,void* data) noexcept {
 auto& s=*static_cast<Snapshot*>(data);uintptr_t unit=0;uint32_t type=99,uid=0;int32_t x=0,y=0;
 if(s.count>=64||!Read(reinterpret_cast<uintptr_t>(client)+0x278,unit)||!unit||!Read(unit,type)||type!=0||!Read(unit+8,uid)||stat(reinterpret_cast<void*>(unit),6,0)<256||!Position(reinterpret_cast<void*>(unit),x,y))return;
 s.players[s.count++]={uid,x,y,Level(reinterpret_cast<void*>(unit))};
}
void* Target(Boss& b,void* unit,const Snapshot& s) noexcept {
 const int32_t level=Level(unit);int32_t x=0,y=0;if(level<1||!Position(unit,x,y))return nullptr;
 if(Legend(b.kind)&&LegendNative::ready&&LegendNative::chooseTarget){
  const uint64_t now=GetTickCount64();
  auto valid=[&](void* candidate)->bool {uint32_t type=99,mode=0;int32_t tx=0,ty=0;return candidate&&candidate!=unit&&Read(reinterpret_cast<uintptr_t>(candidate),type)&&(type==0||(type==1&&alignment(candidate)==2))&&Read(reinterpret_cast<uintptr_t>(candidate)+0xc,mode)&&mode!=0&&mode!=12&&stat(candidate,6,0)>=256&&Level(candidate)==level&&Position(candidate,tx,ty)&&Distance(x,y,tx,ty)<=40;};
  void* previous=b.targetId?lookup(b.game,b.targetType,b.targetId):nullptr;
  if(now<b.nextTarget&&valid(previous)&&(!b.targetLostAt||now-b.targetLostAt<=1000))return previous;
  uintptr_t monster=0,control=0;int32_t distance=0,combat=0;
  if(Read(reinterpret_cast<uintptr_t>(unit)+0x10,monster)&&monster&&Read(monster+0x30,control)&&control&&Readable(control,0x3a)){
   b.nextTarget=now+300;void* picked=LegendNative::chooseTarget(b.game,unit,reinterpret_cast<void*>(control),&distance,&combat,0);
   if(valid(picked)){
    b.targetLostAt=0;
    uint32_t type=99,id=0;Read(reinterpret_cast<uintptr_t>(picked),type);Read(reinterpret_cast<uintptr_t>(picked)+8,id);
    if(picked!=previous&&valid(previous)&&now<b.targetHold){int32_t px=0,py=0,tx=0,ty=0;Position(previous,px,py);Position(picked,tx,ty);if(Distance(x,y,px,py)<=Distance(x,y,tx,ty)+2)return previous;}
    if(type!=b.targetType||id!=b.targetId){Text t;t.Add("ES_NATIVE_TARGET uid=");t.Num(b.uid);t.Add(" source=engine type=");t.Num(type);t.Add(" id=");t.Num(id);t.Add(" engine_distance=");t.Num(distance<0?0:distance);t.Add(" melee=");t.Num(combat!=0);t.Add(" time_ms=");t.Num(now);Log(t.s);b.targetHold=now+1000;b.walkCommit=0;}
    return picked;
   }


   if(valid(previous)){if(!b.targetLostAt)b.targetLostAt=now;if(now-b.targetLostAt<=1000)return previous;}
   else b.targetLostAt=0;
   return nullptr;
  }
 }
 if(b.targetId){void* target=lookup(b.game,b.targetType,b.targetId);int32_t tx=0,ty=0;
  if(target&&Ally(target,b.targetType)&&stat(target,6,0)>=256&&Level(target)==level&&Position(target,tx,ty)&&Distance(x,y,tx,ty)<=40)return target;}
 uint32_t best=41,uid=0;for(uint32_t i=0;i<s.count;++i){const auto& p=s.players[i];if(p.level!=level)continue;const uint32_t d=Distance(x,y,p.x,p.y);if(d<best){best=d;uid=p.uid;}}
 return uid?lookup(b.game,0,uid):nullptr;
}



bool PoisonCorpseAvailable(void* corpse) noexcept {
 using EligibleFn=int32_t(__fastcall*)(void*);
 return LegendNative::ready&&ctx&&reinterpret_cast<EligibleFn>(ctx->exeBase+0x33a510)(corpse)!=0&&!LegendNative::state(corpse,118);
}
void* DeadTargetForSkill(Boss& b,void* unit,uint32_t skill,uint32_t* rejected=nullptr) noexcept {
 if(rejected)*rejected=0;
 int32_t x=0,y=0;if(!Position(unit,x,y))return nullptr;const int32_t level=Level(unit);void* best=nullptr;uint32_t distance=21;

 for(uint32_t n=0;n<32;++n){const auto& c=corpses[(corpseCursor-1-n)&255];if(c.epoch!=b.epoch||c.level!=static_cast<uint32_t>(level))continue;
  void* candidate=lookup(b.game,1,c.uid);uint32_t type=99,kind=0,mode=0;int32_t cx=0,cy=0;
  if(!candidate||!Read(reinterpret_cast<uintptr_t>(candidate),type)||type!=1||!Read(reinterpret_cast<uintptr_t>(candidate)+4,kind)||kind!=c.kind||!Read(reinterpret_cast<uintptr_t>(candidate)+0xc,mode)||mode!=12||stat(candidate,6,0)>0||Level(candidate)!=level||!Position(candidate,cx,cy))continue;
  const uint32_t d=Distance(x,y,cx,cy);if(d>=21)continue;
  if((skill==83||skill==231||(b.sovereign&&(skill==70||skill==80)))&&!PoisonCorpseAvailable(candidate)){if(rejected)++*rejected;continue;}
  if(d<distance){distance=d;best=candidate;}}
 return best;
}
void* DeadTarget(Boss& b,void* unit) noexcept {return DeadTargetForSkill(b,unit,0);}
void CastOne(Boss&,void*,void*,uint32_t,uint64_t) noexcept;
void NativeCast(Boss&,void*,void*,uint32_t,uint64_t) noexcept;
void NativePoll(Boss&,void*,void*,uint64_t) noexcept;
bool NativeDecision(Boss&,void*) noexcept;
void NativeCastMonitor(Boss&,void*) noexcept;
void PetPoll(void*) noexcept;
void NativeReset() noexcept;
void NativeActionWatch(Boss&,void*) noexcept;
bool NativeLevels(Boss&,void*) noexcept;
void DungeonSpeed(Boss&,void*) noexcept;
void NativeAuraPoll(Boss&,void*,uint64_t) noexcept;
void Observe(void* game,uint32_t event,void* unit,void* target,const void* damage) noexcept {
 if(!count||!unit||!target||!damage||(event!=1&&event!=2&&event!=5&&event!=6))return;
 uint32_t identity[3]={};if(!Read(reinterpret_cast<uintptr_t>(unit),identity)||identity[0]!=1||!Allowed(identity[1]))return;
 uint32_t tt=99,uid=0;if(!Read(reinterpret_cast<uintptr_t>(target),tt)||!Ally(target,tt)||!Read(reinterpret_cast<uintptr_t>(target)+8,uid))return;
 for(uint32_t i=0;i<count;++i){auto& b=bosses[i];if(b.active&&b.game==game&&b.uid==identity[2]&&b.kind==identity[1]&&b.thread==GetCurrentThreadId()){
   if(Legend(b.kind)){b.nextTarget=0;return;}
   b.targetType=tt;b.targetId=uid;
   if(CombatBoss(b.kind)){


    uint32_t hitflags=0,mode=0;
    if(b.epoch==static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0))&&Read(reinterpret_cast<uintptr_t>(damage),hitflags)&&(hitflags&0x20)&&Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)&&mode!=0&&mode!=12&&stat(unit,6,0)>=256&&stat(target,6,0)>=256&&Level(unit)==Level(target)){
     int32_t x=0,y=0,tx=0,ty=0;
     if(Position(unit,x,y)&&Position(target,tx,ty)&&Distance(x,y,tx,ty)<=40)CastOne(b,unit,target,0,GetTickCount64());
    }
   }
   return;}}
}
void CastOne(Boss& b,void* unit,void* opponent,uint32_t i,uint64_t now) noexcept {
 const auto& s=b.skills[i];const bool legend=Legend(b.kind);if(legend){NativeCast(b,unit,opponent,i,now);return;}if(!s.id||now<b.next[i]||b.busy||(legend&&now<b.nextAny))return;


 if(legend){int32_t ux=0,uy=0,ox=0,oy=0;uint32_t type=99;
  if(!opponent||!Read(reinterpret_cast<uintptr_t>(opponent),type)||!Ally(opponent,type)||stat(opponent,6,0)<256||Level(unit)!=Level(opponent)||!Position(unit,ux,uy)||!Position(opponent,ox,oy)||Distance(ux,uy,ox,oy)>LegendRange||(s.id==466&&Distance(ux,uy,ox,oy)>BladeWarpRange))return;
 }
 const bool area=Andariel(b.kind)&&s.id==92;
 void* target=opponent;


 if(!area&&(s.kind==1||s.kind==5||s.kind==7))target=unit;
 if(s.kind==2||s.kind==6)target=DeadTarget(b,unit);
 if(!target){b.next[i]=now+750;return;}
 int32_t x=0,y=0,tx=0,ty=0;if(!Position(unit,x,y)||!Position(target,tx,ty))return;
 const uint32_t distance=Distance(x,y,tx,ty);
 if(legend&&distance>LegendRange)return;
 if((s.kind==4&&distance>4)||(s.kind==3&&(distance<4||distance>30)))return;
 uint32_t flags=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+0x128,flags)||(flags&0x10000000u)||!Writable(reinterpret_cast<uintptr_t>(unit)+0x128,4))return;
 if(now/100!=castWindow){castWindow=now/100;castsInWindow=0;}if(castsInWindow>=8)return;++castsInWindow;
 uint32_t gap=s.refresh?s.refresh:s.cooldown;
 if(legend&&gap<LegendGapMs)gap=LegendGapMs;
 if(legend&&s.id==466&&gap<BladeWarpGapMs)gap=BladeWarpGapMs;


 b.next[i]=now+gap;if(legend){b.nextAny=now+cfg.legendGap;b.cursor=(i+1)%3;}b.busy=true;
 Text t;t.Add("ES_BOSS_CAST_BEGIN uid=");t.Num(b.uid);t.Add(" skill=");t.Num(s.id);t.Add(" level=");t.Num(s.level);t.Add(" kind=");t.Num(s.kind);const bool logging=cfg.logCasts||!b.logged[i]||now>=b.logAfter[i];if(logging)Log(t.s);
 *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(unit)+0x128)=flags|0x10000000u;
 const bool capture=area&&!NovaRouteTrace::bossCaptured;
 if(capture){NovaRouteTrace::bossCaptured=true;NovaRouteTrace::Inputs(b.game,unit);NovaRouteTrace::Once();}
 const bool contextLog=legend&&logging;
 const auto contextBefore=contextLog?CastContext::Capture(unit):CastContext::Snapshot{};
 const int32_t result=cast(unit,s.id,s.level,target,1);
 if(contextLog)CastContext::After(unit,target,b.uid,b.kind,s.id,s.level,s.kind,distance,now,contextBefore,result);
 if(Duriel(b.kind)&&logging){Text route;route.Add("ES_SPEED_ROUTE uid=");route.Num(b.uid);route.Add(" skill=258 level=20 mode=combat_self trigger=native_damage cooldown_ms=420000 attempted_at_ms=");route.Num(now);route.Add(" returned=");route.Num(result);Log(route.s,!result);}
 if(area&&logging){Text route;route.Add("ES_NOVA_ROUTE uid=");route.Num(b.uid);uint32_t targetType=99,targetId=0;Read(reinterpret_cast<uintptr_t>(target),targetType);Read(reinterpret_cast<uintptr_t>(target)+8,targetId);route.Add(" mode=combat_opponent trigger=native_damage target_type=");route.Num(targetType);route.Add(" target_id=");route.Num(targetId);route.Add(" attempted_at_ms=");route.Num(now);route.Add(" returned=");route.Num(result);Log(route.s,!result);}
 uint32_t after=0;if(Read(reinterpret_cast<uintptr_t>(unit)+0x128,after)&&Writable(reinterpret_cast<uintptr_t>(unit)+0x128,4))*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(unit)+0x128)=after&~0x10000000u;
 if(capture)NovaRouteTrace::After(unit,result);
 b.busy=false;
 if(logging){b.logged[i]=true;b.logAfter[i]=now+30000;Text end;end.Add(result?"ES_BOSS_CAST_END uid=":"ES_BOSS_CAST_REJECT uid=");end.Num(b.uid);end.Add(" skill=");end.Num(s.id);end.Add(" returned=");end.Num(result);Log(end.s,!result);}
}
void __fastcall Tick(void* game) noexcept {
 priorTick(game);
 if(LegendNative::ready){PetPoll(game);SovereignNative::CombatPoll(game);LegendNative::ContinuousPoll(game);}
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0)),now=GetTickCount64();if(!epoch||!count||now<nextFrame)return;nextFrame=now+100;
 for(uint32_t i=0;i<count;++i){auto& b=bosses[i];if(!b.active||CombatBoss(b.kind)||b.epoch!=epoch||b.game!=game||b.thread!=GetCurrentThreadId())continue;
  void* unit=lookup(game,1,b.uid);uint32_t identity[3]={},mode=0;
  if(!unit)continue;
  if(!Read(reinterpret_cast<uintptr_t>(unit),identity)||identity[0]!=1||identity[1]!=b.kind||identity[2]!=b.uid||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||stat(unit,6,0)<256){b.active=false;continue;}
  NativeCastMonitor(b,unit);
  NativeActionWatch(b,unit);
  NativeLevels(b,unit);
  NativeAuraPoll(b,unit,now);


  void* target=b.targetId?lookup(b.game,b.targetType,b.targetId):nullptr;
  NativePoll(b,unit,target,now);
 }
}
bool InstallTick() noexcept {
 if(tickSlot)return true;
 const uint8_t tick[]={0x48,0x89,0xf1,0xff,0x15,0x73,0x88,0xee,0xff};
 const uint8_t find[]={0xba,0x04,0x00,0x00,0x00,0x41,0x89,0xd8,0xff,0x15,0xa6,0x45,0x2e,0x00};
 if(reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(L"D2RCore.dll"),MAKEINTRESOURCEA(1110)))!=coreBase+0x814ce0||!Bytes(coreBase+0x814cfc,tick,sizeof(tick))||!Bytes(coreBase+0x41bf3c,find,sizeof(find)))return false;
 lookup=reinterpret_cast<LookupFn>(RipSlot(coreBase+0x41bf44));each=reinterpret_cast<EachFn>(RipSlot(coreBase+0x825806));if(!lookup||!each)return false;
 auto* slot=reinterpret_cast<void**>(coreBase+0x6fd578);uintptr_t before=0;if(!Read(reinterpret_cast<uintptr_t>(slot),before)||!Executable(before)||!Writable(reinterpret_cast<uintptr_t>(slot),8))return false;
 priorTick=reinterpret_cast<TickFn>(before);if(InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(Tick),reinterpret_cast<void*>(before))!=reinterpret_cast<void*>(before))return false;tickSlot=slot;
 Log("ES_BOSS_TIMER_READY server_tick=1 live_native_lookup=1 casts_without_hits=1 casts_at_ai_think=1 host_tick_actions=auras_pets_diagnostics_continuous_hammer");Log("ES_LEGEND_PACING range=12 global_gap_ms=INI_legend_skill_gap_ms blade_warp_range=8 blade_warp_gap_ms=6000 round_robin=1 native_reject_consumes_turn=1 recovery_ms=INI_legend_skill_recovery_ms ordinary_sovereign_unchanged=1");return true;
}
bool Register(const R3niBossSkills::Registration* request) noexcept {
 R3niBossSkills::Registration r={};if(!request||!Readable(reinterpret_cast<uintptr_t>(request),sizeof(r)))return false;memcpy(&r,request,sizeof(r));
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));uint32_t id[3]={};uint8_t bank=0,difficulty=255;
 if(r.size!=sizeof(r)||r.abi!=R3niBossSkills::Abi||r.reserved||!epoch||r.session!=epoch||!r.game||!r.unit||!Read(reinterpret_cast<uintptr_t>(r.unit),id)||id[0]!=1||!Allowed(id[1])||!Read(reinterpret_cast<uintptr_t>(r.game)+0x106,bank)||bank<1||bank>3||!Read(reinterpret_cast<uintptr_t>(r.game)+0x104,difficulty)||difficulty>2||(!CombatBoss(id[1])&&(bank<2||difficulty!=2))||alignment(r.unit)==2)return false;
 if(Andariel(id[1])){const auto& s=r.skills[0];if(s.id!=92||s.level!=20||s.kind!=1||s.cooldown!=3000||s.refresh||r.skills[1].id||r.skills[2].id||!skill(bank,92))return false;}
 else if(Duriel(id[1])){const auto& s=r.skills[0];if(s.id!=258||s.level!=20||s.kind!=5||s.cooldown!=420000||s.refresh||r.skills[1].id||r.skills[2].id||!skill(bank,258))return false;}
 else {for(const auto& s:r.skills){const bool shared=SharedOffensive::Supported(s.id);const auto* meta=LegendNative::Meta(s.id);if((!shared&&((s.id<437||s.id>=497)&&(s.id<500||s.id>=533)))||s.level<1||s.level>99||s.kind>7||(shared&&(!meta||s.kind!=meta->kind))||s.cooldown<250||s.cooldown>60000||s.refresh>3600000||!skill(bank,s.id))return false;}}
 if((!CombatBoss(id[1])&&(!LegendNative::Bind()||!InstallTick()))||!Install())return false;
 if(Legend(id[1]))LegendNative::Diagnostics();
 for(uint32_t i=0;i<count;++i)if(bosses[i].game==r.game&&bosses[i].uid==id[2]&&bosses[i].epoch==epoch)return true;
 if(count>=256)return false;auto& b=bosses[count++];b={};b.epoch=epoch;b.game=r.game;b.uid=id[2];b.kind=id[1];b.thread=GetCurrentThreadId();b.active=true;memcpy(b.skills,r.skills,sizeof(b.skills));if(Legend(b.kind))for(auto& s:b.skills)s.level=SkillBalance::Level(2,s.id,s.level);
 if(Legend(b.kind)){for(const auto& s:b.skills){if(SharedOffensive::Supported(s.id)&&!LegendNative::PrepareShared(r.game,r.unit,s)){b.active=false;Log("ES_SHARED_OFFENSIVE_REGISTER_REJECT native_learn_failed=1 profile_unchanged=1",true);return false;}void* learned=LegendNative::find(r.unit,s.id,-1);if(!learned||!Writable(reinterpret_cast<uintptr_t>(learned)+0x40,4)){b.active=false;Log("ES_NATIVE_REGISTER_REJECT learned_skill_missing=1 compile_and_restart=1",true);return false;}*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(learned)+0x40)=s.level;}}
 if(Legend(b.kind)){const auto mask=LegendNativeData::Modes(b.kind);Text t;t.Add("ES_NATIVE_MODEL_CAPS uid=");t.Num(b.uid);t.Add(" class=");t.Num(b.kind);t.Add(" mask=");t.Num(mask);t.Add(" attack1=");t.Num((mask&16)!=0);t.Add(" attack2=");t.Num((mask&32)!=0);t.Add(" all_20_profiles=1 all_320_reserved_classes=1");Log(t.s);}
 if(Legend(b.kind))DungeonSpeed(b,r.unit);
 const uint64_t now=GetTickCount64();for(uint32_t i=0;i<3;++i)b.next[i]=CombatBoss(b.kind)?now:now+cfg.legendGap;if(Legend(b.kind))b.nextAny=now+cfg.legendGap;
 Text t;t.Add("ES_BOSS_ASSIGN uid=");t.Num(b.uid);t.Add(" class=");t.Num(b.kind);t.Add(" skills=");for(uint32_t i=0;i<3;++i){if(i)t.Add(",");t.Num(b.skills[i].id);}t.Add(Andariel(b.kind)?" fixed_one=1 role=terror_andariel trigger=native_damage cooldown_ms=3000":Duriel(b.kind)?" fixed_one=1 role=terror_duriel trigger=native_damage cooldown_ms=420000":" fixed_three=1 role=terror_legend");Log(t.s);
 if(Legend(b.kind)){Text d;d.Add("ES_NATIVE_SCHEDULE uid=");d.Num(b.uid);for(uint32_t i=0;i<3;++i){d.Add(" skill=");d.Num(b.skills[i].id);d.Add(" level=");d.Num(b.skills[i].level);d.Add(" cooldown_ms=");d.Num(b.skills[i].cooldown);d.Add(" refresh_ms=");d.Num(b.skills[i].refresh);}d.Add(" global_gap_ms=");d.Num(cfg.legendGap);d.Add(" recovery_ms=");d.Num(cfg.legendRecovery);d.Add(" factory_ai_during_cooldown=1 cooperative_ai=1");Log(d.s);}return true;
}
const R3niBossSkills::Api api={sizeof(R3niBossSkills::Api),R3niBossSkills::Abi,Register};
bool Publish() noexcept {
 if(ctx->QueryService(&communication)!=D2RL::ServiceQueryResult::Success||!communication||communication->serviceVersion!=1||communication->serviceSize<D2RL::PluginCommunicationServiceSize)return false;
 D2RL::PluginCommunication::PublishServiceRequest r={.structSize=D2RL::PluginCommunication::PublishServiceRequestSize,.name="fixed-boss-skills",.serviceVersion=1,.tableSize=sizeof(api),.table=&api};
 if(communication->publishService(ctx,&r)!=D2RL::PluginCommunication::Result::Success)return false;
 r.name="fixed-boss-native";return communication->publishService(ctx,&r)==D2RL::PluginCommunication::Result::Success;
}
void OnDeath(const D2RL::PluginContext*,const D2RL::Lifecycle::MonsterDeathEvent* e,void*) noexcept {
 if(!e||e->structSize<D2RL::Lifecycle::MonsterDeathEventSize||e->monster.type!=D2RL::Lifecycle::UnitType::Monster)return;
 SovereignNative::OnDeath(e->monster.id,e->monster.classId);
 if(Legend(e->monster.classId)){const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));for(uint32_t i=0;i<count;++i){auto& b=bosses[i];if(b.epoch==epoch&&b.uid==e->monster.id&&b.kind==e->monster.classId&&b.thread==GetCurrentThreadId())b.ownerDied=true;}}
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));if(epoch)corpses[corpseCursor++&255]={epoch,e->monster.id,e->monster.classId,static_cast<uint32_t>(e->levelId)};
}
void Reset() noexcept {SovereignNative::Reset();NativeReset();count=corpseCursor=0;nextFrame=0;memset(bosses,0,sizeof(bosses));memset(corpses,0,sizeof(corpses));}
void Remove() noexcept {LegendNative::Remove();if(tickSlot&&priorTick)InterlockedCompareExchangePointer(tickSlot,reinterpret_cast<void*>(priorTick),reinterpret_cast<void*>(Tick));tickSlot=nullptr;}
}

#include "legend_native_aura.h"
#include "legend_native_runtime.h"
#include "legend_native_actions.h"
#include "legend_native_combat.h"
#include "legend_native_summons.h"
#include "native_pet_limits.h"
#include "legend_dungeon_batch.h"
#include "summon_completion.h"

#include "legend_native_loot.h"

#include "hurricane_diagnostics.h"
