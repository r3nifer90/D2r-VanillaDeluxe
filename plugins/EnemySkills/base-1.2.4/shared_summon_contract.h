#pragma once




namespace SharedSummonContract {
using namespace ES;
bool Jump(uintptr_t p,uintptr_t& destination) noexcept {
 uint8_t op[2]={};if(!Executable(p)||!Read(p,op))return false;
 if(op[0]==0xe9){int32_t d=0;if(!Read(p+1,d))return false;destination=p+5+static_cast<int64_t>(d);}
 else if(op[0]==0xff&&op[1]==0x25){int32_t d=0;if(!Read(p+2,d)||!Read(p+6+static_cast<int64_t>(d),destination))return false;}
 else return false;
 return destination!=p&&Executable(destination);
}
bool Accepted(uint32_t rva,const uint8_t* expected,uint32_t size) noexcept {
 if(size!=20||(rva!=0x4350e0&&rva!=0x432b70))return false;
 const uintptr_t module=reinterpret_cast<uintptr_t>(GetModuleHandleW(L"BindAndSummon.dll"));
 uint16_t mz=0;uint32_t nt=0,signature=0,stamp=0,image=0;
 if(!module||!Read(module,mz)||mz!=0x5a4d||!Read(module+0x3c,nt)||nt>0x400||!Read(module+nt,signature)||signature!=0x4550||!Read(module+nt+8,stamp)||stamp!=1789077713u||!Read(module+nt+0x50,image)||image!=0x2e000)return false;
 static constexpr uint8_t spawn[]={0x48,0x89,0x5c,0x24,0x08,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xd9,0x48,0x81,0xec,0xb0,0,0,0,0x48,0x8b,0xda,0x4c};
 static constexpr uint8_t coordinates[]={0x48,0x83,0xec,0x28,0x44,0x8b,0x15,0x3d,0xd6,0x01,0,0x65,0x48,0x8b,0x04,0x25,0x58,0,0,0,0x41,0xbb,0x10,0,0,0,0x4a,0x8b,0x04,0xd0,0x4d,0x8b};
 const uintptr_t hook=module+(rva==0x4350e0?0xb930u:0xb520u),slot=module+(rva==0x4350e0?0x28520u:0x28528u);
 if(!Executable(hook)||!Bytes(hook,rva==0x4350e0?spawn:coordinates,32))return false;
 const uintptr_t entry=ctx->exeBase+rva;uintptr_t at=entry;
 for(uint32_t i=0;i<3&&at!=hook;++i){uintptr_t next=0;if(!Jump(at,next))return false;at=next;}
 if(at!=hook)return false;
 uintptr_t original=0;if(!Read(slot,original)||original==entry||original==hook||!Executable(original))return false;




 const uint32_t stolen[]={5,rva==0x4350e0?16u:15u};
 for(uint32_t n:stolen){uintptr_t resume=0;
  if(Bytes(original,expected,n)&&Bytes(entry+n,expected+n,size-n)&&Jump(original+n,resume)&&resume==entry+n)return true;
 }
 return false;
}
}
