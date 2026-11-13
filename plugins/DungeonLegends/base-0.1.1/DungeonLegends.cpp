#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <new>
#include <intrin.h>
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/core_exports.h>
#include <D2RLPlugin/reimplementation_exports.h>
#include <D2RLPlugin/lifecycle_events.h>
#include <D2RLPlugin/threads.h>
#include <D2RLPlugin/data_tables.h>
#include <D2RLPlugin/encounters.h>
#include <D2RLPlugin/overlay.h>
#include <D2RLPlugin/network.h>
#include <D2RLPlugin/plugin_communication.h>
#include "runtime_log.h"
#include "core_guard.h"
#include "balance_guard.h"
#include "chat_guard.h"
#include "location_guard.h"
#include "loot_guard.h"
#include "profiles_generated.h"
#include "boss_skills_api.h"
#include "affix_pool.h"
extern "C" void* memset(void* p,int v,size_t n){auto* b=static_cast<volatile unsigned char*>(p);while(n--)*b++=static_cast<unsigned char>(v);return p;}
extern "C" void* memcpy(void* p,const void* q,size_t n){auto* b=static_cast<volatile unsigned char*>(p);auto* a=static_cast<const volatile unsigned char*>(q);while(n--)*b++=*a++;return p;}
namespace TL {
using namespace D2RL;
const PluginContext* ctx=nullptr;HMODULE module=nullptr;uintptr_t coreBase=0;
const LifecycleService* lifecycle=nullptr;const ThreadService* threads=nullptr;const DataTableService* tables=nullptr;const EncounterService* encounters=nullptr;const OverlayService* overlay=nullptr;
const PluginCommunicationService* communication=nullptr;PluginCommunication::AcquiredService skillService={.structSize=PluginCommunication::AcquiredServiceSize};const R3niBossSkills::Api* skillApi=nullptr;
const NetworkService* network=nullptr;Network::ChannelHandle channel=0;Network::PeerHandle peers[64]={};SRWLOCK peerLock=SRWLOCK_INIT;
TerrorEncounters::RuntimeLog* logfile=nullptr;SRWLOCK logLock=SRWLOCK_INIT;
struct Config {uint32_t enabled=1,chance=100,size=150,health=100,damage=100,level=110,skillLevel=30,cooldown=750,refresh=5000,summonCooldown=8000,distance=50,colors=1,shake=1,shakeMs=700,top=50,testBoss=0,testSpecial=0,logRolls=1;uint32_t special[7]={50000,50000,50000,10000,7000,7000,100};} cfg;
struct Settings {uint32_t enabled=1,weight=1,size=150,health=100,damage=100,level=110,skillLevel=30,cooldown=750;};Settings bosses[TLData::Count];
using PathCoordinateFn=uint16_t(__fastcall*)(const void*);PathCoordinateFn getX=nullptr,getY=nullptr;
using GetStatFn=uint32_t(__fastcall*)(const void*,uint32_t,uint32_t);GetStatFn getBaseStat=nullptr,getUnitStat=nullptr;
using SetStatFn=bool(__fastcall*)(void*,uint32_t,int32_t,uint32_t);SetStatFn setStat=nullptr;
using AllocateFn=void*(__fastcall*)(const void*);AllocateFn allocateOne=nullptr;
struct SpawnRequest {void* game;const void* room;void* origin;int32_t monster;uint32_t mode,unused;int32_t x,y;uint32_t flags;uint64_t extra1,extra2;};static_assert(sizeof(SpawnRequest)==64);
using SpawnFn=uint32_t(__fastcall*)(const SpawnRequest*,int32_t,uint32_t,const void*,int32_t,const void*);SpawnFn spawnOne=nullptr;
CoreExports::ExecuteConsoleCommandFn execute=nullptr;Reimplementations::GetLevelIdFromUnitFn getLevel=nullptr;
volatile LONG64 expectedGeneration=0;uint64_t serverGeneration=0,rng=0;bool validTables=false,visited[1024]={};volatile LONG modelsPending=0,modelsStatus=0;uint64_t modelsRevision=0;
Lifecycle::ListenerHandle handles[5]={};Overlay::CallbackHandle frameHandle=0;
volatile LONG uiLevel=0,shakeLevel=0;uint64_t nextPoll=0,nextNetwork=0,shakeEnd=0;bool shaking=false,restaged=false;
void Log(const char* s,bool error=false) noexcept {AcquireSRWLockExclusive(&logLock);if(logfile)logfile->Write(s);if(ctx&&ctx->GetApi()){if(error)ctx->LogError(s);else ctx->LogInfo(s);}ReleaseSRWLockExclusive(&logLock);}
struct Text {char s[384]={};uint32_t n=0;void Add(const char* p){while(*p&&n<382)s[n++]=*p++;s[n]=0;}void Num(uint64_t v){char b[24];uint32_t k=0;do{b[k++]=static_cast<char>('0'+v%10);v/=10;}while(v);while(k&&n<382)s[n++]=b[--k];s[n]=0;}void Int(int32_t v){if(v<0){Add("-");Num(-static_cast<int64_t>(v));}else Num(v);}};
#include "core_bind.h"
bool Writable(uintptr_t p,size_t n) noexcept {if(!Readable(p,n))return false;const uintptr_t end=p+n;while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)))return false;const DWORD x=m.Protect&0xff;if(x!=PAGE_READWRITE&&x!=PAGE_WRITECOPY&&x!=PAGE_EXECUTE_READWRITE&&x!=PAGE_EXECUTE_WRITECOPY)return false;p=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;}return true;}
bool Equal(const char* a,const char* b) noexcept {if(!a||!b)return false;while(*a&&*b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}return *a==*b;}
uint32_t Rand(uint32_t bound) noexcept {if(!bound)return 0;uint64_t v,limit=(uint64_t(0)-bound)%bound;do{rng+=0x9e3779b97f4a7c15ULL;v=rng;v=(v^(v>>30))*0xbf58476d1ce4e5b9ULL;v=(v^(v>>27))*0x94d049bb133111ebULL;v^=v>>31;}while(v<limit);return v%bound;}
uint32_t Scale(uint32_t v,uint32_t percent) noexcept {if(!v)return 0;uint64_t x=static_cast<uint64_t>(v)*percent/100;if(x<1)x=1;if(x>0x7fffffff)x=0x7fffffff;return static_cast<uint32_t>(x);}
bool IniNumber(const wchar_t* path,const wchar_t* section,const wchar_t* key,uint32_t& value,uint32_t min,uint32_t max) noexcept {Text fallback;fallback.Num(value);wchar_t def[32]={},s[32]={};for(uint32_t n=0;fallback.s[n];++n)def[n]=fallback.s[n];GetPrivateProfileStringW(section,key,def,s,32,path);uint32_t v=0,n=0;for(;s[n];++n){if(s[n]<L'0'||s[n]>L'9'||v>max/10||(v==max/10&&static_cast<uint32_t>(s[n]-L'0')>max%10))return false;v=v*10+s[n]-L'0';}if(!n||v<min||v>max)return false;value=v;return true;}
bool Percentage(const wchar_t* path,const wchar_t* key,uint32_t& ppm) noexcept {wchar_t s[32]={};if(!GetPrivateProfileStringW(L"loot",key,L"",s,32,path))return true;uint32_t v=0,places=0;bool decimal=false,digit=false;for(uint32_t i=0;s[i];++i){if(s[i]==L'.'&&!decimal){decimal=true;continue;}if(s[i]<L'0'||s[i]>L'9'||v>1000000||(decimal&&++places>4))return false;digit=true;v=v*10+s[i]-L'0';}if(!digit)return false;while(places++<4){if(v>100000)return false;v*=10;}if(v>1000000)return false;ppm=v;return true;}
bool LoadConfig() noexcept {
 wchar_t path[1024]={};if(!ctx->pluginDirectory)return false;uint32_t n=0;while(ctx->pluginDirectory[n]&&n<950){path[n]=ctx->pluginDirectory[n];++n;}if(n==950)return false;constexpr wchar_t tail[]=L"\\DungeonLegends.ini";for(uint32_t i=0;i<sizeof(tail)/sizeof(wchar_t);++i)path[n+i]=tail[i];if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return false;
 struct Option {const wchar_t* section;const wchar_t* key;uint32_t* value;uint32_t min,max;};
 Option options[]={{L"legends",L"enabled",&cfg.enabled,0,1},{L"legends",L"spawn_chance_percent",&cfg.chance,0,100},{L"legends",L"minimum_player_distance",&cfg.distance,30,150},{L"legends",L"all_color_variants",&cfg.colors,0,1},{L"legends",L"camera_shake",&cfg.shake,0,1},{L"legends",L"camera_shake_ms",&cfg.shakeMs,100,3000},{L"legends",L"log_unsuccessful_rolls",&cfg.logRolls,0,1},{L"defaults",L"size_percent",&cfg.size,50,400},{L"defaults",L"health_percent",&cfg.health,1,100000},{L"defaults",L"damage_percent",&cfg.damage,1,100000},{L"defaults",L"monster_level",&cfg.level,1,150},{L"defaults",L"skill_level",&cfg.skillLevel,1,99},{L"defaults",L"skill_cooldown_ms",&cfg.cooldown,250,60000},{L"defaults",L"buff_refresh_ms",&cfg.refresh,1000,3600000},{L"defaults",L"summon_cooldown_ms",&cfg.summonCooldown,1000,60000},{L"loot",L"top_unique_chance_percent",&cfg.top,0,100},{L"tests",L"force_boss_index",&cfg.testBoss,0,20},{L"tests",L"force_special_item",&cfg.testSpecial,0,7}};
 for(auto& o:options)if(!IniNumber(path,o.section,o.key,*o.value,o.min,o.max))return false;
 const wchar_t* chances[]={L"corruption_percent",L"dungeon_map_percent",L"malus_percent",L"weak_scroll_percent",L"medium_scroll_percent",L"strong_scroll_percent",L"all_skills_scroll_percent"};uint32_t sum=0;for(uint32_t i=0;i<7;++i){if(!Percentage(path,chances[i],cfg.special[i]))return false;sum+=cfg.special[i];}if(sum>1000000)return false;
 for(uint32_t i=0;i<TLData::Count;++i){auto& b=bosses[i];b={1,1,cfg.size,cfg.health,cfg.damage,cfg.level,cfg.skillLevel,cfg.cooldown};const wchar_t* section=TLData::Profiles[i].section;
  Option own[]={{section,L"enabled",&b.enabled,0,1},{section,L"spawn_weight",&b.weight,0,1000},{section,L"size_percent",&b.size,50,400},{section,L"health_percent",&b.health,1,100000},{section,L"damage_percent",&b.damage,1,100000},{section,L"monster_level",&b.level,1,150},{section,L"skill_level",&b.skillLevel,1,99},{section,L"skill_cooldown_ms",&b.cooldown,250,60000}};for(auto& o:own)if(!IniNumber(path,o.section,o.key,*o.value,o.min,o.max))return false;}
 return true;
}
#include "visual_common.h"
#include "native_addon.h"
#include "legend_visuals.h"
namespace Names {
bool Apply(void* unit) noexcept {uint32_t kind=0;uintptr_t data=0;uint16_t flags=0;const uintptr_t u=reinterpret_cast<uintptr_t>(unit);if(!Read(u+4,kind)||kind<TLData::FirstMon||kind>=TLData::TotalMon||!Read(u+0x10,data)||!data||!Read(data+0x1a,flags)||!Writable(data+0x1a,2)||!Writable(data+0x2a,2))return false;*reinterpret_cast<uint16_t*>(data+0x2a)=TLData::FirstSU+(kind-TLData::FirstMon)/TLData::Variants;*reinterpret_cast<uint16_t*>(data+0x1a)=flags|2|8;return true;}
}
#include "affixes_native.h"
bool SkillsReady() noexcept {if(skillApi)return true;if(!communication&&ctx->QueryService(&communication)!=ServiceQueryResult::Success)return false;if(!communication||communication->serviceVersion!=1||communication->serviceSize<PluginCommunicationServiceSize||!communication->acquireService)return false;
 PluginCommunication::AcquireServiceRequest r={.structSize=PluginCommunication::AcquireServiceRequestSize,.providerPluginId="r3ni-enemy-skills",.name="fixed-boss-skills",.minimumVersion=1,.minimumTableSize=sizeof(R3niBossSkills::Api)};
 if(communication->acquireService(ctx,&r,&skillService)!=PluginCommunication::Result::Success){static uint64_t nextWarning=0;if(GetTickCount64()>=nextWarning){nextWarning=GetTickCount64()+30000;Log("DL_SKILL_SERVICE_WAIT: install the included EnemySkills.dll 0.2.0-legends and restart.",true);}return false;}skillApi=static_cast<const R3niBossSkills::Api*>(skillService.table);return skillApi&&skillApi->abi==1&&skillApi->size>=sizeof(*skillApi)&&skillApi->registerBoss;}
