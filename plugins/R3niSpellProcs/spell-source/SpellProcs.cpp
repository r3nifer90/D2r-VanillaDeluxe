#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/lifecycle_events.h>
#include "sites.h"
#include "wide_event_contract.hpp"

extern "C" void* memset(void* p,int v,size_t n){auto* b=static_cast<volatile unsigned char*>(p);while(n--)*b++=static_cast<unsigned char>(v);return p;}
extern "C" void* memcpy(void* p,const void* q,size_t n){auto* b=static_cast<volatile unsigned char*>(p);auto* a=static_cast<const volatile unsigned char*>(q);while(n--)*b++=*a++;return p;}
extern "C" int memcmp(const void* p,const void* q,size_t n){auto* a=static_cast<const unsigned char*>(p);auto* b=static_cast<const unsigned char*>(q);while(n--){if(*a!=*b)return *a<*b?-1:1;++a;++b;}return 0;}

#include "flight_recorder.h"
#include "proc_audit.h"
#include "wide_event_bridge.hpp"

namespace SP {
using namespace D2RL;
namespace Wide=ruffneckk::cast_triggers::wide_events;
const PluginContext* context=nullptr;
const LifecycleService* lifecycle=nullptr;
Lifecycle::ListenerHandle tablesHandle=0,gameHandles[2]={};
uintptr_t base=0,core=0;
DWORD tls=TLS_OUT_OF_INDEXES;
volatile LONG active=0,activation=0;
volatile LONG64 counters[9]={};
enum Count {ManualCasts,AttackDispatches,SpellHits,HitDispatches,ProcAttempts,ProcSuccess,Missiles,Suppressed,CapacitySkips};
volatile LONG64 recoveredRoots=0;
SRWLOCK cacheLock=SRWLOCK_INIT;

struct Text {
 char value[768]={};size_t n=0;
 void Add(const char* s){while(*s&&n<766)value[n++]=*s++;value[n]=0;}
 void Num(uint64_t x){char b[24];int i=0;do{b[i++]=static_cast<char>('0'+x%10);x/=10;}while(x);while(i&&n<766)value[n++]=b[--i];value[n]=0;}
};
void Inc(Count c) noexcept {InterlockedIncrement64(&counters[c]);}
void Log(const char* s,bool error=false) noexcept {SPD::Line(s);if(context){if(error)context->LogError(s);else context->LogInfo(s);}}
bool Running() noexcept {return InterlockedCompareExchange(&active,0,0)!=0;}
void BindCombatObserver() noexcept {
 SPD::PreserveError preserve;if(SPD::externalSink)return;
 HMODULE h=GetModuleHandleW(L"R3niCombatDiag.dll");if(!h)return;
 auto get=reinterpret_cast<R3niDiagnostics::GetObserver>(GetProcAddress(h,"R3niCombatDiagGetObserverV1"));
 const R3niDiagnostics::Observer* p=get?get():nullptr;R3niDiagnostics::Observer a={};
 if(!p||!SPD::Copy(reinterpret_cast<uintptr_t>(p),&a,sizeof(a))||a.size!=sizeof(a)||a.abi!=1||a.recordBytes!=128||a.reserved||!a.sink){Log("SP_COMBAT_OBSERVER_UNAVAILABLE: use Combat Diagnostics0.1.1-cooperative.",true);return;}
 HMODULE held=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(a.sink),&held)||held!=h){if(held)FreeLibrary(held);Log("SP_COMBAT_OBSERVER_UNAVAILABLE: module reference failed.",true);return;}
 SPD::observerModule=held;SPD::externalSink=a.sink;Log("SP_COMBAT_OBSERVER_BOUND read_only=1 shared_native_hooks=5 native_calls_duplicated=0");
}

