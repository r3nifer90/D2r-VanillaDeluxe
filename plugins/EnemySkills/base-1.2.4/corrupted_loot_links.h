#pragma once



namespace LegendNative::CorruptedLootLinks {
using namespace ES;
#include "corrupted_loot_plans.h"
struct Patch {uint32_t index=0,missing=0;bool used=false;alignas(8) uint8_t row[0x150]={};alignas(8) uint8_t entries[10][32]={};};
struct Scope;
struct Slot {volatile LONG thread=0;Scope* top=nullptr;};Slot slots[16];
struct Scope {Slot* slot=nullptr;Scope* previous=nullptr;uint32_t bank=0,uid=0;bool enabled=false;Patch patches[3];};
const D2RL::DataTableService* tables=nullptr;
bool Name(const uint8_t* row,const char* name) noexcept {for(uint32_t i=0;i<256;++i){if(row[0x50+i]!=(uint8_t)name[i])return false;if(!name[i])return true;}return false;}
const uint8_t* Row(const D2RL::DataTables::TableView& v,uint32_t i) noexcept {return (const uint8_t*)v.rows+(size_t)i*0x150;}
const uint8_t* Named(const D2RL::DataTables::TableView& v,const char* name,uint32_t& index) noexcept {
 for(uint32_t i=1;i<v.rowCount;++i)if(Name(Row(v,i),name)){index=i;return Row(v,i);}return nullptr;
}
uint32_t U32(const void* p,uint32_t offset) noexcept {uint32_t v=0;memcpy(&v,(const uint8_t*)p+offset,4);return v;}
uint64_t U64(const void* p,uint32_t offset) noexcept {uint64_t v=0;memcpy(&v,(const uint8_t*)p+offset,8);return v;}
void Put32(void* p,uint32_t offset,uint32_t value) noexcept {memcpy((uint8_t*)p+offset,&value,4);}
void Put64(void* p,uint32_t offset,uint64_t value) noexcept {memcpy((uint8_t*)p+offset,&value,8);}
bool Build(Patch& out,const Plan& plan,const D2RL::DataTables::TableView& v) noexcept {
 uint32_t index=0;const uint8_t* original=Named(v,plan.name,index);if(!original)return false;

 uint16_t unique=0;memcpy(&unique,original+0x2c,2);
 if(U32(original,0x18)!=1||U32(original,0x1c)||U32(original,0x20)!=~0u||unique!=1024)return false;
 const uint64_t count=U64(original,0x40),at=U64(original,0x38);if(count>plan.count||count>10||(count&&(!at||!Readable(at,(size_t)count*32))))return false;
 memcpy(out.row,original,sizeof(out.row));out.index=index;
 uint32_t cursor=0,classic=0,expanded=0,previousClassic=0,previousExpanded=0;
 for(uint32_t i=0;i<plan.count;++i){
  const uint8_t* prior=cursor<count?(const uint8_t*)at+cursor*32:nullptr;const Ref* ref=nullptr;
  for(uint32_t k=0;k<plan.refs;++k)if(plan.ref[k].slot==i)ref=&plan.ref[k];
  uint32_t childIndex=0;const uint8_t* child=ref?Named(v,ref->name,childIndex):nullptr;
  if(ref&&(!child||childIndex>65535||U32(child,0x20)!=~0u||(child[0x24]&0x40)||!U32(child,0x14)))return false;
  const bool present=prior&&(!ref||((prior[0xc]&7)==4&&U32(prior,8)==childIndex));
  uint32_t classicWeight=0;
  if(present){
   if(!ref&&(prior[0xc]&7)!=1)return false;


   const uint32_t expandedNow=U32(prior,4),classicNow=U32(prior,0);
   const uint8_t* next=cursor+1<count?prior+32:nullptr;
   const uint32_t expandedEnd=next?U32(next,4):U32(original,0x14);
   const uint32_t classicEnd=next?U32(next,0):U32(original,0x10);
   if(expandedNow!=previousExpanded||classicNow!=previousClassic||expandedEnd<expandedNow||expandedEnd-expandedNow!=plan.weights[i]||classicEnd<classicNow)return false;
   classicWeight=classicEnd-classicNow;previousClassic=classicEnd;previousExpanded=expandedEnd;
   memcpy(out.entries[i],prior,32);++cursor;
  }else{
   if(!ref)return false;
   Put32(out.entries[i],8,childIndex);out.entries[i][0xc]=4|(U32(child,0x10)?0:0x10);
   classicWeight=U32(child,0x10)?plan.weights[i]:0;++out.missing;
  }
  if(classicWeight>plan.weights[i])return false;
  Put32(out.entries[i],0,classic);Put32(out.entries[i],4,expanded);classic+=classicWeight;expanded+=plan.weights[i];
 }
 if(cursor!=count||previousExpanded!=U32(original,0x14)||previousClassic!=U32(original,0x10))return false;
 Put32(out.row,0x10,classic);Put32(out.row,0x14,expanded);Put64(out.row,0x38,(uint64_t)out.entries);Put64(out.row,0x40,plan.count);Put64(out.row,0x48,plan.count);
 out.used=out.missing!=0;return true;
}
void Enter(Scope& s,bool authorized,uint32_t bank,uint32_t uid) noexcept {
 const LONG thread=(LONG)GetCurrentThreadId();if(!thread)return;

 for(auto& slot:slots)if(InterlockedCompareExchange(&slot.thread,0,0)==thread){s.slot=&slot;break;}
 if(!s.slot)for(auto& slot:slots)if(InterlockedCompareExchange(&slot.thread,thread,0)==0){s.slot=&slot;break;}
 if(!s.slot)return;s.previous=s.slot->top;s.slot->top=&s;s.bank=bank;s.uid=uid;
 if(!authorized||bank<2||bank>3)return;
 if(!tables&&(ctx->QueryService(&tables)!=D2RL::ServiceQueryResult::Success||!tables))return;
 if(tables->serviceVersion!=1||tables->serviceSize<D2RL::DataTableServiceRequiredSize||!tables->getTable)return;
 D2RL::DataTables::TableView v={.structSize=D2RL::DataTables::TableViewSize};
 if(tables->getTable(ctx,(D2RL::DataTables::Bank)bank,D2RL::DataTables::TableId::TreasureClasses,&v)!=D2RL::DataTables::Result::Success||!v.rows||v.rowSize!=0x150||v.rowCount<2||v.rowCount>65535||!Readable((uintptr_t)v.rows,(size_t)v.rowCount*0x150))return;
 for(uint32_t i=0;i<3;++i)if(!Build(s.patches[i],plans[bank-2][i],v)){Log("ES_CORRUPTED_LOOT_LINKS_REJECT current_runtime_layout_or_weights_mismatch=1 native_drop_preserved=1",true);return;}
 s.enabled=true;uint32_t repaired=0;for(const auto& p:s.patches)repaired+=p.missing;
 if(repaired){Text t;t.Add("ES_CORRUPTED_LOOT_LINKS_READY uid=");t.Num(uid);t.Add(" bank=");t.Num(bank);t.Add(" missing_references=");t.Num(repaired);t.Add(" current_weights_preserved=1 tables_read_only=1 new_tables=0 reroll=0 scope=corrupted_leader_drop");Log(t.s);}
}
void Leave(Scope& s) noexcept {if(!s.slot)return;s.slot->top=s.previous;if(!s.previous)InterlockedExchange(&s.slot->thread,0);s.slot=nullptr;}
const void* Resolve(uint8_t bank,uint16_t index,const void* original) noexcept {
 const LONG thread=(LONG)GetCurrentThreadId();
 for(auto& slot:slots)if(InterlockedCompareExchange(&slot.thread,0,0)==thread){
  auto* s=slot.top;if(!s||!s->enabled||s->bank!=bank)return original;
  for(const auto& p:s->patches)if(p.used&&p.index==index){if(!original||!Name((const uint8_t*)original,plans[bank-2][&p-s->patches].name))return original;return p.row;}return original;
 }return original;
}
}
namespace LegendNative {
const void* __fastcall CorruptedTcLookup(uint8_t bank,uint16_t index,int32_t level) noexcept {
 const void* result=priorTcLookup(bank,index,level);return CorruptedLootLinks::Resolve(bank,index,result);
}
}
