#pragma once



namespace EndgameSets {
using StatReader=int32_t(__fastcall*)(void*,int32_t,int32_t);
inline StatReader getStat=nullptr;
inline volatile LONG64 replacements=0;
inline bool Bind(uintptr_t core) noexcept {
    constexpr uint8_t wrapper[]={0x48,0x83,0xec,0x28,0xe8,0xe7,0x6d,0xba,0xff,0x89,0xc0,0x48,0x83,0xc4,0x28,0xc3};
    if(!SP::Match(core+0x831de0,wrapper))return false;
    auto endpoint=GetProcAddress(reinterpret_cast<HMODULE>(core),MAKEINTRESOURCEA(1243));
    if(reinterpret_cast<uintptr_t>(endpoint)!=core+0x831de0)return false;
    getStat=reinterpret_cast<StatReader>(endpoint);return true;
}
struct MissileChoice {
    void* parameters=nullptr;int32_t before=0,after=0;
    explicit MissileChoice(void* p) noexcept {
        SPD::PreserveError preserve;
        if(!getStat||!p)return;
        void* owner=nullptr;int32_t id=0,skill=0,level=0;




        if(!SP::Read(p,8,owner)||!SP::Player(owner)||!SP::Read(p,0x20,id)
          ||!SP::Read(p,0x3c,skill)||!SP::Read(p,0x40,level)||level<1||level>255)return;
        const bool holy=(id==55&&skill==101)||(id==234&&skill==121);
        const bool blade=(id==506||id==508)&&skill==266;
        if(!holy&&!blade)return;
        if(getStat(owner,holy?456:457,0)<=0)return;
        MEMORY_BASIC_INFORMATION m={};
        const uintptr_t slot=reinterpret_cast<uintptr_t>(p)+0x20;
        if(!VirtualQuery(reinterpret_cast<void*>(slot),&m,sizeof(m))||m.State!=MEM_COMMIT
           ||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))||!(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return;
        parameters=p;before=id;after=holy?(id==55?755:756):(id==506?757:758);
        memcpy(reinterpret_cast<uint8_t*>(p)+0x20,&after,sizeof(after));
        InterlockedIncrement64(&replacements);
    }
    ~MissileChoice() noexcept {
        SPD::PreserveError preserve;
        if(parameters)memcpy(reinterpret_cast<uint8_t*>(parameters)+0x20,&before,sizeof(before));
    }
    MissileChoice(const MissileChoice&)=delete;
    MissileChoice& operator=(const MissileChoice&)=delete;
};
}