bool Readable(uintptr_t p,size_t n) noexcept {
 if(!p||p+n<p)return false;const uintptr_t end=p+n;
 while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))||!(m.Protect&0xff))return false;const auto next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;}return true;
}
template<class T> bool Read(const void* p,size_t offset,T& result) noexcept {auto a=reinterpret_cast<uintptr_t>(p)+offset;if(!Readable(a,sizeof(T)))return false;memcpy(&result,reinterpret_cast<void*>(a),sizeof(T));return true;}
bool Equal(const char* a,const char* b) noexcept {if(!a||!b)return false;while(*a&&*b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}return *a==*b;}
template<size_t N> bool Match(uintptr_t at,const uint8_t (&bytes)[N]) noexcept {return Readable(at,N)&&memcmp(reinterpret_cast<void*>(at),bytes,N)==0;}
bool Player(void* p,uint32_t* id=nullptr) noexcept {uint32_t type=99,n=0;if(!Read(p,0,type)||type!=0||!Read(p,8,n))return false;if(id)*id=n;return true;}
bool Automatic(void* p) noexcept {uint32_t f=0;return !Read(p,0x128,f)||(f&0x10000000u)!=0;}
uint32_t Frame(void* game) noexcept {uint32_t f=0;Read(game,0x170,f);return f;}

using SkillFn=int32_t(__fastcall*)(void*,void*,int32_t,int32_t,int32_t,int32_t,int32_t) noexcept;
using CreateFn=void*(__fastcall*)(void*,void*) noexcept;
using DoFn=int32_t(__fastcall*)(void*,void*) noexcept;
using HitFn=int32_t(__fastcall*)(void*,void*,void*,int32_t) noexcept;
using ReportFn=void(__fastcall*)(void*,void*,void*,void*) noexcept;
using TargetFn=void*(__fastcall*)(void*,void*) noexcept;
using LookupFn=void*(__fastcall*)(void*,int32_t,int32_t) noexcept;
using RecordFn=const uint8_t*(__fastcall*)(uint8_t,int32_t) noexcept;
using DispatchFn=int32_t(__fastcall*)(void*,int32_t,void*,void*,void*,void*) noexcept;
using PositionFn=int32_t(__fastcall*)(void*,void*,int32_t,int32_t,int32_t,void*) noexcept;
using UnitFn=int32_t(__fastcall*)(void*,void*,int32_t,int32_t,void*,int32_t,int32_t) noexcept;
SkillFn originalSkill=nullptr;CreateFn originalCreate=nullptr;DoFn originalDo=nullptr;
HitFn originalHit=nullptr;ReportFn originalReport=nullptr;
PositionFn originalPosition=nullptr;UnitFn originalUnit=nullptr;
TargetFn getTarget=nullptr;LookupFn lookup=nullptr;RecordFn record=nullptr;DispatchFn dispatch=nullptr;
using UsedFn=void*(__fastcall*)(void*) noexcept;
UsedFn getUsed=nullptr;



struct Origin {void* game=nullptr;uint32_t owner=0,skill=0;bool manual=false;};
struct Invocation {Origin origin;int stat=0;Invocation* previous=nullptr;uint64_t diagnosticKey=0;};
Invocation* Current() noexcept {return tls==TLS_OUT_OF_INDEXES?nullptr:static_cast<Invocation*>(TlsGetValue(tls));}
struct Scope {
 Invocation invocation;
 Scope(Origin o,int stat=0):invocation{o,stat,Current()}{TlsSetValue(tls,&invocation);}
 ~Scope(){TlsSetValue(tls,invocation.previous);}
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};



