#pragma once




namespace LegendNative {
using VisualSyncFn=void(__fastcall*)(void*,void*,void*,int32_t);
using VisualSendFn=void(__fastcall*)(void*,const void*);
VisualSyncFn priorVisualSync=nullptr;VisualSendFn priorVisualSend=nullptr;
using VisualInitialFn=void(__fastcall*)(void*,void*,void*);
using VisualPacket21Fn=void(__fastcall*)(const void*);
VisualInitialFn priorVisualInitial=nullptr;VisualPacket21Fn priorVisualPacket21=nullptr;
struct Visual {uint64_t epoch=0;void* game=nullptr;uint32_t uid=0,kind=0,owner=0,skill=0,thread=0,packets=0;bool trace=false;};
Visual visualRecords[2048];volatile LONG visualCursor=0;
uint32_t VisualBucket(void* game,uint32_t uid,uint32_t kind) noexcept {uint64_t h=((uintptr_t)game>>4)^(uint64_t(uid)*0x9e3779b97f4a7c15ULL)^(uint64_t(kind)<<23);h^=h>>33;return (uint32_t(h)&511)*4;}


struct VisualBudget {uint64_t epoch=0,next=0;void* game=nullptr;uint32_t owner=0,skill=0,thread=0,samples=0,suppressed=0;};VisualBudget visualBudgets[1024];
bool VisualTrace(void* game,uint32_t owner,uint32_t skill,uint32_t& suppressed) noexcept {
 return false;
}
struct VisualScope {volatile LONG thread=0;Visual* current=nullptr;void* client=nullptr;};VisualScope visualScopes[16];
bool VisualContract() noexcept {
 const uint8_t gate[]={0x80,0xbb,0xb4,0x01,0,0,0};
 const uint8_t native[]={0xc6,0x80,0xb4,0x01,0,0,1};
 const uint8_t sync[]={0x40,0x55,0x53,0x57,0x41,0x56,0x41,0x57};
 const uint8_t send[]={0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x81,0xec,0xd0,0x01,0,0};
 const uint8_t initial[]={0x40,0x55,0x56,0x57,0x41,0x55,0x41,0x56,0x41,0x57};
 const uint8_t client21[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x0f,0xb7,0x79,0x07};
 const uint8_t clientLookup[]={0xe8,0x03,0xec,0xf6,0xff};
 const uint8_t clientLearn[]={0xe8,0xb6,0x35,0x21,0x00};
 const uint8_t update[]={0x48,0x85,0xd2,0x0f,0x84,0xd6,0,0,0,0x53,0x55,0x56,0x57};
 const uint8_t caller[]={0xe8,0x29,0x68,0xf4,0xff};
 return Check(0x537ca3,gate,sizeof(gate))&&Check(0x57d5a1,native,sizeof(native))&&Check(0x537bc0,sync,sizeof(sync))&&Check(0x4784d0,send,sizeof(send))&&Check(0x538690,initial,sizeof(initial))&&Check(0x12b9b0,client21,sizeof(client21))&&Check(0x12b9c8,clientLookup,sizeof(clientLookup))&&Check(0x12b9f5,clientLearn,sizeof(clientLearn))&&Check(0x47f330,update,sizeof(update))&&Check(0x538b02,caller,sizeof(caller));
}



void VisualSkillNodes(void* client,void* game,void* unit) noexcept {
 uint32_t ident[3]={};if(!ready||!diagReady||!client||!unit||!Read((uintptr_t)unit,ident)||ident[0]!=1)return;
 auto* a=SovereignNative::ActorFor(game,unit);auto* b=BossSkills::NativeBoss(game,ident[2],ident[1]);
 if(!a&&(!b||!BossSkills::NativeRoot(*b,unit)))return;
 using UpdateFn=void(__fastcall*)(void*,void*,uint16_t,uint8_t,uint8_t);
 auto update=(UpdateFn)(ctx->exeBase+0x47f330);
 auto send=[&](uint32_t sid,uint32_t level){const auto* s=Pool::Get(Original(sid));if(!s||s->summon||!SkillRoles::SpellOffense(*s)||!find(unit,sid,-1)||!level||level>255)return;update(client,unit,(uint16_t)sid,(uint8_t)level,0);Text t;t.Add("ES_SKILL_CLIENT_NODE uid=");t.Num(ident[2]);t.Add(" skill=");t.Num(sid);t.Add(" level=");t.Num(level);t.Add(" native_packet21=1 after_unit_introduction=1 client_render_not_observed=1");Log(t.s);};
 if(a)for(uint32_t i=0;i<a->skillCount;++i)send(Pool::Skills[a->indices[i]].id,a->levels[i]);
 else for(const auto& s:b->skills)send(s.id,s.level);
}
void __fastcall VisualInitial(void* game,void* unit,void* client) noexcept {priorVisualInitial(game,unit,client);VisualSkillNodes(client,game,unit);}




void __fastcall VisualPacket21(const void* packet) noexcept {
 priorVisualPacket21(packet);
 uint8_t header=0,type=99;uint32_t uid=0;uint16_t sid=0;
 const uintptr_t at=(uintptr_t)packet;
 if(!ready||!diagReady||!Read(at,header)||header!=0x21||!Read(at+1,type)||type!=1||!Read(at+3,uid)||!Read(at+7,sid))return;
 using ClientUnitFn=void*(__fastcall*)(uint32_t,uint32_t);
 void* unit=((ClientUnitFn)(ctx->exeBase+0x09a5d0))(uid,type);uint32_t ident[3]={},mode=0,old=0;uintptr_t row=0;int16_t actual=-1;
 if(!unit||!Read((uintptr_t)unit,ident)||ident[0]!=1||ident[2]!=uid)return;
 const auto* s=Pool::Get(Original(sid));if(!s||s->summon||!SkillRoles::SpellOffense(*s))return;
 if(const auto* model=SovereignNativeData::Get(ident[1]))mode=(sid==107||sid==132)?14:sid==26?4:model->mode;
 else if(BossSkills::Legend(ident[1])){if(SharedOffensive::Supported(sid))mode=SharedMode(ident[1]);else if(const auto* meta=Meta(sid))mode=meta->mode;}
 if(!mode)return;
 void* node=find(unit,sid,-1);
 if(!node||!Read((uintptr_t)node,row)||!row||!Read(row,actual)||actual!=sid||!Read((uintptr_t)node+0x10,old)||!Writable((uintptr_t)node+0x10,4))return;
 *(uint32_t*)((uintptr_t)node+0x10)=mode;
 VisualClientLearn(unit,node,sid,mode);
 Text t;t.Add("ES_SKILL_CLIENT_MODE uid=");t.Num(ident[2]);t.Add(" skill=");t.Num(sid);t.Add(" previous=");t.Num(old);t.Add(" model_mode=");t.Num(mode);t.Add(" actual_client_packet21=1 client_node_only=1 shared_rows_unchanged=1");Log(t.s);
}
Visual* VisualFor(void* game,void* missile) noexcept {
 uint32_t id[3]={};uintptr_t actualGame=0;
 if(!missile||!Read((uintptr_t)missile,id)||id[0]!=3||!Read((uintptr_t)missile+0xd8,actualGame)||actualGame!=(uintptr_t)game)return nullptr;
 const uint64_t epoch=(uint64_t)InterlockedCompareExchange64(&generation,0,0);const uint32_t thread=GetCurrentThreadId();
 const uint32_t bucket=VisualBucket(game,id[2],id[1]);
 for(uint32_t i=0;i<4;++i){auto& v=visualRecords[bucket+i];if(v.epoch==epoch&&v.game==game&&v.uid==id[2]&&v.kind==id[1]&&v.thread==thread)return &v;}return nullptr;
}
void VisualMissile(void* game,void* owner,void* params,void* missile) noexcept {
 if(!ready||!diagReady||!owner||!missile)return;
 uint32_t sid=0,ident[3]={},m[3]={},flags=0;uintptr_t actualGame=0;
 if(!Read((uintptr_t)owner,ident)||ident[0]!=1||!Read((uintptr_t)params+60,sid))return;
 auto* a=SovereignNative::ActorFor(game,owner);auto* b=BossSkills::NativeBoss(game,ident[2],ident[1]);
 if(!a&&(!b||!BossSkills::NativeRoot(*b,owner)))return;
 if(!sid)sid=BossSkills::UsedId(owner);
 bool assigned=false;
 if(a){for(uint32_t i=0;i<a->skillCount;++i)assigned|=Pool::Skills[a->indices[i]].id==sid;}
 else for(const auto& s:b->skills)assigned|=s.id==sid;
 const auto* s=Pool::Get(Original(sid));
 if(!assigned||!s||s->summon||!SkillRoles::SpellOffense(*s))return;
 if(!Read((uintptr_t)missile,m)||m[0]!=3||!Read((uintptr_t)missile+0xd8,actualGame)||actualGame!=(uintptr_t)game||!Read((uintptr_t)missile+0x124,flags)||(flags&0x400000)||!Writable((uintptr_t)missile+0x1b4,1))return;
 MeteorClock(missile,sid,"server");
 const uint64_t epoch=(uint64_t)InterlockedCompareExchange64(&generation,0,0);const uint32_t bucket=VisualBucket(game,m[2],m[1]);uint32_t index=bucket+((uint32_t)InterlockedIncrement(&visualCursor)%4);
 for(uint32_t i=0;i<4;++i)if(visualRecords[bucket+i].epoch!=epoch){index=bucket+i;break;}
 auto& v=visualRecords[index];v={};v.epoch=epoch;v.game=game;v.uid=m[2];v.kind=m[1];v.owner=ident[2];v.skill=sid;v.thread=GetCurrentThreadId();



 const bool force=Original(sid)==112&&cfg.continuousHammer;
 if(force){*(uint8_t*)((uintptr_t)missile+0x1b4)=1;if(Writable((uintptr_t)missile+0x124,4))*(uint32_t*)((uintptr_t)missile+0x124)=flags|1u;}
 uint32_t suppressed=0;v.trace=VisualTrace(game,v.owner,sid,suppressed);
 if(v.trace){Text t;t.Add(force?"ES_SKILL_VISUAL_ARMED owner=":"ES_SKILL_VISUAL_TRACKED owner=");t.Num(v.owner);t.Add(" skill=");t.Num(sid);t.Add(" missile_uid=");t.Num(v.uid);t.Add(" per_missile_override=");t.Num(force);t.Add(" suppressed_total=");t.Num(suppressed);t.Add(" server_missiles_added=0 damage_unchanged=1");Log(t.s);uint32_t paramsSkill=0;Read((uintptr_t)params+60,paramsSkill);ProjectileState(missile,v.owner,sid,paramsSkill,"server");}
}
void __fastcall VisualSync(void* client,void* game,void* missile,int32_t velocity) noexcept {
 Visual* v=ready?VisualFor(game,missile):nullptr;VisualScope* scope=nullptr;Visual* saved=nullptr;void* savedClient=nullptr;const LONG thread=(LONG)GetCurrentThreadId();
 if(v){
  for(auto& s:visualScopes)if(InterlockedCompareExchange(&s.thread,0,0)==thread){scope=&s;break;}
  if(!scope)for(auto& s:visualScopes)if(InterlockedCompareExchange(&s.thread,thread,0)==0){scope=&s;break;}
  if(scope){saved=scope->current;savedClient=scope->client;scope->current=v;scope->client=client;}
 }
 priorVisualSync(client,game,missile,velocity);
 if(scope){scope->current=saved;scope->client=savedClient;if(!saved)InterlockedExchange(&scope->thread,0);}
}
void __fastcall VisualSend(void* client,const void* packet) noexcept {
 uint8_t kind=0;Visual* v=nullptr;const LONG thread=(LONG)GetCurrentThreadId();
 if(ready)for(auto& s:visualScopes)if(InterlockedCompareExchange(&s.thread,0,0)==thread&&s.client==client){v=s.current;break;}
 if(v&&(!v->trace||!Read((uintptr_t)packet,kind)||kind!=0x73))v=nullptr;
 priorVisualSend(client,packet);
 if(v&&++v->packets<=4){Text t;t.Add("ES_SKILL_VISUAL_PACKET owner=");t.Num(v->owner);t.Add(" skill=");t.Num(v->skill);t.Add(" missile_uid=");t.Num(v->uid);t.Add(" packet=115 native_sender_called=1 client_render_not_observed=1");Log(t.s);}
}
}