bool Validate(uint32_t bank) noexcept {DataTables::TableView m={.structSize=DataTables::TableViewSize},s={.structSize=DataTables::TableViewSize},u={.structSize=DataTables::TableViewSize};const auto b=static_cast<DataTables::Bank>(bank);
 if(tables->getTable(ctx,b,DataTables::TableId::MonStats,&m)!=DataTables::Result::Success||!m.rows||m.rowSize!=508||m.rowCount<TLData::TotalMon||tables->getTable(ctx,b,DataTables::TableId::Skills,&s)!=DataTables::Result::Success||!s.rows||s.rowCount<TLData::TotalSkill||tables->getTable(ctx,b,DataTables::TableId::SuperUniques,&u)!=DataTables::Result::Success||!u.rows||u.rowSize!=60||u.rowCount<TLData::TotalSU){Log("TL_TABLES_REJECT: compile VanillaDeluxe at main menu, fully exit and restart; private records are not loaded.",true);return false;}
 for(uint32_t i=0;i<TLData::Count;++i){DataTables::RowView mr={.structSize=DataTables::RowViewSize},ur={.structSize=DataTables::RowViewSize};uint16_t name=0,suname=0;
  if(tables->getRow(ctx,b,DataTables::TableId::MonStats,TLData::FirstMon+i*16,&mr)!=DataTables::Result::Success||tables->getRow(ctx,b,DataTables::TableId::SuperUniques,100+i,&ur)!=DataTables::Result::Success||!Read(reinterpret_cast<uintptr_t>(mr.row)+0x60,name)||!Read(reinterpret_cast<uintptr_t>(ur.row)+4,suname)||name!=suname)return false;}
 return SkillsReady()&&InterlockedCompareExchange(&modelsStatus,0,0)==2;
}
using EachClientFn=void(__fastcall*)(void*,void(__fastcall*)(void*,void*),void*,uint32_t);using SendChatFn=void(__fastcall*)(void*,void*);
#pragma pack(push,1)
struct Chat {uint8_t header=0x26,type=1,language=0,unitType=0;uint32_t unit=0;uint8_t color=1,nameColor=1;char name[61]={};char message[256]={};};
#pragma pack(pop)
struct Notice {Chat packet;uint32_t level;SendChatFn send;uint32_t recipients=0;};
void __fastcall SendNotice(void* client,void* data) noexcept {auto& n=*static_cast<Notice*>(data);uintptr_t p=0;if(!Read(reinterpret_cast<uintptr_t>(client)+0x278,p)||!p||getLevel(reinterpret_cast<void*>(p))!=static_cast<int32_t>(n.level))return;n.send(client,&n.packet);++n.recipients;}
void Announce(void* game,uint32_t profile,uint32_t level) noexcept {using namespace TerrorChat;if(!Bytes(coreBase+PacketConstructionRva,PacketConstructionBytes,sizeof(PacketConstructionBytes))||!Bytes(coreBase+SendChatThunkRva,SendChatThunkBytes,sizeof(SendChatThunkBytes)))return;
 const auto each=reinterpret_cast<EachClientFn>(RipSlot(coreBase+0x825806));if(!each)return;Notice n={};n.level=level;n.send=reinterpret_cast<SendChatFn>(coreBase+SendChatThunkRva);const char* text=TLData::Profiles[profile].message;for(uint32_t i=0;text[i]&&i<255;++i)n.packet.message[i]=text[i];each(game,SendNotice,&n,0);Text t;t.Add("TL_ANNOUNCE profile=");t.Add(TLData::Profiles[profile].key);t.Add(" level=");t.Num(level);t.Add(" confirmed_spawn=1 recipients=");t.Num(n.recipients);Log(t.s);}