constexpr uint32_t CacheSize=65536,ProbeLimit=128;
struct Projectile {uintptr_t game=0;uint32_t guid=0,kind=0,owner=0,skill=0;uint8_t state=0;};
Projectile projectiles[CacheSize];
uint32_t Hash(uintptr_t game,uint32_t guid) noexcept {uint64_t x=(game>>4)^(static_cast<uint64_t>(guid)*0x9e3779b97f4a7c15ULL);x^=x>>33;return static_cast<uint32_t>(x)&(CacheSize-1);}
bool Identity(void* unit,uint32_t& guid,uint32_t& kind) noexcept {uint32_t t=99;return Read(unit,0,t)&&t==3&&Read(unit,4,kind)&&Read(unit,8,guid);}
Origin FindKey(void* game,uint32_t guid,uint32_t kind,bool remove=false) noexcept {
 Origin result={};
 const auto g=reinterpret_cast<uintptr_t>(game);const auto start=Hash(g,guid);
 AcquireSRWLockExclusive(&cacheLock);
 for(uint32_t i=0;i<ProbeLimit;++i){auto& r=projectiles[(start+i)&(CacheSize-1)];if(!r.state)break;if(r.state==1&&r.game==g&&r.guid==guid&&r.kind==kind){result={game,r.owner,r.skill,true};if(remove)r.state=2;break;}}
 ReleaseSRWLockExclusive(&cacheLock);return result;
}
void Remember(void* game,void* missile,Origin origin) noexcept {
 uint32_t guid=0,kind=0;if(!Identity(missile,guid,kind))return;
 const auto g=reinterpret_cast<uintptr_t>(game);const auto start=Hash(g,guid);int32_t slot=-1;bool found=false;
 AcquireSRWLockExclusive(&cacheLock);
 for(uint32_t i=0;i<ProbeLimit;++i){const uint32_t pos=(start+i)&(CacheSize-1);auto& r=projectiles[pos];
  if(r.state==1&&r.game==g&&r.guid==guid){slot=static_cast<int32_t>(pos);found=true;break;}
  if(r.state!=1&&slot<0)slot=static_cast<int32_t>(pos);
  if(!r.state)break;
 }
 if(slot>=0){auto& r=projectiles[slot];if(origin.manual){r={g,guid,kind,origin.owner,origin.skill,1};}else if(found)r.state=2;}
 ReleaseSRWLockExclusive(&cacheLock);
 if(origin.manual){if(slot<0)Inc(CapacitySkips);else Inc(Missiles);}
}
struct CastTicket {uintptr_t game=0;uint32_t owner=0,skill=0,cast=0;bool used=false;};
CastTicket castTickets[1024];


bool CastOnce(void* game,void* player,uint32_t skill,bool rememberOnly=false) noexcept {
 uint32_t owner=0,cast=0;if(!Player(player,&owner)||!Read(player,0x12c,cast)||cast==0xffffffffu)return false;
 const uintptr_t g=reinterpret_cast<uintptr_t>(game);const uint32_t start=Hash(g,owner)&1023;
 bool first=false;AcquireSRWLockExclusive(&cacheLock);
 for(uint32_t n=0;n<1024;++n){auto& r=castTickets[(start+n)&1023];
  if(!r.used||(r.game==g&&r.owner==owner)){
   first=!r.used||r.skill!=skill||r.cast!=cast;
   r={g,owner,skill,cast,true};break;
  }
 }
 ReleaseSRWLockExclusive(&cacheLock);return rememberOnly||first;
}
void Reset() noexcept {AcquireSRWLockExclusive(&cacheLock);memset(projectiles,0,sizeof(projectiles));memset(castTickets,0,sizeof(castTickets));ReleaseSRWLockExclusive(&cacheLock);}

bool IsSpell(void* game,int32_t id,bool& repeating) noexcept {
 repeating=false;if(!game||!record||id<0||id>32767)return false;
 uint8_t bank=0;if(!Read(game,0x106,bank)||bank<2||bank>3)return false;
 const auto* r=record(bank,id);uint64_t flags=0;uint8_t anim=0,sequence=0;
 if(!r||!Read(r,0x24,flags)||!Read(r,0x30,anim)||!Read(r,0x32,sequence)||(flags&0x20))return false;
 repeating=(flags&(1ULL<<11))!=0;
 return anim==10||(anim==18&&(sequence==10||(repeating&&sequence==18)));
}




