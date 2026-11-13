
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/core_exports.h>
#include <D2RLPlugin/lifecycle_events.h>
#include <D2RLPlugin/threads.h>
#include "native_contract.h"
extern "C" void* memcpy(void* d,const void* s,size_t n){auto* a=(volatile uint8_t*)d;auto* b=(const volatile uint8_t*)s;while(n--)*a++=*b++;return d;}
extern "C" void* memset(void* d,int v,size_t n){auto* a=(volatile uint8_t*)d;while(n--)*a++=(uint8_t)v;return d;}
namespace Loot {
constexpr uint32_t Code(const char* p){uint32_t c=0;bool end=false;for(uint32_t i=0;i<4;++i){if(!end&&!*p)end=true;c|=uint32_t(end?' ':*p++)<<(8*i);}return c;}
}
#include "scroll_pools.h"
namespace SC {
using namespace D2RL;
const PluginContext* ctx=nullptr;
HANDLE logFile=INVALID_HANDLE_VALUE;
const LifecycleService* lifecycle=nullptr;
const ThreadService* threads=nullptr;
bool installed=false;
struct Config {uint32_t enabled=1,unique=500000,gold=8,gems=2,rune=500000,runeRolls=2,logging=0;uint32_t special[7]={2500,2500,2500,500,250,250,20};} cfg;
struct Base {uint32_t code,level,required,type,type2,magic;};Base bases[4096];uint32_t baseCount=0;
struct Unique {uint32_t code,level,required,eligible;};Unique uniques[2048],sets[2048];uint32_t uniqueCount=0,setCount=0;
struct Op {void* game;void* object;void* player;void* seed;int32_t level;uint32_t reserved;};static_assert(sizeof(Op)==40);
using OperateFn=int32_t(__fastcall*)(Op*,int32_t) noexcept;
using DropFn=void*(__fastcall*)(void*,void*,uint32_t,int32_t);
using ItemLevelFn=int32_t(__fastcall*)(void*,int32_t);
using RandFn=int32_t(__fastcall*)(void*,int32_t);
OperateFn original=nullptr;DropFn drop=nullptr;ItemLevelFn itemLevel=nullptr;RandFn nativeRand=nullptr;
size_t Length(const char* p){size_t n=0;while(p&&p[n])++n;return n;}
bool Equal(const char* a,const char* b){if(!a||!b)return false;while(*a&&*a==*b){++a;++b;}return *a==*b;}
struct Text {char s[384]{};size_t n=0;void Add(const char* p){while(*p&&n<383)s[n++]=*p++;s[n]=0;}void Num(uint32_t v){char b[12];uint32_t k=0;do{b[k++]=char('0'+v%10);v/=10;}while(v);while(k&&n<383)s[n++]=b[--k];s[n]=0;}void Code(uint32_t v){for(int i=0;i<4&&n<383;++i)s[n++]=char(v>>(8*i));s[n]=0;}};
SRWLOCK logLock=SRWLOCK_INIT;
void Log(const char* s,bool error=false) noexcept {if(!error)return;AcquireSRWLockExclusive(&logLock);if(ctx){if(error)ctx->LogError(s);else ctx->LogInfo(s);}if(logFile!=INVALID_HANDLE_VALUE){DWORD n=0;WriteFile(logFile,s,(DWORD)Length(s),&n,nullptr);WriteFile(logFile,"\r\n",2,&n,nullptr);}ReleaseSRWLockExclusive(&logLock);}
bool Join(const wchar_t* base,const wchar_t* name,wchar_t* out){if(!base)return false;size_t i=0;while(*base){if(i>=1900)return false;out[i++]=*base++;}if(i&&out[i-1]!=L'\\'&&out[i-1]!=L'/')out[i++]=L'\\';while(*name){if(i>=2047)return false;out[i++]=*name++;}out[i]=0;return true;}
bool Read(uintptr_t p,void* out,size_t n){SIZE_T received=0;return p&&ReadProcessMemory(GetCurrentProcess(),(const void*)p,out,n,&received)&&received==n;}
template<class T> bool Read(uintptr_t p,T& out){return Read(p,&out,sizeof(out));}
bool Bytes(uintptr_t p,const uint8_t* expected,size_t n){uint8_t current[64];if(n>64||!Read(p,current,n))return false;for(size_t i=0;i<n;++i)if(current[i]!=expected[i])return false;return true;}
bool Number(const wchar_t* p,uint32_t& out,uint32_t max){out=0;if(!*p)return false;for(size_t i=0;p[i];++i){if(i>6||p[i]<'0'||p[i]>'9')return false;out=out*10+p[i]-'0';if(out>max)return false;}return true;}
bool Percent(const wchar_t* p,uint32_t& out){uint32_t whole=0,frac=0,digits=0;bool dot=false,any=false;for(size_t i=0;p[i];++i){if(p[i]==L'.'&&!dot){dot=true;continue;}if(p[i]<'0'||p[i]>'9')return false;any=true;if(dot){if(++digits>4)return false;frac=frac*10+p[i]-'0';}else{whole=whole*10+p[i]-'0';if(whole>100)return false;}}while(digits++<4)frac*=10;out=whole*10000+frac;return any&&out<=1000000;}
bool ConfigFile(){wchar_t path[2048]{};if(!Join(ctx->pluginDirectory,L"SparklyChests.ini",path))return false;
 struct Entry{const wchar_t* key;const wchar_t* fallback;uint32_t* value;uint32_t max;bool pct;};
 Entry entries[]={{L"enabled",L"1",&cfg.enabled,1,false},{L"unique_chance_percent",L"50",&cfg.unique,0,true},{L"extra_gold_piles",L"8",&cfg.gold,20,false},{L"extra_gems",L"2",&cfg.gems,5,false},{L"rune_chance_percent",L"50",&cfg.rune,0,true},{L"rune_rolls_keep_best",L"2",&cfg.runeRolls,4,false},{L"log_openings",L"0",&cfg.logging,1,false},
 {L"corruption_scroll_percent",L"0.25",&cfg.special[0],0,true},{L"dungeon_map_percent",L"0.25",&cfg.special[1],0,true},{L"infernal_malus_percent",L"0.25",&cfg.special[2],0,true},{L"horadric_weak_percent",L"0.05",&cfg.special[3],0,true},{L"horadric_medium_percent",L"0.025",&cfg.special[4],0,true},{L"horadric_strong_percent",L"0.025",&cfg.special[5],0,true},{L"horadric_all_skills_percent",L"0.002",&cfg.special[6],0,true}};
 for(auto& e:entries){wchar_t s[64]{};GetPrivateProfileStringW(L"SparklyChests",e.key,e.fallback,s,64,path);if(!(e.pct?Percent(s,*e.value):Number(s,*e.value,e.max)))return false;}
 uint32_t total=0;for(auto v:cfg.special)total+=v;return total<=1000000&&cfg.runeRolls>=1;
}
struct Field {const char* p=nullptr;size_t n=0;bool Eq(const char* s)const{return Length(s)==n&&(!n||EqualBytes(s));}bool EqualBytes(const char* s)const{for(size_t i=0;i<n;++i)if(p[i]!=s[i])return false;return true;}uint32_t Num()const{uint32_t v=0;for(size_t i=0;i<n;++i){if(p[i]<'0'||p[i]>'9')return 0;v=v*10+p[i]-'0';}return v;}uint32_t Code()const{if(!n||n>4)return 0;uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(i<n?p[i]:' ')<<(8*i);return v;}};
size_t Fields(const char* p,size_t n,Field* out){size_t count=0,start=0;for(size_t i=0;i<=n;++i)if(i==n||p[i]=='\t'){if(count>=512)return 0;out[count++]={p+start,i-start};start=i+1;}return count;}
int Column(const Field* fields,size_t n,const char* name){for(size_t i=0;i<n;++i)if(fields[i].Eq(name))return (int)i;return -1;}
bool ReadTable(const wchar_t* root,const wchar_t* name,bool unique){wchar_t path[2048]{};if(!Join(root,name,path))return false;HANDLE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;LARGE_INTEGER size{};bool ok=GetFileSizeEx(h,&size)&&size.QuadPart>0&&size.QuadPart<=16*1024*1024;char* b=ok?(char*)HeapAlloc(GetProcessHeap(),0,(size_t)size.QuadPart):nullptr;DWORD got=0;ok=b&&ReadFile(h,b,(DWORD)size.QuadPart,&got,nullptr)&&got==size.QuadPart;CloseHandle(h);if(!ok){if(b)HeapFree(GetProcessHeap(),0,b);return false;}
 Field header[512],row[512];size_t start=(got>=3&&(uint8_t)b[0]==0xef&&(uint8_t)b[1]==0xbb&&(uint8_t)b[2]==0xbf)?3:0,end=start;while(end<got&&b[end]!='\n')++end;size_t length=end-start;if(length&&b[start+length-1]=='\r')--length;size_t columns=Fields(b+start,length,header);
 const int requirement=Column(header,columns,unique?"lvl req":"levelreq"),typeCol=Column(header,columns,"type"),type2Col=Column(header,columns,"type2"),magicCol=Column(header,columns,"magic lvl");
 const int code=Column(header,columns,"code"),level=Column(header,columns,unique?"lvl":"level"),spawn=Column(header,columns,"spawnable"),cond=Column(header,columns,"DropConditionCalc"),disabled=Column(header,columns,"disabled"),carry=Column(header,columns,"carry1");
 ok=requirement>=0&&code>=0&&level>=0&&spawn>=0&&cond>=0&&(!unique||(disabled>=0&&carry>=0));
 start=end+1;while(ok&&start<got){end=start;while(end<got&&b[end]!='\n')++end;length=end-start;if(length&&b[start+length-1]=='\r')--length;size_t count=Fields(b+start,length,row);start=end+1;if(count!=columns||!row[code].n)continue;if(!row[spawn].Eq("1"))continue;uint32_t c=row[code].Code();if(!c)continue;
  if(unique){
   if(row[disabled].Eq("1"))continue;
   uint32_t base=0,baseRequired=0;bool found=false;
   for(uint32_t i=0;i<baseCount;++i)if(bases[i].code==c){base=bases[i].level;baseRequired=bases[i].required;found=true;break;}
   if(!found)continue;if(uniqueCount>=2048){ok=false;break;}
   uint32_t l=row[level].Num(),req=row[requirement].Num();if(baseRequired>req)req=baseRequired;


   bool eligible=!row[carry].Eq("1")&&!row[cond].n;
   uniques[uniqueCount++]={c,l>base?l:base,req,eligible?1u:0u};
  }else{
   if(row[cond].n)continue;if(baseCount>=4096){ok=false;break;}
   bases[baseCount++]={c,row[level].Num(),row[requirement].Num(),typeCol>=0?row[typeCol].Code():0,type2Col>=0?row[type2Col].Code():0,magicCol>=0?row[magicCol].Num():0};
  }
 }
 HeapFree(GetProcessHeap(),0,b);return ok;
}
#include "reward_pools.h"
bool Pools(){baseCount=0;uniqueCount=0;setCount=0;typeCount=0;affixCount=0;wchar_t root[2048]{},path[2048]{};const wchar_t* dirs[]={ctx->modDirectory,ctx->scopeRootDirectory};bool found=false;for(auto* dir:dirs){if(!dir)continue;if(Join(dir,L"VanillaDeluxe.mpq",root)&&Join(root,L"data/global/excel/uniqueitems.txt",path)&&GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES){found=true;break;}if(Join(dir,L"",root)&&Join(root,L"data/global/excel/uniqueitems.txt",path)&&GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES){found=true;break;}}
 if(!found){Log("Sparkly Chests: uniqueitems.txt not found in active mod data/global/excel.",true);return false;}
 if(!ReadTable(root,L"data/global/excel/weapons.txt",false)){Log("Sparkly Chests: cannot read weapons.txt or required columns.",true);return false;}
 if(!ReadTable(root,L"data/global/excel/armor.txt",false)){Log("Sparkly Chests: cannot read armor.txt or required columns.",true);return false;}

 if(baseCount>4093)return false; bases[baseCount++]={Loot::Code("amu"),1,0,Loot::Code("amul"),0,0};bases[baseCount++]={Loot::Code("rin"),1,0,Loot::Code("ring"),0,0};bases[baseCount++]={Loot::Code("jew"),1,0,Loot::Code("jewl"),0,0};
 if(!ReadTable(root,L"data/global/excel/uniqueitems.txt",true)||!uniqueCount){Log("Sparkly Chests: cannot read uniqueitems.txt or no eligible base records.",true);return false;}
 if(!ReadExtra(root,L"data/global/excel/setitems.txt",0)||!ReadExtra(root,L"data/global/excel/itemtypes.txt",1)||!ReadExtra(root,L"data/global/excel/magicprefix.txt",2)||!ReadExtra(root,L"data/global/excel/magicsuffix.txt",2)){Log("Sparkly Chests: set/type/rare-affix tables unavailable.",true);return false;}
 Text t;t.Add("SC_POOLS: eligible_unique_records=");t.Num(uniqueCount);t.Add("; current native TXT read; no tables modified.");Log(t.s);return true;
}
uint32_t Random(Op* op,uint32_t bound){return (uint32_t)nativeRand(op->seed,(int32_t)bound);}
uint32_t Rune(Op* op,uint32_t node){if(node==1)return Random(op,5)<3?1:2;if(node==17)return Random(op,1501)==0?33:Rune(op,16);constexpr uint32_t previous[]={0,0,2,5,7,12,22,45,90,175,215,275,335,475,725,975,1300};uint32_t r=Random(op,previous[node]+5);return r<3?node*2-1:r<5?node*2:Rune(op,node-1);}
uint32_t Special(Op* op,uint32_t difficulty){if(difficulty!=2)return 0;uint32_t r=Random(op,1000000),sum=0;for(uint32_t i=0;i<7;++i){sum+=cfg.special[i];if(r>=sum)continue;switch(i){case 0:return Loot::Code("CO1");case 1:return Loot::Code("Ma1");case 2:return Loot::Code("rhm");case 3:return Loot::Weak[Random(op,20)];case 4:return Loot::Medium[Random(op,14)];case 5:return Loot::Strong[Random(op,14)];default:return Loot::Skills[Random(op,3)];}}return 0;}
bool SafeUnique(uint32_t index,uint32_t ilvl,uint32_t cap){
 if(index>=uniqueCount||!uniques[index].eligible||uniques[index].level>ilvl||uniques[index].required>cap)return false;
 const uint32_t code=uniques[index].code;
 for(uint32_t j=0;j<uniqueCount;++j)
  if(uniques[j].code==code&&uniques[j].level<=ilvl&&uniques[j].required>cap)return false;
 return true;
}
uint32_t RewardCap(Op* op){
 uint32_t type=99;
 if(!op->player||!Read((uintptr_t)op->player,type)||type!=0)return 0;


 int32_t level=itemLevel(op->player,0);if(level<1||level>99)return 0;
 return (uint32_t)level+5;
}
bool SafeSet(uint32_t index,uint32_t ilvl,uint32_t cap){
 if(index>=setCount||!sets[index].eligible||sets[index].level>ilvl||sets[index].required>cap)return false;
 for(uint32_t i=0;i<setCount;++i)if(sets[i].code==sets[index].code&&sets[i].level<=ilvl&&sets[i].required>cap)return false;
 return true;
}
void RareReward(Op* op,uint32_t ilvl,uint32_t cap){
 if(cap<6)return;uint32_t playerLevel=cap-5,candidates[4096],count=0;
 for(uint32_t i=0;i<baseCount;++i)if(SafeRareBase(bases[i],ilvl,playerLevel,cap))candidates[count++]=i;
 if(!count){Log("Sparkly Chests: no level-safe rare base available.",true);return;}


 drop(op->game,op->player,bases[candidates[Random(op,count)]].code,6);
}
void Rewards(Op* op,uint32_t difficulty){const int32_t ilvl=itemLevel(op->object,0);if(ilvl<1||ilvl>255)return;uint32_t actualUnique=0,uniqueRoll=0,uniqueCode=0,runeCode=0,special=0,placed=0;
 const uint32_t cap=RewardCap(op);
 if(Random(op,1000000)<cfg.unique){
  uniqueRoll=1;const bool wantSet=Random(op,2)!=0;uint32_t candidates[2048],count=0;
  if(cap){if(wantSet){for(uint32_t i=0;i<setCount;++i)if(SafeSet(i,(uint32_t)ilvl,cap))candidates[count++]=i;}
   else for(uint32_t i=0;i<uniqueCount;++i)if(SafeUnique(i,(uint32_t)ilvl,cap))candidates[count++]=i;}
  if(count){uniqueCode=(wantSet?sets:uniques)[candidates[Random(op,count)]].code;drop(op->game,op->object,uniqueCode,wantSet?5:7);}
 }
 RareReward(op,(uint32_t)ilvl,cap);
 for(uint32_t i=0;i<cfg.gold;++i)if(drop(op->game,op->object,Loot::Code("gld"),2))++placed;
 constexpr uint32_t normal[]={Loot::Code("gfv"),Loot::Code("gfy"),Loot::Code("gfb"),Loot::Code("gfr"),Loot::Code("gfg"),Loot::Code("gfw"),Loot::Code("skf"),Loot::Code("gsv"),Loot::Code("gsy"),Loot::Code("gsb"),Loot::Code("gsr"),Loot::Code("gsg"),Loot::Code("gsw"),Loot::Code("sku")};
 constexpr uint32_t nightmare[]={Loot::Code("gzv"),Loot::Code("gly"),Loot::Code("glb"),Loot::Code("glr"),Loot::Code("glg"),Loot::Code("glw"),Loot::Code("skl"),Loot::Code("gpv"),Loot::Code("gpy"),Loot::Code("gpb"),Loot::Code("gpr"),Loot::Code("gpg"),Loot::Code("gpw"),Loot::Code("skz")};
 for(uint32_t i=0;i<cfg.gems;++i){const auto c=difficulty==0?normal[Random(op,14)]:difficulty==1?nightmare[Random(op,14)]:nightmare[7+Random(op,7)];drop(op->game,op->object,c,2);}
 if(Random(op,1000000)<cfg.rune){uint32_t best=0;for(uint32_t i=0;i<cfg.runeRolls;++i){uint32_t r=Rune(op,difficulty==0?7:difficulty==1?12:17);if(r>best)best=r;}char code[]={'r',char('0'+best/10),char('0'+best%10),0};runeCode=Loot::Code(code);drop(op->game,op->object,runeCode,2);}
 special=Special(op,difficulty);if(special&&!drop(op->game,op->object,special,2)){Log("SC_SPECIAL_FAILED: native item placement/code unavailable.",true);}
 if(cfg.logging){Text t;t.Add("SC_CHEST difficulty=");t.Num(difficulty);t.Add(" class=397 ilvl=");t.Num(ilvl);t.Add(" unique_roll=");t.Num(uniqueRoll);t.Add(" actual_unique=");t.Num(actualUnique);t.Add(" unique_base=");if(uniqueCode)t.Code(uniqueCode);else t.Add("none");t.Add(" extra_gold_placed=");t.Num(placed);t.Add(" gems=");t.Num(cfg.gems);t.Add(" rune=");if(runeCode)t.Code(runeCode);else t.Add("none");t.Add(" special=");if(special)t.Code(special);else t.Add("none");Log(t.s);}
}
int32_t __fastcall Open(Op* op,int32_t operate) noexcept {Op snapshot{};uint32_t fields[4]{};uint8_t difficulty=255;bool eligible=cfg.enabled&&op&&Read((uintptr_t)op,snapshot)&&snapshot.game&&snapshot.object&&snapshot.seed&&Read((uintptr_t)snapshot.object,fields,sizeof(fields))&&fields[0]==2&&fields[1]==397&&fields[3]==0&&Read((uintptr_t)snapshot.game+0x104,difficulty)&&difficulty<=2;
 const int32_t result=original(op,operate);uint32_t mode=0;if(eligible&&result==1&&Read((uintptr_t)snapshot.object+0xc,mode)&&mode!=0)Rewards(&snapshot,difficulty);return result;
}
bool Contract(){using namespace Native;uintptr_t b=ctx->exeBase;return Bytes(b+OperateRva,OperateBytes,sizeof(OperateBytes))&&Bytes(b+DropCodeRva,DropCodeBytes,sizeof(DropCodeBytes))&&Bytes(b+ItemLevelRva,ItemLevelBytes,sizeof(ItemLevelBytes))&&Bytes(b+RandRva,RandBytes,sizeof(RandBytes))&&Bytes(b+HpCallRva,HpCallBytes,sizeof(HpCallBytes))&&Bytes(b+MpCallRva,MpCallBytes,sizeof(MpCallBytes));}
bool Install(){if(installed||!cfg.enabled)return true;if(!Contract())return false;using namespace Native;
 drop=(DropFn)(ctx->exeBase+DropCodeRva);itemLevel=(ItemLevelFn)(ctx->exeBase+ItemLevelRva);nativeRand=(RandFn)(ctx->exeBase+RandRva);
 if(!ctx->InstallInlineHook(OperateRva,OperateBytes,sizeof(OperateBytes),Open,&original))return false;
 if(!ctx->PatchNop(HpCallRva,HpCallBytes,sizeof(HpCallBytes),5)||!ctx->PatchNop(MpCallRva,MpCallBytes,sizeof(MpCallBytes),5)){cfg.enabled=0;Log("SC_INSTALL_FAILED: native patch registration rejected; rewards disabled.",true);return false;}
 installed=true;Log("SC_READY: class397 hook; direct hp3/mp3 calls removed; one bonus roll per successful opening; all difficulties.");return true;
}
void __cdecl Deferred(const PluginContext*,void*) noexcept {if(!installed&&cfg.enabled&&!Install())Log("SC_NOT_READY: 93847 chest code contract unavailable/conflicting; no reward hook ready.",true);}
void __cdecl OnGameplay(const PluginContext*,const Lifecycle::GameplayEvent*,void*) noexcept {if(!installed&&cfg.enabled)threads->runOnGameThread(ctx,Deferred,nullptr);}
void __cdecl OnTables(const PluginContext*,const Lifecycle::DataTablesLoadedEvent*,void*) noexcept {Deferred(ctx,nullptr);}
void Unload(){if(logFile!=INVALID_HANDLE_VALUE){CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;}ctx=nullptr;installed=false;}
bool Load(const PluginContext* c){if(!c||c->contextSize<PluginContextSize||c->abiVersion!=4||c->loadScope!=LoadScope::Mod||!Equal(c->activeMod,"VanillaDeluxe"))return false;ctx=c;if(!c->exeBase)return true;

 Log("SC_LOAD: SparklyChests 1.0.2; author=r3ni; role=shared; engine-native chest handling.");
 HMODULE core=GetModuleHandleW(L"D2RCore.dll");auto build=core?(CoreExports::GetModBuildVersionFn)GetProcAddress(core,MAKEINTRESOURCEA(CoreExports::GetModBuildVersionInfo.ordinal)):nullptr;
 IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 pe{};if(!build||build()!=93847||!Read(c->exeBase,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<64||dos.e_lfanew>65536||!Read(c->exeBase+dos.e_lfanew,pe)||pe.FileHeader.TimeDateStamp!=1790146604||pe.OptionalHeader.SizeOfImage!=90451968){Log("SC_REJECT: exact supplied D2R 93847 image required.",true);Unload();return false;}
 if(!ConfigFile()){Log("Sparkly Chests: invalid SparklyChests.ini values.",true);Unload();return false;}
 if(cfg.enabled&&!Pools()){Log("Sparkly Chests: bonus unique pool unavailable; check preceding file error.",true);Unload();return false;}
 Text config;config.Add("SC_CONFIG: unique_ppm=");config.Num(cfg.unique);config.Add(" extra_gold=");config.Num(cfg.gold);config.Add(" gems=");config.Num(cfg.gems);config.Add(" rune_ppm=");config.Num(cfg.rune);config.Add(" best_of=");config.Num(cfg.runeRolls);config.Add(" special_ppm=");for(uint32_t i=0;i<7;++i){if(i)config.Add(",");config.Num(cfg.special[i]);}Log(config.s);
 if(!cfg.enabled){Log("SC_DISABLED: enabled=0; no patches/hooks installed.");return true;}
 if(Contract()){if(Install())return true;Unload();return false;}
 if(c->QueryService(&lifecycle)!=ServiceQueryResult::Success||!lifecycle||lifecycle->serviceSize<LifecycleServiceSize||c->QueryService(&threads)!=ServiceQueryResult::Success||!threads||threads->serviceSize<ThreadServiceSize){Log("SC_REJECT: deferred hook services unavailable.",true);Unload();return false;}
 Lifecycle::ListenerHandle h=0;Lifecycle::DataTablesLoadedListener table{.structSize=Lifecycle::DataTablesLoadedListenerSize,.callback=OnTables};Lifecycle::GameplayEventListener game{.structSize=Lifecycle::GameplayEventListenerSize,.kind=Lifecycle::GameplayEventKind::GameJoined,.callback=OnGameplay};
 if(!lifecycle->registerDataTablesLoadedListener||!lifecycle->registerGameplayEventListener||!threads->runOnGameThread||lifecycle->registerDataTablesLoadedListener(c,&table,&h)!=Lifecycle::Result::Success||lifecycle->registerGameplayEventListener(c,&game,&h)!=Lifecycle::Result::Success){Unload();return false;}
 Log("SC_WAITING: code still protected or hooks not yet available; retry on tables/game initialization.");return true;
}
constexpr PluginFlags flags=PluginFlags::ModScopedOnly|PluginFlags::NativeHooks|PluginFlags::Shared;
static_assert(HasValidPluginRole(flags)&&HasOnlyKnownPluginFlags(flags));
const PluginInfo info{sizeof(PluginInfo),4,"sparkly-chests","Sparkly Chests","1.0.2","r3ni","Split unique/set bonus and guaranteed level-safe rare; opener level +5 cap.",flags,{}};
}
extern "C" __declspec(dllexport) const D2RL::PluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept{return &SC::info;}
extern "C" __declspec(dllexport) bool __cdecl D2RLoaderLoadPlugin(const D2RL::PluginContext* c) noexcept{return SC::Load(c);}
extern "C" __declspec(dllexport) void __cdecl D2RLoaderUnloadPlugin() noexcept{SC::Unload();}
extern "C" BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(h);return TRUE;}
#ifdef SPARKLY_FIXTURE
extern "C" __declspec(dllexport) const void* SparklyFixturePointers[]={
 (const void*)&SC::ctx,(const void*)&SC::original,(const void*)&SC::drop,(const void*)&SC::itemLevel,(const void*)&SC::nativeRand,
 (const void*)&SC::uniqueCount,(const void*)&SC::uniques,(const void*)&SC::Open,(const void*)&SC::ReadTable,(const void*)&SC::baseCount,(const void*)&SC::bases,
 (const void*)&SC::cfg,(const void*)&SC::Pools,(const void*)&SC::ConfigFile,(const void*)&SC::SafeUnique,(const void*)&SC::RewardCap,(const void*)&SC::setCount,(const void*)&SC::sets,(const void*)&SC::SafeSet,(const void*)&SC::SafeRareBase,(const void*)&SC::typeCount,(const void*)&SC::types,(const void*)&SC::affixCount,(const void*)&SC::affixes};
#endif