struct ShakePacket {uint32_t level,profile,uid,duration;};
void QueueShake(uint32_t level) noexcept {if(cfg.shake)InterlockedExchange(&shakeLevel,level);}
void BroadcastShake(uint32_t profile,uint32_t uid,uint32_t level) noexcept {QueueShake(level);if(!network||!channel||!cfg.shake)return;ShakePacket p={level,profile,uid,cfg.shakeMs};AcquireSRWLockShared(&peerLock);for(auto peer:peers)if(peer)network->sendToClient(ctx,channel,peer,1,&p,sizeof(p));ReleaseSRWLockShared(&peerLock);}
void OnClientMessage(const PluginContext*,Network::ChannelHandle,uint16_t id,const void* data,uint32_t size,void*) noexcept {if(id!=1||!data||size!=sizeof(ShakePacket))return;ShakePacket p={};memcpy(&p,data,sizeof(p));if(p.level>0&&p.level<1024&&p.profile<20&&p.duration>=100&&p.duration<=3000)QueueShake(p.level);}
void OnConnection(const PluginContext*,const Network::ConnectionEvent* e,void*) noexcept {if(!e||e->structSize<Network::ConnectionEventSize||!e->peer)return;AcquireSRWLockExclusive(&peerLock);for(auto& p:peers)if(p==e->peer){if(e->state!=Network::ConnectionState::Connected)p=0;ReleaseSRWLockExclusive(&peerLock);return;}if(e->state==Network::ConnectionState::Connected)for(auto& p:peers)if(!p){p=e->peer;break;}ReleaseSRWLockExclusive(&peerLock);}
struct Record {uint64_t epoch=0;void* game=nullptr;uint32_t uid=0,kind=0,profile=0,loot=0;};Record records[256];uint32_t recordCount=0;
using DeathFn=void(__fastcall*)(void*,const void*,uintptr_t,uintptr_t);DeathFn priorDeath=nullptr;void** deathSlot=nullptr;
void __fastcall Death(void* game,const void* change,uintptr_t a,uintptr_t b) noexcept {uintptr_t unit=0;uint32_t id[3]={};if(Read(reinterpret_cast<uintptr_t>(change)+8,unit)&&unit&&Read(unit,id)&&id[0]==1&&id[1]>=1779&&id[1]<2099){for(uint32_t i=0;i<recordCount;++i){const auto& r=records[i];if(r.epoch==serverGeneration&&r.game==game&&r.uid==id[2]&&r.kind==id[1]){uint8_t bank=0;if(Read(reinterpret_cast<uintptr_t>(game)+0x106,bank))setStat(reinterpret_cast<void*>(unit),bank==3?446:445,r.loot,0);Text t;t.Add("TL_LOOT_BEGIN uid=");t.Num(r.uid);t.Add(" flags=");t.Num(r.loot);t.Add(" native_drop_once=1");Log(t.s);break;}}}priorDeath(game,change,a,b);}
bool InstallDeath() noexcept {if(deathSlot)return true;void** slot=nullptr;uintptr_t old=0;

 using namespace TerrorLoot;slot=reinterpret_cast<void**>(coreBase+DeathOriginalSlotRva);if(!Bytes(coreBase+DeathWrapperRva,DeathWrapperBytes,sizeof(DeathWrapperBytes))||!Read(reinterpret_cast<uintptr_t>(slot),old)||!Executable(old)||!Writable(reinterpret_cast<uintptr_t>(slot),8))return false;priorDeath=reinterpret_cast<DeathFn>(old);if(InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(Death),reinterpret_cast<void*>(old))!=reinterpret_cast<void*>(old))return false;deathSlot=slot;return true;}
