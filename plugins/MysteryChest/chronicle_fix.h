#pragma once
#include "chronicle_records.h"
#include "chronicle_witnesses.h"
#include <D2RLPlugin/context.h>
#include <D2RLPlugin/data_tables.h>
#include <D2RLPlugin/lifecycle_events.h>
#include <windows.h>
#include <cstring>
#include <atomic>

namespace ChronicleIntegration {
using namespace ChronicleFix;
using namespace ChronicleNative;
inline const D2RL::PluginContext* Context{};
inline const D2RL::DataTableService* Tables{};
inline std::atomic<bool> Ready{false};
inline SRWLOCK Lock=SRWLOCK_INIT;
struct OwnerEvidence {uint32_t id=0xffffffff;Pending items;};
inline OwnerEvidence SavedEvidence[16];
inline thread_local uint32_t RestoreOwner=0xffffffff;
inline std::atomic<uint64_t> Session{0};
inline thread_local uint32_t LastRestoreOwner=0xffffffff;
inline thread_local uintptr_t LastRestoreGame{};
inline thread_local uint64_t LastRestoreSession{};
using UniqueFn=int(__fastcall*)(void*,void*,void*);
using SetFn=void(__fastcall*)(void*,int);
using FindFn=void*(__fastcall*)(void*,uint8_t,uint32_t);
using InsertFn=void(__fastcall*)(void*,uint8_t,uint32_t,uint16_t,uint32_t);
using StageFn=bool(__fastcall*)(void*,const void*);
using DecodeFn=int(__fastcall*)(void*,const void*,size_t,int,void*,unsigned,void*);
using CreateFn=void*(__fastcall*)(void*,void*,int);
using ReadSavedFn=uint64_t(__fastcall*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
using LoadHistoryFn=int(__fastcall*)(void*,void*,void*);
using GetUniqueRecordFn=void*(__fastcall*)(uint8_t,int);
inline UniqueFn OriginalUnique{};inline SetFn OriginalSet{};
inline FindFn OriginalFind{};inline InsertFn OriginalInsert{};
inline StageFn OriginalStage{};inline DecodeFn OriginalDecode{};inline CreateFn OriginalCreate{};
inline ReadSavedFn OriginalReadSaved{};inline ReadSavedFn OriginalReadShared{};inline LoadHistoryFn OriginalLoadHistory{};
inline GetUniqueRecordFn OriginalGetUniqueRecord{};
inline thread_local unsigned RestoreDepth{};
inline std::atomic<uintptr_t> CurrentGame{0};
inline thread_local void* UniqueItem{};
inline thread_local int UniqueSource=-1;
alignas(16) inline thread_local uint8_t GenerationRecord[0x15c];
struct Capture {void* game{};void* item{};};
inline thread_local Capture* ActiveCapture{};

template<class T> bool Read(uintptr_t at,T& value) noexcept {
 SIZE_T n{};return at&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),&value,sizeof value,&n)&&n==sizeof value;
}
inline uint32_t MinutesNow() noexcept {
 FILETIME t;GetSystemTimeAsFileTime(&t);uint64_t v=(uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;
 return uint32_t((v-116444736000000000ULL)/600000000ULL);
}
inline unsigned Kind(void* item) noexcept {
 uintptr_t data{};unsigned quality{};
 if(!Read(reinterpret_cast<uintptr_t>(item)+0x10,data)||!Read(data,quality))return 2;
 return quality==5?0:quality==7?1:2;
}
inline bool Record(void* item,uint32_t& id) noexcept {
 uintptr_t data{};return Read(reinterpret_cast<uintptr_t>(item)+0x10,data)&&Read(data+0x34,id);
}
inline void Observe(void* unit) noexcept {
 unsigned type{},flags{},id{};uintptr_t data{},history{};Evidence e;
 auto u=reinterpret_cast<uintptr_t>(unit);
 if(!Read(u,type)||type!=4||!Read(u+0x10,data)||!Read(data+0x18,flags)||!(flags&0x10)||!Read(data+0x34,id))return;
 unsigned kind=Kind(unit);
 if(!valid(kind,id))return;
 if(!Read(data+0xd0,history)||!history||!Read(history,e.source)||!Read(history+4,e.minutes)||!e.source||!e.minutes)
  e={27229,MinutesNow()}; // Unknown source; this is a recovery date, not an invented original drop date.
 if(!valid(kind,id)||e.minutes<15778080||e.minutes>MinutesNow()+1440)return;
 if(RestoreOwner==0xffffffff)return;
 AcquireSRWLockExclusive(&Lock);
 OwnerEvidence* owner=nullptr;
 for(auto& x:SavedEvidence)if(x.id==RestoreOwner){owner=&x;break;}
 if(!owner)for(auto& x:SavedEvidence)if(x.id==0xffffffff){x.id=RestoreOwner;owner=&x;break;}
 if(owner)remember(owner->items,kind,id,e);
 ReleaseSRWLockExclusive(&Lock);
}
inline int __fastcall Decode(void* unit,const void* bytes,size_t n,int compact,void* used,unsigned version,void* error) noexcept {
 int result=OriginalDecode(unit,bytes,n,compact,used,version,error);
 if(Ready&&RestoreDepth&&result>0)Observe(unit);return result;
}
inline void* __fastcall Find(void* history,uint8_t kind,uint32_t id) noexcept {
 return OriginalFind(history,kind,Ready?canonical(kind,id):id);
}
inline void __fastcall Insert(void* history,uint8_t kind,uint32_t id,uint16_t source,uint32_t minutes) noexcept {
 if(Ready&&valid(kind,id)) {
  id=canonical(kind,id);
  if(void* entry=OriginalFind(history,kind,id)) {
   // Keep the earliest genuine first-find record when legacy identities merge.
   auto at=static_cast<uint8_t*>(entry);
   if(minutes&&(!get<uint32_t>(at+8)||minutes<get<uint32_t>(at+8))) {
    put<uint16_t>(at+6,source);put<uint32_t>(at+8,minutes);
   }
   return;
  }
 }
 OriginalInsert(history,kind,id,source,minutes);
}
inline void Recover(void* player) noexcept {
 if(!Ready||!player)return;
 auto p=reinterpret_cast<uintptr_t>(player);unsigned type{};uintptr_t data{};
 if(!Read(p,type)||type||!Read(p+0x10,data)||!data)return;
 void* history=reinterpret_cast<void*>(data+0x220);uint32_t playerId{};
 if(!Read(p+8,playerId))return;
 AcquireSRWLockShared(&Lock);
 for(const auto& owner:SavedEvidence)if(owner.id==playerId){
  for(unsigned i=0;i<SetCount;i++)if(auto e=owner.items.sets[i];e.source&&e.minutes)Insert(history,0,i,e.source,e.minutes);
  for(unsigned i=0;i<UniqueCount;i++)if(auto e=owner.items.uniques[i];e.source&&e.minutes)Insert(history,1,i,e.source,e.minutes);
 }
 ReleaseSRWLockShared(&Lock);
}
inline bool __fastcall Stage(void* player,const void* record) noexcept {
 bool result=OriginalStage(player,record);
 if(!Ready||!player||!record)return result;
 auto p=reinterpret_cast<uintptr_t>(player);unsigned type{};uintptr_t data{};
 if(!Read(p,type)||type||!Read(p+0x10,data)||!data)return result;
 auto r=static_cast<const uint8_t*>(record);unsigned kind=r[4],id=get<uint32_t>(r);
 if(!valid(kind,id))return result;
 Insert(reinterpret_cast<void*>(data+0x220),uint8_t(kind),id,get<uint16_t>(r+6),get<uint32_t>(r+8));
 // The archive loader aborts on false. A merged alias is a successfully read
 // record, even though the original duplicate check reports no insertion.
 return true;
}
inline uint64_t ReadOwnedItems(ReadSavedFn original,uintptr_t a,uintptr_t player,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f,uintptr_t g) noexcept {
 uint32_t priorOwner=RestoreOwner,type{},owner{};
 if(Read(player,type)) {
  if(type==0&&Read(player+8,owner)) {
   RestoreOwner=owner;LastRestoreOwner=owner;LastRestoreGame=a;LastRestoreSession=Session.load();
  } else if(type==1&&Read(player+4,owner)&&
      (owner==271||owner==338||owner==359||owner==560||owner==561)&&
      LastRestoreGame==a&&LastRestoreSession==Session.load()) {
   // Mercenary equipment is the continuation of the same character's saved
   // inventory, not another player's or a network-observed monster's gear.
   RestoreOwner=LastRestoreOwner;
  }
 }
 ++RestoreDepth;
 uint64_t result=original(a,player,c,d,e,f,g);
 --RestoreDepth;
 RestoreOwner=priorOwner;
 if(Ready) {unsigned type{};uint8_t bank{};
  if(Read(player,type)&&type==0&&Read(a+0x106,bank)&&bank==3)CurrentGame.store(a);
 }
 return result;
}
inline uint64_t __fastcall ReadSaved(uintptr_t a,uintptr_t player,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f,uintptr_t g) noexcept {
 return ReadOwnedItems(OriginalReadSaved,a,player,c,d,e,f,g);
}
inline uint64_t __fastcall ReadShared(uintptr_t game,uintptr_t player,uintptr_t version,uintptr_t bytes,uintptr_t count,uintptr_t used,uintptr_t page) noexcept {
 // Shared pages are restored through 41f0f0, not ReadItemsByVersion (41f0b0).
 // Bind this decode scope to the explicit restoring player on every page.
 return ReadOwnedItems(OriginalReadShared,game,player,version,bytes,count,used,page);
}
inline int __fastcall Load(void* a,void* archive,void* player) noexcept {
 int result=OriginalLoadHistory(a,archive,player);
 if(result==0)Recover(player);
 return result;
}
inline int __fastcall Unique(void* game,void* item,void* specification) noexcept {
 uint8_t bank{};bool active=Ready&&Read(reinterpret_cast<uintptr_t>(game)+0x106,bank)&&bank==3;
 void* prior=UniqueItem;int source=UniqueSource;if(active){UniqueItem=item;UniqueSource=-1;}
 int result=OriginalUnique(game,item,specification);
 UniqueItem=prior;UniqueSource=source;return result;
}
inline void __fastcall Set(void* item,int id) noexcept {
 // This runs between native selection and native property application. Saved
 // items, skill-specific Shard jewels and all RNG rolls retain their identities.
 if(Ready&&item==UniqueItem&&id>=0&&unsigned(id)<UniqueCount){
  int target=PhysicalCanonical[id];UniqueSource=target==id?-1:id;id=target;
 }
 OriginalSet(item,id);
}
inline void* __fastcall GetUniqueRecord(uint8_t bank,int id) noexcept {
 void* row=OriginalGetUniqueRecord(bank,id);
 if(!Ready||!row||bank!=3||!UniqueItem||UniqueSource<0||id!=PhysicalCanonical[UniqueSource])return row;
 void* selected=OriginalGetUniqueRecord(bank,UniqueSource);if(!selected)return row;
 std::memcpy(GenerationRecord,row,sizeof(GenerationRecord));
 // Selection already used the original source's weight, level and repeat rule.
 // Retain only that no-limit bit for the remainder of this generation, so an
 // allowed repeated legacy roll cannot fail after its ID becomes canonical.
 GenerationRecord[0x2c]=uint8_t((GenerationRecord[0x2c]&~2u)|(static_cast<uint8_t*>(selected)[0x2c]&2u));
 return GenerationRecord;
}
inline void* __fastcall Create(void* game,void* spec,int mode) noexcept {
 void* item=OriginalCreate(game,spec,mode);
 if(ActiveCapture&&item){ActiveCapture->game=game;ActiveCapture->item=item;}
 return item;
}
inline void Begin(Capture& capture) noexcept {capture={};ActiveCapture=&capture;}
inline void End() noexcept {ActiveCapture=nullptr;}

#pragma pack(push,1)
struct Packet {uint8_t opcode=0x87,action=0;uint32_t player{},item{};uint16_t record{};uint8_t kind{};uint32_t minutes{};uint32_t source=2231;};
#pragma pack(pop)
static_assert(sizeof(Packet)==21);
struct RewardDiscovery {uint32_t player{};void* item{};bool registered{};};
inline void __fastcall GiveDiscovery(void*,void* player,void* user) noexcept {
 auto& job=*static_cast<RewardDiscovery*>(user);uint32_t id{};
 if(!Read(reinterpret_cast<uintptr_t>(player)+8,id)||id!=job.player)return;
 unsigned kind=Kind(job.item);uint32_t record{};
 if(!valid(kind,0)||!Record(job.item,record)||!valid(kind,record))return;
 record=canonical(kind,record);
 const auto base=Context->exeBase;
 auto mark=reinterpret_cast<bool(__fastcall*)(void*,uint8_t,uint32_t,uint16_t,uint32_t)>(base+MarkPlayerRva);
 Packet packet;packet.player=id;packet.kind=uint8_t(kind);packet.record=uint16_t(record);packet.minutes=MinutesNow();
 Read(reinterpret_cast<uintptr_t>(job.item)+8,packet.item);
 mark(player,packet.kind,record,uint16_t(packet.source),packet.minutes);
 auto client=reinterpret_cast<void*(__fastcall*)(void*)>(base+GetClientRva)(player);
 if(client)reinterpret_cast<void(__fastcall*)(void*,const void*)>(base+SendPacketRva)(client,&packet);
 job.registered=true;
}
inline bool Discover(const Capture& capture,uint32_t playerId) noexcept {
 if(!Ready||!capture.game||!capture.item||playerId==0xffffffff)return false;
 unsigned kind=Kind(capture.item);if(kind>1)return true;
 RewardDiscovery job{playerId,capture.item,false};
 auto each=reinterpret_cast<void(__fastcall*)(void*,void(__fastcall*)(void*,void*,void*),void*)>(Context->exeBase+EachPlayerRva);
 each(capture.game,&GiveDiscovery,&job);
 if(job.registered)reinterpret_cast<void(__fastcall*)(void*)>(Context->exeBase+ClearItemHistoryRva)(capture.item);
 return job.registered;
}
struct RefreshRequest {uint32_t player;bool completed=false;};
inline void __fastcall RefreshPlayer(void*,void* player,void* data) noexcept {
 auto& request=*static_cast<RefreshRequest*>(data);uint32_t id{};uintptr_t playerData{};
 if(!Read(reinterpret_cast<uintptr_t>(player)+8,id)||id!=request.player||!Read(reinterpret_cast<uintptr_t>(player)+0x10,playerData)||!playerData)return;
 Recover(player);
 auto base=Context->exeBase;
 auto client=reinterpret_cast<void*(__fastcall*)(void*)>(base+GetClientRva)(player);
 if(!client)return;
 for(unsigned kind=0;kind<2;kind++) {
  auto map=playerData+0x220+kind*0x20;uint64_t buckets{};uintptr_t array{};
  if(!Read(map,buckets)||buckets>1048576||!Read(map+8,array)||!array)continue;
  unsigned visited=0;
  for(uint64_t i=0;i<buckets;i++) {
   uintptr_t node{};if(!Read(array+i*8,node))return;
   while(node) {
    if(++visited>65536)return;
    uint8_t entry[12];SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(node+0xc),entry,sizeof(entry),&copied)||copied!=sizeof(entry))return;
    unsigned record=get<uint32_t>(entry);
    if(valid(kind,record)) {
     Packet packet;packet.player=id;packet.item=0xffffffff;packet.kind=uint8_t(kind);packet.record=uint16_t(record);
     packet.source=get<uint16_t>(entry+6);packet.minutes=get<uint32_t>(entry+8);
     reinterpret_cast<void(__fastcall*)(void*,const void*)>(base+SendPacketRva)(client,&packet);
    }
    if(!Read(node,node))return;
   }
  }
 }
 request.completed=true;
}
inline bool Refresh(uint32_t playerId) noexcept {
 auto game=CurrentGame.load();if(!Ready||!game||playerId==0xffffffff)return false;
 RefreshRequest request{playerId,false};
 reinterpret_cast<void(__fastcall*)(void*,void(__fastcall*)(void*,void*,void*),void*)>(Context->exeBase+EachPlayerRva)(reinterpret_cast<void*>(game),&RefreshPlayer,&request);
 return request.completed;
}
inline void __cdecl TablesLoaded(const D2RL::PluginContext*,const D2RL::Lifecycle::DataTablesLoadedEvent*,void*) noexcept {
 D2RL::DataTables::TableView u{},s{};u.structSize=s.structSize=sizeof(u);
 bool ok=Tables->getTable(Context,D2RL::DataTables::Bank::Rotw,D2RL::DataTables::TableId::UniqueItems,&u)==D2RL::DataTables::Result::Success&&
  Tables->getTable(Context,D2RL::DataTables::Bank::Rotw,D2RL::DataTables::TableId::SetItems,&s)==D2RL::DataTables::Result::Success&&
  u.rows&&u.rowCount==UniqueCount&&u.rowSize==0x15c&&s.rowCount==SetCount;
 if(ok)for(unsigned i=0;i<UniqueCount;i++) {
  auto row=static_cast<const uint8_t*>(u.rows)+i*u.rowSize;
  if(get<uint16_t>(row)!=i||get<uint32_t>(row+0x28)!=UniqueCodes[i]){ok=false;break;}
 }
 Ready=ok;
 if(!ok)Context->LogError("MysteryChest: Chronicle table identity mismatch; reconciliation disabled.");
}
inline void Leave() noexcept {
 AcquireSRWLockExclusive(&Lock);for(auto& owner:SavedEvidence)owner=OwnerEvidence{};ReleaseSRWLockExclusive(&Lock);
 CurrentGame.store(0);
 Session.fetch_add(1);
}
template<class F,class G> bool Hook(uintptr_t rva,const unsigned char* bytes,G replacement,F* original) noexcept {
 return Context->InstallInlineHook(rva,bytes,20,static_cast<F>(replacement),original);
}
inline bool Install(const D2RL::PluginContext* ctx,const D2RL::DataTableService* tables,const D2RL::LifecycleService* life) noexcept {
 Context=ctx;Tables=tables;
 struct Witness {uintptr_t rva;const unsigned char* bytes;};
 const Witness witnesses[]={{UniqueRollRva,UniqueRollBytes},{RecordSetterRva,RecordSetterBytes},{HistoryFindRva,HistoryFindBytes},{HistoryInsertRva,HistoryInsertBytes},
  {HistoryStageRva,HistoryStageBytes},{DecodeItemRva,DecodeItemBytes},{CreateItemRva,CreateItemBytes},{EachPlayerRva,EachPlayerBytes},{MarkPlayerRva,MarkPlayerBytes},
  {GetClientRva,GetClientBytes},{SendPacketRva,SendPacketBytes},{ClearItemHistoryRva,ClearItemHistoryBytes},
  {ReadSavedItemsRva,ReadSavedItemsBytes},{ReadSharedPageRva,ReadSharedPageBytes},{LoadHistoryRva,LoadHistoryBytes},{GetUniqueRecordRva,GetUniqueRecordBytes}};
 for(auto w:witnesses)if(!ctx->CheckExpectedBytes(w.rva,w.bytes,20))return false;
 if(!Hook(UniqueRollRva,UniqueRollBytes,&Unique,&OriginalUnique)||!Hook(RecordSetterRva,RecordSetterBytes,&Set,&OriginalSet)||
    !Hook(HistoryFindRva,HistoryFindBytes,&Find,&OriginalFind)||!Hook(HistoryInsertRva,HistoryInsertBytes,&Insert,&OriginalInsert)||
    !Hook(HistoryStageRva,HistoryStageBytes,&Stage,&OriginalStage)||!Hook(DecodeItemRva,DecodeItemBytes,&Decode,&OriginalDecode)||
    !Hook(CreateItemRva,CreateItemBytes,&Create,&OriginalCreate)||
    !Hook(ReadSavedItemsRva,ReadSavedItemsBytes,&ReadSaved,&OriginalReadSaved)||
    !Hook(ReadSharedPageRva,ReadSharedPageBytes,&ReadShared,&OriginalReadShared)||
    !Hook(LoadHistoryRva,LoadHistoryBytes,&Load,&OriginalLoadHistory)||
    !Hook(GetUniqueRecordRva,GetUniqueRecordBytes,&GetUniqueRecord,&OriginalGetUniqueRecord))return false;
 D2RL::Lifecycle::DataTablesLoadedListener listener{sizeof(listener),0,&TablesLoaded,nullptr};D2RL::Lifecycle::ListenerHandle handle{};
 return life->registerDataTablesLoadedListener(ctx,&listener,&handle)==D2RL::Lifecycle::Result::Success;
}
}
