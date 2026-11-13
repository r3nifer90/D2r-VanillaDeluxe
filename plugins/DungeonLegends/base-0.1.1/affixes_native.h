#pragma once

namespace Affixes {
using AssignFn=void(__fastcall*)(void*,void*,int32_t,int32_t);
using BankFn=uintptr_t(__fastcall*)(uint8_t);
using EligibleFn=int32_t(__fastcall*)(void*,const void*,uint32_t);
BankFn bankTables=nullptr;EligibleFn eligible=nullptr;AssignFn assign=nullptr;
struct Saved {void* game=nullptr;uint32_t uid=0,kind=0;uint8_t mods[4]={};};
Saved saved[16384];uint32_t savedCount=0;
void Reset() noexcept {savedCount=0;}
bool Bind() noexcept {
 const uint8_t picker[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x55,0x53,0x48,0x81,0xec,0x48,0x01,0x00,0x00};
 const uint8_t call[]={0xff,0x15,0x18,0x8f,0x2f,0x00},bankCall[]={0xff,0x15,0x0d,0x55,0x2f,0x00},eligibleCall[]={0xff,0x15,0x73,0xb5,0x2f,0x00};
 const uint8_t count[]={0x4d,0x39,0xaf,0x50,0x14,0x00,0x00},rows[]={0x49,0x8b,0x87,0x48,0x14,0x00,0x00};
 const uint8_t stride[]={0x48,0x83,0xc6,0x20},disabled[]={0x80,0x7c,0x30,0xf0,0x00};
 const uint8_t weights[]={0x8b,0x4c,0x30,0xfc,0x89,0x4c,0x24,0x28,0x0f,0xb7,0x0c,0x30,0x66,0x89,0x4c,0x24,0x2c};
 if(!Bytes(coreBase+0x40c8c0,picker,sizeof(picker))||!Bytes(coreBase+0x40f0aa,call,sizeof(call))||
    !Bytes(coreBase+0x40c95d,bankCall,sizeof(bankCall))||!Bytes(coreBase+0x40ca1f,eligibleCall,sizeof(eligibleCall))||
    !Bytes(coreBase+0x40c9d0,count,sizeof(count))||!Bytes(coreBase+0x40c9df,rows,sizeof(rows))||
    !Bytes(coreBase+0x40c9c3,stride,sizeof(stride))||!Bytes(coreBase+0x40c9e6,disabled,sizeof(disabled))||!Bytes(coreBase+0x40c9ed,weights,sizeof(weights)))return false;
 bankTables=reinterpret_cast<BankFn>(RipSlot(coreBase+0x40c95d));eligible=reinterpret_cast<EligibleFn>(RipSlot(coreBase+0x40ca1f));assign=reinterpret_cast<AssignFn>(RipSlot(coreBase+0x40f0aa));
 return bankTables&&eligible&&assign;
}


bool NativePool(void* game,void* unit,const uint8_t* old,uint16_t* pool,uint32_t& count) noexcept {
 uint8_t bank=255,difficulty=255;
 if(!Read(reinterpret_cast<uintptr_t>(game)+0x106,bank)||bank<2||bank>3||!Read(reinterpret_cast<uintptr_t>(game)+0x104,difficulty)||difficulty!=2)return false;
 const uintptr_t tables=bankTables(bank);uintptr_t rows=0;uint64_t total=0;
 if(!tables||!Read(tables+0x1448,rows)||!rows||!Read(tables+0x1450,total)||total>4096)return false;
 for(uint16_t id:SPAffixPool::NativeIds){
  bool present=false;for(uint32_t n=0;n<9;++n)if(old[n]==id)present=true;if(present||id>=total)continue;
  const uintptr_t row=rows+id*32;uint8_t disabled=255;int16_t frequency=0;
  if(!Readable(row,32)||!Read(row+8,disabled)||!Read(row+20+difficulty*2,frequency))return false;
  if(!disabled&&frequency>0&&eligible(unit,reinterpret_cast<const void*>(row),bank!=1))pool[count++]=id;
 }
 return true;
}
bool Select(void* game,void* unit,const uint8_t* old,uint8_t* mods,uint32_t& nativeCount,uint32_t& candidateCount) noexcept {
 uint16_t pool[SPAffixPool::MaxCandidates]={},chosen[SPAffixPool::Slots]={};uint32_t count=0;
 if(!NativePool(game,unit,old,pool,count))return false;
 candidateCount=count;
 if(!SPAffixPool::Draw(pool,count,SPAffixPool::Slots,chosen,[](uint32_t n) noexcept {return Rand(n);}))return false;
 for(uint32_t n=0;n<SPAffixPool::Slots;++n)mods[nativeCount++]=static_cast<uint8_t>(chosen[n]);
 return true;
}
const char* Name(uint8_t id) noexcept {
 switch(id){case 5:return "Extra Strong";case 6:return "Extra Fast";case 7:return "Cursed";case 8:return "Magic Resistant";case 9:return "Fire Enchanted";case 17:return "Lightning Enchanted";case 18:return "Cold Enchanted";case 25:return "Mana Burn";case 26:return "Teleportation";case 27:return "Spectral Hit";case 28:return "Stone Skin";case 29:return "Multiple Shots";case 30:return "Aura Enchanted";default:return "unknown";}
}
bool Apply(void* game,void* unit) noexcept {
 uintptr_t data=0;const uintptr_t u=reinterpret_cast<uintptr_t>(unit);
 if(!Read(u+0x10,data)||!data||!Writable(data+0x1a,2)||!Writable(data+0x20,9))return false;
 uint8_t old[9]={};uint16_t flags=0;if(!Read(data+0x1a,flags)||!Read(data+0x20,old))return false;
 uint32_t used=0;while(used<9&&old[used])++used;if(used>5)return false;
 for(uint32_t i=used;i<9;++i)if(old[i])return false;
 if(savedCount>=16384)return false;
 uint8_t mods[9]={};uint32_t nativeCount=0,candidateCount=0;
 if(!Select(game,unit,old,mods,nativeCount,candidateCount))return false;



 *reinterpret_cast<uint16_t*>(data+0x1a)=flags|8;
 for(uint32_t i=0;i<nativeCount;++i)assign(game,unit,mods[i],1);
 uint8_t after[9]={};Read(data+0x20,after);
 bool ok=true;for(uint32_t i=0;i<used;++i)if(after[i]!=old[i])ok=false;
 for(uint32_t i=0;i<nativeCount;++i)if(after[used+i]!=mods[i])ok=false;
 for(uint32_t i=used+nativeCount;i<9;++i)if(after[i])ok=false;
 if(ok&&!Names::Apply(unit))ok=false;
 if(ok){uint32_t uid=0,kind=0;if(Read(u+8,uid)&&Read(u+4,kind)){auto& entry=saved[savedCount++];entry={};entry.game=game;entry.uid=uid;entry.kind=kind;memcpy(entry.mods,mods,4);}}


 Text t;t.Add(ok?"SP_AFFIXES class=":"SP_AFFIXES_PARTIAL class=");uint32_t id=0;Read(u+4,id);t.Num(id);t.Add(" count=");t.Num(nativeCount);t.Add(" total_affixes=4 native_only=1 eligible_count=");t.Num(candidateCount);t.Add(" ids=");
 for(uint32_t i=0;i<nativeCount;++i){if(i)t.Add(",");t.Num(mods[i]);}t.Add(" names=");
 for(uint32_t i=0;i<nativeCount;++i){if(i)t.Add("|");t.Add(Name(mods[i]));}Log(t.s,!ok);
 return ok;
}
void Restore(void* game,void* unit) noexcept {
 if(!cfg.enabled||!expectedGeneration||serverGeneration!=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0)))return;
 uintptr_t u=reinterpret_cast<uintptr_t>(unit),data=0;uint32_t type=99,uid=0,kind=0;
 if(!Read(u,type)||type!=1||!Read(u+4,kind)||kind<TLData::FirstMon||kind>=TLData::TotalMon||!Read(u+8,uid))return;
 const Saved* record=nullptr;for(uint32_t i=0;i<savedCount;++i)if(saved[i].game==game&&saved[i].uid==uid&&saved[i].kind==kind){record=&saved[i];break;}if(!record)return;
 uint8_t current[9]={};uint16_t flags=0;
 if(!Read(u+0x10,data)||!data||!Writable(data+0x1a,2)||!Writable(data+0x20,9)||!Read(data+0x20,current)||!Read(data+0x1a,flags))return;
 uint32_t used=0;while(used<9&&current[used])++used;
 for(uint32_t i=used;i<9;++i)if(current[i])return;
 uint32_t missing=0;bool present[4]={};for(uint32_t i=0;i<4&&record->mods[i];++i){for(uint8_t id:current)if(id==record->mods[i])present[i]=true;if(!present[i])++missing;}
 if(used+missing>9)return;
 *reinterpret_cast<uint16_t*>(data+0x1a)=flags|8;
 for(uint32_t i=0;i<4&&record->mods[i];++i)if(!present[i])assign(game,unit,record->mods[i],1);
 Names::Apply(unit);
 if(missing){Text t;t.Add("SP_AFFIXES_RESTORE class=");t.Num(kind);t.Add(" uid=");t.Num(uid);t.Add(" reapplied=");t.Num(missing);t.Add(" native_only=1 rerolled=0");Log(t.s);}
}
}
