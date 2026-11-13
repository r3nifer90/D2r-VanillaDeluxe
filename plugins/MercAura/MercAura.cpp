#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <D2RLPlugin/context.h>




using AssignFn=uint64_t(__fastcall*)(void*,int,int,int);
using GameFn=void*(__fastcall*)(void*);
using HandFn=void*(__fastcall*)(void*);
using SelectFn=void(__fastcall*)(void*,int,int,int);
static AssignFn originalAssign;
static GameFn getGame;
static HandFn getRight;
static SelectFn selectSkill;
using LookupFn=void*(__fastcall*)(void*,int,int);
using StateFn=int(__fastcall*)(void*,int);
static LookupFn getSkill;
static StateFn hasState;
extern "C" {void* originalIdleMode=nullptr;}
struct Attempt {uintptr_t game;uint32_t unitId,frame;bool used;};
static Attempt attempts[64];
static SRWLOCK attemptLock=SRWLOCK_INIT;
static bool read(uintptr_t at,void* out,size_t n) noexcept;
static const D2RL::PluginContext* context;

static bool nativeAura(int id) noexcept {
    switch(id){case 98:case 99:case 103:case 104:case 108:case 114:return true;default:return false;}
}
static bool candidate(void* unit,int id,int level) noexcept {
    if(!unit||level<=0||!nativeAura(id))return false;
    auto p=(const unsigned char*)unit;
    if(*(const uint32_t*)(p)!=1||*(const uint32_t*)(p+4)!=338)return false;
    if(!*(void*const*)(p+0x10))return false;
    auto mode=*(const uint32_t*)(p+0xc);
    return mode!=0&&mode!=12;
}
static uint64_t __fastcall assigned(void* unit,int id,int level,int flags) {
    const uint64_t result=originalAssign(unit,id,level,flags);
    if(!candidate(unit,id,level)||!getGame(unit))return result;


    void* right=getRight(unit);
    if(right){auto record=*(const unsigned char*const*)right;if(record&&*(const uint16_t*)record==id)return result;}
    selectSkill(unit,0,id,-1);
    return result;
}



static int ownedAura(void* unit,int& state) noexcept {
 int found=-1;
 const int ids[]={98,99,103,104,108,114};
 for(int id:ids){
  uintptr_t skill=(uintptr_t)getSkill(unit,id,-1),record{};int level{};uint16_t actual{};int16_t auraState{};
  if(!read(skill+0x40,&level,sizeof(level))||level<=0||!read(skill,&record,sizeof(record))||!read(record,&actual,sizeof(actual))||actual!=id||!read(record+0xa0,&auraState,sizeof(auraState))||auraState<0)continue;
  if(found>=0)return -1;
  found=id;state=auraState;
 }
 return found;
}
extern "C" void __fastcall restoreIdleAura(void* game,void* unit) noexcept {
 if(!game||!unit)return;


 const auto* nativeHead=(const uint32_t*)unit;
 if(nativeHead[0]!=1||nativeHead[1]!=338)return;
 uint32_t head[4]{};
 if(!read((uintptr_t)unit,head,sizeof(head))||head[0]!=1||head[1]!=338||head[3]==0||head[3]==12||getGame(unit)!=game)return;
 int state=-1,id=ownedAura(unit,state);
 if(id<0||hasState(unit,state))return;
 uint32_t frame{};if(!read((uintptr_t)game+0x170,&frame,sizeof(frame)))return;
 bool ready=true;
 AcquireSRWLockExclusive(&attemptLock);
 Attempt* slot=nullptr;
 for(auto& item:attempts)if(item.used&&item.game==(uintptr_t)game&&item.unitId==head[2]){slot=&item;break;}
 if(slot&&uint32_t(frame-slot->frame)<25)ready=false;
 if(ready){
  if(!slot){for(auto& item:attempts)if(!item.used){slot=&item;break;}}
  if(!slot)slot=&attempts[head[2]%64];
  *slot={(uintptr_t)game,head[2],frame,true};
 }
 ReleaseSRWLockExclusive(&attemptLock);



 if(ready)selectSkill(unit,0,id,-1);
}



