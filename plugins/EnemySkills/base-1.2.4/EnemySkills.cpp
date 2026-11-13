#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "mini_probe.h"
#include <new>
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/lifecycle_events.h>
#include <D2RLPlugin/data_tables.h>
#include "third_party/minhook/include/MinHook.h"
#include "balance_guard.h"
#include "skill_pool.h"
#include "combat_action_state.h"
#include "enemy_skills_api.h"
#include "native_skill_data.h"
extern "C" void* memset(void* p,int v,size_t n){auto* b=static_cast<volatile unsigned char*>(p);while(n--)*b++=static_cast<unsigned char>(v);return p;}
extern "C" void* memcpy(void* p,const void* q,size_t n){auto* b=static_cast<volatile unsigned char*>(p);auto* a=static_cast<const volatile unsigned char*>(q);while(n--)*b++=*a++;return p;}
namespace NovaRouteTrace {void Baseline(void*) noexcept;}
namespace BossSkills {void Observe(void*,uint32_t,void*,void*,const void*) noexcept;void Reset() noexcept;}
namespace SovereignNative {bool Initialize() noexcept;bool InitializeAnimation() noexcept;bool Attach(void*,void*) noexcept;bool Eligible(void*,void*,uint32_t) noexcept;void OnDeath(uint32_t,uint32_t) noexcept;void Reset() noexcept;}
namespace ES {
using namespace D2RL;
const PluginContext* ctx=nullptr;const LifecycleService* lifecycle=nullptr;
Lifecycle::ListenerHandle handles[4]={};HMODULE module=nullptr;
SRWLOCK loglock=SRWLOCK_INIT;
uintptr_t coreBase=0;volatile LONG64 generation=0;volatile LONG active=0;
struct Config {uint32_t enabled=1,level=20,chance=25,cooldown=4000,buffRefresh=30000,onHit=1,onStruck=1,curses=1,damage=1,buffs=1,traps=1,forced=0,logAssign=1,logCasts=0,legendGap=2500,legendRecovery=1000,legendSummonHealth=500,summons=1,auras=1,movement=1,summonMax=2,trapMax=2,hydraMax=1,sovereignSummonHealth=25,trapCastGuard=1,skillCastGuard=1,interruptedRetry=1200,summonRefill=1,summonRefillGap=2500,legendSummonMax=3,legendAttackSpeed=50,legendMoveSpeed=50,legendSummonBatch=8,legendSummonInterval=6000,sovereignSummonBatch=8,continuousHammer=1,hammerInterval=750,skillRoles=1,sovereignCastGap=1000,sovereignRecovery=250,sovereignTrapBatch=5,sovereignCastPriority=1,sovereignFairOffense=1;} cfg;
struct Text {char s[384]={};uint32_t n=0;void Add(const char* t){while(*t&&n<382)s[n++]=*t++;s[n]=0;}void Num(uint64_t v){char b[24];uint32_t k=0;do{b[k++]=static_cast<char>('0'+v%10);v/=10;}while(v);while(k&&n<382)s[n++]=b[--k];s[n]=0;}};
void Log(const char* s,bool error=false) noexcept {if(error)MiniProbe::Line(s);if(!error)return;AcquireSRWLockExclusive(&loglock);if(ctx&&ctx->GetApi())ctx->LogError(s);ReleaseSRWLockExclusive(&loglock);}
bool Readable(uintptr_t p,size_t n) noexcept {if(!p||p+n<p)return false;const uintptr_t end=p+n;while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(m.Protect&0xff))return false;const uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;}return true;}
template<class T>bool Read(uintptr_t p,T& v) noexcept {if(!Readable(p,sizeof(T)))return false;memcpy(&v,reinterpret_cast<void*>(p),sizeof(T));return true;}
bool Bytes(uintptr_t p,const uint8_t* b,size_t n) noexcept {if(!Readable(p,n))return false;auto* a=reinterpret_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i)if(a[i]!=b[i])return false;return true;}
bool Executable(uintptr_t p) noexcept {MEMORY_BASIC_INFORMATION m={};return VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
bool Writable(uintptr_t p,size_t n) noexcept {if(!Readable(p,n))return false;const uintptr_t end=p+n;while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)))return false;const DWORD v=m.Protect&0xff;if(v!=PAGE_READWRITE&&v!=PAGE_WRITECOPY&&v!=PAGE_EXECUTE_READWRITE&&v!=PAGE_EXECUTE_WRITECOPY)return false;const uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;}return true;}
uintptr_t RipSlot(uintptr_t instruction) noexcept {int32_t d=0;uintptr_t p=0;return Read(instruction+2,d)&&Read(instruction+6+d,p)&&Executable(p)?p:0;}
bool Equal(const char* a,const char* b) noexcept {if(!a||!b)return false;while(*a&&*b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}return *a==*b;}
#include "skill_balance.h"
#include "summon_tuning.h"
bool LoadConfig() noexcept {
 wchar_t path[1024]={};uint32_t i=0;if(!ctx->pluginDirectory)return false;while(ctx->pluginDirectory[i]&&i<950){path[i]=ctx->pluginDirectory[i];++i;}if(i==950)return false;
 constexpr wchar_t tail[]=L"\\EnemySkills.ini";for(uint32_t n=0;n<sizeof(tail)/sizeof(wchar_t);++n)path[i+n]=tail[n];if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES){Log("ES_CONFIG_MISSING: EnemySkills.ini is required.",true);return false;}
 struct Option {const wchar_t* key;uint32_t* value;uint32_t min,max;};
 Option opts[]={{L"skill_role_policy",&cfg.skillRoles,0,1},{L"enabled",&cfg.enabled,0,1},{L"skill_level",&cfg.level,1,99},{L"proc_chance_percent",&cfg.chance,0,100},{L"cooldown_ms",&cfg.cooldown,250,60000},{L"buff_refresh_ms",&cfg.buffRefresh,1000,3600000},{L"cast_on_hit",&cfg.onHit,0,1},{L"cast_on_struck",&cfg.onStruck,0,1},{L"enable_curses",&cfg.curses,0,1},{L"enable_damage",&cfg.damage,0,1},{L"enable_buffs",&cfg.buffs,0,1},{L"enable_traps",&cfg.traps,0,1},{L"test_skill_id",&cfg.forced,0,1000},{L"log_assignments",&cfg.logAssign,0,1},{L"log_casts",&cfg.logCasts,0,1},{L"legend_skill_gap_ms",&cfg.legendGap,1000,60000},{L"legend_skill_recovery_ms",&cfg.legendRecovery,0,10000},{L"legend_summon_health_percent",&cfg.legendSummonHealth,100,100000},{L"enable_summons",&cfg.summons,0,1},{L"enable_auras",&cfg.auras,0,1},{L"enable_movement",&cfg.movement,0,1},{L"sovereign_max_summons",&cfg.summonMax,1,8},{L"sovereign_max_traps",&cfg.trapMax,1,5},{L"sovereign_max_hydras",&cfg.hydraMax,1,3},{L"sovereign_summon_owner_health_percent",&cfg.sovereignSummonHealth,0,100},{L"uninterruptible_trap_casts",&cfg.trapCastGuard,0,1},{L"uninterruptible_skill_casts",&cfg.skillCastGuard,0,1},{L"interrupted_cast_retry_ms",&cfg.interruptedRetry,250,5000},{L"legend_attack_speed_bonus_percent",&cfg.legendAttackSpeed,0,200},{L"legend_movement_speed_bonus_percent",&cfg.legendMoveSpeed,0,200},{L"legend_summons_per_cast",&cfg.legendSummonBatch,1,8},{L"legend_summon_interval_ms",&cfg.legendSummonInterval,1000,60000},{L"legend_max_summons",&cfg.legendSummonMax,1,8},{L"sovereign_summons_per_cast",&cfg.sovereignSummonBatch,1,8},{L"continuous_blessed_hammer",&cfg.continuousHammer,0,1},{L"blessed_hammer_interval_ms",&cfg.hammerInterval,250,10000},{L"summon_refill_priority",&cfg.summonRefill,0,1},{L"summon_refill_interval_ms",&cfg.summonRefillGap,1000,60000}};
 Option pacing[]={{L"sovereign_cast_gap_ms",&cfg.sovereignCastGap,250,60000},{L"sovereign_cast_recovery_ms",&cfg.sovereignRecovery,0,10000},{L"sovereign_traps_per_cast",&cfg.sovereignTrapBatch,1,5},{L"sovereign_cast_priority",&cfg.sovereignCastPriority,0,1},{L"sovereign_offensive_rotation",&cfg.sovereignFairOffense,0,1}};
 auto load=[&](Option& o){Text t;t.Num(*o.value);wchar_t def[32]={},text[32]={};for(uint32_t n=0;t.s[n];++n)def[n]=t.s[n];GetPrivateProfileStringW(L"EnemySkills",o.key,def,text,32,path);uint32_t value=0,n=0;for(;text[n];++n){if(n>6||text[n]<L'0'||text[n]>L'9')return false;value=value*10+text[n]-L'0';}if(!n||value<o.min||value>o.max)return false;*o.value=value;return true;};
 for(auto& o:opts)if(!load(o))return false;for(auto& o:pacing)if(!load(o))return false;
 {Text t;t.Add("ES_SOVEREIGN_PACING_CONFIG cast_gap_ms=");t.Num(cfg.sovereignCastGap);t.Add(" recovery_ms=");t.Num(cfg.sovereignRecovery);t.Add(" traps_per_cast=");t.Num(cfg.sovereignTrapBatch);t.Add(" cast_priority=");t.Num(cfg.sovereignCastPriority);t.Add(" native_cast_animation=1 simultaneous_walk_cast=0 existing_ini_preserved=1");Log(t.s);}

 {Text t;t.Add("ES_SOVEREIGN_EFFECT_POLICY death_sentry_assignable=0 curse_target_state=1 persistent_self_state=1 per_slot_failures=1 corpse_summon_native_filter=1 offensive_rotation=");t.Num(cfg.sovereignFairOffense);t.Add(" semantic_roles=3 traps_are_offensive=1 buffs_are_support=1 active_attack_required_for_multi_skill=1 two_skill_weights=50_25_25 three_skill_weights=50_50 shared_gap_preserved=1");Log(t.s);}
 {Text t;t.Add("ES_NATIVE_SUMMON_HEALTH_CONFIG percent=");t.Num(cfg.legendSummonHealth);t.Add(" apply_once=1 registered_boss_pets_only=1 traps_and_prison_excluded=1");Log(t.s);}
 if(cfg.forced&&!Pool::EnemyAssignable(cfg.forced)){Log("ES_CONFIG_REJECT DeathSentry_Valkyrie_Leap_disabled_for_sovereign=1 player_skill_unchanged=1",true);return false;}
 if(cfg.forced){bool found=false;for(const auto& s:Pool::Skills)if(s.id==cfg.forced)found=true;if(!found){Log("ES_CONFIG_REJECT: test_skill_id must belong to the audited skill pool.",true);return false;}}
 {Text t;t.Add("ES_SOVEREIGN_SUMMON_HEALTH_CONFIG owner_percent=");t.Num(cfg.sovereignSummonHealth);t.Add(" per_summon=1 native_floor=1 apply_once_after_spawn=1 constructs_excluded=1");Log(t.s);}
 {Text t;t.Add("ES_SUMMON_REFILL_CONFIG enabled=");t.Num(cfg.summonRefill);t.Add(" interval_ms=");t.Num(cfg.summonRefillGap);t.Add(" sovereign_limit=");t.Num(cfg.summonMax);t.Add(" dungeon_limit=");t.Num(cfg.legendSummonMax);t.Add(" global_gaps_preserved=1 creature_summons_only=1 limits_from_ini=1 traps_hydra_prison_unchanged=1");Log(t.s);}
 return SkillBalance::Load(path)&&SummonTuning::Load(path)&&(!cfg.enabled||cfg.forced||cfg.curses||cfg.damage||cfg.buffs||cfg.traps||cfg.summons||cfg.auras||cfg.movement);
}
constexpr uintptr_t DispatchRva=0x833700;
constexpr uint8_t DispatchBytes[]={0x48,0x83,0xec,0x48,0x4c,0x89,0xc0,0x41,0x89,0xd0,0x48,0x89,0xca,0x48,0x8b,0x4c,0x24,0x70,0x4c,0x8b,0x54,0x24,0x78};
using DispatchFn=int32_t(__fastcall*)(void*,uint32_t,void*,void*,const void*,const void*);
using CastFn=int32_t(__fastcall*)(void*,int32_t,int32_t,void*,int32_t);
using SkillFn=const void*(__fastcall*)(uint8_t,int32_t);
using StatFn=uint32_t(__fastcall*)(const void*,uint32_t,uint32_t);
using AlignFn=uint32_t(__fastcall*)(void*);
DispatchFn original=nullptr;CastFn cast=nullptr;
using CoordinateCastFn=int32_t(__fastcall*)(void*,int32_t,int32_t,int32_t,int32_t,int32_t);
CoordinateCastFn coordinateCast=nullptr;
SkillFn skill=nullptr;StatFn stat=nullptr;AlignFn alignment=nullptr;
bool initialized=false,installed=false;
bool Bind(HMODULE core) noexcept {
 coreBase=reinterpret_cast<uintptr_t>(core);
 const uint8_t coordinateBytes[]={0xff,0x15,0x36,0x04,0x33,0x00};
 const uint8_t castBytes[]={0xff,0x15,0x67,0x04,0x33,0x00},skillBytes[]={0xff,0x15,0x2c,0x05,0x33,0x00};
 const uint8_t flagAddress[]={0x4c,0x8d,0xb6,0x28,0x01,0x00,0x00},flagClear[]={0x41,0x80,0x66,0x03,0xef};
 const uint8_t alignBytes[]={0x48,0x83,0xec,0x28,0xe8,0xb7,0x5d,0xba,0xff,0x89,0xc0,0x48,0x83,0xc4,0x28,0xc3};
 const uint8_t allyFlags[]={0x41,0xf6,0x80,0x26,0x01,0x00,0x00,0x01};
 using namespace TerrorBalance;
 if(reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1307)))!=coreBase+DispatchRva||!Bytes(coreBase+DispatchRva,DispatchBytes,sizeof(DispatchBytes))||!Bytes(coreBase+0x3debe3,castBytes,sizeof(castBytes))||!Bytes(coreBase+0x3dec2c,coordinateBytes,sizeof(coordinateBytes))||!Bytes(coreBase+0x3deb66,skillBytes,sizeof(skillBytes))||!Bytes(coreBase+0x3debf2,flagAddress,sizeof(flagAddress))||!Bytes(coreBase+0x3dec32,flagClear,sizeof(flagClear))||!Bytes(coreBase+UnitStatRva,UnitStatBytes,sizeof(UnitStatBytes))||reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1243)))!=coreBase+UnitStatRva||reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1276)))!=coreBase+0x832bc0||!Bytes(coreBase+0x832bc0,alignBytes,sizeof(alignBytes))||!Bytes(coreBase+0x3a1117,allyFlags,sizeof(allyFlags)))return false;
 cast=reinterpret_cast<CastFn>(RipSlot(coreBase+0x3debe3));coordinateCast=reinterpret_cast<CoordinateCastFn>(RipSlot(coreBase+0x3dec2c));skill=reinterpret_cast<SkillFn>(RipSlot(coreBase+0x3deb66));stat=reinterpret_cast<StatFn>(coreBase+UnitStatRva);alignment=reinterpret_cast<AlignFn>(coreBase+0x832bc0);return cast&&coordinateCast&&skill&&Executable(coreBase+UnitStatRva);
}
uint64_t rng=0;
uint32_t Rand(uint32_t bound) noexcept {uint64_t v,limit=static_cast<uint64_t>(0)-bound;limit%=bound;do{rng+=0x9e3779b97f4a7c15ULL;v=rng;v=(v^(v>>30))*0xbf58476d1ce4e5b9ULL;v=(v^(v>>27))*0x94d049bb133111ebULL;v^=v>>31;}while(v<limit);return static_cast<uint32_t>(v%bound);}
bool Enabled(Pool::Category c) noexcept {switch(c){case Pool::Category::Curse:return cfg.curses;case Pool::Category::Damage:return cfg.damage;case Pool::Category::Buff:return cfg.buffs;case Pool::Category::Trap:return cfg.traps;case Pool::Category::Summon:return cfg.summons;case Pool::Category::Aura:return cfg.auras;case Pool::Category::Movement:return cfg.movement;}return false;}
const char* Category(Pool::Category c) noexcept {switch(c){case Pool::Category::Curse:return "curse";case Pool::Category::Damage:return "damage";case Pool::Category::Buff:return "buff";case Pool::Category::Trap:return "trap";case Pool::Category::Summon:return "summon";case Pool::Category::Aura:return "aura";case Pool::Category::Movement:return "movement";}return "unknown";}
struct NativePool {bool built=false;uint32_t indices[Pool::Count]={},count=0;};NativePool pools[2];
bool ValidatePool(uint8_t bank) noexcept {
 if(bank<2||bank>3)return false;auto& p=pools[bank-2];if(p.built)return p.count!=0;p.built=true;
 for(uint32_t i=0;i<Pool::Count;++i){const auto& s=Pool::Skills[i];if(!Pool::EnemyAssignable(s.id))continue;if(cfg.forced?s.id!=cfg.forced:!Enabled(s.category))continue;if(skill(bank,s.id))p.indices[p.count++]=i;else {Text t;t.Add("ES_SKILL_UNAVAILABLE id=");t.Num(s.id);t.Add(" name=");t.Add(s.name);Log(t.s,true);}}
 Text t;t.Add("ES_POOL bank=");t.Num(bank);t.Add(" candidates=");t.Num(p.count);t.Add(" skill_level=");t.Num(cfg.level);t.Add(" native_learned_skill=1 item_proc=0");Log(t.s,!p.count);return p.count!=0;
}
struct CastVisualState {uint64_t cast=0;uint32_t skill=0;bool sent=false,injecting=false;};
struct HammerState {uint64_t next=0,logAt=0;uint32_t missiles=0,shots=0;bool emitting=false;};
constexpr uint32_t Capacity=32768;
struct Actor {uint64_t epoch=0;void* game=nullptr;uint32_t uid=0,kind=0,index=0,thread=0;uint64_t next=0;bool busy=false,buffed=false;uint64_t started=0,recovery=0,walkSince=0,rollAt=0,logAt=0;uint32_t mode=0,frame=0,handlers=0,effects=0,missiles=0,failures=0;bool factoryDue=false,frameValid=false;uint32_t rootSlot=0xffffffffu,interruptMode=0,attempts=0,trapBlocks=0;uint64_t auraNext=0,diagNext=0;uint32_t diagReason=0;uint32_t executingSkill=0;uint32_t levels[3]={};uint32_t indices[3]={},skillCount=1,skillLevel=20,variant=0,slot=0,cursor=0;uint64_t perNext[3]={},sharedNext=0,supportDiag[3]={};uint32_t perFailures[3]={};CombatActions::State combat;HammerState hammer;CastVisualState visual;};Actor actors[Capacity];
uint64_t castWindow=0;uint32_t castsInWindow=0;bool fullLogged=false;
uint32_t Hash(void* game,uint32_t uid,uint32_t kind) noexcept {uint64_t v=(reinterpret_cast<uintptr_t>(game)>>4)^(static_cast<uint64_t>(uid)*0x9e3779b97f4a7c15ULL)^(static_cast<uint64_t>(kind)<<23);v^=v>>33;v*=0xff51afd7ed558ccdULL;return static_cast<uint32_t>(v^(v>>32))&(Capacity-1);}
Actor* Find(uint64_t epoch,void* game,uint32_t uid,uint32_t kind,bool create) noexcept {const uint32_t start=Hash(game,uid,kind);for(uint32_t n=0;n<Capacity;++n){auto& a=actors[(start+n)&(Capacity-1)];if(a.epoch!=epoch){if(!create)return nullptr;a={epoch,game,uid,kind,0,GetCurrentThreadId(),0,false,false};return &a;}if(a.game==game&&a.uid==uid&&a.kind==kind)return &a;}if(!fullLogged){fullLogged=true;Log("ES_RECORD_LIMIT: existing assignments retained, new skills skipped.",true);}return nullptr;}
bool Ally(void* target,uint32_t type) noexcept {if(type==0)return true;if(type!=1)return false;uint32_t flags=0;return Read(reinterpret_cast<uintptr_t>(target)+0x124,flags)&&(flags&0x80000200u)&&alignment(target)==2;}