bool Filter(void* game,int event,void* owner,void* target,void*& damage,uint64_t key,Wide::Effect effect) {
 SPD::Span diagnostic(SPD::Filter,game,owner,target,damage,0,0,key);
 diagnostic.record.data=reinterpret_cast<uintptr_t>(effect);
 auto* invocation=Current();if(!Running()||!invocation||!invocation->stat){diagnostic.record.flags=1;return false;}
 diagnostic.record.level=static_cast<uint32_t>(invocation->stat);
 if(Wide::StatId(key)!=invocation->stat){diagnostic.record.flags=2;return false;}
 if(reinterpret_cast<uintptr_t>(effect)!=core+Wide::Effect20Rva){diagnostic.record.flags=3;return false;}
 invocation->diagnosticKey=key;damage=nullptr;Inc(ProcAttempts);diagnostic.Mark(SPD::Decision,1);return true;
}
void After(void* game,void* owner,void* target,void* damage,Wide::Effect effect,int result){SPD::Span diagnostic(SPD::Effect,game,owner,target,damage,0,0,reinterpret_cast<uintptr_t>(effect));diagnostic.Mark(SPD::NativeEnd,static_cast<uint32_t>(result));if(result)Inc(ProcSuccess);}
void Dispatch(void* game,void* owner,void* target,int stat) noexcept {
 SPD::Span diagnostic(SPD::Dispatch,game,owner,target,nullptr,0,0,stat);
 if(!Running()||!dispatch||!game||!Player(owner)||Automatic(owner))return;
 if(auto* now=Current();now&&now->stat){Inc(Suppressed);return;}
 Scope scope({},stat);Wide::Bridge bridge(nullptr,Filter,After);



 diagnostic.Mark(SPD::NativeBegin);
 const int32_t diagnosticResult=dispatch(game,stat==195?7:6,owner,target,nullptr,&bridge);
 diagnostic.Mark(SPD::NativeEnd,static_cast<uint32_t>(diagnosticResult));
 Inc(stat==195?AttackDispatches:HitDispatches);
}

