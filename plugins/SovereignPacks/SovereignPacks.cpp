#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <new>
#include <intrin.h>
#include "balance_guard.h"
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/core_exports.h>
#include <D2RLPlugin/reimplementation_exports.h>
#include <D2RLPlugin/lifecycle_events.h>
#include <D2RLPlugin/threads.h>
#include <D2RLPlugin/data_tables.h>
#include <D2RLPlugin/encounters.h>
#include <D2RLPlugin/overlay.h>
#include "runtime_log.h"
#include "core_guard.h"

extern "C" void* memset(void* p,int v,size_t n) { auto* b=static_cast<volatile unsigned char*>(p);while(n--) *b++=static_cast<unsigned char>(v);return p; }
extern "C" void* memcpy(void* p,const void* q,size_t n) { auto* b=static_cast<volatile unsigned char*>(p);auto* a=static_cast<const volatile unsigned char*>(q);while(n--)*b++=*a++;return p; }
#include "data_generated.h"
#include "affix_pool.h"
#include "save_guard.h"
#include "area_policy.h"
#include "enemy_skills_api.h"
namespace SP {
using namespace D2RL;
struct Config {uint32_t enabled=1,minPacks=1,maxPacks=5,tzMin=5,tzMax=10,mapMin=4,mapMax=6,minMinions=3,maxMinions=5,size=300,separation=30,playerDistance=50,health=400,damage=200,minionHealth=100,minionDamage=100,level=95,leaderExperience=500;} cfg;
const PluginContext* ctx=nullptr;const LifecycleService* lifecycle=nullptr;const ThreadService* threads=nullptr;const DataTableService* tables=nullptr;const EncounterService* encounters=nullptr;
HMODULE module=nullptr;TerrorEncounters::RuntimeLog* logfile=nullptr;SRWLOCK loglock=SRWLOCK_INIT;
CoreExports::ExecuteConsoleCommandFn execute=nullptr;Reimplementations::GetLevelIdFromUnitFn getLevel=nullptr;
using PathCoordinateFn=uint16_t(__fastcall*)(const void*);PathCoordinateFn getX=nullptr,getY=nullptr;
struct SpawnRequest {void* game;const void* room;void* origin;int32_t monster;uint32_t mode,unused;int32_t x,y;uint32_t flags;uint64_t extra1,extra2;};static_assert(sizeof(SpawnRequest)==64);
using SpawnFn=uint32_t(__fastcall*)(const SpawnRequest*,int32_t,uint32_t,const void*,int32_t,const void*);SpawnFn spawnOne=nullptr;
using AllocateFn=void*(__fastcall*)(const SpawnRequest*);AllocateFn allocateOne=nullptr;
using GetStatFn=uint32_t(__fastcall*)(const void*,uint32_t,uint32_t);GetStatFn getBaseStat=nullptr,getUnitStat=nullptr;
using SetStatFn=bool(__fastcall*)(void*,uint32_t,int32_t,uint32_t);SetStatFn setStat=nullptr;
uintptr_t coreBase=0;volatile LONG64 expectedGeneration=0;uint64_t serverGeneration=0,rng=0;bool validTables=false;
bool normalRolled[1024]={},terrorRolled[1024]={};
bool normalPlanned[1024]={},terrorPlanned[1024]={};uint8_t normalTargets[1024]={},terrorTargets[1024]={};


struct Job {volatile LONG state=0;void* client=nullptr;uint64_t generation=0;int32_t level=0;bool retry=false;uint64_t nextWaitLog=0;};Job jobs[32];SRWLOCK jobLock=SRWLOCK_INIT;uint64_t nextQueueLog=0;Lifecycle::ListenerHandle handles[4]={};
const OverlayService* overlay=nullptr;Overlay::CallbackHandle frameHandle=0;
volatile LONG uiLevel=0;uint64_t nextPoll=0;
void Log(const char* s,bool error=false) noexcept {
 AcquireSRWLockExclusive(&loglock);
 if(logfile)logfile->Write(s);
 if(ctx && ctx->GetApi()) { if(error)ctx->LogError(s);else ctx->LogInfo(s); }
 ReleaseSRWLockExclusive(&loglock);
}
struct Text {
 char s[256]={};uint32_t n=0;
 void Add(const char* t) { while(*t && n<254)s[n++]=*t++;s[n]=0; }
 void Num(uint64_t v) { char b[24];uint32_t k=0;do{b[k++]=static_cast<char>('0'+v%10);v/=10;}while(v);while(k && n<254)s[n++]=b[--k];s[n]=0; }
 void Int(int32_t v) { if(v<0){Add("-");Num(static_cast<uint64_t>(-static_cast<int64_t>(v)));}else Num(static_cast<uint32_t>(v)); }
};
bool Readable(uintptr_t p,size_t n) noexcept {
 if(!p || p+n<p)return false;
 uintptr_t end=p+n;
 while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)))return false;
  if(m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || !(m.Protect&0xff))return false;
  uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;
 }return true;
}
template<class T> bool Read(uintptr_t p,T& v) noexcept { if(!Readable(p,sizeof(T)))return false;memcpy(&v,reinterpret_cast<void*>(p),sizeof(T));return true; }
bool Bytes(uintptr_t p,const uint8_t* b,size_t n) noexcept {if(!Readable(p,n))return false;auto* a=reinterpret_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i)if(a[i]!=b[i])return false;return true;}
bool Executable(uintptr_t p) noexcept {MEMORY_BASIC_INFORMATION m={};return VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)) && m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
uintptr_t RipSlot(uintptr_t instruction) noexcept {int32_t disp=0;if(!Read(instruction+2,disp))return 0;uintptr_t p=instruction+6+disp,v=0;return Read(p,v) && Executable(v)?v:0;}
bool NativeContract(HMODULE core) noexcept {
 auto b=reinterpret_cast<uintptr_t>(core);coreBase=b;

 if(!Bytes(b+TerrorEncounters::GameDifficultyReadRva,TerrorEncounters::GameDifficultyRead,sizeof(TerrorEncounters::GameDifficultyRead)) || !Bytes(b+TerrorEncounters::TestSpawnBodyRva,TerrorEncounters::TestSpawnBody,sizeof(TerrorEncounters::TestSpawnBody)) || !Bytes(b+TerrorEncounters::SpawnOneBodyRva,TerrorEncounters::SpawnOneBody,sizeof(TerrorEncounters::SpawnOneBody)))return false;
 auto stub=reinterpret_cast<uintptr_t>(encounters->runTestSpawn);
 int32_t d=0;uint8_t opcode=0;
 if(!Read(stub+10,opcode)||opcode!=0xe8||!Read(stub+11,d)||stub+15+d!=b+TerrorEncounters::TestSpawnBodyRva)return false;

 getX=reinterpret_cast<PathCoordinateFn>(RipSlot(b+0x41163f));getY=reinterpret_cast<PathCoordinateFn>(RipSlot(b+0x411650));
 if(!getX || !getY || !Executable(b+TerrorEncounters::SpawnOneBodyRva))return false;
 spawnOne=reinterpret_cast<SpawnFn>(b+TerrorEncounters::SpawnOneBodyRva);
 getLevel=reinterpret_cast<Reimplementations::GetLevelIdFromUnitFn>(GetProcAddress(core,MAKEINTRESOURCEA(2000)));
 execute=reinterpret_cast<CoreExports::ExecuteConsoleCommandFn>(GetProcAddress(core,MAKEINTRESOURCEA(14)));
 using namespace TerrorBalance;
 if(!Bytes(b+UnitStatRva,UnitStatBytes,sizeof(UnitStatBytes)) || !Bytes(b+BaseStatRva,BaseStatBytes,sizeof(BaseStatBytes)) || !Bytes(b+SetStatRva,SetStatBytes,sizeof(SetStatBytes)))return false;
 if(reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1243)))!=b+UnitStatRva || reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1242)))!=b+BaseStatRva || reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1249)))!=b+SetStatRva)return false;
 getBaseStat=reinterpret_cast<GetStatFn>(b+BaseStatRva);getUnitStat=reinterpret_cast<GetStatFn>(b+UnitStatRva);setStat=reinterpret_cast<SetStatFn>(b+SetStatRva);
 allocateOne=reinterpret_cast<AllocateFn>(RipSlot(b+0x411a18));
 return getLevel && execute && allocateOne && Executable(b+BaseStatRva) && Executable(b+SetStatRva);
}
uint32_t Rand(uint32_t bound) noexcept {

 uint64_t v;
 uint64_t limit=static_cast<uint64_t>(0)-bound;limit%=bound;
 do{rng+=0x9e3779b97f4a7c15ULL;v=rng;v=(v^(v>>30))*0xbf58476d1ce4e5b9ULL;v=(v^(v>>27))*0x94d049bb133111ebULL;v^=v>>31;}while(v<limit);
 return static_cast<uint32_t>(v%bound);
}
bool Roll(uint32_t percent) noexcept {return Rand(10000)<percent*100;}
bool Writable(uintptr_t p,size_t n) noexcept {
 if(!Readable(p,n))return false;const uintptr_t end=p+n;while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)))return false;
  DWORD v=m.Protect&0xff;if(v!=PAGE_READWRITE&&v!=PAGE_WRITECOPY&&v!=PAGE_EXECUTE_READWRITE&&v!=PAGE_EXECUTE_WRITECOPY)return false;const uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;}return true;
}
#include "names.h"
#include "affixes.h"
bool EligibleGame(void* game) noexcept;
#include "player_safety.h"
bool LoadConfig() noexcept {
 wchar_t path[1024]={};uint32_t i=0;if(!ctx->pluginDirectory)return false;while(ctx->pluginDirectory[i]&&i<950){path[i]=ctx->pluginDirectory[i];++i;}if(i==950)return false;
 constexpr wchar_t tail[]=L"\\SovereignPacks.ini";for(uint32_t n=0;n<sizeof(tail)/sizeof(wchar_t);++n)path[i+n]=tail[n];
 if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return false;
 Text ini;ini.Add("SP_CONFIG_PATH path=");for(uint32_t n=0;path[n];++n){char c[2]={path[n]<127?static_cast<char>(path[n]):'?',0};ini.Add(c);}Log(ini.s);
 struct Option {const wchar_t* key;uint32_t* value;uint32_t min,max;bool optional=false;};
 Option options[]={{L"enabled",&cfg.enabled,0,1},{L"normal_min_packs",&cfg.minPacks,0,15},{L"normal_max_packs",&cfg.maxPacks,0,15},{L"terror_min_packs",&cfg.tzMin,0,15},{L"terror_max_packs",&cfg.tzMax,0,15},{L"map_min_packs",&cfg.mapMin,0,15},{L"map_max_packs",&cfg.mapMax,0,15},{L"min_minions",&cfg.minMinions,0,8},{L"max_minions",&cfg.maxMinions,0,8},{L"size_percent",&cfg.size,100,500},{L"minimum_pack_distance",&cfg.separation,12,100},{L"minimum_player_distance",&cfg.playerDistance,30,120},{L"leader_health_percent",&cfg.health,1,100000},{L"leader_damage_percent",&cfg.damage,1,100000},{L"minion_health_percent",&cfg.minionHealth,1,100000},{L"minion_damage_percent",&cfg.minionDamage,1,100000},{L"monster_level",&cfg.level,1,99},{L"leader_experience_percent",&cfg.leaderExperience,0,10000}};
 for(auto& o:options){Text fallback;if(!o.optional)fallback.Num(*o.value);wchar_t def[32]={},text[32]={};for(uint32_t n=0;fallback.s[n];++n)def[n]=fallback.s[n];GetPrivateProfileStringW(L"packs",o.key,def,text,32,path);
  if(o.optional&&!text[0])continue;
  uint32_t value=0,n=0;for(;text[n];++n){if(text[n]<L'0'||text[n]>L'9')return false;const uint32_t digit=text[n]-L'0';if(value>o.max/10||(value==o.max/10&&digit>o.max%10))return false;value=value*10+digit;}if(!n||value<o.min||value>o.max)return false;*o.value=value;}
 return cfg.minPacks<=cfg.maxPacks&&cfg.tzMin<=cfg.tzMax&&cfg.mapMin<=cfg.mapMax&&cfg.minMinions<=cfg.maxMinions;
}


