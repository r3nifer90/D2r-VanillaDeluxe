#pragma once

namespace CastContext {
using namespace ES;
using UsedFn=void*(__fastcall*)(const void*);
UsedFn getUsed=nullptr;
bool rejectedCodeCaptured=false;
constexpr uint8_t GetUsedCall[]={0xff,0x15,0xe6,0x18,0x33,0x00,0x48,0x85,0xc0};
struct Snapshot {bool available=false,used=false,row=false;uint32_t mode=0,id=0,baseLevel=0;};
void Hex(Text& t,uint64_t value) noexcept {const char* d="0123456789abcdef";char b[19]={'0','x'};for(uint32_t i=0;i<16;++i)b[2+i]=d[(value>>(60-4*i))&15];t.Add(b);}
void Bind() noexcept {
 getUsed=nullptr;



 uint64_t original=0;uintptr_t endpoint=0;
 if(Bytes(coreBase+0x3dd604,GetUsedCall,sizeof(GetUsedCall))&&Read(coreBase+0x70eef8,original)&&original==0x34ba40){endpoint=RipSlot(coreBase+0x3dd604);if(endpoint)getUsed=reinterpret_cast<UsedFn>(endpoint);}
 Text t;t.Add("ES_CAST_CONTEXT_READY version=1 observation_only=1 active_skill_getter=");t.Num(getUsed?1:0);t.Add(" native_getter=");Hex(t,endpoint);t.Add(" ordinary_procs_unchanged=1 fixed_skill_assignments_unchanged=1");Log(t.s);
}
Snapshot Capture(const void* unit) noexcept {
 Snapshot s;if(!unit)return s;Read(reinterpret_cast<uintptr_t>(unit)+0xc,s.mode);
 if(!getUsed)return s;s.available=true;void* used=getUsed(unit);s.used=used!=nullptr;
 uintptr_t row=0;int16_t id=-1;
 if(used&&Read(reinterpret_cast<uintptr_t>(used),row)&&row&&Read(row,id)&&id>=0){s.row=true;s.id=static_cast<uint16_t>(id);Read(reinterpret_cast<uintptr_t>(used)+0x40,s.baseLevel);}
 return s;
}
void Id(Text& t,const Snapshot& s) noexcept {if(!s.available)t.Add("unavailable");else if(!s.used)t.Add("none");else if(!s.row)t.Add("unreadable");else t.Num(s.id);}
void Code(const char* label,uintptr_t address,uint32_t n) noexcept {
 return;
}
void After(void* unit,void* target,uint32_t uid,uint32_t monster,uint32_t skillId,uint32_t level,uint32_t kind,uint32_t distance,uint64_t now,const Snapshot& before,int32_t result) noexcept {
 return;
}
}
