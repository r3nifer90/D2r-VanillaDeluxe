#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <new>
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/lifecycle_events.h>
#include <D2RLPlugin/threads.h>
#include <D2RLPlugin/data_tables.h>
#include "third_party/minhook/include/MinHook.h"
#include "core_guard.h"
#include "rules.h"
#include "item_rules.h"
#include "canonical_names.h"

extern "C" void* memset(void* p,int v,size_t n) {auto* b=static_cast<volatile unsigned char*>(p);while(n--)*b++=static_cast<unsigned char>(v);return p;}
extern "C" void* memcpy(void* p,const void* q,size_t n) {auto* b=static_cast<volatile unsigned char*>(p);auto* a=static_cast<const volatile unsigned char*>(q);while(n--)*b++=*a++;return p;}
extern "C" void* memmove(void* p,const void* q,size_t n) {auto* b=static_cast<volatile unsigned char*>(p);auto* a=static_cast<const volatile unsigned char*>(q);if(b<a){for(size_t i=0;i<n;++i)b[i]=a[i];}else{while(n){--n;b[n]=a[n];}}return p;}
namespace PLR {
using namespace D2RL;
Config cfg;
const PluginContext* ctx=nullptr;
const LifecycleService* lifecycle=nullptr;
const ThreadService* threads=nullptr;
const DataTableService* tables=nullptr;
HMODULE module=nullptr;
SRWLOCK logLock=SRWLOCK_INIT, dataLock=SRWLOCK_INIT;
volatile LONG active=0, difficulty=-1, preparePending=0;
volatile LONG64 session=0, pendingSession=0;
uintptr_t coreBase=0, hookTarget=0;
bool minHook=false, installed=false, nativeTrace=true;
uint8_t skillPolicy[MaxSkills]={};
Lifecycle::ListenerHandle handles[5]={};
struct BankData {bool ready=false;uint32_t uniqueCount=0,setCount=0,skillCount=0,levelMask=0;uint16_t uniqueLvl[MaxRecords]={},setLvl[MaxRecords]={};uint8_t fixedSet[MaxRecords]={};uint8_t fixedUnique[MaxRecords]={}, itemRule[MaxRecords]={};};
BankData banks[4];
struct Property {uint32_t id;int32_t skill,chance,level;};
static_assert(sizeof(Property)==16);
using TriggerFn=int32_t(__fastcall*)(const uint8_t*,void*,void*,const Property*,bool,int32_t,int32_t,uint32_t);
TriggerFn original=nullptr;
using ItemGetter=int32_t(__fastcall*)(const void*);
using SkillGetter=int32_t(__fastcall*)(uint8_t,int32_t);
using BankGetter=const void*(__fastcall*)(uint8_t);
ItemGetter getQuality=nullptr,getRecord=nullptr,getItemLevel=nullptr;
SkillGetter getRequired=nullptr,getMaximum=nullptr;
BankGetter getNativeBank=nullptr;

struct Text {
    char s[384]={};uint32_t n=0;
    void Add(const char* t) noexcept {while(*t&&n<382)s[n++]=*t++;s[n]=0;}
    void Num(uint64_t v) noexcept {char b[24];uint32_t k=0;do{b[k++]=static_cast<char>('0'+v%10);v/=10;}while(v);while(k&&n<382)s[n++]=b[--k];s[n]=0;}
    void Signed(int32_t v) noexcept {if(v<0){Add("-");Num(static_cast<uint64_t>(-static_cast<int64_t>(v)));}else Num(static_cast<uint32_t>(v));}
    void Hex(uintptr_t v) noexcept {constexpr char digits[]="0123456789ABCDEF";Add("0x");for(int shift=60;shift>=0;shift-=4){char c[2]={digits[(v>>shift)&15],0};Add(c);}}
};
void Log(const char* text,bool error=false) noexcept {
    if(!error)return;
    AcquireSRWLockExclusive(&logLock);
    if(ctx&&ctx->GetApi())ctx->LogError(text);
    ReleaseSRWLockExclusive(&logLock);
}
bool Equal(const char* a,const char* b) noexcept {
    if(!a||!b)return false;
    while(*a&&*b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}
    return *a==*b;
}
bool Readable(uintptr_t p,size_t n) noexcept {
    if(!p||p+n<p)return false;
    const uintptr_t end=p+n;
    while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        const DWORD protection=m.Protect&0xff;
        if(protection!=PAGE_READONLY&&protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;
    }return true;
}
bool Executable(uintptr_t p) noexcept {MEMORY_BASIC_INFORMATION m={};return VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
template<class T> bool Read(uintptr_t p,T& v) noexcept {if(!Readable(p,sizeof(T)))return false;memcpy(&v,reinterpret_cast<const void*>(p),sizeof(T));return true;}
bool Bytes(uintptr_t p,const uint8_t* expected,size_t n) noexcept {if(!Readable(p,n))return false;auto* actual=reinterpret_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i)if(actual[i]!=expected[i])return false;return true;}
uintptr_t RipSlot(uint32_t call) noexcept {int32_t d=0;uintptr_t v=0;if(!Read(coreBase+call+2,d)||!Read(coreBase+call+6+d,v)||!Executable(v))return 0;return v;}
bool NativeContract(HMODULE core) noexcept {
    coreBase=reinterpret_cast<uintptr_t>(core);
    using namespace ProcLevelRolls;
    struct Block {uint32_t rva;const uint8_t* bytes;size_t size;};
    const Block blocks[]={
        {TriggeredBodyRva,TriggeredBody,sizeof(TriggeredBody)},
        {TriggeredInternalWrapperRva,TriggeredInternalWrapper,sizeof(TriggeredInternalWrapper)},
        {TriggeredExportWrapperRva,TriggeredExportWrapper,sizeof(TriggeredExportWrapper)},
        {InfoBindingContractRva,InfoBindingContract,sizeof(InfoBindingContract)},
        {DecodeContextContractRva,DecodeContextContract,sizeof(DecodeContextContract)},
        {GameThreadContractRva,GameThreadContract,sizeof(GameThreadContract)},
        {InfoThreadContractRva,InfoThreadContract,sizeof(InfoThreadContract)},
        {GameDifficultyContractRva,GameDifficultyContract,sizeof(GameDifficultyContract)}
    };
    for(const auto& b:blocks)if(!Bytes(coreBase+b.rva,b.bytes,b.size))return false;
    if(reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1327)))!=coreBase+TriggeredExportWrapperRva)return false;
    getQuality=reinterpret_cast<ItemGetter>(RipSlot(0x41e435));
    getRecord=reinterpret_cast<ItemGetter>(RipSlot(0x41e48d));
    getItemLevel=reinterpret_cast<ItemGetter>(RipSlot(0x41e441));
    getRequired=reinterpret_cast<SkillGetter>(RipSlot(0x3de432));
    getMaximum=reinterpret_cast<SkillGetter>(RipSlot(0x3de492));
    getNativeBank=reinterpret_cast<BankGetter>(RipSlot(0x3de3c8));
    hookTarget=coreBase+TriggeredBodyRva;
    return Executable(hookTarget)&&getQuality&&getRecord&&getItemLevel&&getRequired&&getMaximum&&getNativeBank;
}
bool Parse(const wchar_t* text,uint32_t& result) noexcept {
    if(!text||!*text)return false;
    uint64_t n=0;uint32_t i=0;
    for(;text[i];++i){if(i>=10||text[i]<L'0'||text[i]>L'9')return false;n=n*10+text[i]-L'0';if(n>0xffffffffULL)return false;}
    result=static_cast<uint32_t>(n);return true;
}
bool ReadConfig() noexcept {
    wchar_t path[1024]={};if(!ctx->pluginDirectory)return false;
    uint32_t n=0;while(ctx->pluginDirectory[n]&&n<950){path[n]=ctx->pluginDirectory[n];++n;}if(n==950)return false;
    constexpr wchar_t tail[]=L"\\ProcLevelRolls.ini";for(uint32_t i=0;i<sizeof(tail)/sizeof(wchar_t);++i)path[n+i]=tail[i];
    if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return false;
    struct Entry {const wchar_t* section;const wchar_t* key;uint32_t* value;uint32_t min,max;};
    Entry values[]={
        {L"plugin",L"enabled",&cfg.enabled,0,1},{L"plugin",L"log_rolls",&cfg.logRolls,0,1},{L"plugin",L"roll_salt",&cfg.salt,0,0xffffffffU},
        {L"scope",L"magic",&cfg.magic,0,1},{L"scope",L"rare",&cfg.rare,0,1},{L"scope",L"unique",&cfg.unique,0,1},{L"scope",L"set",&cfg.set,0,1},{L"scope",L"crafted",&cfg.crafted,0,1},
        {L"scope",L"preserve_stronger_unique_set",&cfg.preserve,0,1},{L"scope",L"include_support_skills",&cfg.support,0,1},
        {L"difficulty",L"normal_min",&cfg.difficulty[0].min,1,63},{L"difficulty",L"normal_max",&cfg.difficulty[0].max,1,63},
        {L"difficulty",L"nightmare_min",&cfg.difficulty[1].min,1,63},{L"difficulty",L"nightmare_max",&cfg.difficulty[1].max,1,63},
        {L"difficulty",L"hell_min",&cfg.difficulty[2].min,1,63},{L"difficulty",L"hell_max",&cfg.difficulty[2].max,1,63},
        {L"unique_set",L"low_item_level_max",&cfg.lowMax,1,65534},{L"unique_set",L"mid_item_level_max",&cfg.midMax,2,65535},
        {L"unique_set",L"low_min",&cfg.tiers[0].min,1,63},{L"unique_set",L"low_max",&cfg.tiers[0].max,1,63},
        {L"unique_set",L"mid_min",&cfg.tiers[1].min,1,63},{L"unique_set",L"mid_max",&cfg.tiers[1].max,1,63},
        {L"unique_set",L"high_min",&cfg.tiers[2].min,1,63},{L"unique_set",L"high_max",&cfg.tiers[2].max,1,63}
    };
    for(auto& v:values){wchar_t text[32]={};if(!GetPrivateProfileStringW(v.section,v.key,L"",text,32,path)||!Parse(text,*v.value)||*v.value<v.min||*v.value>v.max)return false;}

    wchar_t trace[16]={};
    if(GetPrivateProfileStringW(L"plugin",L"trace_native",L"",trace,16,path)){
        uint32_t value=0;if(!Parse(trace,value)||value>1)return false;nativeTrace=value!=0;
    }
    wchar_t runewords[16]={};
    if(GetPrivateProfileStringW(L"scope",L"runewords",L"",runewords,16,path)){
        uint32_t value=0;if(!Parse(runewords,value)||value>1)return false;cfg.runewords=value;
    }
    if(cfg.lowMax>=cfg.midMax)return false;
    for(uint32_t i=0;i<3;++i)if(cfg.difficulty[i].min>cfg.difficulty[i].max||cfg.tiers[i].min>cfg.tiers[i].max)return false;
    for(auto id:DamageSkills)skillPolicy[id]=1;
    for(uint32_t id=0;id<MaxSkills;++id){wchar_t key[12]={};Text t;t.Num(id);for(uint32_t i=0;i<t.n;++i)key[i]=t.s[i];wchar_t value[16]={};
        if(GetPrivateProfileStringW(L"skills",key,L"",value,16,path)){uint32_t nValue=0;if(!Parse(value,nValue)||nValue>1)return false;skillPolicy[id]=static_cast<uint8_t>(nValue);}
    }return true;
}
uint16_t Word(const uint8_t* p) noexcept {return static_cast<uint16_t>(p[0]|static_cast<uint16_t>(p[1])<<8);}
bool HasName(const uint8_t* row,uint32_t bytes,const char* name) noexcept {
    uint32_t len=0;while(name[len])++len;if(len>32)len=32;
    if(bytes>160)bytes=160;
    for(uint32_t start=0;start+len<=bytes;++start){uint32_t n=0;while(n<len&&row[start+n]==static_cast<uint8_t>(name[n]))++n;if(n==len)return true;}
    return false;
}
uint16_t ResolveUniqueLevel(const uint8_t* row, const uint8_t* rows, uint32_t count,
                            uint32_t stride, uint32_t offset) noexcept {
    const uint16_t level = Word(row + offset);
    if (level != 1) return level;

    const uint32_t nameOffset = row[0] == 'R' && row[1] == '3' && row[2] == 'A' &&
                                row[3] == 'B' && row[4] == 'U' ? 0 : 2;
    const auto* name = row + nameOffset;
    if (name[0] != 'R' || name[1] != '3' || name[2] != 'A' ||
        name[3] != 'B' || name[4] != 'U' || name[9] != 0) return level;

    uint32_t index = 0;
    for (uint32_t i = 5; i < 9; ++i) {
        if (name[i] < '0' || name[i] > '9') return level;
        index = index * 10 + name[i] - '0';
    }
    if (index >= 512 || !CanonicalNames[index][0]) return level;

    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* candidate = rows + static_cast<size_t>(i) * stride;
        for (uint32_t j = 0; j < 32 && candidate[nameOffset + j] == static_cast<uint8_t>(CanonicalNames[index][j]); ++j) {
            if (candidate[nameOffset + j] == 0) {
                const uint16_t canonicalLevel = Word(candidate + offset);
                return canonicalLevel > 0 && canonicalLevel <= 255 ? canonicalLevel : level;
            }
        }
    }
    return level;
}
bool CacheLevels(DataTables::Bank bank,DataTables::TableId id,const Anchor* anchors,uint32_t count,uint16_t* output,uint32_t& rowCount,uint8_t* fixed=nullptr,uint8_t* itemRules=nullptr) noexcept {
    DataTables::TableView view={.structSize=DataTables::TableViewSize};
    if(tables->getTable(ctx,bank,id,&view)!=DataTables::Result::Success||!view.rows||view.rowCount<5||view.rowCount>MaxRecords||view.rowSize<64||view.rowSize>4096)return false;
    const uint64_t bytes=static_cast<uint64_t>(view.rowSize)*view.rowCount;
    if(!Readable(reinterpret_cast<uintptr_t>(view.rows),static_cast<size_t>(bytes)))return false;
    const uint8_t* rows=static_cast<const uint8_t*>(view.rows);
    const uint8_t* found[5]={};
    if(count!=5)return false;
    for(uint32_t i=0;i<view.rowCount;++i){const auto* row=rows+static_cast<size_t>(i)*view.rowSize;
        for(uint32_t j=0;j<count;++j)if(HasName(row,view.rowSize,anchors[j].name)){if(found[j])return false;found[j]=row;}
    }
    for(uint32_t j=0;j<count;++j)if(!found[j])return false;
    uint32_t offset=0,matches=0;const uint32_t end=view.rowSize<256?view.rowSize:256;
    for(uint32_t o=0;o+4<=end;++o){bool match=true;
        for(uint32_t j=0;j<count;++j)if(Word(found[j]+o)!=anchors[j].lvl||Word(found[j]+o+2)!=anchors[j].required){match=false;break;}
        if(match){offset=o;++matches;}
    }
    if(matches!=1)return false;
    for(uint32_t i=0;i<view.rowCount;++i){const auto* row=rows+static_cast<size_t>(i)*view.rowSize;output[i]=id==DataTables::TableId::UniqueItems?ResolveUniqueLevel(row,rows,view.rowCount,view.rowSize,offset):Word(row+offset);if(fixed)fixed[i]=(HasName(row,view.rowSize,"terror-charms:")||HasName(row,view.rowSize,"R3EHS")||HasName(row,view.rowSize,"R3EFU"))?1:0;}
    if(itemRules){for(uint32_t i=0;i<view.rowCount;++i){
        const auto* row=rows+static_cast<size_t>(i)*view.rowSize;itemRules[i]=0;
        for(uint32_t j=0;j<sizeof(ItemRules)/sizeof(ItemRules[0]);++j)if(HasName(row,view.rowSize,ItemRules[j].name)||HasName(row,view.rowSize,ItemRules[j].alias)){itemRules[i]=static_cast<uint8_t>(j+1);break;}
    }}
    rowCount=view.rowCount;
    Text t;t.Add("TABLE bank=");t.Num(static_cast<uint32_t>(bank));t.Add(id==DataTables::TableId::UniqueItems?" unique_rows=":" set_rows=");t.Num(rowCount);t.Add(" verified_lvl_offset=");t.Num(offset);Log(t.s);
    return true;
}
void BuildCache() noexcept {
    AcquireSRWLockExclusive(&dataLock);
    for(uint32_t b=1;b<=3;++b){auto& out=banks[b];out.ready=false;out.uniqueCount=0;out.setCount=0;out.skillCount=0;out.levelMask=0;
        DataTables::TableView skillView={.structSize=DataTables::TableViewSize};
        auto bank=static_cast<DataTables::Bank>(b);
        if(tables->getTable(ctx,bank,DataTables::TableId::Skills,&skillView)!=DataTables::Result::Success||!skillView.rows||!skillView.rowCount||skillView.rowCount>MaxSkills)continue;
        const uintptr_t native=reinterpret_cast<uintptr_t>(getNativeBank(static_cast<uint8_t>(b)));uint32_t bits=0,mask=0;
        if(!native||!Read(native+0x14d0,bits)||!Read(native+0x14d4,mask)||bits<6||bits>8||mask!=(1U<<bits)-1)continue;
        out.skillCount=skillView.rowCount;out.levelMask=mask;
        const bool uniqueOk=CacheLevels(bank,DataTables::TableId::UniqueItems,UniqueAnchors,5,out.uniqueLvl,out.uniqueCount,out.fixedUnique,out.itemRule);
        const bool setOk=CacheLevels(bank,DataTables::TableId::SetItems,SetAnchors,5,out.setLvl,out.setCount,out.fixedSet);
        out.ready=uniqueOk&&setOk;
        Text t;t.Add(out.ready?"READY bank=":"TABLES_UNAVAILABLE bank=");t.Num(b);t.Add(" skills=");t.Num(out.skillCount);Log(t.s,!out.ready);
    }
    ReleaseSRWLockExclusive(&dataLock);
}
uintptr_t NativeTls() noexcept;
bool QueuedGameDifficulty(uint32_t& value) noexcept {
    uint32_t thread=0;uintptr_t game=0;uint8_t d=0,bank=0;
    if(!Read(coreBase+0x7df084,thread)||!thread||thread!=GetCurrentThreadId())return false;
    const uintptr_t tls=NativeTls();


    if(!tls||!Read(tls+0x1c38,game)||!game||!Read(game+0x104,d)||d>2||!Read(game+0x106,bank)||bank<1||bank>3)return false;
    value=d;return true;
}
void Prepare(const PluginContext*,void*) noexcept {
    AcquireSRWLockShared(&dataLock);const bool cached=banks[2].ready&&banks[3].ready;ReleaseSRWLockShared(&dataLock);
    const LONG64 current=InterlockedCompareExchange64(&session,0,0);
    if(current>0&&current==InterlockedCompareExchange64(&pendingSession,0,0)){
        uint32_t nativeDifficulty=0;
        if(QueuedGameDifficulty(nativeDifficulty)){
            InterlockedExchange(&difficulty,static_cast<LONG>(nativeDifficulty));
            Text t;t.Add("GAME difficulty=");t.Num(nativeDifficulty);t.Add(" session=");t.Num(static_cast<uint64_t>(current));t.Add(" source=authoritative_game");Log(t.s);
        }else{
            InterlockedExchange(&difficulty,-1);
            Log("DIFFICULTY_UNAVAILABLE: authoritative game not readable; magic/rare rolls paused.",true);
        }
        if(!cached)BuildCache();
    }
    InterlockedExchange(&preparePending,0);
}
void Schedule() noexcept {
    if(InterlockedCompareExchange(&preparePending,1,0)!=0)return;
    InterlockedExchange64(&pendingSession,InterlockedCompareExchange64(&session,0,0));
    if(threads->runOnGameThread(ctx,Prepare,nullptr)!=Threads::Result::Success){InterlockedExchange(&preparePending,0);Log("No local authoritative game thread; waiting for host game.");}
}
void OnGameplay(const PluginContext*,const Lifecycle::GameplayEvent* e,void*) noexcept {
    if(!e||e->structSize<Lifecycle::GameplayEventRequiredSize)return;
    if(e->kind==Lifecycle::GameplayEventKind::GameLeft){InterlockedExchange(&difficulty,-1);InterlockedExchange64(&session,0);Log("GAME_LEFT");return;}
    const LONG64 next=static_cast<LONG64>(e->sessionGeneration);
    if(InterlockedExchange64(&session,next)!=next)InterlockedExchange(&difficulty,-1);


    Text t;t.Add("GAME_EVENT kind=");t.Num(static_cast<uint32_t>(e->kind));t.Add(" session=");t.Num(e->sessionGeneration);Log(t.s);
    Schedule();
}
void OnTables(const PluginContext*,const Lifecycle::DataTablesLoadedEvent*,void*) noexcept {BuildCache();}
uintptr_t NativeTls() noexcept {
    uint32_t index=0;uintptr_t array=0,tls=0;
    if(!Read(coreBase+0x7df224,index)||index>1023)return 0;
    array=static_cast<uintptr_t>(__readgsqword(0x58));
    return Read(array+static_cast<uintptr_t>(index)*8,tls)?tls:0;
}
uint32_t OriginalLevel(uint8_t bank,const Property& p,const void* item) noexcept {
    if(p.level>0)return static_cast<uint32_t>(p.level);
    const int32_t required=getRequired(bank,p.skill),ilvl=getItemLevel(item);
    int64_t level=0;
    if(p.level<0){int64_t divisor=(99-static_cast<int64_t>(required));if(divisor<1)divisor=1;divisor/=-(static_cast<int64_t>(p.level));if(divisor<1)divisor=1;level=(static_cast<int64_t>(ilvl)-required)/divisor;}
    else {level=(static_cast<int64_t>(ilvl)-required)/4+1;const int32_t nativeMax=getMaximum(bank,p.skill);const int32_t max=nativeMax>0?nativeMax:20;if(level>max)level=max;}
    if(level<1)level=1;
    return static_cast<uint32_t>(level);
}
bool Select(const uint8_t* bankPtr,void* unit,void* item,const Property* p,int32_t stat,Property& rolled,uint32_t& quality,uint32_t& tier,uint32_t& before) noexcept {
    const bool fromUnit=unit!=nullptr;
    if(unit)item=unit;
    if(!p||!item||!bankPtr||!InterlockedCompareExchange(&active,0,0))return false;
    uint32_t thread=0,type=0;uint8_t bank=0,itemBank=0;uintptr_t data=0,codec=0;
    if(!Read(coreBase+0x7df084,thread)||!thread||thread!=GetCurrentThreadId()||!Read(reinterpret_cast<uintptr_t>(bankPtr),bank)||bank<1||bank>3)return false;
    const uintptr_t tls=NativeTls();if(!tls||!Read(tls+0x1930,codec)||codec)return false;
    if(!Read(reinterpret_cast<uintptr_t>(item),type)||type!=4||!Read(reinterpret_cast<uintptr_t>(item)+0x1bd,itemBank)||itemBank!=bank||!Read(reinterpret_cast<uintptr_t>(item)+0x10,data)||!data||!Readable(reinterpret_cast<uintptr_t>(p),sizeof(Property)))return false;
    if(stat!=195&&stat!=196&&stat!=197&&stat!=198&&stat!=199&&stat!=201)return false;
    if(p->skill<0||p->skill>=static_cast<int32_t>(MaxSkills))return false;
    quality=static_cast<uint32_t>(getQuality(item));
    if(fromUnit&&quality!=2&&quality!=3)return false;
    uint32_t itemFlags=0;
    const bool runeword=(quality==2||quality==3)&&cfg.runewords&&Read(data+0x18,itemFlags)&&(itemFlags&0x04000000);
    const ItemRule* rule=nullptr;
    Range range={};
    bool allowed=false;uint32_t mask=0;
    AcquireSRWLockShared(&dataLock);
    const auto& cache=banks[bank];
    mask=cache.levelMask;
    if(cache.skillCount&&static_cast<uint32_t>(p->skill)<cache.skillCount){
        const int32_t recordId=quality==7?getRecord(item):-1;
        if(quality==7&&cfg.unique&&recordId>=0&&static_cast<uint32_t>(recordId)<cache.uniqueCount&&cache.itemRule[recordId]){
            const uint32_t first = cache.itemRule[recordId] - 1;
            for (uint32_t i = first; i < sizeof(ItemRules) / sizeof(ItemRules[0]); ++i) {
                const auto& candidate = ItemRules[i];
                if (!Equal(candidate.name, ItemRules[first].name)) break;
                if (candidate.skill != p->skill || candidate.stat != stat) continue;
                rule = &candidate;
                range = rule->level;
                tier = cache.uniqueLvl[recordId];
                allowed = true;
                break;
            }
        }
        if(rule){}else if((quality==4&&cfg.magic)||(quality==6&&cfg.rare)||(quality==8&&cfg.crafted)||runeword){
            const LONG d=InterlockedCompareExchange(&difficulty,0,0);
            if(d>=0&&d<=2){tier=static_cast<uint32_t>(d);range=cfg.difficulty[d];allowed=true;}
        }else if((quality==7&&cfg.unique)||(quality==5&&cfg.set)){
            const int32_t id=getRecord(item);
            const uint32_t count=quality==7?cache.uniqueCount:cache.setCount;
            if(id>=0&&static_cast<uint32_t>(id)<count&&!(quality==7?cache.fixedUnique[id]:cache.fixedSet[id])){const uint16_t lvl=quality==7?cache.uniqueLvl[id]:cache.setLvl[id];
                if(lvl){range=ItemRange(cfg,lvl);tier=lvl;allowed=true;}
            }
        }
    }
    ReleaseSRWLockShared(&dataLock);
    if(!allowed||(!rule&&!cfg.support&&!skillPolicy[p->skill]))return false;
    before=OriginalLevel(bank,*p,item)&mask;
    if(!rule&&(runeword||((quality==7||quality==5)&&cfg.preserve)))range=PreserveFloor(range,before);
    uint32_t generation=0,seed=0;
    if(!Read(reinterpret_cast<uintptr_t>(item)+0x30,generation)||!Read(data+0x10,seed))return false;
    uint64_t key=static_cast<uint64_t>(generation)<<32|seed;
    key=Mix(key^(static_cast<uint64_t>(static_cast<uint16_t>(stat))<<48)^static_cast<uint32_t>(p->skill));
    key=Mix(key^(static_cast<uint64_t>(p->id)<<32)^static_cast<uint32_t>(p->chance));
    key=Mix(key^(static_cast<uint64_t>(static_cast<uint32_t>(p->level))<<32)^cfg.salt);
    rolled=*p;rolled.level=static_cast<int32_t>(RandomLevel(key,range));
    if(rule)rolled.chance=static_cast<int32_t>(RandomLevel(Mix(key^0x5233455055435443ULL),rule->chance));
    return true;
}
int32_t __fastcall Hook(const uint8_t* bank,void* unit,void* item,const Property* p,bool replace,int32_t stat,int32_t state,uint32_t flags) noexcept {
    Property rolled={};uint32_t quality=0,tier=0,before=0;
    if(!Select(bank,unit,item,p,stat,rolled,quality,tier,before))
        return original(bank,unit,item,p,replace,stat,state,flags);
    return original(bank,unit,item,&rolled,replace,stat,state,flags);
}
ConsoleCommandResult Status(D2R::Game::Client*,const ConsoleCommandContext*,void*) noexcept {
    Text t;t.Add("Proc Level Rolls 0.1.14-alias-names; hook=");t.Num(installed);
    if(ctx&&ctx->GetApi())ctx->LogInfo(t.s);
    return ConsoleCommandResult::Handled;
}
const PluginInfo info={sizeof(PluginInfo),4,"vanilla-proc-level-rolls","Vanilla Deluxe Proc Level Rolls","0.1.14-alias-names","r3ni","One-time item proc rolls including Terror charm levels 40-50; no runtime diagnostic output.",PluginFlags::ModScopedOnly|PluginFlags::NativeHooks|PluginFlags::Shared,{}};
void Cleanup() noexcept {
    InterlockedExchange(&active,0);InterlockedExchange(&difficulty,-1);InterlockedExchange64(&session,0);


    if(installed){const auto result=MH_DisableHook(MH_ALL_HOOKS);if(result!=MH_OK)Log("Could not disable native hook during unload.",true);installed=false;}
    if(minHook){MH_Uninitialize();minHook=false;}
    if(ctx&&lifecycle){for(uint32_t i=0;i<4;++i)if(handles[i]&&lifecycle->unregisterGameplayEventListener)lifecycle->unregisterGameplayEventListener(ctx,handles[i]);if(handles[4]&&lifecycle->unregisterDataTablesLoadedListener)lifecycle->unregisterDataTablesLoadedListener(ctx,handles[4]);}
    ctx=nullptr;
}
bool Load(const PluginContext* context) noexcept {
    if(!HasContext(context)||!context->GetApi())return false;
    ctx=context;
    Log("Proc Level Rolls 0.1.14-alias-names: starting.");
    if(ctx->loadScope!=LoadScope::Mod||!Equal(ctx->activeMod,"VanillaDeluxe")){Log("Requires mods/VanillaDeluxe/d2rloader/plugins.",true);return false;}
    if(!ReadConfig()){Log("Missing/invalid ProcLevelRolls.ini. Level ranges must be 1..63 and min <= max.",true);return false;}
    if(!cfg.enabled){Log("DISABLED: enabled=0.");return true;}
    if(!ctx->buildName||!Equal(ctx->buildName,"95032")){Log("Unsupported game build. This package targets build 95032.",true);return false;}
    if(ctx->QueryService(&lifecycle)!=ServiceQueryResult::Success||!lifecycle||lifecycle->serviceVersion!=LifecycleService::AbiVersion||lifecycle->serviceSize<LifecycleServiceSize||!lifecycle->registerGameplayEventListener||!lifecycle->registerDataTablesLoadedListener||
       ctx->QueryService(&threads)!=ServiceQueryResult::Success||!threads||threads->serviceVersion!=ThreadService::AbiVersion||threads->serviceSize<ThreadServiceSize||!threads->runOnGameThread||
       ctx->QueryService(&tables)!=ServiceQueryResult::Success||!tables||tables->serviceVersion!=DataTableService::AbiVersion||tables->serviceSize<DataTableServiceSize||!tables->getTable){Log("Required SDK 0.3.0 services unavailable.",true);return false;}
    HMODULE core=GetModuleHandleW(L"D2RCore.dll");if(!core||!NativeContract(core)){Log("Core triggered-property contract mismatch. No hook installed.",true);return false;}
    const Lifecycle::GameplayEventKind kinds[]={Lifecycle::GameplayEventKind::GameJoined,Lifecycle::GameplayEventKind::GameLeft,Lifecycle::GameplayEventKind::LocalPlayerReady,Lifecycle::GameplayEventKind::LevelChanged};
    for(uint32_t i=0;i<4;++i){Lifecycle::GameplayEventListener l={.structSize=Lifecycle::GameplayEventListenerSize,.kind=kinds[i],.callback=OnGameplay};if(lifecycle->registerGameplayEventListener(ctx,&l,&handles[i])!=Lifecycle::Result::Success)return false;}
    Lifecycle::DataTablesLoadedListener listener={.structSize=Lifecycle::DataTablesLoadedListenerSize,.callback=OnTables};
    if(lifecycle->registerDataTablesLoadedListener(ctx,&listener,&handles[4])!=Lifecycle::Result::Success)return false;
    auto command=MakeConsoleCommand("vdprocs_status",Status,"Show Proc Level Rolls hook state.");command.scope=ConsoleCommandScope::LocalOnly;
    if(!ctx->RegisterConsoleCommand(command))return false;
    if(MH_Initialize()!=MH_OK){Log("Cannot initialize native hook library.",true);return false;}minHook=true;
    if(MH_CreateHook(reinterpret_cast<void*>(hookTarget),reinterpret_cast<void*>(&Hook),reinterpret_cast<void**>(&original))!=MH_OK||!original){Log("Cannot prepare triggered-property hook.",true);return false;}
    InterlockedExchange(&active,1);
    if(MH_EnableHook(reinterpret_cast<void*>(hookTarget))!=MH_OK){InterlockedExchange(&active,0);Log("Cannot enable triggered-property hook.",true);return false;}
    installed=true;Log("ACTIVE: triggered-property hook installed. Waiting for authoritative game and verified item tables.");return true;
}
}
extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept {return &PLR::info;}
extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept {if(PLR::Load(ctx))return true;PLR::Cleanup();return false;}
extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept {PLR::Log("Proc Level Rolls unloading; restoring native function.");PLR::Cleanup();}
extern "C" BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)PLR::module=h;return TRUE;}