using SaveFn=int32_t(__fastcall*)(void*,void*,const uint8_t*,uint32_t,void**,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
SaveFn originalSave=nullptr;void** saveSlot=nullptr;
struct Gate {void* game=nullptr;bool known=false,completed=false;};Gate gates[8];SRWLOCK gateLock=SRWLOCK_INIT;
bool Completion(const uint8_t* data,uint32_t size,bool& complete) noexcept {
 if(!data||size<8||size>16*1024*1024||!Readable(reinterpret_cast<uintptr_t>(data),size))return false;
 auto word=[](const uint8_t* p) noexcept -> uint16_t{return p[0]|static_cast<uint16_t>(p[1])<<8;};
 auto dword=[&](const uint8_t* p) noexcept -> uint32_t{return word(p)|static_cast<uint32_t>(word(p+2))<<16;};
 if(dword(data)!=0xaa55aa55)return false;bool found=false;const uint32_t bound=size<65536?size:65536;
 for(uint32_t n=8;n+298<=bound;++n){const auto* p=data+n;if(dword(p)!=0x216f6f57||dword(p+4)!=6||word(p+8)!=298)continue;
  if(found)return false;found=true;complete=(word(p+10+2*96+40*2)&1)!=0;}return found;
}
int32_t __fastcall Save(void* game,void* client,const uint8_t* data,uint32_t bytes,void** player,uintptr_t room,uintptr_t x,uintptr_t y,uintptr_t version) noexcept {
 const int32_t result=originalSave(game,client,data,bytes,player,room,x,y,version);bool complete=false;uintptr_t unit=0;uint32_t type=99;
 if(result==0&&player&&Read(reinterpret_cast<uintptr_t>(player),unit)&&unit&&Read(unit,type)&&type==0){
  if(!Players::Track(game,client))Log("SP_PLAYER_TRACK_REJECT: native live client lookup unavailable.",true);
  const bool known=Completion(data,bytes,complete);AcquireSRWLockExclusive(&gateLock);Gate* target=nullptr;
  for(auto& g:gates){if(g.game==game){target=&g;break;}if(!g.game&&!target)target=&g;}
  if(target&&!target->game){*target={game,known,complete};Text t;t.Add("SP_ENDGAME_GATE known=");t.Num(known);t.Add(" hell_baal_completed=");t.Num(complete);t.Add(" source=first_validated_player_save");Log(t.s);}
  ReleaseSRWLockExclusive(&gateLock);
 }return result;
}
bool EligibleGame(void* game) noexcept {AcquireSRWLockShared(&gateLock);bool eligible=false;for(auto& g:gates)if(g.game==game){eligible=g.known&&g.completed;break;}ReleaseSRWLockShared(&gateLock);return eligible;}
bool InstallSave() noexcept {
 if(!Bytes(coreBase+SPSave::CallRva,SPSave::CallBytes,sizeof(SPSave::CallBytes)))return false;
 auto* slot=reinterpret_cast<void**>(coreBase+SPSave::SlotRva);uintptr_t previous=0;if(!Read(reinterpret_cast<uintptr_t>(slot),previous)||!Executable(previous)||!Writable(reinterpret_cast<uintptr_t>(slot),8))return false;
 originalSave=reinterpret_cast<SaveFn>(previous);if(InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(Save),reinterpret_cast<void*>(previous))!=reinterpret_cast<void*>(previous))return false;saveSlot=slot;return true;
}
#include "colors_generated.h"
#include "visuals.h"
bool ValidateTables(uint32_t bank) noexcept {DataTables::TableView view={.structSize=DataTables::TableViewSize};
 if(tables->getTable(ctx,static_cast<DataTables::Bank>(bank),DataTables::TableId::MonStats,&view)!=DataTables::Result::Success||!view.rows||view.rowCount!=SPData::TotalRows){Log("SP_TABLES_REJECT: expected 1779 MonStats rows; install companion tables and compile VanillaDeluxe.",true);return false;}

 for(const auto& k:SPData::Kinds)for(uint32_t id:{k.leader,k.minion}){DataTables::RowView row={.structSize=DataTables::RowViewSize};if(tables->getRow(ctx,static_cast<DataTables::Bank>(bank),DataTables::TableId::MonStats,id,&row)!=DataTables::Result::Success||!row.row||!row.rowSize)return false;}
 return Names::Validate(bank)&&Visuals::Prepare();
}
void NotifyEnemySkills(void* game,void* unit) noexcept {



 HMODULE plugin=GetModuleHandleW(L"EnemySkills.dll");if(!plugin)return;
 auto reg=reinterpret_cast<R3niEnemySkills::RegisterFn>(GetProcAddress(plugin,"R3niEnemySkillsRegisterV1"));if(!reg)return;
 uint32_t type=99,kind=0;const uintptr_t u=reinterpret_cast<uintptr_t>(unit);
 if(!game||!unit||!EligibleGame(game)||!serverGeneration||serverGeneration!=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0))||!Read(u,type)||type!=1||!Read(u+4,kind)||kind<815||kind>=SPData::TotalRows||(kind-815)%2)return;
 R3niEnemySkills::Registration r={};r.game=game;r.unit=unit;r.session=serverGeneration;reg(&r);
}
uint32_t Scale(uint32_t value,uint32_t percent) noexcept {const uint64_t out=static_cast<uint64_t>(value)*percent/100;return out>0x7fffffff?0x7fffffff:out?static_cast<uint32_t>(out):value?1:0;}
void* Allocate(const SpawnRequest& request,bool leader) noexcept {
 uint32_t index=0;uintptr_t tls=0;const uintptr_t array=static_cast<uintptr_t>(__readgsqword(0x58));uint32_t nesting=0;
 if(!Read(coreBase+0x7df224,index)||index>1023||!Read(array+index*8,tls)||!Writable(tls+0x1958,4)||!Read(tls+0x1958,nesting)||nesting>65535)return nullptr;
 auto* count=reinterpret_cast<volatile uint32_t*>(tls+0x1958);*count=nesting+1;void* unit=allocateOne(&request);*count=nesting;if(!unit)return nullptr;
 uint32_t type=99,id=0;if(!Read(reinterpret_cast<uintptr_t>(unit),type)||type!=1||!Read(reinterpret_cast<uintptr_t>(unit)+4,id)||id!=static_cast<uint32_t>(request.monster)){Log("SP_ALLOCATION_REJECT: native returned unexpected actor.",true);return unit;}
 const uint32_t before=getBaseStat(unit,12,0);setStat(unit,12,cfg.level,0);
 const uint32_t hp=leader?cfg.health:cfg.minionHealth,dmg=leader?cfg.damage:cfg.minionDamage;
 const uint32_t nativeMax=getBaseStat(unit,7,0);
 if(hp!=100){const uint64_t requested=static_cast<uint64_t>(nativeMax)*hp/100;
  if(requested>0x7fffffffULL){Text cap;cap.Add("SP_HEALTH_CAPPED class=");cap.Num(id);cap.Add(" requested_percent=");cap.Num(hp);cap.Add(" native_max_fixed=");cap.Num(nativeMax);cap.Add(" applied_max_fixed=2147483647");Log(cap.s);}
  uint32_t max=Scale(nativeMax,hp),current=Scale(getBaseStat(unit,6,0),hp);if(max<256)max=256;if(current>max)current=max;setStat(unit,7,max,0);setStat(unit,6,current,0);}
 if(leader){setStat(unit,13,cfg.leaderExperience?Scale(getBaseStat(unit,13,0),cfg.leaderExperience):0,0);}
 if(dmg!=100){constexpr uint32_t damage[]={21,22,23,24,48,49,50,51,52,53,54,55,57,58};for(uint32_t stat:damage){uint32_t value=getBaseStat(unit,stat,0);if(value)setStat(unit,stat,Scale(value,dmg),0);}}
 if(leader&&!Affixes::Apply(request.game,unit))Log("SP_AFFIXES_REJECT: four eligible native modifiers unavailable; actor retained without forced incompatible effects.",true);
 if(leader)NotifyEnemySkills(request.game,unit);
 Text t;t.Add("SP_ACTOR class=");t.Num(id);t.Add(" leader=");t.Num(leader);t.Add(" native_level=");t.Num(before);t.Add(" final_level=");t.Num(getBaseStat(unit,12,0));t.Add(" health_percent=");t.Num(hp);t.Add(" damage_percent=");t.Num(dmg);Log(t.s,before!=95);
 return unit;
}
#include "placement.h"
bool GetContext(void* client,uint32_t expected,SpawnRequest& r,uint32_t& bank,bool& terror) noexcept {
 uintptr_t c=reinterpret_cast<uintptr_t>(client),game=0,player=0,path=0,room=0;uint32_t type=99;uint8_t difficulty=255,b=0;
 if(!Read(c+0x2b0,game)||!game||!Read(c+0x278,player)||!player||!Read(player,type)||type!=0||!Read(player+0x38,path)||!path||getLevel(reinterpret_cast<void*>(player))!=static_cast<int32_t>(expected)||!Read(path+0x20,room)||!room||!Read(game+0x104,difficulty)||difficulty!=2||!Read(game+0x106,b)||b<2||b>3||!EligibleGame(reinterpret_cast<void*>(game)))return false;
 if(expected==1||expected==40||expected==75||expected==103||expected==109)return false;
 uint8_t tz=0;
 if(AreaPolicy::IsMap(expected)){



  uintptr_t room2=0,level=0;uint32_t actual=0;
  if(!Placement::RoomLevel(reinterpret_cast<void*>(room),room2,level,actual)||actual!=expected)return false;
 }else {
  uintptr_t regions=0,region=0;
  if(!Read(game+0x1d0,regions)||!regions||!Read(regions+expected*8,region)||!region||!Read(region+0x2e8,tz)||tz>1)return false;
 }
 if(!Players::Track(reinterpret_cast<void*>(game),client))return false;
 bank=b;terror=tz==1;r={reinterpret_cast<void*>(game),reinterpret_cast<void*>(room),nullptr,0,1,0,getX(reinterpret_cast<void*>(path)),getY(reinterpret_cast<void*>(path)),3,0,0};return true;
}
void OnGameThread(const PluginContext*,void* data) noexcept {
 auto* j=static_cast<Job*>(data);AcquireSRWLockExclusive(&jobLock);const bool claimed=InterlockedCompareExchange(&j->state,2,1)==1;ReleaseSRWLockExclusive(&jobLock);if(!claimed)return;
 if(j->generation!=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0))||!cfg.enabled){InterlockedExchange(&j->state,0);return;}
 if(serverGeneration!=j->generation){serverGeneration=j->generation;memset(normalRolled,0,sizeof(normalRolled));memset(terrorRolled,0,sizeof(terrorRolled));memset(normalPlanned,0,sizeof(normalPlanned));memset(terrorPlanned,0,sizeof(terrorPlanned));memset(normalTargets,0,sizeof(normalTargets));memset(terrorTargets,0,sizeof(terrorTargets));Placement::Reset();validTables=false;}
 SpawnRequest r={};uint32_t bank=0;bool terror=false;
 if(!GetContext(j->client,j->level,r,bank,terror)){Log("SP_ENTRY_SKIP: town, non-Hell, incomplete host save, stale client or unavailable native context.");InterlockedExchange(&j->state,0);return;}
 if(!validTables)validTables=ValidateTables(bank);
 if(!validTables||!Placement::Install()){Log("SP_ENTRY_SKIP: companion tables/resources or native hooks unavailable.",true);InterlockedExchange(&j->state,0);return;}
 const auto kind=AreaPolicy::Classify(j->level,terror);
 bool& rolled=kind==AreaPolicy::Kind::Terror?terrorRolled[j->level]:normalRolled[j->level];
 bool& planned=kind==AreaPolicy::Kind::Terror?terrorPlanned[j->level]:normalPlanned[j->level];
 uint8_t& target=kind==AreaPolicy::Kind::Terror?terrorTargets[j->level]:normalTargets[j->level];
 if(!rolled){rolled=true;
  const uint32_t minimum=kind==AreaPolicy::Kind::Map?cfg.mapMin:kind==AreaPolicy::Kind::Terror?cfg.tzMin:cfg.minPacks;
  const uint32_t maximum=kind==AreaPolicy::Kind::Map?cfg.mapMax:kind==AreaPolicy::Kind::Terror?cfg.tzMax:cfg.maxPacks;
  const uint32_t total=minimum+Rand(maximum-minimum+1);
  target=static_cast<uint8_t>(total);
  Text choice;choice.Add("SP_AREA_ROLL level=");choice.Num(j->level);choice.Add(" category=");choice.Add(AreaPolicy::Label(kind));choice.Add(" minimum=");choice.Num(minimum);choice.Add(" maximum=");choice.Num(maximum);choice.Add(" rolled_total=");choice.Num(total);Log(choice.s);


 }else if(planned&&!j->retry)Log("SP_ENTRY_ALREADY_PLANNED: no repeated pack or loot rolls.");


 if(!planned){planned=Placement::Plan(r,j->level,target,kind==AreaPolicy::Kind::Terror);if(!planned&&!j->retry)Log("SP_PLAN_DEFERRED: same rolled target retained until authoritative context is ready.");}
 Placement::ScanLoaded(r,j->retry);
 InterlockedExchange(&j->state,0);
}
ConsoleCommandResult Dispatch(D2R::Game::Client* client,const ConsoleCommandContext* c,void*) noexcept {
 if(!client || !c || c->contextSize<ConsoleCommandContextSize || !c->args || !c->argsLength || c->argsLength>5){Log("SP_DISPATCH_REJECT: missing server client or invalid command context.");return ConsoleCommandResult::InvalidArguments;}
 const bool retry=c->args[c->argsLength-1]=='r';const uint32_t length=c->argsLength-(retry?1:0);if(!length||length>4)return ConsoleCommandResult::InvalidArguments;
 uint32_t level=0;
 for(uint32_t i=0;i<length;++i){const char a=c->args[i];if(a<'0'||a>'9')return ConsoleCommandResult::InvalidArguments;level=level*10+static_cast<uint32_t>(a-'0');}
 if(level<1||level>=1024)return ConsoleCommandResult::InvalidArguments;
 const uint64_t generation=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0));
 if(!cfg.enabled||!generation){Log("SP_DISPATCH_SKIP: disabled or no active host session.");return ConsoleCommandResult::Handled;}
 if(!retry){Text t;t.Add("SP_DISPATCH level=");t.Num(level);t.Add(" host_session=");t.Num(generation);Log(t.s);}



 Job* selected=nullptr;bool merged=false,reportWait=false;const uint64_t now=GetTickCount64();
 AcquireSRWLockExclusive(&jobLock);
 for(auto& j:jobs)if(j.state==1&&j.generation==generation&&j.client==client){

  j.retry=j.level==static_cast<int32_t>(level)?j.retry&&retry:retry;j.level=static_cast<int32_t>(level);
  if(now>=j.nextWaitLog){j.nextWaitLog=now+30000;reportWait=true;}merged=true;break;
 }
 if(!merged)for(auto& j:jobs){

  if(InterlockedCompareExchange(&j.state,3,0)!=0)continue;
  j.client=client;j.generation=generation;j.level=static_cast<int32_t>(level);j.retry=retry;j.nextWaitLog=now+30000;InterlockedExchange(&j.state,1);selected=&j;break;
 }
 const bool busy=!merged&&!selected;const bool reportBusy=busy&&now>=nextQueueLog;if(reportBusy)nextQueueLog=now+30000;
 ReleaseSRWLockExclusive(&jobLock);
 if(merged){if(reportWait)Log("SP_DISPATCH_WAIT: pending client request coalesced; no extra game-thread task.");return ConsoleCommandResult::Handled;}
 if(!selected){if(reportBusy)Log("SP_DISPATCH_BUSY: all task slots in use; bounded polling will retry.",true);return ConsoleCommandResult::Handled;}
 auto result=threads->runOnGameThread(ctx,OnGameThread,selected);
 if(result!=Threads::Result::Success){InterlockedCompareExchange(&selected->state,0,1);Log("No authoritative game thread; remote clients do not roll encounters.");}
 return ConsoleCommandResult::Handled;
}
void OnFrame(const PluginContext*,const Overlay::Frame* frame,void*) noexcept {
 (void)frame;


 const LONG level=InterlockedCompareExchange(&uiLevel,0,0);
 if(!cfg.enabled||!InterlockedCompareExchange64(&expectedGeneration,0,0)||level<1||level>=1024)return;
 const uint64_t now=GetTickCount64();if(now<nextPoll)return;nextPoll=now+2000;
 Text t;t.Add("r3ni_packs_lan_v1 ");t.Num(static_cast<uint32_t>(level));t.Add("r");execute(t.s);
}
void OnGameplay(const PluginContext*,const Lifecycle::GameplayEvent* e,void*) noexcept {
 if(!e || e->structSize<Lifecycle::GameplayEventSize)return;
 if(e->kind==Lifecycle::GameplayEventKind::GameLeft){Log("SP_GAME_LEFT: pending work invalidated.");InterlockedExchange(&uiLevel,0);InterlockedExchange64(&expectedGeneration,0);AcquireSRWLockExclusive(&gateLock);memset(gates,0,sizeof(gates));ReleaseSRWLockExclusive(&gateLock);Players::Reset();return;}
 InterlockedExchange64(&expectedGeneration,static_cast<LONG64>(e->sessionGeneration));
 {Text t;t.Add("SP_EVENT kind=");t.Num(static_cast<uint32_t>(e->kind));t.Add(" session=");t.Num(e->sessionGeneration);t.Add(" previous=");t.Num(static_cast<uint32_t>(e->previousValue));t.Add(" current=");t.Num(static_cast<uint32_t>(e->currentValue));Log(t.s);}
 if(e->kind!=Lifecycle::GameplayEventKind::LevelChanged || !cfg.enabled || e->currentValue<1 || e->currentValue>=1024)return;
 const int32_t level=e->currentValue;InterlockedExchange(&uiLevel,(level==1||level==40||level==75||level==103||level==109)?0:level);nextPoll=GetTickCount64()+2000;



 Text t;t.Add("r3ni_packs_lan_v1 ");t.Num(static_cast<uint32_t>(e->currentValue));
 if(!execute(t.s))Log("Core could not queue encounter dispatch.",true);else Log("SP_NOTIFY: Core accepted entry command; awaiting host dispatch.");
}
void OnTables(const PluginContext*,const Lifecycle::DataTablesLoadedEvent*,void*) noexcept {validTables=false;Visuals::Prepare();}
bool Equal(const char* a,const char* b){if(!a||!b)return false;while(*a&&*b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}return *a==*b;}
const PluginInfo info={sizeof(PluginInfo),4,"r3ni-sovereign-packs","Sovereign Packs","0.3.10-colors","r3ni","Configurable orange monster packs after Hell completion; authoritative LAN spawning.",PluginFlags::ModScopedOnly|PluginFlags::NativeHooks|PluginFlags::Shared,{}};
}
extern "C" __declspec(dllexport) bool R3niSovereignUnlinkPopulation(void* observer,void* replacement) noexcept {
 return InterlockedCompareExchangePointer(reinterpret_cast<void**>(&SP::Placement::originalPopulation),replacement,observer)==observer;
}
extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept {return &SP::info;}
extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept {
 using namespace SP;if(!D2RL::HasContext(context)||!context->GetApi())return false;ctx=context;
 void* storage=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(TerrorEncounters::RuntimeLog));if(storage)logfile=new(storage) TerrorEncounters::RuntimeLog(module);
 Log("Sovereign Packs 0.3.10-colors author=r3ni");
 Log("SP_DISPATCH_READY same_client_polls_coalesced=1 stale_callbacks_not_reused=1 wait_log_ms=30000");
 Log("SP_EXPLORATION_READY cell=8 radius=32 sampled_server_positions=1 all_LAN_players=1 partition=voronoi native_collision_preserved=1");
 if(!Equal(ctx->activeMod,"VanillaDeluxe")||ctx->loadScope!=LoadScope::Mod||!LoadConfig()){Log("SP_LOAD_REJECT: mod scope or INI invalid.",true);return false;}
 Text settings;settings.Add("SP_CONFIG_READY minions=");settings.Num(cfg.minMinions);settings.Add("..");settings.Num(cfg.maxMinions);settings.Add(" level=");settings.Num(cfg.level);settings.Add(" leader_hp_percent=");settings.Num(cfg.health);settings.Add(" minion_hp_percent=");settings.Num(cfg.minionHealth);settings.Add(" leader_damage_percent=");settings.Num(cfg.damage);settings.Add(" minion_damage_percent=");settings.Num(cfg.minionDamage);settings.Add(" leader_xp_percent=");settings.Num(cfg.leaderExperience);Log(settings.s);
 if(ctx->QueryService(&lifecycle)!=ServiceQueryResult::Success||!lifecycle||lifecycle->serviceSize<LifecycleServiceSize||lifecycle->serviceVersion!=1||!lifecycle->registerGameplayEventListener||!lifecycle->registerDataTablesLoadedListener||ctx->QueryService(&threads)!=ServiceQueryResult::Success||!threads||threads->serviceSize<ThreadServiceSize||threads->serviceVersion!=1||!threads->runOnGameThread||ctx->QueryService(&tables)!=ServiceQueryResult::Success||!tables||tables->serviceSize<DataTableServiceSize||tables->serviceVersion!=1||!tables->getTable||!tables->getRow||ctx->QueryService(&encounters)!=ServiceQueryResult::Success||!encounters||encounters->serviceSize<EncounterServiceSize||encounters->serviceVersion!=1||!encounters->runTestSpawn)return false;
 HMODULE core=GetModuleHandleW(L"D2RCore.dll");if(!core||!NativeContract(core)||!Names::Bind()||!Affixes::Bind()||!Players::Bind()||!InstallSave()){Log("SP_LOAD_REJECT: supplied Core ABI/save delegate does not match.",true);return false;}
 Log("SP_NATIVE_AFFIXES_READY: four distinct eligible native affixes; native descriptions; custom skill/UI hooks removed.");
 auto command=MakeConsoleCommand("r3ni_packs_lan_v1",Dispatch,"Internal authoritative monster-pack entry.");command.flags=ConsoleCommandHidden|ConsoleCommandSuppressEcho;command.scope=ConsoleCommandScope::RemoteAllowed;command.access=ConsoleCommandAccess::Any;if(!ctx->RegisterConsoleCommand(command))return false;
 Lifecycle::GameplayEventKind kinds[]={Lifecycle::GameplayEventKind::GameJoined,Lifecycle::GameplayEventKind::GameLeft,Lifecycle::GameplayEventKind::LevelChanged};
 for(uint32_t n=0;n<3;++n){Lifecycle::GameplayEventListener l={.structSize=Lifecycle::GameplayEventListenerSize,.kind=kinds[n],.callback=OnGameplay};if(lifecycle->registerGameplayEventListener(ctx,&l,&handles[n])!=Lifecycle::Result::Success)return false;}
 Lifecycle::DataTablesLoadedListener l={.structSize=Lifecycle::DataTablesLoadedListenerSize,.callback=OnTables};if(lifecycle->registerDataTablesLoadedListener(ctx,&l,&handles[3])!=Lifecycle::Result::Success)return false;
 if(ctx->QueryService(&overlay)==ServiceQueryResult::Success&&overlay&&overlay->serviceSize>=OverlayServiceSize&&overlay->serviceVersion==1&&overlay->registerFrameCallback&&overlay->unregisterFrameCallback){
  Overlay::CallbackRegistration frame={.structSize=Overlay::CallbackRegistrationSize,.phase=Overlay::Phase::AfterGameUi,.callback=OnFrame};
  if(overlay->registerFrameCallback(ctx,&frame,&frameHandle)!=Overlay::Result::Success)frameHandle=0;
 }
 Log(frameHandle?"SP_POLL_READY: pending loaded rooms checked every two seconds through authoritative LAN dispatch.":"SP_POLL_UNAVAILABLE: Overlay service unavailable; only room observers remain.",!frameHandle);
 LARGE_INTEGER counter={};QueryPerformanceCounter(&counter);rng=counter.QuadPart^GetTickCount64()^(static_cast<uint64_t>(GetCurrentProcessId())<<32)^reinterpret_cast<uintptr_t>(module);
 Text safety;safety.Add("SP_PLAYER_SAFETY_READY minimum_distance=");safety.Num(cfg.playerDistance);safety.Add(" source=live_server_clients leader_and_minions=1");Log(safety.s);
 Text ranges;ranges.Add("SP_READY normal=");ranges.Num(cfg.minPacks);ranges.Add("..");ranges.Num(cfg.maxPacks);ranges.Add(" terror=");ranges.Num(cfg.tzMin);ranges.Add("..");ranges.Num(cfg.tzMax);ranges.Add(" maps=");ranges.Num(cfg.mapMin);ranges.Add("..");ranges.Num(cfg.mapMax);ranges.Add(" Hell_completed_host_only=1 existing_elites_unchanged=1");Log(ranges.s);return true;
}
extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept {
 using namespace SP;InterlockedExchange(&uiLevel,0);InterlockedExchange64(&expectedGeneration,0);if(frameHandle&&overlay&&ctx)overlay->unregisterFrameCallback(ctx,frameHandle);frameHandle=0;Placement::Remove();if(saveSlot&&originalSave)InterlockedCompareExchangePointer(saveSlot,reinterpret_cast<void*>(originalSave),reinterpret_cast<void*>(Save));saveSlot=nullptr;
 if(ctx&&lifecycle){for(uint32_t n=0;n<3;++n)if(handles[n]&&lifecycle->unregisterGameplayEventListener)lifecycle->unregisterGameplayEventListener(ctx,handles[n]);if(handles[3]&&lifecycle->unregisterDataTablesLoadedListener)lifecycle->unregisterDataTablesLoadedListener(ctx,handles[3]);}
 if(logfile){logfile->~RuntimeLog();HeapFree(GetProcessHeap(),0,logfile);logfile=nullptr;}ctx=nullptr;
}
extern "C" BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)SP::module=h;return TRUE;}