extern "C" __attribute__((naked)) void idleModeGateway() {
 __asm__ volatile(
  "pushfq\n\tpush %rax\n\tpush %rcx\n\tpush %rdx\n\tpush %r8\n\tpush %r9\n\tpush %r10\n\tpush %r11\n\tsub $0x88,%rsp\n\t"
  "movdqu %xmm0,0x20(%rsp)\n\tmovdqu %xmm1,0x30(%rsp)\n\tmovdqu %xmm2,0x40(%rsp)\n\tmovdqu %xmm3,0x50(%rsp)\n\tmovdqu %xmm4,0x60(%rsp)\n\tmovdqu %xmm5,0x70(%rsp)\n\t"
  "mov %rdi,%rcx\n\tmov %rbx,%rdx\n\tcall restoreIdleAura\n\t"
  "movdqu 0x20(%rsp),%xmm0\n\tmovdqu 0x30(%rsp),%xmm1\n\tmovdqu 0x40(%rsp),%xmm2\n\tmovdqu 0x50(%rsp),%xmm3\n\tmovdqu 0x60(%rsp),%xmm4\n\tmovdqu 0x70(%rsp),%xmm5\n\t"
  "add $0x88,%rsp\n\tpop %r11\n\tpop %r10\n\tpop %r9\n\tpop %r8\n\tpop %rdx\n\tpop %rcx\n\tpop %rax\n\tpopfq\n\tjmp *originalIdleMode(%rip)\n\t");
}
static void* idleRelay;
static bool prepareIdleRelay(uintptr_t main) noexcept {
 uintptr_t first=(main+0x5643000+0xffff)&~uintptr_t(0xffff);
 for(uintptr_t delta=0;delta<0x70000000;delta+=0x10000){
  uintptr_t at=first+delta;if(int64_t(at)-int64_t(main+0x4a6d34)>INT32_MAX-0x1000)break;
  idleRelay=VirtualAlloc((void*)at,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(idleRelay)break;
 }
 if(!idleRelay)return false;
 unsigned char jump[14]={0xff,0x25};uintptr_t target=(uintptr_t)idleModeGateway;memcpy(jump+6,&target,8);memcpy(idleRelay,jump,sizeof(jump));
 DWORD old;if(!VirtualProtect(idleRelay,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(idleRelay,0,MEM_RELEASE);idleRelay=nullptr;return false;}
 FlushInstructionCache(GetCurrentProcess(),idleRelay,4096);return true;
}
static void report(const char* text,bool error=false) noexcept {
    if(context&&context->api){auto f=error?context->api->logError:context->api->logInfo;if(f)f(context,text);}
}
static bool read(uintptr_t at,void* out,size_t n) noexcept {
    SIZE_T done=0;return at&&ReadProcessMemory(GetCurrentProcess(),(const void*)at,out,n,&done)&&done==n;
}
template<class T>static bool read(uintptr_t at,T& out) noexcept{return read(at,&out,sizeof(out));}
static bool executable(uintptr_t at) noexcept {
    MEMORY_BASIC_INFORMATION m{};if(!at||!VirtualQuery((void*)at,&m,sizeof(m))||m.State!=MEM_COMMIT)return false;
    return !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static bool image(uintptr_t base,uint32_t timestamp,uint32_t size) noexcept {
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    return read(base,dos)&&dos.e_magic==IMAGE_DOS_SIGNATURE&&dos.e_lfanew>0&&dos.e_lfanew<0x10000&&read(base+dos.e_lfanew,nt)&&nt.Signature==IMAGE_NT_SIGNATURE&&nt.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&nt.FileHeader.TimeDateStamp==timestamp&&nt.OptionalHeader.SizeOfImage==size;
}
static bool binding(uintptr_t core,uint32_t callRva,uint32_t slotRva) noexcept {
    unsigned char code[6]{};if(!read(core+callRva,code,6)||code[0]!=0xff||code[1]!=0x15)return false;
    int32_t displacement{};memcpy(&displacement,code+2,4);
    return (intptr_t)(core+callRva+6)+displacement==(intptr_t)(core+slotRva);
}
static const D2RL::PluginInfo info{
 sizeof(D2RL::PluginInfo),4,"r3ni-merc-native-aura","Mercenary Native Aura","0.3.0","r3ni",
 "Restores the existing Act II merc aura from native Idle before combat selection; checks owned skills and active aura state; no new skills or levels.",
 D2RL::PluginFlags::ModScopedOnly|D2RL::PluginFlags::Shared|D2RL::PluginFlags::NativeHooks,{}
};
D2RL_PLUGIN_EXPORT const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &info;}
D2RL_PLUGIN_EXPORT bool D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept {
 context=ctx;
 if(!ctx||!ctx->api||ctx->api->apiSize<D2RL::PluginApiV2Size||!ctx->api->installInlineHook||!ctx->api->patchBytes)return false;
 const uintptr_t core=(uintptr_t)GetModuleHandleW(L"D2RCore.dll"),main=(uintptr_t)GetModuleHandleW(nullptr);
 if(!image(core,0x6ab37824,0x12e0000)||!image(main,0x6ab3782c,0x5643000)){
  report("Mercenary Native Aura: unsupported D2RCore or game build; no hook installed.",true);return false;
 }


 if(!binding(core,0x82034c,0x70f1d0)||!binding(core,0x820361,0x70f1e8)||!binding(core,0x820035,0x700550)||!binding(core,0x3cd4c1,0x70eda0)){
  report("Mercenary Native Aura: native binding verification failed; no hook installed.",true);return false;
 }
 uintptr_t assign{},game{},right{},select{};
 if(!read(core+0x70f1d0,assign)||!read(core+0x700550,game)||!read(core+0x70eda0,right)||!read(core+0x70f1e8,select)||
    assign<main||assign+32>main+0x5643000||!executable(assign)||!executable(game)||!executable(right)||!executable(select)){
  report("Mercenary Native Aura: required native functions unavailable; no hook installed.",true);return false;
 }


 static const unsigned char idleBefore[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x70,0x45,0x85,0xc0,0x48,0x8b,0xf9,0xbe,0x01,0,0,0,0x48,0x8b,0xca,0x41,0x0f,0x45,0xf0,0x48,0x8b,0xda,0xe8,0x27,0x3e,0xea,0xff};
 static const unsigned char lookupCall[]={0xe8,0x6a,0xea,0xd7,0xff};
 static const unsigned char stateCall[]={0xe8,0xa4,0x5b,0xd7,0xff};
 unsigned char idleActual[sizeof(idleBefore)]{},call[5]{};
 if(game!=main+0x48ff00||select!=main+0x438a70||
    !read(main+0x4a6d10,idleActual,sizeof(idleActual))||memcmp(idleActual,idleBefore,sizeof(idleBefore))||
    !read(main+0x5bf261,call,5)||memcmp(call,lookupCall,5)||
    !read(main+0x5bf607,call,5)||memcmp(call,stateCall,5)||
    !executable(main+0x33dcd0)||!executable(main+0x3351b0)||!executable(main+0x34ab60)){
  report("Mercenary Native Aura: native Idle/owned-skill/state bindings differ from supplied trace; no hooks installed.",true);return false;
 }
 unsigned char expected[32]{};if(!read(assign,expected,sizeof(expected)))return false;
 getGame=(GameFn)game;getRight=(HandFn)right;selectSkill=(SelectFn)select;
 getSkill=(LookupFn)(main+0x33dcd0);hasState=(StateFn)(main+0x3351b0);originalIdleMode=(void*)(main+0x34ab60);
 auto hook=D2RL::MakeInlineHook(assign-main,expected,sizeof(expected),(void*)assigned,(void**)&originalAssign);
 if(!ctx->api->installInlineHook(ctx,&hook)||!originalAssign){report("Mercenary Native Aura: loader could not install the assignment hook.",true);return false;}
 if(!prepareIdleRelay(main)){report("Mercenary Native Aura: near relay allocation failed.",true);return false;}
 int64_t delta=int64_t((uintptr_t)idleRelay)-int64_t(main+0x4a6d34+5);
 if(delta<INT32_MIN||delta>INT32_MAX){report("Mercenary Native Aura: near relay out of range.",true);return false;}
 unsigned char patch[5]={0xe8};int32_t rel=(int32_t)delta;memcpy(patch+1,&rel,4);
 if(!ctx->api->patchBytes(ctx,0x4a6d34,idleBefore+36,5,patch,5)){report("Mercenary Native Aura: loader rejected Idle CALL patch.",true);return false;}
 report("Mercenary Native Aura 0.3.0: native Idle CALL armed before combat skill selection; owned aura/state check; Valhalla idle prefix preserved.");
 return true;
}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {



}
