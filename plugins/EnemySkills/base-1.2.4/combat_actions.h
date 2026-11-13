#pragma once


namespace LegendNative {
struct CombatRecord {CombatActions::State* state=nullptr;uint32_t kind=0,uid=0;bool spell=false,sovereign=false;};
CombatRecord CombatActor(void* game,void* unit) noexcept {
 if(auto* a=SovereignNative::ActorFor(game,unit))return {&a->combat,a->kind,a->uid,a->busy||a->started!=0,true};
 uint32_t id[3]={};if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1||!BossSkills::Legend(id[1]))return {};
 auto* b=BossSkills::NativeBoss(game,id[2],id[1]);
 if(!b||!BossSkills::NativeRoot(*b,unit))return {};
 return {&b->combat,b->kind,b->uid,b->busy||b->nativeStarted!=0,false};
}
uint16_t CombatMask(uint32_t kind) noexcept {
 const auto* model=SovereignNativeData::Get(kind);return model?model->mask:LegendNativeData::Modes(kind);
}
uint32_t BasicMode(uint16_t mask) noexcept {
 for(uint32_t mode:{4u,5u,7u,8u,9u,10u,11u})if(mask&(1u<<mode))return mode;
 return 0;
}
void BasicDamageFloor(void* unit,uint32_t uid) noexcept {


 for(uint32_t id:{22u,24u,49u,51u,53u,55u,58u})if(stat(unit,id,0)>0)return;
 uint32_t level=stat(unit,12,0);if(level<1||level>255)level=cfg.level;
 const uint32_t minimum=level>1?level/2:1,maximum=level>1?level:2;
 setStat(unit,21,static_cast<int32_t>(minimum),0);setStat(unit,22,static_cast<int32_t>(maximum),0);
 Text t;t.Add("ES_BASIC_ATTACK_DAMAGE_FLOOR uid=");t.Num(uid);t.Add(" min=");t.Num(minimum);t.Add(" max=");t.Num(maximum);t.Add(" zero_damage_actor_only=1 existing_elemental_damage_preserved=1 unit_stats_only=1");Log(t.s);
}
void* BasicNode(void* unit,uint32_t mode) noexcept {
 void* node=find?find(unit,0,-1):nullptr;
 if(!node){
  const uint8_t entry[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20,0x8b,0xea,0x48,0x8b,0xf9};
  const uint8_t fields[]={0x89,0x43,0x10,0xc7,0x43,0x40,0x01,0,0,0,0xc7,0x43,0x4c,0xff,0xff,0xff,0xff};
  if(!Check(0x33a010,entry,sizeof(entry))||!Check(0x33a182,fields,sizeof(fields)))return nullptr;
  using Learn=void*(__fastcall*)(void*,int32_t);node=reinterpret_cast<Learn>(ctx->exeBase+0x33a010)(unit,0);
 }
 uintptr_t row=0;int16_t id=-1;int32_t owner=0;
 if(!node||!Read(reinterpret_cast<uintptr_t>(node),row)||!row||!Read(row,id)||id!=0||!Read(reinterpret_cast<uintptr_t>(node)+0x4c,owner)||owner!=-1||!Writable(reinterpret_cast<uintptr_t>(node)+0x10,4))return nullptr;

 *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(node)+0x10)=mode;
 return node;
}
void CombatAiSeen(void* game,void* unit) noexcept {
 auto r=CombatActor(game,unit);if(r.state)r.state->aiFrameValid=Read(reinterpret_cast<uintptr_t>(game)+0x170,r.state->aiFrame);
}
void CombatCastTimeout(void* game,void* unit,uint32_t skill,uint32_t mode,uint32_t frame,bool valid) noexcept {
 auto r=CombatActor(game,unit);if(!r.state)return;
 r.state->staleSkill=skill;r.state->staleMode=mode;r.state->staleFrame=frame;r.state->staleValid=valid;
}
void CombatObserved(CombatRecord& r,void* game,uint32_t mode,bool basic) noexcept {
 auto& s=*r.state;s.watching=Read(reinterpret_cast<uintptr_t>(game)+0x170,s.watchFrame);s.watchMode=mode;
 if(basic){s.basicActive=true;s.basicMode=mode;s.basicFrame=s.watchFrame;s.basicFrameValid=s.watching;}
}
bool CombatActive(void* game,void* unit) noexcept {
 auto r=CombatActor(game,unit);if(!r.state||!r.state->basicActive)return false;auto& s=*r.state;uint32_t mode=0,frame=0;
 if(r.spell||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode!=s.basicMode||BossSkills::UsedId(unit)!=0||FreeMode(unit)){s.basicActive=false;return false;}
 const bool expired=s.basicFrameValid&&Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)&&static_cast<uint32_t>(frame-s.basicFrame)>=250&&static_cast<uint32_t>(frame-s.basicFrame)<0x80000000u;
 if(expired){
  s.basicActive=false;s.nextBasic=GetTickCount64()+1500;const int32_t result=move(game,unit,1,nullptr);idle(game,unit,3);
  Text t;t.Add("ES_BASIC_ATTACK_TIMEOUT uid=");t.Num(r.uid);t.Add(" role=");t.Add(r.sovereign?"sovereign":"dungeon_legend");t.Add(" mode=");t.Num(mode);t.Add(" frames=250 own_attack_only=1 native_neutral_result=");t.Num(static_cast<uint32_t>(result));Log(t.s,true);return true;
 }
 idle(game,unit,3);return true;
}
void CombatYieldForSpell(void* game,void* unit) noexcept {
 auto* a=SovereignNative::ActorFor(game,unit);if(!a)return;auto& s=a->combat;
 uint32_t mode=0,frame=0;
 if(!s.basicActive||s.issuing||!s.basicFrameValid||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode!=s.basicMode||BossSkills::UsedId(unit)!=0||!Read(reinterpret_cast<uintptr_t>(game)+0x170,frame))return;
 const uint32_t elapsed=frame-s.basicFrame;if(elapsed<8||elapsed>=0x80000000u||!SovereignNative::PriorityReady(*a,unit,GetTickCount64()))return;


 const int32_t result=move(game,unit,1,nullptr);if(!result)return;
 s.basicActive=false;s.watching=false;s.nextBasic=GetTickCount64()+1000;
 Text t;t.Add("ES_SOVEREIGN_CAST_PRIORITY uid=");t.Num(a->uid);t.Add(" yielded_basic_frames=");t.Num(elapsed);t.Add(" native_neutral=1 learned_cast_next=1 no_extra_damage=1");Log(t.s);
}
bool BasicOpponent(void* game,void* unit,void* target) noexcept {
 uint32_t type=99,uid=0,mode=0;int32_t x=0,y=0,tx=0,ty=0;
 return target&&target!=unit&&Read(reinterpret_cast<uintptr_t>(target),type)&&(type==0||(type==1&&alignment(target)==2))&&Read(reinterpret_cast<uintptr_t>(target)+8,uid)&&BossSkills::lookup(game,type,uid)==target&&Read(reinterpret_cast<uintptr_t>(target)+0xc,mode)&&mode!=0&&mode!=12&&stat(target,6,0)>=256&&BossSkills::Level(unit)==BossSkills::Level(target)&&BossSkills::Position(unit,x,y)&&BossSkills::Position(target,tx,ty)&&BossSkills::Distance(x,y,tx,ty)<=3;
}
void BasicEvent(void* game,void* unit,int32_t result) noexcept {
 auto r=CombatActor(game,unit);if(!r.state||r.spell||(!r.state->basicActive&&!r.state->issuing))return;
 auto& s=*r.state;++s.handlers;
 if(s.handlers<=3||!result){Text t;t.Add("ES_BASIC_ATTACK_HANDLER uid=");t.Num(r.uid);t.Add(" role=");t.Add(r.sovereign?"sovereign":"dungeon_legend");t.Add(" skill=0 returned=");t.Num(static_cast<uint32_t>(result));t.Add(" count=");t.Num(s.handlers);t.Add(" native_handler=1");Log(t.s,!result);}
}
bool BasicDamageTrack(void* game,void* unit) noexcept {
 auto r=CombatActor(game,unit);return r.state&&!r.spell&&(r.state->basicActive||r.state->issuing)&&BossSkills::UsedId(unit)==0;
}
void BasicDamageResult(void* game,void* unit,void* target,uint32_t before,uint32_t after,uintptr_t source) noexcept {
 auto r=CombatActor(game,unit);if(!r.state)return;auto& s=*r.state;++s.damageEvents;
 const uint64_t now=GetTickCount64();if(now<s.damageLog)return;s.damageLog=now+5000;uint32_t id[3]={};Read(reinterpret_cast<uintptr_t>(target),id);
 Text t;t.Add("ES_BASIC_ATTACK_OWNER_DAMAGE uid=");t.Num(r.uid);t.Add(" role=");t.Add(r.sovereign?"sovereign":"dungeon_legend");t.Add(" target_type=");t.Num(id[0]);t.Add(" target_id=");t.Num(id[2]);t.Add(" hp_loss_raw=");t.Num(before>after?before-after:0);t.Add(" events=");t.Num(s.damageEvents);t.Add(" native_source_rva=");t.Num(source>=ctx->exeBase?source-ctx->exeBase:0);t.Add(" own_attack_active=1 damage_unchanged=1");Log(t.s);
}
bool CombatFallback(void* game,void* unit) noexcept {
 auto r=CombatActor(game,unit);if(!r.state||r.spell||!FreeMode(unit))return false;auto& s=*r.state;
 const uint64_t now=GetTickCount64();if(s.issuing||now<s.nextBasic)return false;
 void* target=currentTarget?currentTarget(game,unit):nullptr;
 uintptr_t monster=0,control=0;int32_t distance=0,combat=0;
 if(!Read(reinterpret_cast<uintptr_t>(unit)+0x10,monster)||!monster||!Read(monster+0x30,control)||!control||!Readable(control,0x3a))return false;
 void* picked=chooseTarget(game,unit,reinterpret_cast<void*>(control),&distance,&combat,0);



 if(!combat)return false;
 if(BasicOpponent(game,unit,target)){if(picked!=target)return false;}else target=picked;
 if(!BasicOpponent(game,unit,target)||collision(unit,target,4))return false;
 const uint32_t mode=BasicMode(CombatMask(r.kind));if(!mode||!BasicNode(unit,mode))return false;
 BasicDamageFloor(unit,r.uid);
 int32_t x=0,y=0;uintptr_t path=0;if(!BossSkills::Position(target,x,y)||!Read(reinterpret_cast<uintptr_t>(unit)+0x38,path)||!path)return false;
 face(reinterpret_cast<void*>(path),x,y,1);s.issuing=true;s.nextBasic=now+1500;++s.attempts;
 const int32_t result=use(game,unit,static_cast<uint8_t>(mode),0,target,x,y,0);s.issuing=false;
 if(result)CombatObserved(r,game,mode,true);
 if(now>=s.logAt||!result){s.logAt=now+5000;uint32_t type=0,uid=0;Read(reinterpret_cast<uintptr_t>(target),type);Read(reinterpret_cast<uintptr_t>(target)+8,uid);
  Text t;t.Add("ES_BASIC_ATTACK_BEGIN uid=");t.Num(r.uid);t.Add(" class=");t.Num(r.kind);t.Add(" role=");t.Add(r.sovereign?"sovereign":"dungeon_legend");t.Add(" skill=0 mode=");t.Num(mode);t.Add(" target_type=");t.Num(type);t.Add(" target_id=");t.Num(uid);t.Add(" returned=");t.Num(static_cast<uint32_t>(result));t.Add(" attempts=");t.Num(s.attempts);t.Add(" native_damage=1 damage_not_confirmed_by_acceptance=1");Log(t.s,!result);
 }
 return result!=0;
}
int32_t CombatMode(void* game,void* request,int32_t force,bool& handled) noexcept {
 handled=false;uint32_t mode=0;void* unit=nullptr;
 if(!ready||!request||!Read(reinterpret_cast<uintptr_t>(request),mode)||(mode!=2&&mode!=15&&mode!=4&&mode!=5)||!Read(reinterpret_cast<uintptr_t>(request)+8,unit))return 0;
 auto r=CombatActor(game,unit);if(!r.state)return 0;handled=true;auto& s=*r.state;const uint16_t mask=CombatMask(r.kind);
 const bool ordinary=!r.spell&&BossSkills::UsedId(unit)==0;
 if(mask&(1u<<mode)){const int32_t result=priorMode(game,request,force);if(result&&ordinary&&(mode==4||mode==5))CombatObserved(r,game,mode,s.issuing);return result;}
 uint32_t replacement=0;
 if(mode==2||mode==15){const uint32_t other=mode==2?15:2;if(mask&(1u<<other))replacement=other;}
 else if(ordinary){replacement=BasicMode(mask);if(replacement>5){void* node=BasicNode(unit,replacement);if(node){setUsed(unit,node,-1);BasicDamageFloor(unit,r.uid);}else replacement=0;}}
 const uint64_t now=GetTickCount64();
 if(now>=s.logAt){s.logAt=now+5000;Text t;t.Add(mode==2||mode==15?"ES_NATIVE_LOCOMOTION_GUARD uid=":"ES_NATIVE_ATTACK_GUARD uid=");t.Num(r.uid);t.Add(" class=");t.Num(r.kind);t.Add(" role=");t.Add(r.sovereign?"sovereign":"dungeon_legend");t.Add(" requested_mode=");t.Num(mode);t.Add(" replacement_mode=");t.Num(replacement);t.Add(" action=");t.Add(mode==2||mode==15?"supported_native_locomotion":replacement?"supported_basic_attack":"reject_unsupported_skill_mode");t.Add(" original_request_preserved=1 player_units_untouched=1");Log(t.s);}
 if(!replacement){if(ordinary)idle(game,unit,3);return 0;}
 alignas(8) uint8_t copy[48]={};if(!Read(reinterpret_cast<uintptr_t>(request),copy))return 0;
 *reinterpret_cast<uint32_t*>(copy)=replacement;using Moving=int32_t(__fastcall*)(void*,int32_t);
 copy[0x21]=reinterpret_cast<Moving>(ctx->exeBase+0x448950)(unit,replacement)?101:100;
 const int32_t result=priorMode(game,copy,force);
 if(result&&ordinary&&(mode==4||mode==5))CombatObserved(r,game,replacement,replacement>5||s.issuing);
 if(!result&&ordinary)idle(game,unit,3);return result;
}
void CombatWatch(void* game,void* unit) noexcept {
 LeapWatch(game,unit);
 auto r=CombatActor(game,unit);if(!r.state)return;auto& s=*r.state;uint32_t mode=0,frame=0;
 if(s.staleValid&&!r.spell){
  if(!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode!=s.staleMode||BossSkills::UsedId(unit)!=s.staleSkill)s.staleValid=false;
  else if(Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)&&static_cast<uint32_t>(frame-s.staleFrame)>=250&&static_cast<uint32_t>(frame-s.staleFrame)<0x80000000u){
   s.staleValid=false;const int32_t result=move(game,unit,1,nullptr);idle(game,unit,3);
   Text t;t.Add("ES_COMBAT_STALE_CAST_RECOVERY uid=");t.Num(r.uid);t.Add(" role=");t.Add(r.sovereign?"sovereign":"dungeon_legend");t.Add(" skill=");t.Num(s.staleSkill);t.Add(" own_timed_out_cast_only=1 grace_game_frames=250 native_neutral_result=");t.Num(static_cast<uint32_t>(result));Log(t.s,true);return;
  }
 }
 if(r.spell||s.issuing||BossSkills::UsedId(unit)!=0){s.watching=false;return;}
 if(s.basicActive){if(s.basicFrameValid&&Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)&&static_cast<uint32_t>(frame-s.basicFrame)>=250&&static_cast<uint32_t>(frame-s.basicFrame)<0x80000000u)CombatActive(game,unit);return;}
 if(!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||(mode!=4&&mode!=5)||!Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)){s.watching=false;return;}
 if(!s.watching||s.watchMode!=mode){s.watching=true;s.watchMode=mode;s.watchFrame=frame;return;}
 const uint32_t elapsed=frame-s.watchFrame;if(elapsed>=0x80000000u){s.watchFrame=frame;return;}
 const bool supported=(CombatMask(r.kind)&(1u<<mode))!=0;
 if(elapsed<(supported?250u:25u)||(supported&&s.aiFrameValid&&static_cast<uint32_t>(frame-s.aiFrame)<250))return;
 s.watchFrame=frame;int32_t result=1;uint32_t after=mode;
 if(!supported){result=move(game,unit,1,nullptr);Read(reinterpret_cast<uintptr_t>(unit)+0xc,after);s.watching=after==4||after==5;}
 idle(game,unit,3);Text t;t.Add("ES_NATIVE_ACTION_WATCH uid=");t.Num(r.uid);t.Add(" role=");t.Add(r.sovereign?"sovereign":"dungeon_legend");t.Add(" mode=");t.Num(mode);t.Add(" supported=");t.Num(supported);t.Add(" elapsed_game_frames=");t.Num(elapsed);t.Add(" action=");t.Add(supported?"wake_native_ai":"recover_unsupported_attack");t.Add(" native_result=");t.Num(static_cast<uint32_t>(result));t.Add(" mode_after=");t.Num(after);t.Add(" valid_animation_preserved=");t.Num(supported);t.Add(" offensive_cast=0 path_override=0");Log(t.s,!supported||!result);
}
}
namespace SovereignNative {
uint64_t combatPollAt=0;
void CombatPoll(void* game) noexcept {
 const uint64_t now=GetTickCount64();if(now<combatPollAt)return;combatPollAt=now+500;
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));if(!epoch)return;
 for(auto& a:actors){if(a.epoch!=epoch||a.game!=game||a.thread!=GetCurrentThreadId())continue;
  void* unit=BossSkills::lookup(game,1,a.uid);if(!unit||ActorFor(game,unit)!=&a)continue;
  if(a.started){uint32_t mode=0,frame=0;Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode);
   if(mode!=a.mode){Finish(a,unit,a.interruptMode?"native_interrupt":"animation_exit");}
   else if(a.frameValid&&Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)&&static_cast<uint32_t>(frame-a.frame)>=150&&static_cast<uint32_t>(frame-a.frame)<0x80000000u){a.next=now+5000;LegendNative::CombatCastTimeout(game,unit,Pool::Skills[a.index].id,a.mode,a.frame,a.frameValid);Finish(a,unit,"game_frame_timeout");LegendNative::idle(game,unit,3);Log("ES_SOVEREIGN_TIMEOUT source=host_monitor native_ai_resume=1 forced_mode_change=0 retry_ms=5000",true);}
  }
  LegendNative::CombatWatch(game,unit);
 }
}
}
