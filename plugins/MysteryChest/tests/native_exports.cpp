#include "../chronicle_fix.h"
using namespace ChronicleIntegration;
static D2RL::PluginContext TestContext{};
#define TEST_EXPORT extern "C" __declspec(dllexport)
TEST_EXPORT void test_setup(uintptr_t nativeBase,uintptr_t originalFind,uintptr_t originalInsert) {
 TestContext.exeBase=nativeBase;Context=&TestContext;Ready=true;Leave();
 OriginalFind=reinterpret_cast<FindFn>(originalFind);OriginalInsert=reinterpret_cast<InsertFn>(originalInsert);
 OriginalStage=reinterpret_cast<StageFn>(nativeBase+HistoryStageRva);OriginalSet=reinterpret_cast<SetFn>(nativeBase+RecordSetterRva);
 OriginalGetUniqueRecord=reinterpret_cast<GetUniqueRecordFn>(nativeBase+GetUniqueRecordRva);
}
TEST_EXPORT void test_insert(void* map,unsigned kind,unsigned id,unsigned source,unsigned minutes) {Insert(map,uint8_t(kind),id,uint16_t(source),minutes);}
TEST_EXPORT void* test_find(void* map,unsigned kind,unsigned id) {return Find(map,uint8_t(kind),id);}
TEST_EXPORT bool test_stage(void* player,void* entry) {return Stage(player,entry);}
TEST_EXPORT void test_observe(void* item,uint32_t owner) {RestoreOwner=owner;Observe(item);RestoreOwner=0xffffffff;}
TEST_EXPORT void test_recover(void* player) {Recover(player);}
static int __fastcall fake_decode(void*,const void*,size_t,int,void*,unsigned,void*) {return 1;}
static uint64_t __fastcall fake_saved(uintptr_t,uintptr_t,uintptr_t item,uintptr_t,uintptr_t,uintptr_t,uintptr_t) {Decode(reinterpret_cast<void*>(item),nullptr,1,0,nullptr,105,nullptr);return 0;}
TEST_EXPORT void test_saved_scope(void* game,void* player,void* item) {
 OriginalDecode=&fake_decode;OriginalReadSaved=&fake_saved;ReadSaved(reinterpret_cast<uintptr_t>(game),reinterpret_cast<uintptr_t>(player),reinterpret_cast<uintptr_t>(item),105,0,0,0);
}
TEST_EXPORT void test_network_decode(void* item) {OriginalDecode=&fake_decode;Decode(item,nullptr,1,0,nullptr,105,nullptr);}
TEST_EXPORT bool test_refresh(uint32_t id) {return Refresh(id);}
TEST_EXPORT bool test_discover(void* game,void* item,uint32_t id) {return Discover(Capture{game,item},id);}
TEST_EXPORT void test_set_record(void* item,int id,int fresh) {UniqueItem=fresh?item:nullptr;Set(item,id);UniqueItem=nullptr;}
TEST_EXPORT unsigned test_canonical(unsigned id) {return id<UniqueCount?UniqueCanonical[id]:0xffffffff;}
TEST_EXPORT unsigned test_physical(unsigned id) {return id<UniqueCount?PhysicalCanonical[id]:0xffffffff;}
TEST_EXPORT void test_original_set(uintptr_t fn) {OriginalSet=reinterpret_cast<SetFn>(fn);}
TEST_EXPORT void test_original_record(uintptr_t fn) {OriginalGetUniqueRecord=reinterpret_cast<GetUniqueRecordFn>(fn);}
static uintptr_t seenScope{},seenItem{};
TEST_EXPORT void test_set_hook(void* item,int id) {seenScope=reinterpret_cast<uintptr_t>(UniqueItem);seenItem=reinterpret_cast<uintptr_t>(item);Set(item,id);}
TEST_EXPORT uintptr_t test_seen_scope() {return seenScope;}
TEST_EXPORT uintptr_t test_seen_item() {return seenItem;}
TEST_EXPORT void* test_record_hook(uint8_t bank,int id) {return GetUniqueRecord(bank,id);}
TEST_EXPORT int test_unique(void* game,void* item,void* spec) {
 OriginalUnique=reinterpret_cast<UniqueFn>(Context->exeBase+UniqueRollRva);return Unique(game,item,spec);
}
TEST_EXPORT void* test_generation_record(int id,int source) {
 UniqueItem=reinterpret_cast<void*>(1);UniqueSource=source;void* row=GetUniqueRecord(3,id);
 UniqueItem=nullptr;UniqueSource=-1;return row;
}
TEST_EXPORT unsigned test_packet(void* bytes,unsigned player,unsigned id,unsigned source,unsigned time) {
 Packet packet;packet.player=player;packet.item=0xffffffff;packet.kind=1;packet.record=uint16_t(id);packet.source=source;packet.minutes=time;
 std::memcpy(bytes,&packet,sizeof(packet));return sizeof(packet);
}