void Try(void*,uint32_t,void*,void*,const void*) noexcept {}
int32_t __fastcall Hook(void* game,uint32_t event,void* unit,void* target,const void* damage,const void* metadata) noexcept {const int32_t result=original(game,event,unit,target,damage,metadata);BossSkills::Observe(game,event,unit,target,damage);Try(game,event,unit,target,damage);return result;}
bool Install() noexcept {
 if(installed)return true;if(!Bytes(coreBase+DispatchRva,DispatchBytes,sizeof(DispatchBytes)))return false;
 if(!initialized){if(MH_Initialize()!=MH_OK)return false;initialized=true;}
 auto* address=reinterpret_cast<void*>(coreBase+DispatchRva);const auto create=MH_CreateHook(address,reinterpret_cast<void*>(Hook),reinterpret_cast<void**>(&original));if(create!=MH_OK||!original)return false;
 InterlockedExchange(&active,1);if(MH_EnableHook(address)!=MH_OK){InterlockedExchange(&active,0);MH_RemoveHook(address);original=nullptr;return false;}
 installed=true;Log("ES_HOST_HOOK_READY: registered_leaders_only=1 existing_native_effects_once=1 allies=players_mercs_summons");return true;
}
}
#include "skill_roles.h"
namespace ES {
bool Register(const R3niEnemySkills::Registration* request,uint32_t requestedCount=1,uint32_t requestedLevel=0,uint32_t variant=0) noexcept {
 if(!requestedLevel)requestedLevel=cfg.level;if(requestedCount>3||requestedLevel<1||requestedLevel>99||variant>1)return false;
 R3niEnemySkills::Registration r={};if(!cfg.enabled||!ctx||!request||!Readable(reinterpret_cast<uintptr_t>(request),sizeof(r)))return false;memcpy(&r,request,sizeof(r));
 const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0));
 if(r.size!=sizeof(r)||r.abi!=R3niEnemySkills::Abi||r.reserved||r.role!=R3niEnemySkills::Role::SovereignLeader||!epoch||r.session!=epoch||!r.game||!r.unit)return false;
 uint32_t type=99,kind=0,uid=0;uint8_t difficulty=255,bank=0;const uintptr_t u=reinterpret_cast<uintptr_t>(r.unit);
 if(!Read(u,type)||type!=1||!Read(u+4,kind)||kind<815||kind>=1779||(kind-815)%2||!Read(u+8,uid)||!Read(reinterpret_cast<uintptr_t>(r.game)+0x104,difficulty)||difficulty!=2||!Read(reinterpret_cast<uintptr_t>(r.game)+0x106,bank)||!ValidatePool(bank)||stat(r.unit,6,0)<256||alignment(r.unit)==2)return false;


 Actor* a=Find(epoch,r.game,uid,kind,false);if(a){return a->thread==GetCurrentThreadId()&&a->skillCount==requestedCount&&a->skillLevel==requestedLevel&&a->variant==variant&&SovereignNative::Attach(r.game,r.unit);}
 if(!SovereignNative::Initialize()){Log("ES_SOVEREIGN_NATIVE_REJECT native_contract_unavailable=1 actor_and_affixes_retained=1 no_item_fallback=1",true);return false;}
 auto& p=pools[bank-2];uint32_t eligible[Pool::Count]={},n=0;for(uint32_t k=0;k<p.count;++k)if(SovereignNative::Eligible(r.game,r.unit,p.indices[k]))eligible[n++]=p.indices[k];if(!n){Log("ES_SOVEREIGN_ASSIGN_REJECT no_model_compatible_skill=1 forced_skill_or_aura_conflict=1",true);return false;}
 a=Find(epoch,r.game,uid,kind,true);if(!a)return false;a->skillCount=requestedCount;a->skillLevel=requestedLevel;a->variant=variant;


 if(!SkillRoles::Select(eligible,n,requestedCount,a->indices,variant)){a->epoch=0;Log("ES_ROLE_POLICY_REJECT active_attack_or_composition_unavailable=1 existing_ini_filters_preserved=1",true);return false;}
 const uint32_t chosen=requestedCount;
 for(uint32_t i=0;i<requestedCount;++i)if(SkillRoles::Get(Pool::Skills[a->indices[i]])==SkillRoles::Role::Offensive){Text t;t.Add("ES_SOVEREIGN_OFFENSIVE_DRAW uid=");t.Num(uid);t.Add(" skill=");t.Num(Pool::Skills[a->indices[i]].id);t.Add(" variant=");t.Num(variant);t.Add(" rotation=");t.Num(cfg.sovereignFairOffense);t.Add(" variant_draw_count=");t.Num(SkillRoles::offensiveDraws[variant][a->indices[i]]);t.Add(" model_eligibility_preserved=1");Log(t.s);}
 for(uint32_t i=0;i<requestedCount;++i)a->levels[i]=SkillBalance::Level(variant,Pool::Skills[a->indices[i]].id,requestedLevel);
 a->index=requestedCount?a->indices[0]:0;
 {Text t;t.Add("ES_SOVEREIGN_LOADOUT uid=");t.Num(uid);t.Add(" variant=");t.Num(variant);t.Add(" requested=");t.Num(requestedCount);t.Add(" assigned=");t.Num(chosen);t.Add(" role_policy=");t.Num(cfg.skillRoles);t.Add(" composition_policy=three_roles active_attack_required=1");Log(t.s);}
 const auto& s=Pool::Skills[a->index];if(cfg.logAssign){Text t;t.Add("ES_ASSIGN uid=");t.Num(uid);t.Add(" class=");t.Num(kind);t.Add(" skill=");t.Num(s.id);t.Add(" name=");t.Add(s.name);t.Add(" category=");t.Add(Category(s.category));t.Add(" level=");t.Num(a->skillCount?a->levels[0]:a->skillLevel);t.Add(" host_only=1 native_affixes_unchanged=1");Log(t.s);}
 if(!SovereignNative::Attach(r.game,r.unit)){a->epoch=0;return false;}return true;
}
void OnGameplay(const PluginContext*,const Lifecycle::GameplayEvent* e,void*) noexcept {if(!e||e->structSize<Lifecycle::GameplayEventSize)return;if(e->kind==Lifecycle::GameplayEventKind::GameLeft){BossSkills::Reset();InterlockedExchange64(&generation,0);Log("ES_GAME_LEFT: old registrations cannot proc.");return;}InterlockedExchange64(&generation,static_cast<LONG64>(e->sessionGeneration));}
const PluginInfo info={sizeof(PluginInfo),4,"r3ni-enemy-skills","Enemy Skills","1.2.4-target-lists","r3ni","Native enemy skills, persistent Dungeon Legends pursuit and cooperative player Multishot visuals.",PluginFlags::ModScopedOnly|PluginFlags::NativeHooks|PluginFlags::Shared,{}};
}

