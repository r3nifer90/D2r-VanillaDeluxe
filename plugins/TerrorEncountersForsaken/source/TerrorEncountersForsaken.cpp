#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/core_exports.h>
#include <D2RLPlugin/overlay.h>
#include "MinHook.h"
#include "policy.h"
extern "C" void* memset(void* p,int v,size_t n){auto*b=(volatile unsigned char*)p;while(n--)*b++=(unsigned char)v;return p;}
extern "C" void* memcpy(void* p,const void* s,size_t n){auto*b=(volatile unsigned char*)p;auto*a=(const volatile unsigned char*)s;while(n--)*b++=*a++;return p;}
namespace CF {
using namespace D2RL;
const PluginContext* ctx=nullptr;uintptr_t base=0;Forsaken::Config cfg;
using Worker=void(__fastcall*)(const PluginContext*,void*);
Worker originalWorker=nullptr;
const OverlayService* overlay=nullptr;Overlay::CallbackHandle frameHandle=0;
bool workerHook=false,mhOwned=false;
uint64_t rng=0,generation=0;void* game=nullptr;void* ownerClient=nullptr;uintptr_t ownerPlayer=0;uint32_t ownerId=0;bool rolled=false,pending=false;uint32_t selected=0,charm=0;int32_t entryX=0,entryY=0;uint64_t lastTry=0,lastReport=0,lastUi=0;
volatile LONG wake=0;volatile LONG64 wakeGeneration=0;
struct Request {void*game;const void*room;void*origin;int32_t monster;uint32_t mode,unused;int32_t x,y;uint32_t flags;uint64_t extra1,extra2;};static_assert(sizeof(Request)==64);
struct Job {volatile LONG state;void*client;uint64_t generation;int32_t level;bool poll;};static_assert(offsetof(Job,poll)==28);
struct Balance {const wchar_t* section;const char*name;uint32_t health,damage,size,reserved;};
template<class T>T& At(uint32_t rva){return *reinterpret_cast<T*>(base+rva);}
template<class T>T Fn(uint32_t rva){return reinterpret_cast<T>(base+rva);}
bool Readable(uintptr_t p,size_t n){if(!p||p+n<p)return false;auto end=p+n;while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(m.Protect&255))return false;uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=p)return false;p=next;}return true;}
template<class T>bool Read(uintptr_t p,T& v){if(!Readable(p,sizeof(T)))return false;memcpy(&v,(void*)p,sizeof(T));return true;}
void Log(const char* s,bool err=false){if(base&&At<uintptr_t>(0x582e0))Fn<void(*)(const char*,bool)>(0x11c0)(s,err);else if(ctx){if(err)ctx->LogError(s);else ctx->LogInfo(s);}}
struct Text {char s[320]={};unsigned n=0;void Add(const char*t){while(*t&&n<318)s[n++]=*t++;s[n]=0;}void Num(uint64_t v){char b[24];unsigned k=0;do{b[k++]=char('0'+v%10);v/=10;}while(v);while(k&&n<318)s[n++]=b[--k];s[n]=0;}};
uint32_t Random(uint32_t bound){uint64_t v,limit=(uint64_t(0)-bound)%bound;do{rng+=0x9e3779b97f4a7c15ULL;v=rng;v=(v^(v>>30))*0xbf58476d1ce4e5b9ULL;v=(v^(v>>27))*0x94d049bb133111ebULL;v^=v>>31;}while(v<limit);return uint32_t(v%bound);}
bool Roll(uint32_t p){return Random(100)<p;}
bool Context(Job&j,Request&r,uint32_t&bank,uint32_t&diff){uintptr_t g=0,p=0,path=0,room=0;uint32_t type=99;uint8_t b=0,d=255;
 if(!Read((uintptr_t)j.client+0x2b0,g)||!g||!Read((uintptr_t)j.client+0x278,p)||!p||!Read(p,type)||type!=0||!Read(p+0x38,path)||!path||!Readable(p,0x100)||!Readable(path,0x100))return false;
 auto getLevel=At<int32_t(__fastcall*)(const void*)>(0x582a0);auto getX=At<uint16_t(__fastcall*)(const void*)>(0x582a8);auto getY=At<uint16_t(__fastcall*)(const void*)>(0x582b0);
 if(!getLevel||!getX||!getY||getLevel((void*)p)!=161||!Read(g+0x104,d)||d>2||!Read(g+0x106,b)||b<1||b>3||!Read(path+0x20,room)||!room)return false;
 r.game=(void*)g;r.room=(void*)room;r.mode=1;r.flags=3;r.x=getX((void*)path);r.y=getY((void*)path);bank=b;diff=d;return true;
}
bool Ready(uint32_t bank){if(!At<uintptr_t>(0x58b40)&&!Fn<bool(*)()>(0xdf10)())return false;if(!At<bool>(0x58700)){At<bool>(0x58700)=Fn<bool(*)(uint32_t)>(0x3660)(bank);if(!At<bool>(0x58700))return false;}return true;}
void Reset(uint64_t gen,void*g,int32_t x,int32_t y){generation=gen;game=g;rolled=false;pending=false;ownerClient=nullptr;ownerPlayer=0;ownerId=0;entryX=x;entryY=y;lastTry=lastReport=0;InterlockedExchange(&wake,0);InterlockedExchange64(&wakeGeneration,gen);}
void TryPlace(const Request& r){
 if(!pending)return;auto now=GetTickCount64();if(lastTry&&now-lastTry<500)return;lastTry=now;
 uintptr_t room2=0,level=0,head=0;uint32_t id=0;
 if(!Read((uintptr_t)r.room+0x18,room2)||!room2||!Read(room2+0x90,level)||!level||!Read(level+0x1f8,id)||id!=161||!Read(level+0x10,head)||!head)return;
 uint32_t offset=0,matches=0;for(uint32_t o=0;o<=0x58;o+=8){uintptr_t a=0;if(Read(room2+o,a)&&a==(uintptr_t)r.room){offset=o;++matches;}}if(matches!=1)return;


 uintptr_t rooms[256]={};int32_t boxes[256][4]={};uint32_t count=0,walk=0;uintptr_t cur=head;
 for(;cur&&walk<2048;++walk){uintptr_t parent=0,next=0,active=0,back=0;int32_t x=0,y=0,w=0,h=0;
  if(!Read(cur+0x90,parent)||parent!=level||!Read(cur+0x48,next)||next==cur||!Read(cur+0x60,x)||!Read(cur+0x64,y)||!Read(cur+0x68,w)||!Read(cur+0x6c,h)||x<0||y<0||w<1||h<1||x>13107||y>13107||w>13107-x||h>13107-y)return;
  if(count<256&&w*5>8&&h*5>8&&Read(cur+offset,active)&&active&&Read(active+0x18,back)&&back==cur){rooms[count]=active;boxes[count][0]=x*5;boxes[count][1]=y*5;boxes[count][2]=w*5;boxes[count][3]=h*5;++count;}cur=next;
 }if(cur)return;
 uint32_t attempts=0;
 for(uint32_t k=0;k<256&&attempts<16&&count;++k){uint32_t n=Random(count);auto*b=boxes[n];int32_t x=b[0]+4+int32_t(Random(b[2]-8)),y=b[1]+4+int32_t(Random(b[3]-8));
  if(!Forsaken::InRange(cfg,x,y,r.x,r.y)||Forsaken::DistanceSquared(x,y,entryX,entryY)<uint64_t(cfg.minDistance)*cfg.minDistance)continue;
  Request candidate=r;candidate.room=(void*)rooms[n];candidate.x=x;candidate.y=y;candidate.monster=selected;bool confirmed=false;++attempts;
  uint32_t bi=selected<=802?(selected-793)/2:selected<=804?5:selected==805?6:selected==806?7:selected<=808?8:9+(selected-809)/2;
  auto&balance=At<Balance>(0x58040+bi*32);
  auto result=Fn<uint32_t(*)(const Request&,uint32_t,const Balance&,bool&,int32_t)>(0x7eb0)(candidate,selected,balance,confirmed,int32_t(charm));
  if(result){pending=false;InterlockedExchange(&wake,0);Text t;t.Add("CF_THRONE_SPAWN level=161 class=");t.Num(selected);t.Add(" confirmed=");t.Num(confirmed);t.Add(" x=");t.Num(x);t.Add(" y=");t.Num(y);t.Add(" distance_squared=");t.Num(Forsaken::DistanceSquared(x,y,r.x,r.y));t.Add(" entry_distance_squared=");t.Num(Forsaken::DistanceSquared(x,y,entryX,entryY));Log(t.s,!confirmed);
   if(confirmed)Fn<void(*)(void*,uint32_t)>(0x5880)(r.game,selected);return;
  }
 }
 if(!lastReport||now-lastReport>=10000){lastReport=now;Text t;t.Add("CF_THRONE_DEFER level=161 class=");t.Num(selected);t.Add(" loaded_rooms=");t.Num(count);t.Add(" native_attempts=");t.Num(attempts);t.Add(" min_distance=");t.Num(cfg.minDistance);t.Add(" max_distance=");t.Num(cfg.maxDistance);t.Add(" retry=1 reroll=0 entry_fallback=0");Log(t.s);}
}
void __fastcall Work(const PluginContext*c,void*data){auto*j=(Job*)data;if(!j||j->level!=161){originalWorker(c,data);return;}
 if(InterlockedCompareExchange(&j->state,2,1)!=1)return;
 auto finish=[&](){InterlockedExchange(&j->state,0);};
 if(!base||!At<uintptr_t>(0x582e0)||!cfg.enabled||!At<uint32_t>(0x58000)||!j->generation||j->generation!=uint64_t(InterlockedCompareExchange64(&At<LONG64>(0x582e8),0,0))){finish();return;}


 if(At<uint64_t>(0x582f0)!=j->generation){Job init={1,j->client,j->generation,0,false};originalWorker(c,&init);if(At<uint64_t>(0x582f0)!=j->generation){finish();return;}}
 Request r={};uint32_t bank=0,diff=0;bool have=Context(*j,r,bank,diff);


 if(!have&&j->poll&&pending&&generation==j->generation&&ownerClient){uintptr_t p=0,g=0;uint32_t id=0;
  if(Read((uintptr_t)ownerClient+0x278,p)&&p==ownerPlayer&&Read(p+8,id)&&id==ownerId&&Read((uintptr_t)ownerClient+0x2b0,g)&&g==(uintptr_t)game){Job retry={1,ownerClient,j->generation,161,true};have=Context(retry,r,bank,diff);}
 }
 if(!have||diff<At<uint32_t>(0x58024)||diff>At<uint32_t>(0x58028)){finish();return;}
 if(generation!=j->generation||game!=r.game)Reset(j->generation,r.game,r.x,r.y);
 if(!j->poll){ownerClient=j->client;Read((uintptr_t)j->client+0x278,ownerPlayer);Read(ownerPlayer+8,ownerId);}
 if(!rolled&&!j->poll){if(!Ready(bank)){Log("CF_THRONE_SKIP native_loot_or_tables_unavailable=1",true);finish();return;}
  rolled=true;At<bool>(0x58300+161)=true;uint32_t roll=Random(100),kind=Forsaken::Select(cfg,roll);
  if(kind==1)selected=793+Random(5)*2+uint32_t(Roll(At<uint32_t>(0x58014)));
  if(kind==2){bool tristram=Roll(At<uint32_t>(0x58010));uint32_t member=Random(3);bool reward=tristram?(member==2&&Roll(At<uint32_t>(0x5801c))):Roll(At<uint32_t>(0x58020));selected=tristram?(member==2?807+uint32_t(reward):805+member):809+member*2+uint32_t(reward);}
  pending=kind!=0;if(pending){charm=Roll(At<uint32_t>(0x58034));InterlockedExchange64(&wakeGeneration,j->generation);InterlockedExchange(&wake,1);}
  Text t;t.Add("CF_THRONE_ROLL level=161 roll=");t.Num(roll);t.Add(" act=");t.Num(kind==1);t.Add(" uber=");t.Num(kind==2);t.Add(" class=");t.Num(pending?selected:0);t.Add(" min_distance=");t.Num(cfg.minDistance);t.Add(" max_distance=");t.Num(cfg.maxDistance);t.Add(" one_roll_per_game=1");Log(t.s);
 }
 if(pending)TryPlace(r);finish();
}
void UI(const PluginContext*,const Overlay::Frame*,void*) noexcept{if(!InterlockedCompareExchange(&wake,0,0))return;auto gen=uint64_t(InterlockedCompareExchange64(&At<LONG64>(0x582e8),0,0));if(!gen||gen!=uint64_t(InterlockedCompareExchange64(&wakeGeneration,0,0)))return;
 auto now=GetTickCount64();if(lastUi&&now-lastUi<750)return;lastUi=now;auto execute=At<CoreExports::ExecuteConsoleCommandFn>(0x58298);if(execute)execute("vdterror_poll_v1 161");
}
bool Config(){wchar_t path[1024]={};if(!ctx->pluginDirectory)return false;uint32_t i=0;while(ctx->pluginDirectory[i]&&i<950){path[i]=ctx->pluginDirectory[i];++i;}if(i==950)return false;constexpr wchar_t tail[]=L"\\TerrorEncounters.ini";for(uint32_t j=0;j<sizeof(tail)/2;++j)path[i+j]=tail[j];if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return false;
 struct Entry{const wchar_t*key;uint32_t*value;const wchar_t*fallback;};Entry e[]={{L"enabled",&cfg.enabled,L"1"},{L"act_boss_chance_percent",&cfg.act,L"20"},{L"uber_boss_chance_percent",&cfg.uber,L"10"},{L"minimum_spawn_distance",&cfg.minDistance,L"100"},{L"maximum_spawn_distance",&cfg.maxDistance,L"150"}};
 for(auto&x:e){wchar_t v[32]={};if(!GetPrivateProfileStringW(L"corrupted_forsaken_throne",x.key,x.fallback,v,32,path))return false;uint32_t n=0,k=0;for(;v[k];++k){if(k>=4||v[k]<'0'||v[k]>'9')return false;n=n*10+v[k]-L'0';}*x.value=n;}return Forsaken::Valid(cfg);
}
bool Contract();
void Remove(){if(frameHandle&&overlay){overlay->unregisterFrameCallback(ctx,frameHandle);frameHandle=0;}if(workerHook){MH_DisableHook((void*)(base+0x163c0));MH_RemoveHook((void*)(base+0x163c0));workerHook=false;}if(mhOwned){MH_Uninitialize();mhOwned=false;}InterlockedExchange(&wake,0);}
const PluginInfo info={sizeof(PluginInfo),4,"r3ni-terror-forsaken","Terror Encounters - Corrupted Forsaken Throne","0.1.0","r3ni","Exclusive act/uber encounters with distant native placement.",PluginFlags::ModScopedOnly|PluginFlags::NativeHooks|PluginFlags::Shared,{}};
}
#include "contract.h"
extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &CF::info;}
extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext*c) noexcept{using namespace CF;if(!D2RL::HasContext(c)||!c->GetApi()||c->loadScope!=D2RL::LoadScope::Mod)return false;ctx=c;const char*m=c->activeMod;const char*expected="VanillaDeluxe";if(!m)return false;while(*m&&*m==*expected){++m;++expected;}if(*m||*expected)return false;
 if(!Config()){Log("CF_THRONE_CONFIG_REJECT: chances must sum to <=100; distance range 16..500.",true);return false;}
 base=(uintptr_t)GetModuleHandleW(L"TerrorEncounters.dll");if(!base||!Contract()){base=0;Log("CF_THRONE_CONTRACT_REJECT: requires supplied TerrorEncounters 0.3.15-duriel-speed.",true);return false;}
 auto result=MH_Initialize();if(result!=MH_OK&&result!=MH_ERROR_ALREADY_INITIALIZED)return false;mhOwned=result==MH_OK;
 if(MH_CreateHook((void*)(base+0x163c0),(void*)Work,(void**)&originalWorker)!=MH_OK){Remove();return false;}workerHook=true;
 if(ctx->QueryService(&overlay)!=ServiceQueryResult::Success||!overlay||overlay->serviceSize<OverlayServiceSize||overlay->serviceVersion!=OverlayService::AbiVersion||!overlay->registerFrameCallback||!overlay->unregisterFrameCallback){Remove();return false;}
 Overlay::CallbackRegistration registration={sizeof(Overlay::CallbackRegistration),0,Overlay::Phase::AfterGameUi,0,UI,nullptr};
 if(overlay->registerFrameCallback(ctx,&registration,&frameHandle)!=Overlay::Result::Success||!frameHandle){Remove();return false;}
 if(MH_EnableHook((void*)(base+0x163c0))!=MH_OK){Remove();return false;}
 LARGE_INTEGER q={};QueryPerformanceCounter(&q);rng=uint64_t(q.QuadPart)^GetTickCount64()^((uint64_t)GetCurrentProcessId()<<32)^(uintptr_t)c;
 Log("CF_THRONE_READY version=0.1.0 level=161 original_TZ_rules_preserved=1");return true;}
extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept{CF::Remove();CF::ctx=nullptr;}
extern "C" BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
