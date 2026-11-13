#pragma once


namespace LegendNative {
using SkillDamageFn=int32_t(__fastcall*)(uint8_t,void*,int32_t,int32_t,int32_t);
SkillDamageFn balanceDamagePrior[4]={};
bool WeaponSkill(uint32_t id) noexcept {switch(SkillBalance::Canon(id)){case 388:case 26:case 106:case 107:case 111:case 126:case 147:case 152:case 257:case 277:return true;default:return false;}}
uint32_t* BalanceSkillField(void* game,void* unit) noexcept {
 if(auto* a=SovereignNative::ActorFor(game,unit))return &a->executingSkill;
 uint32_t id[3]={};if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),id)||id[0]!=1)return nullptr;
 auto* b=BossSkills::NativeBoss(game,id[2],id[1]);return b&&!b->sovereign&&BossSkills::NativeRoot(*b,unit)?&b->executingSkill:nullptr;
}
uint32_t DamageScale(uint32_t raw,uint32_t basis) noexcept {const uint64_t value=static_cast<uint64_t>(raw)*basis/10000;return value>0x7fffffffu?0x7fffffffu:static_cast<uint32_t>(value);}

struct DamageOwner {uint32_t profile=0,id=0,level=0;bool known=false,pet=false,cacheSetup=false;};
DamageOwner BalanceOwner(uint8_t bank,void* unit,int32_t id) noexcept {
 if(!ready||!SkillBalance::enabled||id<0||bank<2||bank>3||!unit)return {};
 uintptr_t game=0;uint32_t identity[3]={};uint8_t actualBank=0;
 if(!Read(reinterpret_cast<uintptr_t>(unit),identity)||identity[0]!=1||!Read(reinterpret_cast<uintptr_t>(unit)+0xd8,game)||!game||!Read(game+0x106,actualBank)||actualBank!=bank)return {};
 auto* g=reinterpret_cast<void*>(game);
 if(auto* a=SovereignNative::ActorFor(g,unit)){
  for(uint32_t i=0;i<a->skillCount;++i)if(Pool::Skills[a->indices[i]].id==static_cast<uint32_t>(id)){auto* root=SovereignNative::PetRoot(g,a->uid,a->kind);return {a->variant,static_cast<uint32_t>(id),a->levels[i],true,false,root&&root->healthSpawnDepth!=0};}
  return {};
 }
 if(auto* b=BossSkills::NativeBoss(g,identity[2],identity[1]);b&&!b->sovereign&&BossSkills::NativeRoot(*b,unit)){
  for(const auto& s:b->skills)if(s.id==static_cast<uint32_t>(id))return {2,s.id,s.level,true,false,b->healthSpawnDepth!=0};
  return {};
 }
 auto* p=BossSkills::NativePet(g,unit);if(!p||p->retire||p->missing||BossSkills::lookup(g,1,p->uid)!=unit)return {};
 auto* b=BossSkills::NativeBoss(g,p->owner,p->ownerKind);void* owner=b?BossSkills::lookup(g,1,b->uid):nullptr;
 if(!b||!BossSkills::NativeRoot(*b,owner)||ES::stat(unit,6,0)<256)return {};
 const uint32_t profile=b->sovereign?([](void* game,void* root)->uint32_t{auto* a=SovereignNative::ActorFor(game,root);return a?a->variant:0;})(g,owner):2;
 return {profile,p->skill,0,true,true};
}
int32_t BalanceDamageValue(uint32_t component,uint8_t bank,void* unit,int32_t id,int32_t level,int32_t flags) noexcept {
 const auto owner=BalanceOwner(bank,unit,id);
 const int32_t execution=owner.known&&!owner.pet?static_cast<int32_t>(owner.level):level;
 const int32_t native=balanceDamagePrior[component](bank,unit,id,execution,flags);
 if(!owner.known||native<=0||WeaponSkill(owner.id))return native;


 const auto* meta=Meta(owner.id);if(!owner.pet&&owner.cacheSetup&&meta&&meta->summon)return native;
 const uint32_t basis=SkillBalance::DamageBasisPoints(owner.profile,owner.id);
 const uint64_t value=static_cast<uint64_t>(native)*basis/10000;
 const uint32_t result=value>0x7fffffffu?0x7fffffffu:static_cast<uint32_t>(value);

 static volatile LONG64 nextLog[1024]={};uint32_t uid=0;Read(reinterpret_cast<uintptr_t>(unit)+8,uid);const uint32_t thread=GetCurrentThreadId();const uint64_t now=GetTickCount64();const uint32_t key=(uid^(owner.id*31)^(component*131)^(thread*17))&1023;auto* deadline=&nextLog[key];const LONG64 previous=InterlockedCompareExchange64(deadline,0,0);
 if(SkillBalance::logDamage&&now>=static_cast<uint64_t>(previous)&&InterlockedCompareExchange64(deadline,static_cast<LONG64>(now+5000),previous)==previous){Text t;t.Add("ES_SKILL_DAMAGE_SCALE uid=");t.Num(uid);t.Add(" profile=");t.Num(owner.profile);t.Add(" skill=");t.Num(SkillBalance::Canon(owner.id));t.Add(" native_query=");t.Num(id);t.Add(" execution_level=");t.Num(execution);t.Add(" component=");t.Num(component);t.Add(" percent_x100=");t.Num(basis);t.Add(" native_raw=");t.Num(native);t.Add(" result_raw=");t.Num(result);t.Add(" pet=");t.Num(owner.pet);t.Add(" before_resistance=1 death_sentry_excluded=1");Log(t.s);}
 return static_cast<int32_t>(result);
}
int32_t __fastcall BalancePhysicalMin(uint8_t bank,void* unit,int32_t id,int32_t level,int32_t flags) noexcept {return BalanceDamageValue(0,bank,unit,id,level,flags);}
int32_t __fastcall BalancePhysicalMax(uint8_t bank,void* unit,int32_t id,int32_t level,int32_t flags) noexcept {return BalanceDamageValue(1,bank,unit,id,level,flags);}
int32_t __fastcall BalanceElementMin(uint8_t bank,void* unit,int32_t id,int32_t level,int32_t flags) noexcept {return BalanceDamageValue(2,bank,unit,id,level,flags);}
int32_t __fastcall BalanceElementMax(uint8_t bank,void* unit,int32_t id,int32_t level,int32_t flags) noexcept {return BalanceDamageValue(3,bank,unit,id,level,flags);}



