#pragma once
namespace BossFrame {
constexpr uintptr_t Rva=0x1a5cd0,UiReturn=0xcfc2b;
uintptr_t base=0;
using Predicate=int32_t(__fastcall*)(void*);
Predicate original=nullptr;
bool Bytes(uintptr_t address,const unsigned char* bytes,unsigned count){
 unsigned char found[32]{};SIZE_T got=0;
 if(count>sizeof(found)||!ReadProcessMemory(GetCurrentProcess(),(void*)address,found,count,&got)||got!=count)return false;
 for(unsigned i=0;i<count;++i)if(found[i]!=bytes[i])return false;return true;
}
bool Contracts(){
 constexpr unsigned char predicate[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x85,0xc9,0x74,0x1e};
 constexpr unsigned char caller[]={0x48,0x8b,0xce,0xe8,0xa5,0x60,0x0d,0x00,0x85,0xc0};
 return Bytes(base+Rva,predicate,sizeof(predicate))&&Bytes(base+0xcfc23,caller,sizeof(caller));
}
bool Legend(uint32_t type,uint32_t kind){


 return type==1&&((kind>=1779&&kind<2099)||(kind>=2102&&kind<2278));
}
int32_t Resolve(void* unit,uintptr_t caller,int32_t prior){
 if(prior||caller!=base+UiReturn||!unit)return prior;
 uint32_t identity[2]{};SIZE_T got=0;
 if(!ReadProcessMemory(GetCurrentProcess(),unit,identity,sizeof(identity),&got)||got!=sizeof(identity))return prior;
 return Legend(identity[0],identity[1])?1:prior;
}
__attribute__((noinline)) int32_t __fastcall Unique(void* unit){
 const uintptr_t caller=(uintptr_t)__builtin_return_address(0);
 const int32_t prior=original(unit);const DWORD error=GetLastError();
 const int32_t result=Resolve(unit,caller,prior);SetLastError(error);return result;
}
}
#ifdef HEALTH_LAYOUT_FIXTURE
extern "C" __declspec(dllexport) int32_t BossFrameFixtureResolve(void* unit,uintptr_t caller,int32_t prior,uintptr_t base){BossFrame::base=base;return BossFrame::Resolve(unit,caller,prior);}
extern "C" __declspec(dllexport) void BossFrameFixtureSetup(uintptr_t base,BossFrame::Predicate prior){BossFrame::base=base;BossFrame::original=prior;}
extern "C" __declspec(dllexport) int32_t BossFrameFixtureHook(void* unit){return BossFrame::Unique(unit);}
#endif
