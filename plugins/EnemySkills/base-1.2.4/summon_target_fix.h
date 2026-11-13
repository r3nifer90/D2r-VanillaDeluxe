#pragma once
#include <stdint.h>
namespace SummonTargetFix {
using Fn=uintptr_t(__fastcall*)(void*,void*,int32_t,int32_t);
uintptr_t base=0;unsigned char* callsite=nullptr;void* bridge=nullptr;unsigned char replacement[5]{};bool installed=false;
constexpr unsigned char originalCall[]={0xe8,0x01,0x7e,0x01,0x00};
bool Read(uintptr_t p,void* out,SIZE_T n){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&got)&&got==n;}
uintptr_t __fastcall Hook(void* game,void* pet,int32_t value,int32_t index){
 if(game&&pet&&index==11&&*static_cast<const uint32_t*>(pet)==1)return 0;

 return reinterpret_cast<Fn>(base+0x58d950)(game,pet,value,index);
}
bool Same(const void* a,const void* b,size_t n){const auto* x=static_cast<const unsigned char*>(a);const auto* y=static_cast<const unsigned char*>(b);for(size_t i=0;i<n;++i)if(x[i]!=y[i])return false;return true;}
bool WriteCall(const unsigned char* expected,const unsigned char* bytes){
 unsigned char now[5]{};if(!Read(reinterpret_cast<uintptr_t>(callsite),now,5)||!Same(now,expected,5))return false;
 DWORD old=0;if(!VirtualProtect(callsite,5,PAGE_EXECUTE_READWRITE,&old))return false;
 memcpy(callsite,bytes,5);FlushInstructionCache(GetCurrentProcess(),callsite,5);
 DWORD ignored=0;
 if(!VirtualProtect(callsite,5,old,&ignored)){
  memcpy(callsite,expected,5);FlushInstructionCache(GetCurrentProcess(),callsite,5);VirtualProtect(callsite,5,old,&ignored);return false;
 }
 return true;
}
bool Install(uintptr_t image){
 if(installed)return base==image;
 base=image;callsite=reinterpret_cast<unsigned char*>(base+0x575b4a);
 unsigned char actual[5]{};if(!Read(reinterpret_cast<uintptr_t>(callsite),actual,5)||!Same(actual,originalCall,5))return false;

 const unsigned char guard[]={0x83,0xfe,0x08,0x7c,0x1d};unsigned char got[5]{};
 if(!Read(base+0x58d9d2,got,5)||!Same(guard,got,5))return false;
 const uintptr_t centre=reinterpret_cast<uintptr_t>(callsite)&~uintptr_t(0xffff);
 for(uintptr_t distance=0x10000;distance<0x7fff0000&&!bridge;distance+=0x10000){
  const uintptr_t choices[]={centre>=distance?centre-distance:0,centre+distance};
  for(uintptr_t address:choices){
   const int64_t delta=static_cast<int64_t>(address)-static_cast<int64_t>(reinterpret_cast<uintptr_t>(callsite)+5);
   if(!address||delta<INT32_MIN||delta>INT32_MAX)continue;
   bridge=VirtualAlloc(reinterpret_cast<void*>(address),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(bridge)break;
  }
 }
 if(!bridge)return false;

 unsigned char jump[14]={0xff,0x25,0,0,0,0};const uintptr_t hook=reinterpret_cast<uintptr_t>(Hook);memcpy(jump+6,&hook,8);memcpy(bridge,jump,sizeof(jump));
 DWORD previous=0;if(!VirtualProtect(bridge,4096,PAGE_EXECUTE_READ,&previous)){VirtualFree(bridge,0,MEM_RELEASE);bridge=nullptr;return false;}
 FlushInstructionCache(GetCurrentProcess(),bridge,sizeof(jump));replacement[0]=0xe8;
 const int32_t relative=static_cast<int32_t>(reinterpret_cast<intptr_t>(bridge)-reinterpret_cast<intptr_t>(callsite+5));memcpy(replacement+1,&relative,4);
 if(!WriteCall(originalCall,replacement)){VirtualFree(bridge,0,MEM_RELEASE);bridge=nullptr;return false;}
 installed=true;return true;
}
void Remove(){
 if(installed&&WriteCall(replacement,originalCall)){
  if(bridge)VirtualFree(bridge,0,MEM_RELEASE);bridge=nullptr;
 }

 installed=false;
}
}
