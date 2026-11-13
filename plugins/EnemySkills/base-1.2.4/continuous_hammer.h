#pragma once
namespace LegendNative {
using HammerFn=int32_t(__fastcall*)(void*,void*,int32_t,int32_t);
struct HammerOwner {ES::HammerState* state=nullptr;uint32_t id=0,level=0,uid=0;bool sovereign=false;};
HammerOwner Hammer(void* game,void* unit) noexcept {
 if(!ready||!cfg.enabled||!cfg.continuousHammer||!cfg.damage||!cfg.chance||!unit)return {};
 if(auto* a=SovereignNative::ActorFor(game,unit)){
  for(uint32_t i=0;i<a->skillCount;++i)if(Pool::Skills[a->indices[i]].id==112)return {&a->hammer,112,a->levels[i],a->uid,true};return {};
 }
 uint32_t ident[3]={};if(!Read((uintptr_t)unit,ident)||ident[0]!=1||!BossSkills::Legend(ident[1]))return {};
 auto* b=BossSkills::NativeBoss(game,ident[2],ident[1]);if(!b||!BossSkills::NativeRoot(*b,unit))return {};
 for(const auto& s:b->skills)if(Original(s.id)==112)return {&b->hammer,s.id,s.level,b->uid,false};return {};
}
bool ContinuousCoordinates(void* game,void* unit,int32_t* x,int32_t* y) noexcept {
 const auto h=Hammer(game,unit);if(!h.state||!h.state->emitting||!x||!y||!Writable((uintptr_t)x,4)||!Writable((uintptr_t)y,4))return false;


 return BossSkills::Position(unit,*x,*y);
}
bool ContinuousMissile(void* game,void* unit,void* missile) noexcept {
 const auto h=Hammer(game,unit);if(!h.state||!h.state->emitting)return false;if(missile)++h.state->missiles;return true;
}
void HammerStep(void* game,void* unit,uint64_t now) noexcept {
 using namespace BossSkills;const auto h=Hammer(game,unit);if(!h.state||h.state->emitting||now<h.state->next)return;
 h.state->next=now+cfg.hammerInterval;


 void* target=currentTarget?currentTarget(game,unit):nullptr;uint32_t ident[3]={};Read((uintptr_t)unit,ident);
 Boss* b=NativeBoss(game,ident[2],ident[1]);
 if(!target&&b&&b->targetId)target=lookup(game,b->targetType,b->targetId);
 auto valid=[&](void*t)->bool {uint32_t type=99,mode=0;int32_t x=0,y=0,tx=0,ty=0;return t&&t!=unit&&Read((uintptr_t)t,type)&&Ally(t,type)&&Read((uintptr_t)t+0xc,mode)&&mode!=0&&mode!=12&&stat(t,6,0)>=256&&Level(t)==Level(unit)&&Position(unit,x,y)&&Position(t,tx,ty)&&Distance(x,y,tx,ty)<=25;};
 if(!valid(target)){uintptr_t data=0,control=0;int32_t d=0,combat=0;if(Read((uintptr_t)unit+0x10,data)&&data&&Read(data+0x30,control)&&control&&Readable(control,0x3a))target=chooseTarget(game,unit,(void*)control,&d,&combat,0);}
 if(!valid(target))return;
 void* learned=find(unit,h.id,-1);



 if(!learned&&h.sovereign&&SovereignNative::learn)learned=SovereignNative::learn(unit,h.id);
 uintptr_t row=0,handler=0;int16_t sid=-1,execute=-1;int32_t owner=0;uint32_t level=0;
 const bool identity=learned&&Read((uintptr_t)learned,row)&&row&&Read(row,sid)&&sid==h.id&&Read(row+0x4e,execute)&&execute==73&&Read((uintptr_t)learned+0x4c,owner)&&owner==-1;
 const bool handlerValid=Read(ctx->exeBase+0x238ea00+73*8,handler)&&handler==ctx->exeBase+0x561f80;
 const bool levelRead=learned&&Read((uintptr_t)learned+0x40,level);
 if(identity&&handlerValid&&levelRead&&level!=h.level&&h.level>=1&&h.level<=99&&Writable((uintptr_t)learned+0x40,4)){
  *(uint32_t*)((uintptr_t)learned+0x40)=h.level;
  if(now>=h.state->logAt){Text t;t.Add("ES_CONTINUOUS_HAMMER_LEVEL_RESTORE uid=");t.Num(h.uid);t.Add(" skill=");t.Num(h.id);t.Add(" previous=");t.Num(level);t.Add(" configured=");t.Num(h.level);t.Add(" owner_node_only=1 player_rows_unchanged=1");Log(t.s);}
  level=h.level;
 }
 const bool verified=identity&&handlerValid&&levelRead&&level>=1&&level<=99;
 if(!verified){if(now>=h.state->logAt){h.state->logAt=now+5000;Text t;t.Add("ES_CONTINUOUS_HAMMER_REJECT uid=");t.Num(h.uid);t.Add(" reason=native_Do73_or_learned_skill_contract learned=");t.Num(learned!=nullptr);t.Add(" row_skill=");t.Num((uint16_t)sid);t.Add(" execute=");t.Num((uint16_t)execute);t.Add(" owner_is_native=");t.Num(owner==-1);t.Add(" level_read=");t.Num(levelRead);t.Add(" node_level=");t.Num(level);t.Add(" configured_level=");t.Num(h.level);t.Add(" handler_matches=");t.Num(handlerValid);t.Add(" player_rows_unchanged=1");Log(t.s,true);}return;}
 uint32_t savedFlags=0;if(!Read((uintptr_t)unit+0x124,savedFlags)||!Writable((uintptr_t)unit+0x124,4))return;
 uint32_t* field=BalanceSkillField(game,unit);const uint32_t saved=field?*field:0;if(field)*field=h.id;
 h.state->missiles=0;h.state->emitting=true;
 const int32_t result=((HammerFn)handler)(game,unit,h.id,h.level);
 h.state->emitting=false;if(field)*field=saved;++h.state->shots;


 uint32_t afterFlags=0;if(lookup(game,1,h.uid)==unit&&Read((uintptr_t)unit+0x124,afterFlags)&&Writable((uintptr_t)unit+0x124,4))*(uint32_t*)((uintptr_t)unit+0x124)=(afterFlags&~0x40u)|(savedFlags&0x40u);
 if(now>=h.state->logAt||!result){h.state->logAt=now+5000;Text t;t.Add("ES_CONTINUOUS_HAMMER uid=");t.Num(h.uid);t.Add(" role=");t.Add(h.sovereign?"sovereign":"dungeon");t.Add(" skill=");t.Num(h.id);t.Add(" level=");t.Num(h.level);t.Add(" interval_ms=");t.Num(cfg.hammerInterval);t.Add(" native_return=");t.Num(result);t.Add(" missiles_created=");t.Num(h.state->missiles);t.Add(" shots=");t.Num(h.state->shots);t.Add(" native_spiral=1 movement_and_attacks_preserved=1 animation_commands=0");Log(t.s,!result);}
}
void ContinuousPoll(void* game) noexcept {
 if(!ready||!cfg.enabled||!cfg.continuousHammer)return;const uint64_t epoch=uint64_t(InterlockedCompareExchange64(&generation,0,0)),now=GetTickCount64();if(!epoch)return;
 for(uint32_t i=0;i<BossSkills::count;++i){auto& b=BossSkills::bosses[i];if(b.active&&b.epoch==epoch&&b.game==game&&b.thread==GetCurrentThreadId()&&BossSkills::Legend(b.kind))HammerStep(game,BossSkills::lookup(game,1,b.uid),now);}
 for(auto& a:ES::actors){if(a.epoch==epoch&&a.game==game&&a.thread==GetCurrentThreadId())HammerStep(game,BossSkills::lookup(game,1,a.uid),now);}
}
}