bool FinalCrossbowAttack(void* unit,int32_t id) noexcept;
int32_t __fastcall Skill(void* game,void* unit,int32_t id,int32_t level,int32_t pay,int32_t item,int32_t effect) noexcept {
 SPD::Span diagnostic(SPD::Skill,game,unit,nullptr,nullptr,id,level,(uint64_t(uint32_t(pay))<<32)|(uint64_t(uint16_t(item))<<16)|uint16_t(effect));
 if(!Running())return originalSkill(game,unit,id,level,pay,item,effect);
 uint32_t owner=0;bool repeating=false;
 const bool player=Player(unit,&owner);
 const bool manual=game&&pay==1&&!item&&!effect&&level>0&&player&&!Automatic(unit)
  &&!(Current()&&Current()->stat)&&(IsSpell(game,id,repeating)||FinalCrossbowAttack(unit,id));
 void* target=manual?getTarget(game,unit):nullptr;
 diagnostic.record.target=reinterpret_cast<uintptr_t>(target);SPD::Identity(target,diagnostic.record.targetType,diagnostic.record.targetClass,diagnostic.record.targetId);
 int32_t result=0;
 diagnostic.Mark(SPD::Decision,manual);
 {Scope scope({game,owner,static_cast<uint32_t>(id),manual});diagnostic.Mark(SPD::NativeBegin);result=originalSkill(game,unit,id,level,pay,item,effect);diagnostic.Mark(SPD::NativeEnd,static_cast<uint32_t>(result));}
 if(auto* proc=Current();proc&&proc->stat&&proc->diagnosticKey&&player)SPD::AuditSkill(diagnostic.record,proc->diagnosticKey,result);
 if(result&&manual){CastOnce(game,unit,static_cast<uint32_t>(id),true);Inc(ManualCasts);Dispatch(game,unit,target,195);}
 return result;
}
}
#include "endgame_sets.h"
namespace SP {




bool FinalCrossbowAttack(void* unit,int32_t id) noexcept {
 switch(id){case 0:case 6:case 7:case 11:case 12:case 16:case 21:case 22:case 26:case 27:case 31:break;default:return false;}
 return EndgameSets::getStat&&EndgameSets::getStat(unit,458,0)>0;
}





Origin DirectSpellOrigin(void* game,void* params,void* owner,Invocation* current) noexcept {
 Origin none={};uint32_t uid=0,mode=0;uintptr_t actualGame=0;uint8_t bank=0,unitBank=0;
 int32_t sid=-1,level=0,itemOwner=0;uint16_t usedId=0xffff;uintptr_t row=0;
 if(!getUsed||!game||!Player(owner,&uid)||Automatic(owner)||
    !Read(params,0x3c,sid)||!Read(params,0x40,level)||sid<0||sid>32767||level<1||
    !Read(owner,0xd8,actualGame)||actualGame!=reinterpret_cast<uintptr_t>(game)||
    !Read(game,0x106,bank)||bank<2||bank>3||!Read(owner,0x1bd,unitBank)||unitBank!=bank||
    !Read(owner,0xc,mode)||(mode!=10&&mode!=18))return none;
 if(current&&(current->stat||current->origin.game!=game||current->origin.owner!=uid||current->origin.skill!=static_cast<uint32_t>(sid)))return none;
 bool repeating=false;if(!IsSpell(game,sid,repeating))return none;
 void* used=getUsed(owner);
 if(!used||!Read(used,0,row)||!row||!Read(reinterpret_cast<void*>(row),0,usedId)||usedId!=sid||
    record(bank,sid)!=reinterpret_cast<const uint8_t*>(row)||!Read(used,0x4c,itemOwner)||itemOwner!=-1)return none;
 return {game,uid,static_cast<uint32_t>(sid),true};
}
void* __fastcall Create(void* game,void* params) noexcept {
 SPD::Span diagnostic(SPD::Create,game,nullptr,nullptr,params);
 if(!Running())return originalCreate(game,params);
 Origin origin={};auto* current=Current();void* owner=nullptr;uint32_t id=0;
 Read(params,8,owner);
 if(current&&!current->stat&&current->origin.manual&&current->origin.game==game
   &&Read(params,8,owner)&&Player(owner,&id)&&id==current->origin.owner&&!Automatic(owner))origin=current->origin;
 bool recovered=false;
 if(!origin.manual){origin=DirectSpellOrigin(game,params,owner,current);recovered=origin.manual;}


 diagnostic.record.actor=reinterpret_cast<uintptr_t>(owner);diagnostic.record.actorType=owner?0:0xffffffff;diagnostic.record.actorId=id;diagnostic.record.skill=origin.skill;


 {SPD::PreserveError preserve;void* observed=nullptr;if(SPD::Copy(reinterpret_cast<uintptr_t>(params)+8,&observed,sizeof(observed))){diagnostic.record.actor=reinterpret_cast<uintptr_t>(observed);SPD::Identity(observed,diagnostic.record.actorType,diagnostic.record.actorClass,diagnostic.record.actorId);}}
 EndgameSets::MissileChoice choice(params);
 Scope scope(origin);diagnostic.Mark(SPD::NativeBegin,origin.manual);void* missile=originalCreate(game,params);diagnostic.Mark(SPD::NativeEnd,reinterpret_cast<uintptr_t>(missile));
 if(missile){
  Remember(game,missile,origin);
  if(recovered){InterlockedIncrement64(&recoveredRoots);diagnostic.Mark(SPD::Decision,1);
   if(CastOnce(game,owner,origin.skill)){Inc(ManualCasts);Dispatch(game,owner,getTarget(game,owner),195);}
  }
 }
 return missile;
}
int32_t __fastcall MissileDo(void* game,void* missile) noexcept {
 SPD::Span diagnostic(SPD::MissileDo,game,missile);
 if(!Running())return originalDo(game,missile);
 uint32_t guid=0,kind=0;const bool known=Identity(missile,guid,kind);
 Origin origin=known?FindKey(game,guid,kind):Origin{};Scope scope(origin);diagnostic.record.skill=origin.skill;diagnostic.record.extra=origin.owner;
 diagnostic.Mark(SPD::NativeBegin,origin.manual);const int32_t result=originalDo(game,missile);diagnostic.Mark(SPD::NativeEnd,static_cast<uint32_t>(result));


 if(result==2&&known)FindKey(game,guid,kind,true);return result;
}
int32_t __fastcall MissileHit(void* game,void* missile,void* target,int32_t expire) noexcept {
 SPD::Span diagnostic(SPD::MissileHit,game,missile,target,nullptr,0,0,expire);
 if(!Running())return originalHit(game,missile,target,expire);
 uint32_t guid=0,kind=0;const bool known=Identity(missile,guid,kind);
 Origin origin=known?FindKey(game,guid,kind):Origin{};Scope scope(origin);diagnostic.record.skill=origin.skill;diagnostic.record.extra=origin.owner;
 diagnostic.Mark(SPD::NativeBegin,origin.manual);const int32_t result=originalHit(game,missile,target,expire);diagnostic.Mark(SPD::NativeEnd,static_cast<uint32_t>(result));
 if(result==2&&known)FindKey(game,guid,kind,true);return result;
}
void __fastcall Report(void* game,void* attacker,void* defender,void* damage) noexcept {
 SPD::Span diagnostic(SPD::Report,game,attacker,defender,damage);


 Origin origin={};uint32_t owner=0,flags=0,targetType=99;uint16_t result=0;
 auto* invocation=Current();
 const bool eligible=Running()&&invocation&&!invocation->stat&&invocation->origin.manual
  &&invocation->origin.game==game&&Player(attacker,&owner)&&owner==invocation->origin.owner
  &&!Automatic(attacker)&&Read(defender,0,targetType)&&targetType==1
  &&Read(damage,0,flags)&&!(flags&0x20)&&Read(damage,4,result)
  &&(result&1)&&!(result&0x8390);
 if(eligible)origin=invocation->origin;
 diagnostic.record.flags=flags;diagnostic.record.skill=origin.skill;
 {SPD::PreserveError preserve;SPD::Copy(reinterpret_cast<uintptr_t>(damage),&diagnostic.record.flags,sizeof(diagnostic.record.flags));}
 diagnostic.Mark(SPD::NativeBegin,eligible);originalReport(game,attacker,defender,damage);diagnostic.Mark(SPD::NativeEnd);
 if(origin.manual){Inc(SpellHits);Dispatch(game,attacker,defender,198);}
}

ConsoleCommandResult Status(D2R::Game::Client*,const ConsoleCommandContext*,void*) noexcept {
 SPD::AuditSummary("console");SPD::Flush();
 Text t;t.Add("R3NI_SPELL_PROCS version=0.1.7-proc-audit active=");t.Num(Running());t.Add(" activation=");t.Num(static_cast<uint32_t>(activation));
 const char* names[]={" manual_casts="," attack_dispatches="," spell_hits="," hit_dispatches="," native_rolls="," native_success="," tracked_missiles="," suppressed="," capacity_skips="};
 for(uint32_t i=0;i<9;++i){t.Add(names[i]);t.Num(static_cast<uint64_t>(InterlockedCompareExchange64(&counters[i],0,0)));}
 t.Add(" holy_set_swaps=");t.Num(static_cast<uint64_t>(InterlockedCompareExchange64(&EndgameSets::replacements,0,0)));
 t.Add(" holy_set_reader=");t.Num(EndgameSets::getStat!=nullptr);
 t.Add(" combat_observer=");t.Num(SPD::externalSink!=nullptr);
 t.Add(" recovered_spell_roots=");t.Num(static_cast<uint64_t>(InterlockedCompareExchange64(&recoveredRoots,0,0)));
 t.Add(" diag_ready=");t.Num(SPD::header!=nullptr);t.Add(" trace_records=");t.Num(SPD::header?SPD::header->issued:0);t.Add(" trace_dropped=");t.Num(SPD::header?SPD::header->dropped:0);t.Add(" observed_exceptions=");t.Num(SPD::exceptions);
 if(context)context->WriteConsoleMessage(t.value);Log(t.value);SPD::Line(t.value);return ConsoleCommandResult::Handled;
}
bool Check(uintptr_t rva,const uint8_t* expected,uint32_t size,const char* name) noexcept {
 if(context->CheckExpectedBytes(rva,expected,size))return true;
 Text t;t.Add("SP_NATIVE_REJECT site=");t.Add(name);t.Add(" rva=");t.Num(rva);Log(t.value,true);return false;
}
template<size_t N> bool Check(uintptr_t rva,const uint8_t (&bytes)[N],const char* name) noexcept {return Check(rva,bytes,static_cast<uint32_t>(N),name);}
bool Bind() noexcept {
 HMODULE h=GetModuleHandleW(L"D2RCore.dll");core=reinterpret_cast<uintptr_t>(h);
 if(!h||!Readable(core+Wide::DispatchBodyRva,Wide::DispatchBodyBytes.size())||!Readable(core+Wide::SkillEffectBodyRva,Wide::SkillEffectBodyBytes.size())||!Wide::Validate(reinterpret_cast<const uint8_t*>(core)))return false;
 if(reinterpret_cast<uintptr_t>(GetProcAddress(h,MAKEINTRESOURCEA(1307)))!=core+Wide::DispatchExportRva)return false;
 dispatch=reinterpret_cast<DispatchFn>(core+Wide::DispatchExportRva);
 if(!EndgameSets::Bind(core))Log("EHS_HOLY_DISABLED: stat-reader contract mismatch; ordinary SpellProcs remains available.",true);
 const uint8_t instruction[]={0xff,0x15,0x2c,0x05,0x33,0x00};int32_t displacement=0;uintptr_t endpoint=0;
 if(!Match(core+0x3deb66,instruction)||!Read(reinterpret_cast<void*>(core+0x3deb66),2,displacement)
   ||!Read(reinterpret_cast<void*>(core+0x3deb66+6+displacement),0,endpoint)||endpoint!=base+0x97790)return false;
 record=reinterpret_cast<RecordFn>(endpoint);getTarget=reinterpret_cast<TargetFn>(base+Sites::TargetRva);
 getUsed=reinterpret_cast<UsedFn>(base+Sites::UsedSkillRva);
 return true;
}
bool Validate() noexcept {
 using namespace Sites;
 return Check(SkillHandlerRva,SkillHandlerBytes,"skill_handler")
  &&Check(SkillHandlerContextRva,SkillHandlerContextBytes,"skill_context")
  &&Check(SkillStrideRva,SkillStrideBytes,"skill_stride")
  &&Check(CreateMissileRva,CreateMissileBytes,"create_missile")
  &&Check(MissileDoRva,MissileDoBytes,"missile_do")
  &&Check(MissileHitRva,MissileHitBytes,"missile_hit")
  &&Check(ReportRva,ReportBytes,"damage_report")
  &&Check(ReportCallRva,ReportCallBytes,"damage_report_call")
  &&Check(ReportAdmissionRva,ReportAdmissionBytes,"damage_permission")
  &&Check(TargetRva,TargetBytes,"target_lookup")
  &&Check(UsedSkillRva,UsedSkillBytes,"live_used_skill")
  &&Check(MissileSkillFieldsRva,MissileSkillFieldsBytes,"missile_skill_fields")
  &&Check(CastIdRva,CastIdBytes,"native_cast_identity");
}
void OnTables(const PluginContext*,const Lifecycle::DataTablesLoadedEvent* e,void*) noexcept {
 if(!e||e->structSize<Lifecycle::DataTablesLoadedEventRequiredSize||InterlockedCompareExchange(&activation,1,0)!=0)return;
 if(GetModuleHandleW(L"d2rl-ruffneckk-cast-triggers.dll")||GetModuleHandleW(L"d2rl-celestialrayone-cast-on-cast.dll")){
  Log("SP_REFUSED: remove both supplied Cast Triggers / Cast on Cast DLLs; SpellProcs owns its own source tracking.",true);InterlockedExchange(&activation,3);return;}
 BindCombatObserver();
 if(!Bind()||!Validate()){Log("SP_REFUSED: Core/native contract mismatch; no hooks installed.",true);InterlockedExchange(&activation,3);return;}
 using namespace Sites;
 const bool ok=context->InstallInlineHook(SkillHandlerRva,SkillHandlerBytes,sizeof(SkillHandlerBytes),Skill,&originalSkill)
  &&context->InstallInlineHook(CreateMissileRva,CreateMissileBytes,sizeof(CreateMissileBytes),Create,&originalCreate)
  &&context->InstallInlineHook(MissileDoRva,MissileDoBytes,sizeof(MissileDoBytes),MissileDo,&originalDo)
  &&context->InstallInlineHook(MissileHitRva,MissileHitBytes,sizeof(MissileHitBytes),MissileHit,&originalHit)
  &&context->InstallInlineHook(ReportRva,ReportBytes,sizeof(ReportBytes),Report,&originalReport);
 if(!ok){InterlockedExchange(&activation,3);Log("SP_REFUSED: hook installation failed; installed wrappers only forward. Restart before retrying.",true);return;}
 InterlockedExchange(&activation,2);InterlockedExchange(&active,1);SPD::Ready(core);
 Log("SP_READY version=0.1.7-proc-audit on_attack=manual_spell_cast on_striking=confirmed_manual_spell_hit chance=native item_stats=195,198 source_tracking=1 live_used_skill_recovery=1 native_cast_id_dedup=1 per_caster_holy_set=1");
}
void OnGame(const PluginContext*,const Lifecycle::GameplayEvent* e,void*) noexcept {
 if(!e||e->structSize<Lifecycle::GameplayEventSize)return;
 SPD::AuditSummary(e->kind==Lifecycle::GameplayEventKind::GameLeft?"game_left":"game_joined");
 Reset();SPD::Event(static_cast<uint32_t>(e->kind),e->sessionGeneration);SPD::Flush();Log("SP_SESSION_RESET: prior projectile identities discarded.");
}
const PluginInfo info={sizeof(PluginInfo),4,"r3ni-spell-procs","Spell Procs","0.1.7-proc-audit","r3ni","Recovers native player spell projectiles from the live used skill; preserves native CtC chance, level and anti-chain rules.",PluginFlags::ModScopedOnly|PluginFlags::NativeHooks|PluginFlags::Shared,{}};
}
extern "C" __declspec(dllexport) uint32_t R3niSpellProcsCombatStateV1() noexcept {return R3niDiagnostics::SupportedState|static_cast<uint32_t>(InterlockedCompareExchange(&SP::activation,0,0))|(SPD::externalSink?R3niDiagnostics::ConnectedState:0);}
extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept {return &SP::info;}
extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept {
 using namespace SP;
 if(!D2RL::HasContext(ctx)||!ctx->GetApi()||!Equal(ctx->activeMod,"VanillaDeluxe")||ctx->loadScope!=D2RL::LoadScope::Mod)return false;
 context=ctx;base=ctx->exeBase;
 tls=TlsAlloc();if(tls==TLS_OUT_OF_INDEXES)return false;SPD::Initialize(base);
 if(ctx->QueryService(&lifecycle)!=ServiceQueryResult::Success||!lifecycle||lifecycle->serviceVersion!=1||lifecycle->serviceSize<LifecycleServiceSize||!lifecycle->registerDataTablesLoadedListener||!lifecycle->registerGameplayEventListener)return false;
 const auto command=MakeConsoleCommand("r3ni_spell_procs",Status,"Show native spell proc rolls and projectile provenance counters.");
 if(!ctx->RegisterConsoleCommand(command))return false;
 const auto diagnosticCommand=MakeConsoleCommand("r3ni_spell_diag",Status,"Flush the diagnostic flight recorder and show its status.");
 if(!ctx->RegisterConsoleCommand(diagnosticCommand))return false;
 Lifecycle::DataTablesLoadedListener listener={.structSize=Lifecycle::DataTablesLoadedListenerSize,.callback=OnTables};
 if(lifecycle->registerDataTablesLoadedListener(ctx,&listener,&tablesHandle)!=Lifecycle::Result::Success)return false;
 Lifecycle::GameplayEventKind kinds[]={Lifecycle::GameplayEventKind::GameJoined,Lifecycle::GameplayEventKind::GameLeft};
 for(uint32_t i=0;i<2;++i){Lifecycle::GameplayEventListener l={.structSize=Lifecycle::GameplayEventListenerSize,.kind=kinds[i],.callback=OnGame};if(lifecycle->registerGameplayEventListener(ctx,&l,&gameHandles[i])!=Lifecycle::Result::Success)return false;}
 Log("Spell Procs 0.1.7-proc-audit by r3ni; automatic flight recorder enabled; waiting for DataTablesLoaded; existing item chance and level preserved.");return true;
}
extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept {
 using namespace SP;InterlockedExchange(&active,0);InterlockedExchange(&activation,4);
 if(lifecycle&&context){if(tablesHandle&&lifecycle->unregisterDataTablesLoadedListener)lifecycle->unregisterDataTablesLoadedListener(context,tablesHandle);for(auto h:gameHandles)if(h&&lifecycle->unregisterGameplayEventListener)lifecycle->unregisterGameplayEventListener(context,h);}
 Reset();if(tls!=TLS_OUT_OF_INDEXES){TlsFree(tls);tls=TLS_OUT_OF_INDEXES;}
 SPD::externalSink=nullptr;if(SPD::observerModule){FreeLibrary(SPD::observerModule);SPD::observerModule=nullptr;}
 SPD::AuditSummary("unload");SPD::Close();context=nullptr;
}
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)SPD::module=instance;return TRUE;}
