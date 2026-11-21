// Host-only unit harness. Never installed into mods. Tests actual plugin code
// against mocked SDK services; it cannot validate the native D2R engine.
#include "../MysteryChest.cpp"
#include <cassert>
#include <vector>
#include <iostream>
using namespace D2RL;
static ItemService itemApi{};static InventoryService invApi{};static ThreadService threadApi{};
static NetworkService netApi{};static DataTableService tablesApi{};
static PluginApi api{};static PluginContext ctx{};
static std::vector<std::pair<Threads::Callback,void*>> pending;
static std::vector<unsigned char> uniques(2200*16),sets(300*16);
static Items::ItemInfo chest{};static int chestQuantity=1,commits=0,attempts=0;
static bool nativeLayout=false;
static std::vector<unsigned char> nativeU(UniqueCount*0x15c),nativeS(SetCount*0x15c);
static bool tableMismatch=false,transactionFails=false,remote=false,missingCode=false;
static Items::ItemCreateSpec last{};static PlayerHandle lastPlayer{};static bool extra=false,moveFails=false;static int nativeCalls=0,moves=0;
void drain(){auto copy=pending;pending.clear();for(auto [fn,p]:copy)fn(C,p);assert(!Jobs);}
void log(const PluginContext*,const char*) noexcept {}
Items::Result getInfo(const PluginContext*,ItemHandle,Items::ItemInfo* out) noexcept {*out=chest;out->handle=13;out->itemSeed=100;out->generationSeed=200;return Items::Result::Success;}
Threads::Result schedule(const PluginContext*,Threads::Callback cb,void* p) noexcept {
 if(remote)return Threads::Result::Unavailable;pending.emplace_back(cb,p);return Threads::Result::Success;
}
Inventory::Result localPlayer(const PluginContext*,PlayerHandle* out) noexcept {*out=42;return Inventory::Result::Success;}
void nativeTransmute() noexcept {++nativeCalls;}
Inventory::Result each(const PluginContext* c,PlayerHandle player,const Inventory::ItemFilter* f,Inventory::ItemCallback cb,void* p) noexcept {
 if(player==42){
  if(chestQuantity&&(f->containerMask&Items::ContainerBit(chest.container))){chest.quantity=chestQuantity;cb(c,&chest,p);}
  if(extra){auto material=chest;material.code=Items::MakeItemCode("r01");material.runtimeId=999;material.handle=999;material.container=Items::ItemContainer::Cube;cb(c,&material,p);}
 }
 return Inventory::Result::Success;
}
Items::Result moveItem(const PluginContext*,const Items::ExistingItemTransaction* tx,Items::ExistingItemTransactionResult*) noexcept {
 assert(tx->player==42&&tx->operationCount==1);auto& op=*tx->operations;
 assert(op.kind==Items::ExistingItemOperationKind::Move&&op.item==chest.handle);
 assert(op.move.destination.container==Items::ItemContainer::Cube&&op.move.destination.placement==Items::Placement::Exact);
 if(moveFails)return Items::Result::Conflict;
 ++moves;chest.x=op.move.destination.x;chest.y=op.move.destination.y;return Items::Result::Success;
}
Items::Result exchange(const PluginContext*,const Items::Transaction* tx,Items::TransactionResult* r) noexcept {
 ++attempts;last=*tx->outputs;lastPlayer=tx->player;
 assert(tx->inputCount==1 && tx->outputCount==1 && tx->outputCapacity==1);
 assert(tx->inputs->item==chest.handle && tx->inputs->quantity==1);
 assert(last.destination.container==Items::ItemContainer::Cube);
 assert(last.destination.placement==Items::Placement::Automatic);
 assert(last.propertyCount==0 && last.properties==nullptr);
 if(transactionFails)return Items::Result::InvalidArgument;
 --chestQuantity;++commits;r->outputCount=1;*tx->outputItems=987;return Items::Result::Success;
}
DataTables::Result table(const PluginContext*,DataTables::Bank bank,DataTables::TableId id,DataTables::TableView* out) noexcept {
 assert(bank==DataTables::Bank::Rotw);out->revision=tableMismatch?2:1;
 if(nativeLayout){out->rowSize=0x15c;out->rowCount=id==DataTables::TableId::UniqueItems?UniqueCount:SetCount;out->rows=id==DataTables::TableId::UniqueItems?nativeU.data():nativeS.data();return DataTables::Result::Success;}

 out->rowSize=16;out->rowCount=id==DataTables::TableId::UniqueItems?2200:300;
 out->rows=id==DataTables::TableId::UniqueItems?uniques.data():sets.data();
 if(tableMismatch)out->rows=sets.data(),out->rowCount=300;
 return DataTables::Result::Success;
}
DataTables::Result code(const PluginContext*,DataTables::Bank,DataTables::TableId id,uint32_t,DataTables::RowView*) noexcept {
 assert(id==DataTables::TableId::Items);return missingCode?DataTables::Result::NotFound:DataTables::Result::Success;
}
Network::Result peerPlayer(const PluginContext*,Network::ChannelHandle,Network::PeerHandle peer,PlayerHandle* p) noexcept {
 if(peer!=99)return Network::Result::NotFound;*p=42;return Network::Result::Success;
}
Network::Result connect(const PluginContext*,Network::ChannelHandle) noexcept {return Network::Result::Success;}
static Request sent{};static bool transportFails=false;
Network::Result toHost(const PluginContext*,Network::ChannelHandle,uint16_t id,const void* p,uint32_t s) noexcept {
 assert(id==1&&s==sizeof(sent));std::memcpy(&sent,p,s);return transportFails?Network::Result::Unavailable:Network::Result::Success;
}
Network::Result toClient(const PluginContext*,Network::ChannelHandle,Network::PeerHandle,uint16_t,const void*,uint32_t) noexcept {return Network::Result::Success;}
static LifecycleService lifeApi{};
static Lifecycle::DataTablesLoadedListener loaded{};
static unsigned hookCount=0,gameListeners=0;
static std::vector<unsigned char> nativeImage(0x600000);
ServiceQueryResult query(const PluginContext*,ServiceId id,uint32_t,const void** out) noexcept {
 switch(id){
 case ServiceId::Item:*out=&itemApi;break;case ServiceId::Inventory:*out=&invApi;break;
 case ServiceId::Thread:*out=&threadApi;break;case ServiceId::Network:*out=&netApi;break;
 case ServiceId::DataTable:*out=&tablesApi;break;case ServiceId::Lifecycle:*out=&lifeApi;break;
 default:return ServiceQueryResult::UnknownService;
 }return ServiceQueryResult::Success;
}
bool checkBytes(const PluginContext*,uint64_t rva,const void* expected,uint32_t n) noexcept {
 return !std::memcmp(nativeImage.data()+rva,expected,n);
}
bool installHook(const PluginContext*,const InlineHookRegistration* h) noexcept {
 assert(checkBytes(nullptr,h->rva,h->expected,h->expectedSize));
 ++hookCount;if(h->original)*h->original=nativeImage.data()+h->rva;return true;
}
Lifecycle::Result registerTables(const PluginContext*,const Lifecycle::DataTablesLoadedListener* listener,Lifecycle::ListenerHandle* handle) noexcept {
 loaded=*listener;*handle=1;return Lifecycle::Result::Success;
}
Lifecycle::Result registerGame(const PluginContext*,const Lifecycle::GameplayEventListener*,Lifecycle::ListenerHandle* h) noexcept {
 *h=++gameListeners;return Lifecycle::Result::Success;
}
Network::Result registerChannel(const PluginContext*,const Network::ChannelRegistration*,Network::ChannelHandle* h) noexcept {*h=1;return Network::Result::Success;}
void testInitialization() {
 itemApi.serviceSize=sizeof(itemApi);itemApi.serviceVersion=ItemService::AbiVersion;
 invApi.serviceSize=sizeof(invApi);invApi.serviceVersion=InventoryService::AbiVersion;
 threadApi.serviceSize=sizeof(threadApi);threadApi.serviceVersion=ThreadService::AbiVersion;
 netApi.serviceSize=sizeof(netApi);netApi.serviceVersion=NetworkService::AbiVersion;
 tablesApi.serviceSize=sizeof(tablesApi);tablesApi.serviceVersion=DataTableService::AbiVersion;
 lifeApi.serviceSize=sizeof(lifeApi);lifeApi.serviceVersion=LifecycleService::AbiVersion;
 api.queryService=query;api.checkExpectedBytes=checkBytes;api.installInlineHook=installHook;
 lifeApi.registerDataTablesLoadedListener=registerTables;lifeApi.registerGameplayEventListener=registerGame;netApi.registerChannel=registerChannel;
 ctx.contextSize=sizeof(ctx);ctx.abiVersion=4;ctx.activeMod="VanillaDeluxe";ctx.buildName="95032";ctx.exeBase=reinterpret_cast<uintptr_t>(nativeImage.data());
 for(auto& w:CubeWitnesses)std::memcpy(nativeImage.data()+w.rva,w.data,w.size);
 using namespace ChronicleNative;
 struct W{uintptr_t r;const unsigned char* b;};
 W witnesses[]={{UniqueRollRva,UniqueRollBytes},{RecordSetterRva,RecordSetterBytes},{HistoryFindRva,HistoryFindBytes},{HistoryInsertRva,HistoryInsertBytes},{HistoryStageRva,HistoryStageBytes},{DecodeItemRva,DecodeItemBytes},{CreateItemRva,CreateItemBytes},{EachPlayerRva,EachPlayerBytes},{MarkPlayerRva,MarkPlayerBytes},{GetClientRva,GetClientBytes},{SendPacketRva,SendPacketBytes},{ClearItemHistoryRva,ClearItemHistoryBytes},{ReadSavedItemsRva,ReadSavedItemsBytes},{LoadHistoryRva,LoadHistoryBytes},{GetUniqueRecordRva,GetUniqueRecordBytes}};
 for(auto w:witnesses)std::memcpy(nativeImage.data()+w.r,w.b,20);
 assert(D2RLoaderLoadPlugin(&ctx));assert(hookCount==11&&gameListeners==2&&loaded.callback);
 // Wrong build is rejected before hooks are installed.
 ctx.buildName="other";assert(!D2RLoaderLoadPlugin(&ctx));assert(hookCount==11);ctx.buildName="95032";
 ChronicleIntegration::TablesLoaded(&ctx,nullptr,nullptr);assert(!ChronicleIntegration::Ready);
 for(unsigned i=0;i<UniqueCount;i++){ChronicleFix::put<uint16_t>(nativeU.data()+i*0x15c,uint16_t(i));ChronicleFix::put<uint32_t>(nativeU.data()+i*0x15c+0x28,UniqueCodes[i]);}
 nativeLayout=true;loaded.callback(&ctx,nullptr,loaded.userData);assert(ChronicleIntegration::Ready);
 nativeU[0x28]^=1;loaded.callback(&ctx,nullptr,loaded.userData);assert(!ChronicleIntegration::Ready);nativeLayout=false;
 std::cout<<"PASS: production initializer, SDK registration, native witnesses, wrong-build guard and table mismatch guard.\n";
}
int main(){
 api.apiSize=PluginApiSize;api.logInfo=api.logWarn=api.logError=log;ctx.api=&api;C=&ctx;
 tablesApi.getTable=table;testInitialization();Item=&itemApi;Inv=&invApi;Thread=&threadApi;Net=&netApi;Tables=&tablesApi;
 itemApi.getItemInfo=getInfo;itemApi.executeTransaction=exchange;itemApi.executeExistingItemTransaction=moveItem;invApi.forEachInventoryItem=each;invApi.getLocalPlayer=localPlayer;OriginalTransmute=nativeTransmute;
 threadApi.runOnGameThread=schedule;tablesApi.getTable=table;tablesApi.findRowByCode=code;
 netApi.getPeerPlayer=peerPlayer;netApi.connectToHost=connect;netApi.sendToHost=toHost;netApi.sendToClient=toClient;
 for(const auto& r:Rewards)if(r.quality==7||r.quality==5){auto& t=r.quality==7?uniques:sets;std::memcpy(t.data()+r.record*16+8,&r.code,4);}
 chest.structSize=Items::ItemInfoSize;chest.handle=15;chest.code=Chest;chest.runtimeId=123;chest.itemSeed=456;chest.generationSeed=789;chest.container=Items::ItemContainer::Cube;chest.x=1;chest.y=2;
 Request req{123,Chest};
 // Ordinary recipes, empty cube, and mixed inputs retain native Transmute.
 chest.code=Items::MakeItemCode("r33");TransmuteUi();assert(nativeCalls==1&&pending.empty());chest.code=Chest;
 chestQuantity=0;TransmuteUi();assert(nativeCalls==2&&pending.empty());chestQuantity=1;
 extra=true;TransmuteUi();assert(nativeCalls==3&&pending.empty());extra=false;
 assert(ValidateCatalog());
 // Every catalog entry selects that EXACT original row/base/quality once.
 const uint32_t n=sizeof(Rewards)/sizeof(*Rewards);
 for(uint32_t i=0;i<n;++i){chestQuantity=1;mockRandom=n+i;assert(Queue(42,0,req));drain();const auto& r=Rewards[i];assert(last.code==r.code&&uint32_t(last.quality)==r.quality&&last.qualityRecordId==(r.quality==7?PhysicalCanonical[r.record]:r.record));assert(last.flags==(r.quality==7?1u:0u));assert(chestQuantity==0);}
 assert(commits==int(n));
 // Actual Transmute intercept, server-only handle, and interior input cell.
 chestQuantity=1;chest.x=1;chest.y=2;auto prior=commits;auto nativeBefore=nativeCalls;
 TransmuteUi();assert(pending.size()==1&&nativeCalls==nativeBefore);drain();assert(commits==prior+1&&chestQuantity==0&&chest.x==0&&chest.y==0);
 // Mixed items introduced after the click must prevent host-side exchange.
 chestQuantity=1;TransmuteUi();extra=true;auto attemptBefore=attempts;drain();assert(attempts==attemptBefore&&chestQuantity==1);extra=false;
 // Chest moved out of the cube after click is never consumed.
 TransmuteUi();chest.container=Items::ItemContainer::Inventory;drain();assert(attempts==attemptBefore&&chestQuantity==1);chest.container=Items::ItemContainer::Cube;
 // A staging move failure preserves input; failed exchange restores input cell.
 moveFails=true;chest.x=1;chest.y=2;Queue(42,0,req);drain();assert(attempts==attemptBefore&&chestQuantity==1);moveFails=false;
 transactionFails=true;Queue(42,0,req);drain();assert(chestQuantity==1&&chest.x==1&&chest.y==2);transactionFails=false;
 // Two pending clicks for one chest produce one reward.
 chestQuantity=1;auto before=commits;Queue(42,0,req);Queue(42,0,req);drain();assert(commits==before+1);
 // Invalid owner, stale identity and code must not reach transaction API.
 before=attempts;chestQuantity=1;Queue(999,0,req);auto stale=req;++stale.runtime;Queue(42,0,stale);stale=req;++stale.code;Queue(42,0,stale);drain();assert(attempts==before&&chestQuantity==1);
 transactionFails=true;Queue(42,0,req);drain();assert(chestQuantity==1);transactionFails=false;
 before=attempts;mockRngFails=true;Queue(42,0,req);drain();assert(attempts==before&&chestQuantity==1);mockRngFails=false;
 CatalogValid=false;tableMismatch=true;Queue(42,0,req);drain();assert(attempts==before&&chestQuantity==1);tableMismatch=false;
 CatalogValid=false;missingCode=true;Queue(42,0,req);drain();assert(attempts==before&&chestQuantity==1);missingCode=false;
 // Remote client sends only identity; authoritative host resolves peer owner.
 remote=true;TransmuteUi();assert(pending.empty()&&Jobs==nullptr);assert(sent.runtime==req.runtime&&sent.code==req.code);remote=false;
 HostMessage(C,0,99,1,&sent,sizeof(sent),nullptr);drain();assert(chestQuantity==0&&lastPlayer==42);
 before=attempts;chestQuantity=1;HostMessage(C,0,100,1,&sent,sizeof(sent),nullptr);drain();assert(attempts==before&&chestQuantity==1);
 // Failed transport leaves input intact and does not invoke old m15 recipes.
 remote=true;transportFails=true;nativeBefore=nativeCalls;TransmuteUi();assert(nativeCalls==nativeBefore);assert(!Jobs&&pending.empty()&&chestQuantity==1);remote=false;transportFails=false;
 // Rejection sampling discards low values before modulo (no modulo bias).
 constexpr uint32_t threshold=(0u-n)%n;mockRandom=0;uint32_t drawn{};assert(Draw(drawn));assert(mockRandom==threshold+1&&drawn==threshold%n);
 // Canceled queued jobs can be reclaimed safely at unload.
 Queue(42,0,req);pending.clear();D2RLoaderUnloadPlugin();assert(!Jobs);
 std::cout<<"PASS: "<<n<<" exact original outputs; Cube-only output and native recipe fallback; movement rollback; stale/duplicate clicks, catalog mismatch, RNG failure, LAN owner binding, rejection sampling and unload cleanup.\n";
}