using WeaponBuildFn=void(__fastcall*)(void*,void*,void*,void*,int32_t,uint8_t);
WeaponBuildFn balanceWeaponPrior=nullptr;
void __fastcall BalanceWeaponBuild(void* game,void* unit,void* target,void* packet,int32_t flags,uint8_t sourceDamage) noexcept {
 uint32_t* field=BalanceSkillField(game,unit);const uint32_t id=field?*field:0;uint8_t bank=0;
 DamageOwner source={};if(id&&WeaponSkill(id)&&Read(reinterpret_cast<uintptr_t>(game)+0x106,bank))source=BalanceOwner(bank,unit,id);
 balanceWeaponPrior(game,unit,target,packet,flags,sourceDamage);
 if(!source.known||!WeaponSkill(source.id))return;const uint32_t basis=SkillBalance::DamageBasisPoints(source.profile,source.id);if(basis==10000)return;
 constexpr uint32_t offsets[]={0x18,0x20,0x2c,0x30,0x34,0x38};
 for(uint32_t offset:offsets){int32_t raw=0;const uintptr_t at=reinterpret_cast<uintptr_t>(packet)+offset;if(Read(at,raw)&&raw>0&&Writable(at,4))*reinterpret_cast<int32_t*>(at)=DamageScale(static_cast<uint32_t>(raw),basis);}
 if(SkillBalance::logDamage){Text t;t.Add("ES_SKILL_WEAPON_SCALE skill=");t.Num(SkillBalance::Canon(source.id));t.Add(" profile=");t.Num(source.profile);t.Add(" percent_x100=");t.Num(basis);t.Add(" before_resistance=1 ordinary_attack_unchanged=1");Log(t.s);}
}
void BalanceWeaponMissile(void* game,void* owner,void* params,void* missile) noexcept {
 uint32_t type=99,id=0;uint8_t bank=0;uintptr_t data=0;int16_t nativeId=-1;
 if(!params||!missile||!Read(reinterpret_cast<uintptr_t>(missile),type)||type!=3||!Read(reinterpret_cast<uintptr_t>(params)+0x3c,id)||!Read(reinterpret_cast<uintptr_t>(missile)+0x10,data)||!data||!Read(data+0xc,nativeId)||nativeId<0||static_cast<uint32_t>(nativeId)!=id||!Read(reinterpret_cast<uintptr_t>(game)+0x106,bank))return;
 const auto source=BalanceOwner(bank,owner,id);if(!source.known||!WeaponSkill(source.id))return;const uint32_t basis=SkillBalance::DamageBasisPoints(source.profile,source.id);if(basis==10000)return;
 constexpr uint32_t stats[]={21,22,48,49,50,51,52,53,54,55,57,58};
 for(uint32_t statId:stats){const int32_t raw=static_cast<int32_t>(baseStat(missile,statId,0));if(raw>0)setStat(missile,statId,static_cast<int32_t>(DamageScale(raw,basis)),0);}
 if(SkillBalance::logDamage){Text t;t.Add("ES_SKILL_WEAPON_MISSILE_SCALE skill=");t.Num(SkillBalance::Canon(source.id));t.Add(" profile=");t.Num(source.profile);t.Add(" percent_x100=");t.Num(basis);t.Add(" native_missile_cache=1 lengths_unchanged=1");Log(t.s);}
}
bool CreateBalanceDamageHooks() noexcept {
 if(!SkillBalance::enabled)return true;
 #include "skill_balance_contract.inc"
 const uintptr_t rvas[]={0x3385b0,0x338160,0x3387d0,0x338340};
 void* hooks[]={reinterpret_cast<void*>(BalancePhysicalMin),reinterpret_cast<void*>(BalancePhysicalMax),reinterpret_cast<void*>(BalanceElementMin),reinterpret_cast<void*>(BalanceElementMax)};
 for(uint32_t i=0;i<4;++i){auto* target=reinterpret_cast<void*>(ctx->exeBase+rvas[i]);if(installedHooks>=HookCapacity||MH_CreateHook(target,hooks[i],reinterpret_cast<void**>(&balanceDamagePrior[i]))!=MH_OK||!balanceDamagePrior[i])return false;targets[installedHooks++]=reinterpret_cast<uintptr_t>(target);}
 auto* weapon=reinterpret_cast<void*>(ctx->exeBase+0x44c030);if(installedHooks>=HookCapacity||MH_CreateHook(weapon,reinterpret_cast<void*>(BalanceWeaponBuild),reinterpret_cast<void**>(&balanceWeaponPrior))!=MH_OK||!balanceWeaponPrior)return false;targets[installedHooks++]=reinterpret_cast<uintptr_t>(weapon);
 Log("ES_SKILL_DAMAGE_READY components=physical_min_max_elemental_min_max weapon_builder=1 weapon_missile_cache=1 native_getters=1 per_source_owner=1 death_sentry_level_only=1");return true;
}
}
