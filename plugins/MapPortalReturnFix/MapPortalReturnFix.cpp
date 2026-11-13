#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <D2RLPlugin/context.h>

static uintptr_t patchAt;
static bool active;
static unsigned char saved[16], patched[16];
extern "C" {void* portalLookup=nullptr;void* portalLookupResume=nullptr;}





extern "C" __attribute__((naked)) void portalLookupGuard() {
    __asm__ volatile(
        "pushfq\n\t"
        "mov (%rcx,%rax,8),%rdx\n\t"
        "movzbl 0x106(%rbx),%ecx\n\t"
        "test %rdx,%rdx\n\t"
        "jz 1f\n\t"
        "popfq\n\t"
        "call *portalLookup(%rip)\n\t"
        "jmp *portalLookupResume(%rip)\n\t"
        "1: popfq\n\txor %eax,%eax\n\t"
        "jmp *portalLookupResume(%rip)\n\t");
}
template<class T> static bool read(uintptr_t at,T& out){
    SIZE_T n=0;return at && ReadProcessMemory(GetCurrentProcess(),(void*)at,&out,sizeof(out),&n) && n==sizeof(out);
}
static bool install(){
    uintptr_t main=(uintptr_t)GetModuleHandleW(nullptr);
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    if(!read(main,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||dos.e_lfanew>0x100000||
       !read(main+(uintptr_t)dos.e_lfanew,nt)||nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=0x8664||
       nt.OptionalHeader.Magic!=0x20b||nt.FileHeader.TimeDateStamp!=0x6ab3782c||nt.OptionalHeader.SizeOfImage!=0x5643000)return false;
    const unsigned char expected[]={0x48,0x8b,0x14,0xc1,0x0f,0xb6,0x8b,0x06,0x01,0,0,0xe8,0xaf,0x06,0xe6,0xff};
    const unsigned char wrapper[]={0x48,0x8b,0x52,0x70,0xe9,0xe7,0x80,0x03,0};
    unsigned char currentWrapper[9];
    patchAt=main+0x490151;
    if(!read(patchAt,saved)||memcmp(saved,expected,sizeof(saved))||
       !read(main+0x2f0810,currentWrapper)||memcmp(currentWrapper,wrapper,sizeof(wrapper)))return false;
    portalLookup=(void*)(main+0x2f0810);portalLookupResume=(void*)(main+0x490161);
    memset(patched,0x90,sizeof(patched));patched[0]=0xff;patched[1]=0x25;memset(patched+2,0,4);
    void* target=(void*)&portalLookupGuard;memcpy(patched+6,&target,8);
    DWORD old;
    if(!VirtualProtect((void*)patchAt,sizeof(patched),PAGE_EXECUTE_READWRITE,&old))return false;
    memcpy((void*)patchAt,patched,sizeof(patched));DWORD ignored;
    VirtualProtect((void*)patchAt,sizeof(patched),old,&ignored);
    FlushInstructionCache(GetCurrentProcess(),(void*)patchAt,sizeof(patched));active=true;return true;
}
static const D2RL::PluginInfo info{sizeof(D2RL::PluginInfo),4,"r3ni-map-portal-return-fix","Map Portal Return Fix","0.1.0","r3ni",
    "Guards the verified client portal-object lookup when its act is unloaded. Preserves normal lookup and native no-object handling.",
    D2RL::PluginFlags::ModScopedOnly|D2RL::PluginFlags::Client|D2RL::PluginFlags::NativeHooks,{}};
D2RL_PLUGIN_EXPORT const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &info;}
D2RL_PLUGIN_EXPORT bool D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept{
    DWORD error=GetLastError();bool ok=install();
    if(ctx && ctx->api && ctx->api->logInfo)ctx->api->logInfo(ctx,ok?
        "Map Portal Return Fix 0.1.0: verified client act-null guard installed at main+490151; normal lookup unchanged.":
        "Map Portal Return Fix 0.1.0: unsupported build or changed call-site signature; no changes applied.");
    SetLastError(error);return ok;
}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept{
    DWORD error=GetLastError();unsigned char now[16];
    if(active && read(patchAt,now) && !memcmp(now,patched,sizeof(now))){
        DWORD old;
        if(VirtualProtect((void*)patchAt,sizeof(saved),PAGE_EXECUTE_READWRITE,&old)){
            memcpy((void*)patchAt,saved,sizeof(saved));DWORD ignored;
            VirtualProtect((void*)patchAt,sizeof(saved),old,&ignored);
            FlushInstructionCache(GetCurrentProcess(),(void*)patchAt,sizeof(saved));active=false;
        }
    }
    SetLastError(error);
}
extern "C" BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(module);
    return TRUE;
}
