#pragma once
bool Readable(uintptr_t p,size_t n) noexcept {
 if(!p || p+n<p)return false;
 uintptr_t end=p+n;
 while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)))return false;
  if(m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || !(m.Protect&0xff))return false;
  uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;
 }return true;
}
template<class T> bool Read(uintptr_t p,T& v) noexcept { if(!Readable(p,sizeof(T)))return false;memcpy(&v,reinterpret_cast<void*>(p),sizeof(T));return true; }
bool Bytes(uintptr_t p,const uint8_t* b,size_t n) noexcept {if(!Readable(p,n))return false;auto* a=reinterpret_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i)if(a[i]!=b[i])return false;return true;}
bool Executable(uintptr_t p) noexcept {MEMORY_BASIC_INFORMATION m={};return VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)) && m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
uintptr_t RipSlot(uintptr_t instruction) noexcept {int32_t disp=0;if(!Read(instruction+2,disp))return 0;uintptr_t p=instruction+6+disp,v=0;return Read(p,v) && Executable(v)?v:0;}
bool NativeContract(HMODULE core) noexcept {
 auto b=reinterpret_cast<uintptr_t>(core);coreBase=b;

 if(!Bytes(b+TerrorEncounters::GameDifficultyReadRva,TerrorEncounters::GameDifficultyRead,sizeof(TerrorEncounters::GameDifficultyRead)) || !Bytes(b+TerrorEncounters::TestSpawnBodyRva,TerrorEncounters::TestSpawnBody,sizeof(TerrorEncounters::TestSpawnBody)) || !Bytes(b+TerrorEncounters::SpawnOneBodyRva,TerrorEncounters::SpawnOneBody,sizeof(TerrorEncounters::SpawnOneBody)))return false;
 auto stub=reinterpret_cast<uintptr_t>(encounters->runTestSpawn);
 int32_t d=0;uint8_t opcode=0;
 if(!Read(stub+10,opcode)||opcode!=0xe8||!Read(stub+11,d)||stub+15+d!=b+TerrorEncounters::TestSpawnBodyRva)return false;

 getX=reinterpret_cast<PathCoordinateFn>(RipSlot(b+0x41163f));getY=reinterpret_cast<PathCoordinateFn>(RipSlot(b+0x411650));
 if(!getX || !getY || !Executable(b+TerrorEncounters::SpawnOneBodyRva))return false;
 spawnOne=reinterpret_cast<SpawnFn>(b+TerrorEncounters::SpawnOneBodyRva);
 getLevel=reinterpret_cast<Reimplementations::GetLevelIdFromUnitFn>(GetProcAddress(core,MAKEINTRESOURCEA(2000)));
 execute=reinterpret_cast<CoreExports::ExecuteConsoleCommandFn>(GetProcAddress(core,MAKEINTRESOURCEA(14)));
 using namespace TerrorBalance;
 if(!Bytes(b+UnitStatRva,UnitStatBytes,sizeof(UnitStatBytes)) || !Bytes(b+BaseStatRva,BaseStatBytes,sizeof(BaseStatBytes)) || !Bytes(b+SetStatRva,SetStatBytes,sizeof(SetStatBytes)))return false;
 if(reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1243)))!=b+UnitStatRva || reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1242)))!=b+BaseStatRva || reinterpret_cast<uintptr_t>(GetProcAddress(core,MAKEINTRESOURCEA(1249)))!=b+SetStatRva)return false;
 getBaseStat=reinterpret_cast<GetStatFn>(b+BaseStatRva);getUnitStat=reinterpret_cast<GetStatFn>(b+UnitStatRva);setStat=reinterpret_cast<SetStatFn>(b+SetStatRva);
 allocateOne=reinterpret_cast<AllocateFn>(RipSlot(b+0x411a18));
 return getLevel && execute && allocateOne && Executable(b+BaseStatRva) && Executable(b+SetStatRva);
}