bool Spawn(const SpawnRequest& request,uint32_t profile,uint32_t variant,uint32_t level) noexcept {
 if(recordCount>=256)return false;uint32_t index=0,nesting=0;uintptr_t tls=0,array=static_cast<uintptr_t>(__readgsqword(0x58));if(!Read(coreBase+0x7df224,index)||index>1023||!Read(array+index*8,tls)||!Read(tls+0x1958,nesting)||nesting>65535||!Writable(tls+0x1958,4))return false;
 SpawnRequest r=request;r.monster=TLData::FirstMon+profile*16+variant;r.flags=3;r.mode=1;r.origin=nullptr;r.extra1=0;
 *reinterpret_cast<uint32_t*>(tls+0x1958)=nesting+1;void* unit=allocateOne(&r);*reinterpret_cast<uint32_t*>(tls+0x1958)=nesting;if(!unit)return false;
 uint32_t identity[3]={};if(!Read(reinterpret_cast<uintptr_t>(unit),identity)||identity[0]!=1||identity[1]!=static_cast<uint32_t>(r.monster)){Log("TL_SPAWN_PARTIAL: allocated unit identity did not match; no announcement or skill registration.",true);return true;}
 if(!Affixes::Apply(r.game,unit))Log("TL_AFFIX_PARTIAL: inspect runtime log; native assignment did not confirm all four modifiers.",true);
 const auto& b=bosses[profile];uint32_t hp=Scale(getBaseStat(unit,7,0),b.health);if(hp<256)hp=256;setStat(unit,7,hp,0);setStat(unit,6,hp,0);setStat(unit,12,b.level,0);
 const uint32_t damageStats[]={21,22,23,24,48,49,50,51,52,53,54,55,57,58};for(uint32_t s:damageStats){const uint32_t old=getBaseStat(unit,s,0);if(old)setStat(unit,s,Scale(old,b.damage),0);}uint8_t bank=0;Read(reinterpret_cast<uintptr_t>(r.game)+0x106,bank);setStat(unit,bank==3?445:444,static_cast<int32_t>(b.damage)-100,0);
 uint32_t loot=Rand(100)<cfg.top?8:0,draw=Rand(1000000),sum=0;uint32_t special=cfg.testSpecial;if(!special)for(uint32_t i=0;i<7;++i){sum+=cfg.special[i];if(draw<sum){special=i+1;break;}}loot+=special;
 setStat(unit,bank==3?446:445,loot,0);records[recordCount++]={serverGeneration,r.game,identity[2],identity[1],profile,loot};
 R3niBossSkills::Registration skills={};skills.game=r.game;skills.unit=unit;skills.session=serverGeneration;
 for(uint32_t i=0;i<3;++i){const auto& p=TLData::Profiles[profile];const uint32_t refresh=p.kinds[i]==5||p.kinds[i]==6?cfg.summonCooldown:p.refresh[i]?cfg.refresh:0;skills.skills[i]={p.skills[i],b.skillLevel,p.kinds[i],b.cooldown,refresh};}
 const bool registered=SkillsReady()&&skillApi->registerBoss(&skills);Text t;t.Add("TL_SPAWN_CONFIRMED profile=");t.Add(TLData::Profiles[profile].key);t.Add(" class=");t.Num(identity[1]);t.Add(" uid=");t.Num(identity[2]);t.Add(" skill_registration=");t.Num(registered);t.Add(" loot_flags=");t.Num(loot);t.Add(" level=");t.Num(level);Log(t.s,!registered);
 Announce(r.game,profile,level);BroadcastShake(profile,identity[2],level);return true;
}

