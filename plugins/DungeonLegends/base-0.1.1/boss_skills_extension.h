#pragma once


#include "boss_skills_api.h"
#include <D2RLPlugin/plugin_communication.h>
namespace BossSkills {
using namespace ES;
using LookupFn=void*(__fastcall*)(void*,uint32_t,uint32_t);
using TickFn=void(__fastcall*)(void*);
using EachFn=void(__fastcall*)(void*,void(__fastcall*)(void*,void*),void*,uint32_t);
LookupFn lookup=nullptr;TickFn priorTick=nullptr;void** tickSlot=nullptr;EachFn each=nullptr;
const D2RL::PluginCommunicationService* communication=nullptr;
struct Boss {uint64_t epoch=0;void* game=nullptr;uint32_t uid=0,kind=0,thread=0,targetType=0,targetId=0;R3niBossSkills::Skill skills[3];uint64_t next[3]={};bool active=false,busy=false,logged[3]={};uint64_t logAfter[3]={};};
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
int32_t Level(void* u) noexcept {using Fn=int32_t(__fastcall*)(const void*);auto f=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"D2RCore.dll"),MAKEINTRESOURCEA(2000)));return f?f(u):-1;}
void __fastcall Capture(void* client,void* data) noexcept {
 auto& s=*static_cast<Snapshot*>(data);uintptr_t unit=0;uint32_t type=99,uid=0;int32_t x=0,y=0;
 if(s.count>=64||!Read(reinterpret_cast<uintptr_t>(client)+0x278,unit)||!unit||!Read(unit,type)||type!=0||!Read(unit+8,uid)||stat(reinterpret_cast<void*>(unit),6,0)<256||!Position(reinterpret_cast<void*>(unit),x,y))return;
 s.players[s.count++]={uid,x,y,Level(reinterpret_cast<void*>(unit))};
}
void* Target(Boss& b,void* unit,const Snapshot& s) noexcept {
 const int32_t level=Level(unit);int32_t x=0,y=0;if(level<1||!Position(unit,x,y))return nullptr;
 if(b.targetId){void* target=lookup(b.game,b.targetType,b.targetId);int32_t tx=0,ty=0;
  if(target&&Ally(target,b.targetType)&&stat(target,6,0)>=256&&Level(target)==level&&Position(target,tx,ty)&&Distance(x,y,tx,ty)<=40)return target;}
 uint32_t best=41,uid=0;for(uint32_t i=0;i<s.count;++i){const auto& p=s.players[i];if(p.level!=level)continue;const uint32_t d=Distance(x,y,p.x,p.y);if(d<best){best=d;uid=p.uid;}}
 return uid?lookup(b.game,0,uid):nullptr;
}
void* DeadTarget(Boss& b,void* unit) noexcept {
 int32_t x=0,y=0;if(!Position(unit,x,y))return nullptr;const int32_t level=Level(unit);void* best=nullptr;uint32_t distance=21;

 for(uint32_t n=0;n<32;++n){const auto& c=corpses[(corpseCursor-1-n)&255];if(c.epoch!=b.epoch||c.level!=static_cast<uint32_t>(level))continue;
  void* candidate=lookup(b.game,1,c.uid);uint32_t type=99,kind=0,mode=0;int32_t cx=0,cy=0;
  if(!candidate||!Read(reinterpret_cast<uintptr_t>(candidate),type)||type!=1||!Read(reinterpret_cast<uintptr_t>(candidate)+4,kind)||kind!=c.kind||!Read(reinterpret_cast<uintptr_t>(candidate)+0xc,mode)||mode!=12||stat(candidate,6,0)>0||Level(candidate)!=level||!Position(candidate,cx,cy))continue;
  const uint32_t d=Distance(x,y,cx,cy);if(d<distance){distance=d;best=candidate;}}
 return best;
}
void Observe(void* game,uint32_t event,void* unit,void* target,const void* damage) noexcept {
 if(!count||!unit||!target||!damage||(event!=1&&event!=2&&event!=5&&event!=6))return;
 uint32_t identity[3]={};if(!Read(reinterpret_cast<uintptr_t>(unit),identity)||identity[0]!=1||identity[1]<1779||identity[1]>=2099)return;
 uint32_t tt=99,uid=0;if(!Read(reinterpret_cast<uintptr_t>(target),tt)||!Ally(target,tt)||!Read(reinterpret_cast<uintptr_t>(target)+8,uid))return;
 for(uint32_t i=0;i<count;++i){auto& b=bosses[i];if(b.active&&b.game==game&&b.uid==identity[2]&&b.kind==identity[1]&&b.thread==GetCurrentThreadId()){b.targetType=tt;b.targetId=uid;return;}}
}
void CastOne(Boss& b,void* unit,void* opponent,uint32_t i,uint64_t now) noexcept {
 const auto& s=b.skills[i];if(now<b.next[i]||b.busy)return;
 void* target=opponent;
 if(s.kind==1||s.kind==5||s.kind==7)target=unit;
 if(s.kind==2||s.kind==6)target=DeadTarget(b,unit);
 if(!target){b.next[i]=now+750;return;}
 int32_t x=0,y=0,tx=0,ty=0;if(!Position(unit,x,y)||!Position(target,tx,ty))return;
 const uint32_t distance=Distance(x,y,tx,ty);
 if((s.kind==4&&distance>4)||(s.kind==3&&(distance<4||distance>30)))return;
 uint32_t flags=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+0x128,flags)||(flags&0x10000000u)||!Writable(reinterpret_cast<uintptr_t>(unit)+0x128,4))return;
 if(now/100!=castWindow){castWindow=now/100;castsInWindow=0;}if(castsInWindow>=8)return;++castsInWindow;
 b.next[i]=now+(s.refresh?s.refresh:s.cooldown);b.busy=true;
 Text t;t.Add("ES_BOSS_CAST_BEGIN uid=");t.Num(b.uid);t.Add(" skill=");t.Num(s.id);t.Add(" level=");t.Num(s.level);t.Add(" kind=");t.Num(s.kind);const bool logging=cfg.logCasts||!b.logged[i]||now>=b.logAfter[i];if(logging)Log(t.s);
 *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(unit)+0x128)=flags|0x10000000u;
 const int32_t result=cast(unit,s.id,s.level,target,1);
 uint32_t after=0;if(Read(reinterpret_cast<uintptr_t>(unit)+0x128,after)&&Writable(reinterpret_cast<uintptr_t>(unit)+0x128,4))*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(unit)+0x128)=after&~0x10000000u;
 b.busy=false;if(!result)b.next[i]=now+(s.refresh?s.refresh:s.cooldown);
 if(logging){b.logged[i]=true;b.logAfter[i]=now+30000;Text end;end.Add(result?"ES_BOSS_CAST_END uid=":"ES_BOSS_CAST_REJECT uid=");end.Num(b.uid);end.Add(" skill=");end.Num(s.id);end.Add(" returned=");end.Num(result);Log(end.s,!result);}
}
void __fastcall Tick(void* game) noexcept {
 priorTick(game);
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0)),now=GetTickCount64();if(!epoch||!count||now<nextFrame)return;nextFrame=now+100;
 Snapshot players;each(game,Capture,&players,0);
 for(uint32_t i=0;i<count;++i){auto& b=bosses[i];if(!b.active||b.epoch!=epoch||b.game!=game||b.thread!=GetCurrentThreadId())continue;
  void* unit=lookup(game,1,b.uid);uint32_t identity[3]={},mode=0;
  if(!unit)continue;
  if(!Read(reinterpret_cast<uintptr_t>(unit),identity)||identity[0]!=1||identity[1]!=b.kind||identity[2]!=b.uid||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||stat(unit,6,0)<256){b.active=false;continue;}
  void* target=Target(b,unit,players);if(!target)continue;
  for(uint32_t k=0;k<3;++k)CastOne(b,unit,target,k,now);
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
 Log("ES_BOSS_TIMER_READY server_tick=1 live_native_lookup=1 casts_without_hits=1");return true;
}
bool Register(const R3niBossSkills::Registration* request) noexcept {
 R3niBossSkills::Registration r={};if(!request||!Readable(reinterpret_cast<uintptr_t>(request),sizeof(r)))return false;memcpy(&r,request,sizeof(r));
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));uint32_t id[3]={};uint8_t bank=0,difficulty=255;
 if(r.size!=sizeof(r)||r.abi!=R3niBossSkills::Abi||r.reserved||!epoch||r.session!=epoch||!r.game||!r.unit||!Read(reinterpret_cast<uintptr_t>(r.unit),id)||id[0]!=1||id[1]<1779||id[1]>=2099||!Read(reinterpret_cast<uintptr_t>(r.game)+0x106,bank)||bank<2||bank>3||!Read(reinterpret_cast<uintptr_t>(r.game)+0x104,difficulty)||difficulty!=2||alignment(r.unit)==2)return false;
 for(const auto& s:r.skills)if(s.id<437||s.id>=497||s.level<1||s.level>99||s.kind>7||s.cooldown<250||s.cooldown>60000||s.refresh>3600000||!skill(bank,s.id))return false;
 if(!InstallTick()||!Install())return false;
 for(uint32_t i=0;i<count;++i)if(bosses[i].game==r.game&&bosses[i].uid==id[2]&&bosses[i].epoch==epoch)return true;
 if(count>=256)return false;auto& b=bosses[count++];b={};b.epoch=epoch;b.game=r.game;b.uid=id[2];b.kind=id[1];b.thread=GetCurrentThreadId();b.active=true;memcpy(b.skills,r.skills,sizeof(b.skills));
 const uint64_t now=GetTickCount64();for(uint32_t i=0;i<3;++i)b.next[i]=now+350+i*200;
 Text t;t.Add("ES_BOSS_ASSIGN uid=");t.Num(b.uid);t.Add(" class=");t.Num(b.kind);t.Add(" skills=");for(uint32_t i=0;i<3;++i){if(i)t.Add(",");t.Num(b.skills[i].id);}t.Add(" fixed_three=1 role=terror_legend");Log(t.s);return true;
}
const R3niBossSkills::Api api={sizeof(R3niBossSkills::Api),R3niBossSkills::Abi,Register};
bool Publish() noexcept {
 if(ctx->QueryService(&communication)!=D2RL::ServiceQueryResult::Success||!communication||communication->serviceVersion!=1||communication->serviceSize<D2RL::PluginCommunicationServiceSize)return false;
 D2RL::PluginCommunication::PublishServiceRequest r={.structSize=D2RL::PluginCommunication::PublishServiceRequestSize,.name="fixed-boss-skills",.serviceVersion=1,.tableSize=sizeof(api),.table=&api};
 return communication->publishService(ctx,&r)==D2RL::PluginCommunication::Result::Success;
}
void OnDeath(const D2RL::PluginContext*,const D2RL::Lifecycle::MonsterDeathEvent* e,void*) noexcept {
 if(!e||e->structSize<D2RL::Lifecycle::MonsterDeathEventSize||e->monster.type!=D2RL::Lifecycle::UnitType::Monster)return;
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));if(epoch)corpses[corpseCursor++&255]={epoch,e->monster.id,e->monster.classId,static_cast<uint32_t>(e->levelId)};
}
void Reset() noexcept {count=corpseCursor=0;nextFrame=0;memset(bosses,0,sizeof(bosses));memset(corpses,0,sizeof(corpses));}
void Remove() noexcept {if(tickSlot&&priorTick)InterlockedCompareExchangePointer(tickSlot,reinterpret_cast<void*>(priorTick),reinterpret_cast<void*>(Tick));tickSlot=nullptr;}
}
