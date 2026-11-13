#pragma once


namespace LegendNative {
using ClientCastModeFn=int32_t(__fastcall*)(void*,void*);
using SignalTargetFn=void(__fastcall*)(void*,int32_t,int32_t,int32_t,int32_t,uint8_t);
using SignalCoordinatesFn=void(__fastcall*)(void*,int32_t,int32_t,uint16_t,uint16_t,uint8_t);
ClientCastModeFn priorClientCastMode=nullptr;
SignalTargetFn priorSignalTarget=nullptr;SignalCoordinatesFn priorSignalCoordinates=nullptr;
struct SignalRecord {CastVisualState* state=nullptr;uint64_t cast=0;uint32_t uid=0,level=0,mode=0;};
SignalRecord SignalFor(void* game,void* unit,int32_t sid,bool selected=true) noexcept {
 if(!ready||!diagReady||!unit||sid<=0||!ClientProjectileSkill((uint32_t)sid)||(Original(sid)==112&&cfg.continuousHammer))return {};
 if(selected&&BossSkills::UsedId(unit)!=(uint32_t)sid)return {};
 if(auto* a=SovereignNative::ActorFor(game,unit)){
  if(!a->started||Pool::Skills[a->index].id!=(uint32_t)sid)return {};
  return {&a->visual,a->started,a->uid,SovereignNative::Level(*a,sid),a->mode};
 }
 uint32_t id[3]={};if(!Read((uintptr_t)unit,id)||id[0]!=1||!BossSkills::Legend(id[1]))return {};
 auto* b=BossSkills::NativeBoss(game,id[2],id[1]);if(!b||!BossSkills::NativeRoot(*b,unit)||!b->nativeStarted||b->nativeSkill!=(uint32_t)sid)return {};
 for(const auto& s:b->skills)if(s.id==(uint32_t)sid)return {&b->visual,b->nativeStarted,b->uid,s.level,b->nativeMode};
 return {};
}
void SignalEpoch(SignalRecord& r,uint32_t sid) noexcept {
 if(r.state&&(r.state->cast!=r.cast||r.state->skill!=sid)){*r.state={};r.state->cast=r.cast;r.state->skill=sid;}
}
SignalRecord SignalUnit(void* unit,int32_t sid) noexcept {
 uintptr_t game=0;if(!unit||!Read((uintptr_t)unit+0xd8,game)||!game)return {};
 return SignalFor((void*)game,unit,sid);
}
void SignalLog(const SignalRecord& r,uint32_t sid,const char* source) noexcept {
 Text t;t.Add("ES_OFFENSIVE_CAST_SIGNAL uid=");t.Num(r.uid);t.Add(" skill=");t.Num(sid);t.Add(" cast=");t.Num(r.cast);t.Add(" level=");t.Num(r.level);t.Add(" source=");t.Add(source);t.Add(" native_unit_message=1 extra_server_cast=0 extra_server_missile=0");Log(t.s);
}
void __fastcall SignalTarget(void* unit,int32_t sid,int32_t level,int32_t type,int32_t uid,uint8_t extra) noexcept {
 auto r=SignalUnit(unit,sid);if(r.state){SignalEpoch(r,sid);if(r.state->sent)return;r.state->sent=true;priorSignalTarget(unit,sid,(int32_t)r.level,type,uid,extra);SignalLog(r,sid,r.state->injecting?"bridge_target":"native_target");return;}
 priorSignalTarget(unit,sid,level,type,uid,extra);
}
void __fastcall SignalCoordinates(void* unit,int32_t sid,int32_t level,uint16_t x,uint16_t y,uint8_t extra) noexcept {
 auto r=SignalUnit(unit,sid);if(r.state){SignalEpoch(r,sid);if(r.state->sent)return;r.state->sent=true;priorSignalCoordinates(unit,sid,(int32_t)r.level,x,y,extra);SignalLog(r,sid,r.state->injecting?"bridge_coordinates":"native_coordinates");return;}
 priorSignalCoordinates(unit,sid,level,x,y,extra);
}


bool SignalPending(void* unit,uintptr_t previousTail,uint32_t sid,uint32_t level,bool& valid) noexcept {
 uintptr_t current=0;valid=previousTail?Read(previousTail,current):Read((uintptr_t)unit+0x168,current);if(!valid)return false;
 for(uint32_t i=0;current&&i<256;++i){uint8_t type=0;uint16_t id=0;uintptr_t next=0;
  if(!Read(current+8,type)||!Read(current+0x10,id)||!Read(current,next)){valid=false;return false;}
  if((type==0x99||type==0x9a)&&id==sid){uint8_t old=0;if(!Read(current+0x12,old)){valid=false;return false;}if(old!=level){if(!Writable(current+0x12,1)){valid=false;return false;}*(uint8_t*)(current+0x12)=(uint8_t)level;}return true;}if(next==current){valid=false;return false;}current=next;
 }
 if(current)valid=false;return false;
}



bool SignalNeedsTarget(uint32_t sid) noexcept {
 switch(Original(sid)){case 53:case 93:case 121:return true;default:return false;}
}
bool SignalLiveTarget(void* game,void* unit,void* target,uint32_t& type,uint32_t& uid) noexcept {
 uint32_t mode=0;uintptr_t targetGame=0;
 return target&&target!=unit&&Read((uintptr_t)target,type)&&(type==0||type==1)&&Read((uintptr_t)target+8,uid)&&BossSkills::lookup&&BossSkills::lookup(game,type,uid)==target&&Read((uintptr_t)target+0xd8,targetGame)&&targetGame==(uintptr_t)game&&Read((uintptr_t)target+0xc,mode)&&mode!=0&&mode!=12&&stat(target,6,0)>=256;
}
int32_t __fastcall UseWithVisualSignal(void* game,void* unit,uint8_t mode,int32_t sid,void* target,int32_t x,int32_t y,uint8_t flag) noexcept {
 auto before=SignalFor(game,unit,sid,false);uintptr_t tail=0;const bool tailValid=before.state&&Read((uintptr_t)unit+0x170,tail);
 const int32_t result=nativeUse(game,unit,mode,sid,target,x,y,flag);
 if(!result||!before.state||!tailValid)return result;
 auto r=SignalFor(game,unit,sid);uint32_t currentMode=0;
 if(!r.state||r.cast!=before.cast||r.state!=before.state||r.level<1||r.level>255||!Read((uintptr_t)unit+0xc,currentMode)||currentMode!=mode||x<=0||y<=0||x>65535||y>65535)return result;
 SignalEpoch(r,sid);if(r.state->sent)return result;
 bool valid=false;if(SignalPending(unit,tail,sid,r.level,valid)){r.state->sent=true;SignalLog(r,sid,"existing_native_message");return result;}if(!valid)return result;


 uint32_t targetType=99,targetId=0;
 r.state->injecting=true;
 if(SignalNeedsTarget(sid)&&SignalLiveTarget(game,unit,target,targetType,targetId))
  ((SignalTargetFn)(ctx->exeBase+0x5390d0))(unit,sid,r.level,(int32_t)targetType,(int32_t)targetId,0);
 else ((SignalCoordinatesFn)(ctx->exeBase+0x5391a0))(unit,sid,r.level,(uint16_t)x,(uint16_t)y,0);
 r.state->injecting=false;
 return result;
}
uint32_t ClientActionMode(uint32_t kind,uint32_t sid) noexcept {
 if(const auto* model=SovereignNativeData::Get(kind))return (sid==107||sid==132)?14:sid==26?4:model->mode;
 if(BossSkills::Legend(kind)){if(SharedOffensive::Supported(sid))return SharedMode(kind);if(const auto* meta=Meta(sid))return meta->mode;}
 return 0;
}
int32_t __fastcall ClientCastMode(void* unit,void* node) noexcept {
 const int32_t native=priorClientCastMode(unit,node);uint32_t id[3]={};uintptr_t row=0;int16_t sid=-1;int32_t owner=0;
 if(!ready||!diagReady||!unit||!node||!Read((uintptr_t)unit,id)||!ClientModel(id)||!Read((uintptr_t)node,row)||!row||!Read(row,sid)||sid<0||!ClientProjectileSkill(sid)||!Read((uintptr_t)node+0x4c,owner)||owner!=-1)return native;
 using Lookup=void*(__fastcall*)(uint32_t,uint32_t);
 if(((Lookup)(ctx->exeBase+0x09a5d0))(id[2],1)!=unit||find(unit,sid,-1)!=node)return native;
 const uint32_t mode=ClientActionMode(id[1],sid);if(!mode||!Writable((uintptr_t)node+0x10,4))return native;
 *(uint32_t*)((uintptr_t)node+0x10)=mode;VisualClientLearn(unit,node,sid,mode);ClientPrepareAction(unit,sid,(uintptr_t)__builtin_return_address(0));
 if((uint32_t)native!=mode){Text t;t.Add("ES_CLIENT_ACTION_MODE_RESTORE uid=");t.Num(id[2]);t.Add(" skill=");t.Num(sid);t.Add(" previous=");t.Num((uint32_t)native);t.Add(" mode=");t.Num(mode);t.Add(" native_action_selection=1 node_only=1");Log(t.s);}
 return (int32_t)mode;
}



using SignalWireFn=void(__fastcall*)(void*,uint8_t,uint32_t,uint8_t,uint32_t,uint16_t,uint16_t,uint16_t,uint16_t,uint8_t,int32_t);
SignalWireFn priorSignalWire=nullptr;
void __fastcall SignalWire(void* client,uint8_t casterType,uint32_t casterId,uint8_t targetType,uint32_t targetId,uint16_t sid,uint16_t x,uint16_t y,uint16_t extra,uint8_t level,int32_t extended) noexcept {
 if(ready&&diagReady&&client&&casterType==1&&SignalNeedsTarget(sid)){
  using GameFn=void*(__fastcall*)(void*);void* game=((GameFn)(ctx->exeBase+0x484de0))(client);
  void* unit=game&&BossSkills::lookup?BossSkills::lookup(game,1,casterId):nullptr;
  auto* a=SovereignNative::ActorFor(game,unit);bool assigned=false;
  if(a)for(uint32_t i=0;i<a->skillCount;++i)assigned|=Pool::Skills[a->indices[i]].id==sid;
  void* target=assigned?BossSkills::lookup(game,targetType,targetId):nullptr;uint32_t type=99,uid=0;
  if(assigned&&level&&SignalLiveTarget(game,unit,target,type,uid)&&type==targetType&&uid==targetId){
   uint8_t packet[20]={0x4c,1};

   *(uint32_t*)(packet+2)=casterId;*(uint16_t*)(packet+6)=sid;packet[8]=level;packet[9]=targetType;*(uint32_t*)(packet+10)=targetId;
   *(uint16_t*)(packet+14)=x;*(uint16_t*)(packet+16)=y;*(uint16_t*)(packet+18)=extra;
   using SendFn=void(__fastcall*)(void*,const void*,uint32_t);((SendFn)(ctx->exeBase+0x4817f0))(client,packet,sizeof(packet));
   Text t;t.Add("ES_OFFENSIVE_TARGET_WIRE uid=");t.Num(casterId);t.Add(" skill=");t.Num(sid);t.Add(" target_type=");t.Num(targetType);t.Add(" target_id=");t.Num(targetId);t.Add(" packet=76 target_preserved=1 extra_server_cast=0 damage_unchanged=1");Log(t.s);return;
  }
 }
 priorSignalWire(client,casterType,casterId,targetType,targetId,sid,x,y,extra,level,extended);
}
bool CastSignalContract() noexcept {
 const uint8_t queue[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
 const uint8_t target[]={0xc6,0x45,0x08,0x99,0x66,0x89,0x5d,0x10};const uint8_t coords[]={0xc6,0x45,0x08,0x9a,0x66,0x89,0x5d,0x10};
 const uint8_t getter[]={0x48,0x85,0xc9,0x74,0x09,0x48,0x85,0xd2,0x74,0x04,0x8b,0x42,0x10,0xc3};
 const uint8_t wire[]={0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
 const uint8_t fixed[]={0xb8,0x4c,0,0,0,0x44,0x88,0x75,0xc1,0xb9,0x99,0,0,0};
 return Check(0x47f7a0,wire,sizeof(wire))&&Check(0x47f988,fixed,sizeof(fixed))&&Check(0x5390d0,queue,sizeof(queue))&&Check(0x5391a0,queue,sizeof(queue))&&Check(0x53913a,target,sizeof(target))&&Check(0x53920d,coords,sizeof(coords))&&Check(0x33f770,getter,sizeof(getter));
}
}
