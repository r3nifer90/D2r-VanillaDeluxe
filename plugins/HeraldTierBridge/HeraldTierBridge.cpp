#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <D2RLPlugin/context.h>
static uintptr_t core;
static const D2RL::PluginContext* context;
template<class T> static bool read(uintptr_t address,T& value) {SIZE_T done=0;return address&&ReadProcessMemory(GetCurrentProcess(),(const void*)address,&value,sizeof(value),&done)&&done==sizeof(value);}
template<class T> static T value(uintptr_t address,T fallback={}) {T out;return read(address,out)?out:fallback;}
struct Hook {
 uintptr_t address;unsigned size;unsigned char saved[32],patch[32];void* trampoline;bool active;
 bool prepare(uintptr_t at,const unsigned char* expected,unsigned length,void* target) {
  if(length<14||length>32)return false;
  address=at;size=length;
  if(!read(at,saved)||memcmp(saved,expected,length))return false;
  trampoline=VirtualAlloc(nullptr,64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!trampoline)return false;
  memcpy(trampoline,saved,length);unsigned char* tail=(unsigned char*)trampoline+length;
  tail[0]=0xff;tail[1]=0x25;memset(tail+2,0,4);uintptr_t resume=at+length;memcpy(tail+6,&resume,8);
  DWORD old;if(!VirtualProtect(trampoline,64,PAGE_EXECUTE_READ,&old))return false;
  FlushInstructionCache(GetCurrentProcess(),trampoline,64);
  memset(patch,0x90,sizeof(patch));patch[0]=0xff;patch[1]=0x25;memset(patch+2,0,4);memcpy(patch+6,&target,8);return true;
 }
 bool apply() {
  unsigned char now[32];if(!read(address,now)||memcmp(now,saved,size))return false;
  DWORD old;if(!VirtualProtect((void*)address,size,PAGE_EXECUTE_READWRITE,&old))return false;
  memcpy((void*)address,patch,size);DWORD ignored;VirtualProtect((void*)address,size,old,&ignored);
  FlushInstructionCache(GetCurrentProcess(),(void*)address,size);active=true;return true;
 }
 void release() {
  bool detached=!active;unsigned char now[32];
  if(active && read(address,now) && !memcmp(now,patch,size)) {
   DWORD old;if(VirtualProtect((void*)address,size,PAGE_EXECUTE_READWRITE,&old)) {
    memcpy((void*)address,saved,size);DWORD ignored;VirtualProtect((void*)address,size,old,&ignored);
    FlushInstructionCache(GetCurrentProcess(),(void*)address,size);detached=true;active=false;
   }
  }
  if(detached && trampoline){VirtualFree(trampoline,0,MEM_RELEASE);trampoline=nullptr;}
 }
};
static Hook wideHook;
using StatFn=unsigned long long(*)(void*,unsigned,unsigned);
static StatFn nativeOriginal;
static bool enabled;
static const D2RL::PluginInfo info {
 sizeof(D2RL::PluginInfo),4,"r3ni-herald-tier-bridge","Herald Tier Compatibility",
 "1.0.0","r3ni","Reads native Herald tier 367 as table stat 429 for Herald monsters; preserves item stat IDs and saves.",
 D2RL::PluginFlags::ModScopedOnly|D2RL::PluginFlags::NativeHooks|D2RL::PluginFlags::Shared,{}
};
static bool tier(void* unit,unsigned stat,unsigned layer,unsigned long long& result) {
 if(!unit||layer!=0||(stat!=367&&stat!=429))return false;
 uintptr_t u=(uintptr_t)unit;
 if(value<unsigned>(u,~0u)!=1)return false;
 uintptr_t md=value<uintptr_t>(u+16);

 if((value<unsigned short>(md+0x1a)&0x600)!=0x200)return false;
 auto raw=(StatFn)wideHook.trampoline;
 if(!raw)return false;
 auto n=raw(unit,367,0);
 if(n<1||n>5)return false;
 result=n;return true;
}
static unsigned long long wide(void* unit,unsigned stat,unsigned layer) {
 unsigned long long n;

 if(stat==429&&tier(unit,stat,layer,n))return n;
 return ((StatFn)wideHook.trampoline)(unit,stat,layer);
}
static unsigned long long native(void* unit,unsigned stat,unsigned layer) {
 unsigned long long n;
 if(tier(unit,stat,layer,n))return n;
 return nativeOriginal(unit,stat,layer);
}
static void log(const char* text,bool error=false) {
 if(context&&context->api){auto fn=error?context->api->logError:context->api->logInfo;if(fn)fn(context,text);}
}
D2RL_PLUGIN_EXPORT const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &info;}
D2RL_PLUGIN_EXPORT bool D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept {
 context=ctx;
 if(!ctx||!ctx->api||ctx->api->apiSize<D2RL::PluginApiV2Size||!ctx->api->installInlineHook){log("Herald bridge requires native inline-hook API.",true);return false;}
 core=(uintptr_t)GetModuleHandleW(L"D2RCore.dll");
 IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 pe{};
 if(!core||!read(core,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||!read(core+dos.e_lfanew,pe)||pe.Signature!=IMAGE_NT_SIGNATURE||pe.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||pe.FileHeader.TimeDateStamp!=0x6ab37824||pe.OptionalHeader.SizeOfImage!=0x12e0000){log("Herald bridge: unsupported Core build; no patches applied.",true);return false;}

 uintptr_t gameStat=value<uintptr_t>(core+0x6fef50);
 if(!ctx->exeBase||gameStat!=ctx->exeBase+0x2f48c0){log("Herald bridge: native stat binding mismatch; no patches applied.",true);return false;}
 const unsigned char prologue[]={0x41,0x57,0x41,0x56,0x56,0x57,0x53,0x48,0x83,0xec,0x20,0x31,0xc0,0x48,0x85,0xc9};
 if(!wideHook.prepare(core+0x3d8bd0,prologue,sizeof(prologue),(void*)&wide)){wideHook.release();log("Herald bridge: wide stat reader signature conflict; no patches applied.",true);return false;}
 unsigned char nativeBytes[16];
 if(!read(gameStat,nativeBytes)){wideHook.release();log("Herald bridge: native reader is not readable.",true);return false;}
 auto hook=D2RL::MakeInlineHook(0x2f48c0,nativeBytes,sizeof(nativeBytes),(void*)&native,(void**)&nativeOriginal);
 if(!ctx->api->installInlineHook(ctx,&hook)){wideHook.release();log("Herald bridge: native stat hook rejected; no wide-reader patch applied.",true);return false;}
 if(!nativeOriginal||!wideHook.apply()){wideHook.release();log("Herald bridge: activation failed; loader must roll back managed native hook.",true);return false;}
 enabled=true;
 log("Herald tier bridge active: Herald-only 367 -> 429; native name reads restored; item/player stats and table IDs preserved.");
 return true;
}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
 wideHook.release();enabled=false;

}
extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID){return TRUE;}