using ES::stat;
#include "armageddon_probe.h"
#include "cast_context_diag.h"
#include "boss_skills_extension.h"
#include "sovereign_native.h"
#include "combat_actions.h"
#include "native_leap.h"
#include "skill_balance_damage.h"
#include "continuous_hammer.h"
#include "shared_offensive_diagnostics.h"
#include "skill_visual_sync.h"
#include "client_offensive_flow.h"
#include "offensive_cast_signal.h"
#include "loot_unique_zero.h"
#include "summon_target_fix.h"
extern "C" __declspec(dllexport) bool R3niEnemySkillsRegisterV1(const R3niEnemySkills::Registration* r) noexcept {return ES::Register(r);}
extern "C" __declspec(dllexport) bool R3niEnemySkillsRegisterV2(const R3niEnemySkills::RegistrationV2* request) noexcept {
 R3niEnemySkills::RegistrationV2 v={};if(!request||!ES::Readable(reinterpret_cast<uintptr_t>(request),sizeof(v)))return false;memcpy(&v,request,sizeof(v));if(v.size!=sizeof(v)||v.abi!=2||v.reserved||v.reserved2)return false;
 R3niEnemySkills::Registration r={};r.game=v.game;r.unit=v.unit;r.session=v.session;r.role=v.role;return ES::Register(&r,v.skillCount,v.skillLevel,v.variant);
}
extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept {return &ES::info;}
extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept {
 using namespace ES;if(!D2RL::HasContext(context)||!context->GetApi())return false;ctx=context;ArmageddonProbe::Open(ctx->pluginDirectory);MiniProbe::Open(ctx->pluginDirectory,L"MultishotVisualProbe_ES.log");MiniProbe::Line("ES_LOAD 1.2.4-target-lists");
 if(!Equal(ctx->activeMod,"VanillaDeluxe")||ctx->loadScope!=LoadScope::Mod||!LoadConfig()){Log("ES_LOAD_REJECT: mod scope or INI invalid.",true);return false;}
 if(!cfg.enabled)Log("ES_ORDINARY_DISABLED: Sovereign native skills off; fixed boss service remains available.");
 HMODULE core=GetModuleHandleW(L"D2RCore.dll");if(!core||!Bind(core)){Log("ES_LOAD_REJECT: supplied native Core ABI does not match.",true);return false;}
 if(ctx->QueryService(&lifecycle)!=ServiceQueryResult::Success||!lifecycle||lifecycle->serviceVersion!=1||lifecycle->serviceSize<LifecycleServiceSize||!lifecycle->registerGameplayEventListener)return false;
 Lifecycle::GameplayEventKind kinds[]={Lifecycle::GameplayEventKind::GameJoined,Lifecycle::GameplayEventKind::GameLeft,Lifecycle::GameplayEventKind::LevelChanged};for(uint32_t n=0;n<3;++n){Lifecycle::GameplayEventListener l={.structSize=Lifecycle::GameplayEventListenerSize,.kind=kinds[n],.callback=OnGameplay};if(lifecycle->registerGameplayEventListener(ctx,&l,&handles[n])!=Lifecycle::Result::Success)return false;}
 LegendNative::ObserveSharedBank();
 CastContext::Bind();
 if(!BossSkills::Publish()){Log("ES_BOSS_SERVICE_REJECT: plugin communication unavailable.",true);return false;}
 Lifecycle::MonsterDeathListener death={.structSize=Lifecycle::MonsterDeathListenerSize,.callback=BossSkills::OnDeath};if(lifecycle->registerMonsterDeathListener(ctx,&death,&handles[3])!=Lifecycle::Result::Success)return false;
 if(!LootUniqueZero::Install())Log("ES_LOOT_UNIQUE_ZERO_UNAVAILABLE loader_contract_or_memory_protection=1 skills_unchanged=1",true);
 if(!LegendNative::InstallPlayerVisual())MiniProbe::Line("PLAYER_CLIENT_HOOK_READY=0 startup=1 native_client_forwarding_unchanged=1");
 if(!SummonTargetFix::Install(ctx->exeBase))Log("ES_SUMMON_TARGET_FIX_UNAVAILABLE: native caller contract or memory protection did not match; existing skills remain enabled.",true);
 LARGE_INTEGER counter={};QueryPerformanceCounter(&counter);rng=counter.QuadPart^GetTickCount64()^(static_cast<uint64_t>(GetCurrentProcessId())<<32)^reinterpret_cast<uintptr_t>(module);
 if(cfg.enabled&&!SovereignNative::InitializeAnimation())Log("ES_SOVEREIGN_SEQUENCE_UNAVAILABLE charge_leap_candidates_disabled=1",true);
 Text t;t.Add("ES_READY skill_level=");t.Num(cfg.level);t.Add(" native_ai_chance_percent=");t.Num(cfg.chance);t.Add(" cooldown_ms=");t.Num(cfg.cooldown);t.Add(" uninterruptible_skill_casts=");t.Num(cfg.skillCastGuard);t.Add(" uninterruptible_trap_casts=");t.Num(cfg.trapCastGuard);t.Add(" pool_size=");t.Num(Pool::Count);t.Add(" bridge=SovereignPacks_0.3.6_or_newer no_UI_edits=1");Log(t.s);return true;
}
extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept {
 using namespace ES;SummonTargetFix::Remove();LootUniqueZero::Remove();BossSkills::Remove();BossSkills::Reset();if(handles[3]&&lifecycle&&ctx&&lifecycle->unregisterMonsterDeathListener)lifecycle->unregisterMonsterDeathListener(ctx,handles[3]);InterlockedExchange(&active,0);InterlockedExchange64(&generation,0);if(initialized){MH_Uninitialize();initialized=false;}installed=false;original=nullptr;if(ctx&&lifecycle&&lifecycle->unregisterGameplayEventListener){for(uint32_t i=0;i<3;++i)if(handles[i])lifecycle->unregisterGameplayEventListener(ctx,handles[i]);}ArmageddonProbe::Close();MiniProbe::Close();ctx=nullptr;
}
extern "C" BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)ES::module=h;return TRUE;}
