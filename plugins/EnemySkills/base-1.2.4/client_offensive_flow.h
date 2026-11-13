#pragma once


namespace LegendNative {
using ClientStartFn=int32_t(__fastcall*)(void*,int32_t,int32_t,int32_t,int32_t);
using ClientDoFn=int32_t(__fastcall*)(void*,int32_t,int32_t,int32_t,int32_t);
using ClientFrameFn=void(__fastcall*)(void*);
using ClientMissileFn=void*(__fastcall*)(void*,int32_t);
ClientStartFn priorClientStart=nullptr;ClientDoFn priorClientDo=nullptr;
ClientFrameFn priorClientFrame=nullptr;ClientMissileFn priorClientMissile=nullptr;
struct ClientFlow {uint64_t epoch=0,at=0;void* unit=nullptr;void* node=nullptr;uint32_t uid=0,kind=0,skill=0,mode=0,thread=0,serial=0,missiles=0;bool active=false,done=false,fallback=false,prepared=false,actionBound=false,deferred=false,duplicate=false;int32_t doResult=0;};


ClientFlow clientFlows[2048];uint32_t clientFlowCursor=0;
uint32_t ClientBucket(void* unit,uint32_t sid) noexcept {uint64_t h=((uintptr_t)unit>>4)^(uint64_t(sid)*0x9e3779b97f4a7c15ULL);h^=h>>33;return (uint32_t(h)&511)*4;}
bool ClientModel(const uint32_t (&id)[3]) noexcept {return id[0]==1&&(SovereignNativeData::Leader(id[1])||BossSkills::Legend(id[1]));}
struct ClientUnit {uint64_t epoch=0,next=0;void* unit=nullptr;uint32_t uid=0,kind=0,thread=0,skills[3]={};};ClientUnit clientUnits[512];
ClientUnit* ClientKnown(void* unit,const uint32_t (&id)[3],bool create=false) noexcept {
 const uint64_t epoch=(uint64_t)InterlockedCompareExchange64(&generation,0,0);const uint32_t thread=GetCurrentThreadId();const uint32_t bucket=(((uintptr_t)unit>>4)&127)*4;ClientUnit* empty=nullptr;
 for(uint32_t i=0;i<4;++i){auto& u=clientUnits[bucket+i];if(u.epoch==epoch&&u.unit==unit&&u.uid==id[2]&&u.kind==id[1]&&u.thread==thread)return &u;if(!empty&&u.epoch!=epoch)empty=&u;}
 if(!create)return nullptr;if(!empty)empty=&clientUnits[bucket];*empty={};empty->epoch=epoch;empty->unit=unit;empty->uid=id[2];empty->kind=id[1];empty->thread=thread;return empty;
}
bool ClientProjectileSkill(uint32_t id) noexcept {
 const auto* s=Pool::Get(Original(id));
 return s&&!s->summon&&!s->persistent&&SkillRoles::SpellOffense(*s);
}
ClientFlow* ClientFlowFor(void* unit,uint32_t sid) noexcept {
 if(!ready||!diagReady||!unit)return nullptr;
 uint32_t id[3]={};if(!ClientProjectileSkill(sid)||!Read((uintptr_t)unit,id)||!ClientModel(id))return nullptr;
 const auto epoch=(uint64_t)InterlockedCompareExchange64(&generation,0,0);const auto thread=GetCurrentThreadId();
 const uint32_t bucket=ClientBucket(unit,sid);
 for(uint32_t i=0;i<4;++i){auto& f=clientFlows[bucket+i];if(f.epoch==epoch&&f.thread==thread&&f.unit==unit&&f.uid==id[2]&&f.kind==id[1]&&f.skill==sid){
  using Lookup=void*(__fastcall*)(uint32_t,uint32_t);
  if(((Lookup)(ctx->exeBase+0x09a5d0))(id[2],1)==unit&&find(unit,sid,-1)==f.node)return &f;
 }}
 return nullptr;
}
void VisualClientLearn(void* unit,void* node,uint32_t sid,uint32_t mode) noexcept {
 uint32_t id[3]={};if(!Read((uintptr_t)unit,id)||!ClientModel(id))return;
 if(auto* u=ClientKnown(unit,id,true))for(auto& skill:u->skills){if(skill==sid)break;if(!skill){skill=sid;break;}}
 if(!ClientProjectileSkill(sid))return;
 ClientFlow* slot=ClientFlowFor(unit,sid);if(slot)return;
 const auto epoch=(uint64_t)InterlockedCompareExchange64(&generation,0,0);const uint32_t bucket=ClientBucket(unit,sid);uint32_t index=bucket+(clientFlowCursor++%4);
 for(uint32_t i=0;i<4;++i)if(clientFlows[bucket+i].epoch!=epoch){index=bucket+i;break;}
 auto& f=clientFlows[index];f={};f.epoch=epoch;f.unit=unit;f.node=node;f.uid=id[2];f.kind=id[1];f.skill=sid;f.mode=mode;f.thread=GetCurrentThreadId();
}



bool ClientSingleEffect(uint32_t sid) noexcept {
 switch(Original(sid)){
  case 36:case 38:case 39:case 44:case 45:case 47:case 48:case 49:case 51:case 53:case 55:case 56:case 59:case 64:case 67:case 84:case 92:case 93:case 121:case 154:case 225:case 229:case 234:case 240:case 244:case 245:case 394:case 395:case 398:case 399:case 401:case 402:return true;
  default:return false;
 }
}
void ClientPrepareAction(void* unit,uint32_t sid,uintptr_t caller) noexcept {
 if(!ctx||(caller!=ctx->exeBase+0x1a569c&&caller!=ctx->exeBase+0x1a5789))return;
 auto* f=ClientFlowFor(unit,sid);if(!f||!ClientSingleEffect(sid))return;
 f->at=GetTickCount64();++f->serial;f->active=false;f->done=false;f->fallback=false;f->missiles=0;f->prepared=true;f->actionBound=true;f->deferred=false;f->duplicate=false;f->doResult=0;
}



void MeteorClock(void* missile,uint32_t sid,const char* side) noexcept {
 if(Original(sid)!=56)return;uint32_t id[3]={};uintptr_t data=0;int16_t total=0;float remaining=0;
 if(!missile||!Read((uintptr_t)missile,id)||id[0]!=3||id[1]!=101||!Read((uintptr_t)missile+0x10,data)||!data||!Read(data+0x10,total)||!Read(data+0x14,remaining)||total!=0||remaining!=0||!Writable(data+0xa,0x12))return;
 using Set=void(__fastcall*)(void*,int32_t);
 ((Set)(ctx->exeBase+0x3bdbc0))(missile,60);
 ((Set)(ctx->exeBase+0x3bd450))(missile,60);
 ((Set)(ctx->exeBase+0x3bd200))(missile,60);
 Text t;t.Add("ES_METEOR_CLOCK side=");t.Add(side);t.Add(" missile_uid=");t.Num(id[2]);t.Add(" missile_class=101 old_frames=0 frames=60 per_unit_only=1 native_damage_values_unchanged=1");Log(t.s);
}


void ProjectileState(void* missile,uint32_t owner,uint32_t context,uint32_t paramsSkill,const char* side) noexcept {
 return;
}
int32_t __fastcall ClientStart(void* unit,int32_t sid,int32_t owner,int32_t level,int32_t item) noexcept {
 auto* f=ClientFlowFor(unit,(uint32_t)sid);
 if(f){
  if(f->prepared&&f->actionBound){f->prepared=false;f->active=true;}
  else {f->at=GetTickCount64();++f->serial;f->active=true;f->done=false;f->fallback=false;f->missiles=0;f->actionBound=false;f->deferred=false;f->duplicate=false;f->doResult=0;}
 }
 const int32_t result=priorClientStart(unit,sid,owner,level,item);
 if(f){f->active=result!=0;Text t;t.Add("ES_CLIENT_OFFENSIVE_START uid=");t.Num(f->uid);t.Add(" skill=");t.Num(f->skill);t.Add(" serial=");t.Num(f->serial);t.Add(" returned=");t.Num((uint32_t)result);uintptr_t path=0,target=0;uint32_t type=99,uid=0;if(Read((uintptr_t)unit+0x38,path)&&path){Read(path+0x70,target);Read(path+0x78,type);Read(path+0x7c,uid);}t.Add(" target_type=");t.Num(type);t.Add(" target_id=");t.Num(uid);t.Add(" target_bound=");t.Num(target!=0);t.Add(" native_client_start_once=1");Log(t.s);}
 return result;
}
int32_t __fastcall ClientDo(void* unit,int32_t sid,int32_t level,int32_t extra,int32_t item) noexcept {
 const uintptr_t caller=(uintptr_t)__builtin_return_address(0);const uintptr_t source=ctx&&caller>=ctx->exeBase?caller-ctx->exeBase:0;
 auto* f=ClientFlowFor(unit,(uint32_t)sid);
 if(f&&f->actionBound&&ClientSingleEffect(sid)){
  if(f->prepared&&BossSkills::UsedId(unit)==(uint32_t)sid){if(!f->deferred){f->deferred=true;Text t;t.Add("ES_CLIENT_EFFECT_DEFERRED uid=");t.Num(f->uid);t.Add(" skill=");t.Num(sid);t.Add(" serial=");t.Num(f->serial);t.Add(" source_rva=");t.Num(source);t.Add(" awaiting_native_target_binding=1 no_client_missile_created=1 server_unchanged=1");Log(t.s);}return 1;}
  uint32_t currentMode=0;const bool sameAction=BossSkills::UsedId(unit)==(uint32_t)sid&&Read((uintptr_t)unit+0xc,currentMode)&&currentMode==f->mode;
  if(f->active&&f->done&&sameAction){if(!f->duplicate){f->duplicate=true;Text t;t.Add("ES_CLIENT_EFFECT_DUPLICATE uid=");t.Num(f->uid);t.Add(" skill=");t.Num(sid);t.Add(" serial=");t.Num(f->serial);t.Add(" source_rva=");t.Num(source);t.Add(" suppressed=1 same_native_action=1 server_unchanged=1");Log(t.s);}return f->doResult;}
 }
 if(f)f->done=true;
 const int32_t result=priorClientDo(unit,sid,level,extra,item);
 if(f){f->doResult=result;Text t;t.Add("ES_CLIENT_OFFENSIVE_DO uid=");t.Num(f->uid);t.Add(" skill=");t.Num(f->skill);t.Add(" serial=");t.Num(f->serial);t.Add(" fallback=");t.Num(f->fallback);t.Add(" returned=");t.Num((uint32_t)result);t.Add(" source_rva=");t.Num(source);t.Add(" native_client_do_forwarded=1 rendered_frame_unconfirmed=1");Log(t.s);}
 return result;
}
void __fastcall ClientFrame(void* unit) noexcept {
 uint32_t id[3]={};
 if(!ready||!diagReady||!Read((uintptr_t)unit,id)||!ClientModel(id)){priorClientFrame(unit);return;}
 auto* known=ClientKnown(unit,id);if(!known){priorClientFrame(unit);return;}
 const uint32_t sid=BossSkills::UsedId(unit);auto* f=ClientFlowFor(unit,sid);
 uint32_t event=0,mode=0;
 if(f&&f->active&&!f->done){using Event=uint32_t(__fastcall*)(void*);event=((Event)(ctx->exeBase+0x349e30))(unit);Read((uintptr_t)unit+0xc,mode);}
 const uint64_t now=GetTickCount64();
 if(now>=known->next){
  uint32_t observedMode=0;Read((uintptr_t)unit+0xc,observedMode);
  if(observedMode!=0&&observedMode!=1&&observedMode!=2&&observedMode!=12&&observedMode!=15){known->next=now+3000;using Event=uint32_t(__fastcall*)(void*);const uint32_t observedEvent=((Event)(ctx->exeBase+0x349e30))(unit);Text t;t.Add("ES_CLIENT_ACTION_STATE uid=");t.Num(id[2]);t.Add(" used=");t.Num(sid);t.Add(" mode=");t.Num(observedMode);t.Add(" event=");t.Num(observedEvent);t.Add(" assigned1=");t.Num(known->skills[0]);t.Add(" assigned2=");t.Num(known->skills[1]);t.Add(" assigned3=");t.Num(known->skills[2]);t.Add(" start_seen=");t.Num(f&&f->active);t.Add(" do_seen=");t.Num(f&&f->done);t.Add(" observation_only=1");Log(t.s);}
 }
 priorClientFrame(unit);


 if(f&&f->active&&!f->done&&!f->fallback&&mode==f->mode&&event>=1&&event<=4&&GetTickCount64()-f->at<=6000&&BossSkills::UsedId(unit)==sid){
  f->fallback=true;using Complete=void(__fastcall*)(void*);((Complete)(ctx->exeBase+0x216c60))(unit);
 }
}
using SpellVisualFn=void*(__cdecl*)(void*,int32_t,ClientMissileFn) noexcept;
void* ForwardClientMissile(void* params,int32_t extra) noexcept {

 const DWORD before=GetLastError();void* player=nullptr;uint32_t playerType=99;
 const bool eligibleOwner=Read((uintptr_t)params+8,player)&&Read((uintptr_t)player,playerType)&&playerType==0;
 SetLastError(before);if(!eligibleOwner)return priorClientMissile(params,extra);
 HMODULE h=GetModuleHandleW(L"R3niSpellProcs.dll");
 static HMODULE known=nullptr;static SpellVisualFn bridge=nullptr;
 if(h!=known){known=h;bridge=h?reinterpret_cast<SpellVisualFn>(GetProcAddress(h,"R3niSpellProcsClientVisualV1")):nullptr;}
 if(MiniProbe::Available()){
  const DWORD saved=GetLastError();void* owner=nullptr;uint32_t type=99,sid=0,mid=0,level=0,flags=0;Read((uintptr_t)params+8,owner);Read((uintptr_t)owner,type);
  Read((uintptr_t)params+60,sid);Read((uintptr_t)params+32,mid);
  if(type==0&&sid==84&&mid==192){Read((uintptr_t)params+64,level);Read((uintptr_t)params,flags);Text t;t.Add("ES_CLIENT owner_type=");t.Num(type);t.Add(" skill=");t.Num(sid);t.Add(" missile=");t.Num(mid);t.Add(" level=");t.Num(level);t.Add(" flags=");t.Num(flags);t.Add(" module=");t.Num(h!=nullptr);t.Add(" bridge=");t.Num(bridge!=nullptr);MiniProbe::Line(t.s);}
  SetLastError(saved);
 }
 return bridge?bridge(params,extra,priorClientMissile):priorClientMissile(params,extra);
}
void* __fastcall ClientMissile(void* params,int32_t extra) noexcept {
 void* owner=nullptr;uint32_t sid=0;Read((uintptr_t)params+8,owner);Read((uintptr_t)params+60,sid);
 const uint32_t paramsSkill=sid;if(!sid&&owner)sid=BossSkills::UsedId(owner);auto* f=ClientFlowFor(owner,sid);
 void* missile=ForwardClientMissile(params,extra);ArmageddonProbe::Missile(params,missile,1);
 if(f&&missile){MeteorClock(missile,sid,"client");uint32_t id[3]={};Read((uintptr_t)missile,id);++f->missiles;
  if(f->missiles<=4||id[1]==100){Text t;t.Add("ES_CLIENT_OFFENSIVE_MISSILE uid=");t.Num(f->uid);t.Add(" skill=");t.Num(sid);t.Add(" missile_class=");t.Num(id[1]);t.Add(" serial=");t.Num(f->serial);t.Add(" native_client_creation=1 rendered_frame_unconfirmed=1");Log(t.s);ProjectileState(missile,f->uid,sid,paramsSkill,"client");}
 }
 return missile;
}
bool InstallPlayerVisual() noexcept {
 if(playerVisualHook)return true;if(!ctx||!ctx->exeBase)return false;
 const uint8_t factory[]={0x89,0x54,0x24,0x10,0x55,0x57,0x41,0x55,0x41,0x57};
 if(!Check(0x1b7760,factory,sizeof(factory)))return false;
 if(!initialized){const auto result=MH_Initialize();if(result!=MH_OK&&result!=MH_ERROR_ALREADY_INITIALIZED)return false;initialized=true;}
 auto* target=reinterpret_cast<void*>(ctx->exeBase+0x1b7760);
 if(MH_CreateHook(target,reinterpret_cast<void*>(ClientMissile),reinterpret_cast<void**>(&priorClientMissile))!=MH_OK||!priorClientMissile)return false;
 if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);priorClientMissile=nullptr;return false;}
 playerVisualHook=true;MiniProbe::Line("PLAYER_CLIENT_HOOK_READY=1 startup=1 boss_registration_required=0");return true;
}
void RemovePlayerVisual() noexcept {
 if(!playerVisualHook)return;auto* target=reinterpret_cast<void*>(ctx->exeBase+0x1b7760);MH_DisableHook(target);MH_RemoveHook(target);playerVisualHook=false;priorClientMissile=nullptr;
}
bool ClientFlowContract() noexcept {
 const uint8_t start[]={0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
 const uint8_t effect[]={0x44,0x89,0x4c,0x24,0x20,0x44,0x89,0x44,0x24,0x18,0x53,0x55};
 const uint8_t frame[]={0x48,0x85,0xc9,0x0f,0x84,0xca,0x02,0,0,0x53,0x48,0x83,0xec,0x40};
 const uint8_t factory[]={0x89,0x54,0x24,0x10,0x55,0x57,0x41,0x55,0x41,0x57};
 const uint8_t factoryTail[]={0x57,0x41,0x55,0x41,0x57};
 const bool factoryReady=playerVisualHook?(priorClientMissile&&Check(0x1b7765,factoryTail,sizeof(factoryTail))):Check(0x1b7760,factory,sizeof(factory));
 const uint8_t total[]={0x66,0x89,0x43,0x10};const uint8_t remaining[]={0xf3,0x0f,0x11,0x43,0x14};const uint8_t activate[]={0x66,0x89,0x43,0x0a};
 return Check(0x218030,start,sizeof(start))&&Check(0x216d20,effect,sizeof(effect))&&Check(0x1a7a80,frame,sizeof(frame))&&factoryReady&&Check(0x3bdc60,total,sizeof(total))&&Check(0x3bd4e9,remaining,sizeof(remaining))&&Check(0x3bd2a0,activate,sizeof(activate));
}
}