constexpr bool IsDungeon(uint32_t id) noexcept {return id==146||id==150||id==151||id==153||id==154||id==155||id==156||id==159||id==161||id==162||id==163||id==164;}
#include "legend_placement.h"
struct Job {volatile LONG state=0;void* client=nullptr;uint64_t epoch=0;uint32_t level=0;};Job jobs[32];SRWLOCK jobLock=SRWLOCK_INIT;
void Reset(uint64_t epoch) noexcept {serverGeneration=epoch;memset(visited,0,sizeof(visited));memset(records,0,sizeof(records));recordCount=0;validTables=false;Affixes::Reset();Placement::Reset();}
void OnGameThread(const PluginContext*,void* data) noexcept {
 auto* j=static_cast<Job*>(data);AcquireSRWLockExclusive(&jobLock);const bool claimed=InterlockedCompareExchange(&j->state,2,1)==1;ReleaseSRWLockExclusive(&jobLock);if(!claimed)return;if(j->epoch!=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0))){InterlockedExchange(&j->state,0);return;}if(serverGeneration!=j->epoch)Reset(j->epoch);
 SpawnRequest r={};uintptr_t player=0,path=0,game=0,room=0;uint8_t difficulty=255,bank=0;const uintptr_t client=reinterpret_cast<uintptr_t>(j->client);
 if(!cfg.enabled||!IsDungeon(j->level)||!Read(client+0x2b0,game)||!game||!Read(client+0x278,player)||!player||getLevel(reinterpret_cast<void*>(player))!=static_cast<int32_t>(j->level)||!Read(game+0x104,difficulty)||difficulty!=2||!Read(game+0x106,bank)||bank<2||bank>3||!Read(player+0x38,path)||!path||!Read(path+0x20,room)||!room){InterlockedExchange(&j->state,0);return;}
 uintptr_t room2=0,parentLevel=0;uint32_t actualLevel=0;if(!Placement::RoomLevel(reinterpret_cast<void*>(room),room2,parentLevel,actualLevel)||actualLevel!=j->level){InterlockedExchange(&j->state,0);return;}r.game=reinterpret_cast<void*>(game);r.room=reinterpret_cast<void*>(room);r.x=getX(reinterpret_cast<void*>(path));r.y=getY(reinterpret_cast<void*>(path));r.flags=3;r.mode=1;
 if(!validTables)validTables=Validate(bank);if(!validTables||!Placement::Install()||!InstallDeath()||!Affixes::Bind()){InterlockedExchange(&j->state,0);return;}
 if(!visited[j->level]){uint32_t options[20]={},total=0,count=0;
  for(uint32_t i=0;i<20;++i){bool area=false;for(uint16_t a:TLData::Profiles[i].areas)if(a==j->level)area=true;if(!area||!bosses[i].enabled||!bosses[i].weight||!Assets::masks[i]||(cfg.testBoss&&cfg.testBoss!=i+1))continue;options[count++]=i;total+=bosses[i].weight;}
  if(count){visited[j->level]=true;const bool selected=Rand(100)<cfg.chance;if(selected){uint32_t draw=Rand(total),chosen=options[0];for(uint32_t i=0;i<count;++i){const uint32_t p=options[i];if(draw<bosses[p].weight){chosen=p;break;}draw-=bosses[p].weight;}uint32_t variants[16]={},n=0;for(uint32_t v=0;v<16;++v)if(Assets::masks[chosen]&(1u<<v))variants[n++]=v;Placement::Plan(r,j->level,chosen,variants[Rand(n)]);}if(cfg.logRolls||selected){Text t;t.Add("TL_ZONE_ROLL level=");t.Num(j->level);t.Add(" chance_percent=");t.Num(cfg.chance);t.Add(" eligible_profiles=");t.Num(count);t.Add(" selected=");t.Num(selected);t.Add(" once_per_host_game=1");Log(t.s);}}
 }
 Placement::Poll(r);InterlockedExchange(&j->state,0);
}
ConsoleCommandResult Dispatch(D2R::Game::Client* client,const ConsoleCommandContext* c,void*) noexcept {if(!client||!c||c->contextSize<ConsoleCommandContextSize||!c->args||!c->argsLength||c->argsLength>4)return ConsoleCommandResult::InvalidArguments;uint32_t level=0;for(uint32_t i=0;i<c->argsLength;++i){if(c->args[i]<'0'||c->args[i]>'9')return ConsoleCommandResult::InvalidArguments;level=level*10+c->args[i]-'0';}if(!level||level>=1024)return ConsoleCommandResult::InvalidArguments;const uint64_t epoch=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0));if(!epoch||!cfg.enabled)return ConsoleCommandResult::Handled;
 AcquireSRWLockExclusive(&jobLock);Job* selected=nullptr;for(auto& j:jobs)if(j.state==1&&j.client==client&&j.epoch==epoch){j.level=level;ReleaseSRWLockExclusive(&jobLock);return ConsoleCommandResult::Handled;}for(auto& j:jobs)if(InterlockedCompareExchange(&j.state,3,0)==0){j.client=client;j.epoch=epoch;j.level=level;InterlockedExchange(&j.state,1);selected=&j;break;}ReleaseSRWLockExclusive(&jobLock);
 if(selected&&threads->runOnGameThread(ctx,OnGameThread,selected)!=Threads::Result::Success)InterlockedExchange(&selected->state,0);return ConsoleCommandResult::Handled;
}
void OnGameplay(const PluginContext*,const Lifecycle::GameplayEvent* e,void*) noexcept {if(!e||e->structSize<Lifecycle::GameplayEventSize)return;if(e->kind==Lifecycle::GameplayEventKind::GameLeft){InterlockedExchange64(&expectedGeneration,0);InterlockedExchange(&uiLevel,0);InterlockedExchange(&shakeLevel,0);return;}InterlockedExchange64(&expectedGeneration,e->sessionGeneration);if(e->kind==Lifecycle::GameplayEventKind::LevelChanged&&e->currentValue>0&&e->currentValue<1024){InterlockedExchange(&uiLevel,e->currentValue);if(IsDungeon(e->currentValue)){Text t;t.Add("r3ni_legends_lan_v1 ");t.Num(e->currentValue);execute(t.s);}nextPoll=GetTickCount64()+2000;}}
void OnFrame(const PluginContext*,const Overlay::Frame* f,void*) noexcept {
 if(!f||f->structSize<Overlay::FrameSize||f->screenWidth<=0||f->screenHeight<=0||!ctx||!ctx->exeBase)return;
 if(InterlockedCompareExchange(&modelsPending,0,1)==1){
  InterlockedExchange(&modelsStatus,1);Visuals::archiveReady=true;Text t;t.Add("DL_MODELS_BEGIN post_tables_revision=");t.Num(modelsRevision);t.Add(" phase=AfterGameUi");Log(t.s);
  const bool ready=Assets::Prepare();InterlockedExchange(&modelsStatus,ready?2:-1);Log(ready?"DL_MODELS_END ready=1":"DL_MODELS_END ready=0; inspect TL_MODEL messages",!ready);
 }
 if(!restaged){restaged=true;Addon::Stage();}const uint64_t now=GetTickCount64();const LONG level=InterlockedCompareExchange(&uiLevel,0,0),requested=InterlockedExchange(&shakeLevel,0);
 if(requested&&requested==level&&cfg.shake&&InterlockedCompareExchange64(&expectedGeneration,0,0)){const bool accepted=execute("camerashakestart 1");shaking=accepted;shakeEnd=now+cfg.shakeMs;Log(accepted?"TL_CAMERA_SHAKE_BEGIN native_command_accepted=1":"TL_CAMERA_SHAKE_REJECT native command unavailable",!accepted);}
 if(shaking&&(now>=shakeEnd||!InterlockedCompareExchange64(&expectedGeneration,0,0))){execute("camerashakestart 0");shaking=false;}
 if(network&&channel&&now>=nextNetwork){nextNetwork=now+2000;network->connectToHost(ctx,channel);}
 if(cfg.enabled&&IsDungeon(level)&&InterlockedCompareExchange64(&expectedGeneration,0,0)&&now>=nextPoll){nextPoll=now+2000;Text t;t.Add("r3ni_legends_lan_v1 ");t.Num(level);execute(t.s);}
}
void OnTables(const PluginContext*,const Lifecycle::DataTablesLoadedEvent* e,void*) noexcept {
 if(!e||e->structSize<Lifecycle::DataTablesLoadedEventSize||!e->revision||!ctx||!ctx->exeBase||!coreBase){Log("DL_TABLES_EVENT_SKIP: native archive access remains disabled.");return;}
 validTables=false;Assets::ready=false;Visuals::archiveReady=false;InterlockedExchange(&modelsStatus,0);modelsRevision=e->revision;InterlockedExchange(&modelsPending,1);Log("DL_MODELS_QUEUED: tables loaded; native archive access waits for AfterGameUi.");
}
void OnDeath(const PluginContext*,const Lifecycle::MonsterDeathEvent* e,void*) noexcept {if(!e||e->structSize<Lifecycle::MonsterDeathEventSize||e->monster.classId<1779||e->monster.classId>=2099)return;Text t;t.Add("TL_DEATH uid=");t.Num(e->monster.id);t.Add(" class=");t.Num(e->monster.classId);t.Add(" level=");t.Num(e->levelId);Log(t.s);}
const PluginInfo info={sizeof(PluginInfo),4,"r3ni-dungeon-legends","Dungeon Legends","0.1.1-beta","r3ni","Twenty Dungeon Map bosses with four native affixes, three fixed EnemySkills abilities and native loot.",PluginFlags::ModScopedOnly|PluginFlags::NativeHooks|PluginFlags::Shared,{}};
}
extern "C" __declspec(dllexport) bool R3niLegendsUnlinkPopulation(void* old,void* replacement) noexcept {using namespace TL::Placement;if(reinterpret_cast<void*>(priorPopulation)!=old)return false;priorPopulation=reinterpret_cast<PopulateFn>(replacement);return true;}
extern "C" __declspec(dllexport) bool R3niLegendsUnlinkAllocation(void* old,void* replacement) noexcept {using namespace TL::Placement;if(reinterpret_cast<void*>(priorAllocation)!=old)return false;priorAllocation=reinterpret_cast<TL::AllocateFn>(replacement);if(reinterpret_cast<void*>(TL::allocateOne)==old)TL::allocateOne=priorAllocation;return true;}
extern "C" __declspec(dllexport) bool R3niLegendsUnlinkDeath(void* old,void* replacement) noexcept {if(reinterpret_cast<void*>(TL::priorDeath)!=old)return false;TL::priorDeath=reinterpret_cast<TL::DeathFn>(replacement);return true;}
extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &TL::info;}
extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext* c) noexcept {
 using namespace TL;if(!D2RL::HasContext(c)||!c->GetApi())return false;ctx=c;Visuals::archiveReady=false;Assets::ready=false;InterlockedExchange(&modelsPending,0);InterlockedExchange(&modelsStatus,0);void* storage=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(TerrorEncounters::RuntimeLog));if(storage)logfile=new(storage) TerrorEncounters::RuntimeLog(module);Log("Dungeon Legends 0.1.1-beta author=r3ni");
 if(!Equal(ctx->activeMod,"VanillaDeluxe")||ctx->loadScope!=LoadScope::Mod)return false;if(!LoadConfig()){Log("DL_CONFIG_REJECT: DungeonLegends.ini missing or out of range; special chances must sum to <=100%.",true);return false;}Log("DL_SOURCE_STAGE_BEGIN");if(!Addon::Stage())return false;Log("DL_SOURCE_STAGE_END");if(!ctx->exeBase){Log("TL_COMPILER_SOURCE_READY");return true;}
 Log("DL_SERVICES_BEGIN");if(ctx->QueryService(&lifecycle)!=ServiceQueryResult::Success||!lifecycle||lifecycle->serviceVersion!=LifecycleService::AbiVersion||lifecycle->serviceSize<LifecycleServiceSize||!lifecycle->registerGameplayEventListener||!lifecycle->registerDataTablesLoadedListener||!lifecycle->registerMonsterDeathListener||ctx->QueryService(&threads)!=ServiceQueryResult::Success||!threads||threads->serviceVersion!=ThreadService::AbiVersion||threads->serviceSize<ThreadServiceSize||!threads->runOnGameThread||ctx->QueryService(&tables)!=ServiceQueryResult::Success||!tables||tables->serviceVersion!=DataTableService::AbiVersion||tables->serviceSize<DataTableServiceSize||!tables->getTable||!tables->getRow||ctx->QueryService(&encounters)!=ServiceQueryResult::Success||!encounters||encounters->serviceVersion!=EncounterService::AbiVersion||encounters->serviceSize<EncounterServiceSize||!encounters->runTestSpawn)return false;
 Log("DL_SERVICES_READY");HMODULE core=GetModuleHandleW(L"D2RCore.dll");if(!core||!NativeContract(core)){Log("TL_CORE_REJECT: supplied spawn/stat bindings differ.",true);return false;}Log("DL_NATIVE_BIND_READY: no game/archive functions executed during plugin initialization.");
 Log("DL_CALLBACKS_BEGIN");const auto command=MakeConsoleCommand("r3ni_legends_lan_v1",Dispatch,"Internal host Dungeon Map entry notification.");auto cmd=command;cmd.flags=ConsoleCommandHidden|ConsoleCommandSuppressEcho;cmd.scope=ConsoleCommandScope::RemoteAllowed;cmd.access=ConsoleCommandAccess::Any;if(!ctx->RegisterConsoleCommand(cmd))return false;
 Lifecycle::GameplayEventKind kinds[]={Lifecycle::GameplayEventKind::GameJoined,Lifecycle::GameplayEventKind::GameLeft,Lifecycle::GameplayEventKind::LevelChanged};for(uint32_t i=0;i<3;++i){Lifecycle::GameplayEventListener l={.structSize=Lifecycle::GameplayEventListenerSize,.kind=kinds[i],.callback=OnGameplay};if(lifecycle->registerGameplayEventListener(ctx,&l,&handles[i])!=Lifecycle::Result::Success)return false;}
 Lifecycle::DataTablesLoadedListener dl={.structSize=Lifecycle::DataTablesLoadedListenerSize,.callback=OnTables};if(lifecycle->registerDataTablesLoadedListener(ctx,&dl,&handles[3])!=Lifecycle::Result::Success)return false;Lifecycle::MonsterDeathListener death={.structSize=Lifecycle::MonsterDeathListenerSize,.callback=OnDeath};if(lifecycle->registerMonsterDeathListener(ctx,&death,&handles[4])!=Lifecycle::Result::Success)return false;
 if(ctx->QueryService(&overlay)!=ServiceQueryResult::Success||!overlay||overlay->serviceVersion!=OverlayService::AbiVersion||overlay->serviceSize<OverlayServiceSize||!overlay->registerFrameCallback)return false;Overlay::CallbackRegistration frame={.structSize=Overlay::CallbackRegistrationSize,.phase=Overlay::Phase::AfterGameUi,.callback=OnFrame};if(overlay->registerFrameCallback(ctx,&frame,&frameHandle)!=Overlay::Result::Success)return false;
 if(ctx->QueryService(&network)==ServiceQueryResult::Success&&network&&network->serviceVersion==NetworkService::AbiVersion&&network->serviceSize>=NetworkServiceSize&&network->registerChannel&&network->connectToHost&&network->sendToClient){Network::ChannelRegistration reg={.structSize=Network::ChannelRegistrationSize,.localChannelId=1,.compatibilityToken=0x72336e694c470100ULL,.clientMessage=OnClientMessage,.connectionState=OnConnection};if(network->registerChannel(ctx,&reg,&channel)!=Network::Result::Success)channel=0;}
 Log("DL_CALLBACKS_READY");LARGE_INTEGER clock={};QueryPerformanceCounter(&clock);rng=clock.QuadPart^GetTickCount64()^reinterpret_cast<uintptr_t>(module);Log("TL_READY: Hell VanillaDeluxe Dungeon Maps only; 100% default, one boss per combat floor; no entourage; fixed skills require EnemySkills 0.2.0; compile then restart after first install.");return true;
}
extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept {using namespace TL;Visuals::archiveReady=false;InterlockedExchange(&modelsPending,0);InterlockedExchange(&modelsStatus,0);InterlockedExchange64(&expectedGeneration,0);if(shaking&&execute)execute("camerashakestart 0");shaking=false;if(ctx&&overlay&&frameHandle)overlay->unregisterFrameCallback(ctx,frameHandle);if(ctx&&network&&channel)network->unregisterChannel(ctx,channel);Placement::Remove();if(deathSlot&&priorDeath&&InterlockedCompareExchangePointer(deathSlot,reinterpret_cast<void*>(priorDeath),reinterpret_cast<void*>(Death))!=reinterpret_cast<void*>(Death))Placement::UnlinkKnown(L"TerrorEncounters.dll","R3niTerrorUnlinkDeath",reinterpret_cast<void*>(Death),reinterpret_cast<void*>(priorDeath));if(ctx&&communication&&skillService.handle)communication->releaseService(ctx,skillService.handle);if(ctx&&lifecycle){for(uint32_t i=0;i<3;++i)if(handles[i])lifecycle->unregisterGameplayEventListener(ctx,handles[i]);if(handles[3])lifecycle->unregisterDataTablesLoadedListener(ctx,handles[3]);if(handles[4])lifecycle->unregisterMonsterDeathListener(ctx,handles[4]);}if(logfile){logfile->~RuntimeLog();HeapFree(GetProcessHeap(),0,logfile);logfile=nullptr;}ctx=nullptr;}
extern "C" BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)TL::module=h;return TRUE;}
