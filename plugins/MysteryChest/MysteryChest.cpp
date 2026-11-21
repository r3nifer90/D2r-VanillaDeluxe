#include <windows.h>
#include <bcrypt.h>
#include <D2RLPlugin/api.h>
#include <cstring>
#include <cstdio>
#include "catalog.hpp"
#include "cube_witnesses.hpp"
#include "chronicle_fix.h"
namespace {
using namespace D2RL;
const PluginContext* C{};
const ItemService* Item{};const InventoryService* Inv{};const ThreadService* Thread{};
const NetworkService* Net{};const DataTableService* Tables{};const LifecycleService* Life{};
Network::ChannelHandle Channel{};Lifecycle::ListenerHandle GameHandle{};
constexpr uint32_t Chest=Items::MakeItemCode("m15");
constexpr uint32_t Containers=Items::ContainerBit(Items::ItemContainer::Cube);
using TransmuteUiFn=void(__fastcall*)() noexcept;
TransmuteUiFn OriginalTransmute{};
// Seeds are mutable PRNG state, not cross-client/server identity. The host
// resolves this runtime ID only inside the requesting player's owned inventory.
uint32_t LocalPlayerId=0xffffffff;
struct Request{uint32_t runtime,code,player;};
constexpr uint64_t ProtocolToken=CatalogToken^0x4D435F4348524F4EULL;
struct Job{Job* next;PlayerHandle player;Network::PeerHandle peer;Request request;};
SRWLOCK JobLock=SRWLOCK_INIT;Job* Jobs{};
void Retire(Job* job) noexcept {
 AcquireSRWLockExclusive(&JobLock);Job** link=&Jobs;
 while(*link&&*link!=job)link=&(*link)->next;
 if(*link)*link=job->next;
 ReleaseSRWLockExclusive(&JobLock);HeapFree(GetProcessHeap(),0,job);
}
uint64_t ValidRevision{};bool CatalogValid{};
// Compiled table pointers are read-only. This discovers a common code column
// across ALL chosen original rows instead of assuming a native layout offset.
bool CheckTable(const DataTables::TableView& table,uint32_t quality) noexcept {
 if(!table.rows||table.rowSize<4||table.rowSize>16384)return false;
 for(uint32_t offset=0;offset+4<=table.rowSize;++offset){
  bool match=true;
  for(const auto& r:Rewards){if(r.quality!=quality)continue;
   if(r.record>=table.rowCount){match=false;break;}
   uint32_t code{};std::memcpy(&code,static_cast<const unsigned char*>(table.rows)+size_t(r.record)*table.rowSize+offset,4);
   if(code!=r.code){match=false;break;}
  }
  if(match)return true;
 }
 return false;
}
bool ValidateCatalog() noexcept {
 DataTables::TableView u{},s{};u.structSize=s.structSize=DataTables::TableViewSize;
 if(Tables->getTable(C,DataTables::Bank::Rotw,DataTables::TableId::UniqueItems,&u)!=DataTables::Result::Success ||
    Tables->getTable(C,DataTables::Bank::Rotw,DataTables::TableId::SetItems,&s)!=DataTables::Result::Success)return false;
 if(u.revision!=s.revision)return false;
 if(ValidRevision==u.revision&&CatalogValid)return true;
 CatalogValid=CheckTable(u,7)&&CheckTable(s,5);ValidRevision=u.revision;
 if(!CatalogValid){C->LogError("MysteryChest: catalog differs from current tables; chest preserved.");return false;}
 for(const auto& r:Rewards){DataTables::RowView row{};row.structSize=DataTables::RowViewSize;
  if(Tables->findRowByCode(C,DataTables::Bank::Rotw,DataTables::TableId::Items,r.code,&row)!=DataTables::Result::Success){CatalogValid=false;return false;}
 }
 return true;
}
struct Find {Request request;ItemHandle found{};Items::ItemInfo info{};uint32_t count{};};
Inventory::IterationAction __cdecl FindChest(const PluginContext*,const Items::ItemInfo* info,void* data) noexcept {
 auto& f=*static_cast<Find*>(data);
 if(!info||info->container!=Items::ItemContainer::Cube)return Inventory::IterationAction::Continue;
 ++f.count;
 if(info->code==Chest&&info->runtimeId==f.request.runtime&&info->quantity==1){f.found=info->handle;f.info=*info;}
 return Inventory::IterationAction::Continue;
}
bool MoveInCube(PlayerHandle player,ItemHandle item,int x,int y) noexcept {
 if(x<0||y<0)return false;
 Items::ExistingItemOperation op{};op.structSize=Items::ExistingItemOperationSize;
 op.kind=Items::ExistingItemOperationKind::Move;op.item=item;
 op.move.destination.structSize=Items::ItemDestinationSize;op.move.destination.container=Items::ItemContainer::Cube;
 op.move.destination.placement=Items::Placement::Exact;op.move.destination.x=uint32_t(x);op.move.destination.y=uint32_t(y);
 Items::ExistingItemTransaction tx{Items::ExistingItemTransactionSize,0,player,1,0,&op};
 Items::ExistingItemTransactionResult result{};result.structSize=Items::ExistingItemTransactionResultSize;
 return Item->executeExistingItemTransaction(C,&tx,&result)==Items::Result::Success;
}
bool Draw(uint32_t& result) noexcept {
 constexpr uint32_t n=uint32_t(sizeof(Rewards)/sizeof(Rewards[0]));constexpr uint32_t threshold=(0u-n)%n;
 uint32_t value{};
 do{if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),sizeof(value),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)return false;}while(value<threshold);
 result=value%n;return true;
}
void __cdecl Open(const PluginContext*,void* userdata) noexcept {
 auto& job=*static_cast<Job*>(userdata);auto player=job.player;uint32_t status=1;
 if(job.peer && Net->getPeerPlayer(C,Channel,job.peer,&player)!=Network::Result::Success){Retire(&job);return;}
 if(job.request.code==0){
  Inventory::ItemFilter check{sizeof(check),0,Containers,0};
  auto stop=[](const PluginContext*,const Items::ItemInfo*,void*) noexcept {return Inventory::IterationAction::Stop;};
  if(player&&Inv->forEachInventoryItem(C,player,&check,stop,nullptr)==Inventory::Result::Success)
   ChronicleIntegration::Refresh(job.request.player);
  Retire(&job);return;
 }
 Find find{job.request};Inventory::ItemFilter filter{sizeof(filter),0,Containers,0};
 if(player && job.request.code==Chest && ValidateCatalog() &&
    Inv->forEachInventoryItem(C,player,&filter,FindChest,&find)==Inventory::Result::Success && find.found&&find.count==1){
  uint32_t selected{};
  if(Draw(selected)){
   // Reserve a full 2x4 rectangle even if the chest initially occupies the
   // middle column. A lone 1x1 input at (0,0) leaves columns 1/2 available in
   // the native 3x4 cube. All catalog bases are at most 2x4.
   const bool moved=find.info.x!=0||find.info.y!=0;
   if(moved&&!MoveInCube(player,find.found,0,0)){C->LogWarn("MysteryChest: cube placement unavailable; chest preserved.");if(job.peer)Net->sendToClient(C,Channel,job.peer,2,&status,sizeof(status));Retire(&job);return;}
   const auto& reward=Rewards[selected];Items::ItemCreateSpec spec{};spec.structSize=Items::ItemCreateSpecSize;
   spec.code=reward.code;spec.quality=static_cast<Items::Quality>(reward.quality);spec.qualityRecordId=reward.record;
   if(reward.quality==7&&reward.record<UniqueCount)spec.qualityRecordId=PhysicalCanonical[reward.record];
   // This API flag permits another instance of the SAME original unique ID.
   // It never appends, replaces, or edits a UniqueItems record.
   if(reward.quality==7)spec.flags=Items::ItemCreateFlagBit(Items::ItemCreateFlag::AllowDuplicateUnique);
   spec.itemLevel=99;spec.seedMode=Items::SeedMode::Random;spec.quantity=spec.durability=Items::DefaultValue;
   spec.stateFlags=Items::ItemStateIdentified;
   spec.destination.structSize=Items::ItemDestinationSize;spec.destination.container=Items::ItemContainer::Cube;
   spec.destination.placement=Items::Placement::Automatic;
   Items::TransactionInput input{find.found,1,Items::SocketedItemPolicy::RejectIfNotEmpty};ItemHandle output{};
   Items::Transaction tx{Items::TransactionSize,0,player,1,1,&input,&spec,&output,1,0};
   Items::TransactionResult result{Items::TransactionResultSize};
   ChronicleIntegration::Capture created;
   ChronicleIntegration::Begin(created);
   const auto outcome=Item->executeTransaction(C,&tx,&result);
   ChronicleIntegration::End();
   if(outcome==Items::Result::Success && result.outputCount==1 && output){
    status=0;
    if(reward.quality==5||reward.quality==7){
     Items::ItemInfo actual{};actual.structSize=Items::ItemInfoSize;uint32_t nativeId{};
     bool match=Item->getItemInfo(C,output,&actual)==Items::Result::Success&&created.item&&
       ChronicleIntegration::Read(reinterpret_cast<uintptr_t>(created.item)+8,nativeId)&&nativeId==actual.runtimeId;
     if(!match||!ChronicleIntegration::Discover(created,job.request.player))
      C->LogWarn("MysteryChest: reward preserved; discovery requires reconciliation on next load.");
    }
   }
   else{if(moved&&!MoveInCube(player,find.found,find.info.x,find.info.y))C->LogWarn("MysteryChest: chest preserved in cube at the staging cell.");char msg[144];std::snprintf(msg,sizeof(msg),"MysteryChest: exchange failed (%u); chest preserved, no substitute reward.",unsigned(outcome));C->LogWarn(msg);}
  }
 }
 if(job.peer)Net->sendToClient(C,Channel,job.peer,2,&status,sizeof(status));
 Retire(&job);
}
bool Queue(PlayerHandle player,Network::PeerHandle peer,const Request& request) noexcept {
 auto* job=static_cast<Job*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Job)));if(!job)return false;
 job->player=player;job->peer=peer;job->request=request;
 AcquireSRWLockExclusive(&JobLock);job->next=Jobs;Jobs=job;ReleaseSRWLockExclusive(&JobLock);
 if(Thread->runOnGameThread(C,Open,job)==Threads::Result::Success)return true;
 Retire(job);return false;
}
void __cdecl HostMessage(const PluginContext*,Network::ChannelHandle,Network::PeerHandle peer,uint16_t id,const void* data,uint32_t size,void*) noexcept {
 if((id!=1&&id!=3)||!data||size!=sizeof(Request)||!peer)return;
 Request request{};std::memcpy(&request,data,size);if((id==1&&request.code==Chest)||(id==3&&request.code==0))Queue(0,peer,request);
}
void __cdecl ClientMessage(const PluginContext*,Network::ChannelHandle,uint16_t id,const void* data,uint32_t size,void*) noexcept {
 if(id==2&&size==4&&data){uint32_t status{};std::memcpy(&status,data,4);if(status)C->LogWarn("MysteryChest: request not completed; chest preserved.");}
}
struct CubeInput {uint32_t count{};bool chest{};Request request{};};
Inventory::IterationAction __cdecl InspectCube(const PluginContext*,const Items::ItemInfo* i,void* data) noexcept {
 auto& cube=*static_cast<CubeInput*>(data);
 if(i&&i->container==Items::ItemContainer::Cube){++cube.count;if(i->code==Chest&&i->quantity==1){cube.chest=true;cube.request={i->runtimeId,Chest,LocalPlayerId};}}
 return Inventory::IterationAction::Continue;
}
void __fastcall TransmuteUi() noexcept {
 PlayerHandle player{};CubeInput cube;Inventory::ItemFilter filter{sizeof(filter),0,Containers,0};
 if(Inv->getLocalPlayer(C,&player)!=Inventory::Result::Success||!player||
    Inv->forEachInventoryItem(C,player,&filter,InspectCube,&cube)!=Inventory::Result::Success||cube.count!=1||!cube.chest){
  OriginalTransmute();return;
 }
 if(!Queue(player,0,cube.request)){
  const auto conn=Net->connectToHost(C,Channel);
  if((conn!=Network::Result::Success&&conn!=Network::Result::Busy)||Net->sendToHost(C,Channel,1,&cube.request,sizeof(cube.request))!=Network::Result::Success)
   C->LogWarn("MysteryChest: host connection not ready; chest preserved. Try Transmute again.");
 }
 // Suppress only the original conversion for exactly one m15. All other
 // recipes take OriginalTransmute above, including empty/mixed cube inputs.
}
void RequestRefresh() noexcept {
 if(LocalPlayerId==0xffffffff)return;
 Request request{0,0,LocalPlayerId};PlayerHandle player{};
 if(Inv->getLocalPlayer(C,&player)==Inventory::Result::Success&&player&&Queue(player,0,request))return;
 Net->sendToHost(C,Channel,3,&request,sizeof(request));
}
void __cdecl Connection(const PluginContext*,const Network::ConnectionEvent* event,void*) noexcept {
 if(event&&event->state==Network::ConnectionState::Connected)RequestRefresh();
}
void __cdecl Gameplay(const PluginContext*,const Lifecycle::GameplayEvent* e,void*) noexcept {
 if(e&&e->kind==Lifecycle::GameplayEventKind::LocalPlayerReady){LocalPlayerId=e->playerId;Net->connectToHost(C,Channel);RequestRefresh();}
 if(e&&e->kind==Lifecycle::GameplayEventKind::GameLeft){LocalPlayerId=0xffffffff;ChronicleIntegration::Leave();}
}
template<class T>bool Query(const T*& service,uint32_t size) noexcept {
 return C->QueryService(&service)==ServiceQueryResult::Success&&service&&service->serviceSize>=size&&service->serviceVersion==T::AbiVersion;
}
const PluginInfo Info{sizeof(PluginInfo),4,"r3ni-mystery-chest","Mystery Chest","2.1.1-chronicle","r3ni","Equal-weight original item rewards; isolated chest exchange",PluginFlags::ModScopedOnly|PluginFlags::Shared|PluginFlags::NativeHooks};
}
D2RL_PLUGIN_EXPORT const D2RL::PluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept{return &Info;}
D2RL_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept {
 if(!ctx||ctx->abiVersion!=4||!ctx->activeMod||std::strcmp(ctx->activeMod,"VanillaDeluxe")||!ctx->exeBase||!ctx->buildName||std::strcmp(ctx->buildName,"95032"))return false;C=ctx;
 for(const auto& w:CubeWitnesses)if(std::memcmp(reinterpret_cast<const void*>(ctx->exeBase+w.rva),w.data,w.size))return false;
 if(!Query(Item,ItemServiceRequiredSize)||!Query(Inv,InventoryServiceSize)||!Query(Thread,ThreadServiceSize)||
    !Query(Net,NetworkServiceRequiredSize)||
    !Query(Tables,DataTableServiceRequiredSize)||!Query(Life,LifecycleServiceSize))return false;
 Network::ChannelRegistration channel{Network::ChannelRegistrationSize,0,1,0,0,ProtocolToken,HostMessage,ClientMessage,Connection,nullptr};
 if(Net->registerChannel(C,&channel,&Channel)!=Network::Result::Success)return false;
 Lifecycle::GameplayEventListener gameplay{};gameplay.structSize=Lifecycle::GameplayEventListenerSize;gameplay.kind=Lifecycle::GameplayEventKind::LocalPlayerReady;gameplay.callback=Gameplay;
 if(Life->registerGameplayEventListener(C,&gameplay,&GameHandle)!=Lifecycle::Result::Success)return false;
 Lifecycle::GameplayEventListener leave=gameplay;leave.kind=Lifecycle::GameplayEventKind::GameLeft;Lifecycle::ListenerHandle leaveHandle{};
 if(Life->registerGameplayEventListener(C,&leave,&leaveHandle)!=Lifecycle::Result::Success)return false;
 if(!ChronicleIntegration::Install(C,Tables,Life))return false;
 if(!C->InstallInlineHook(0x2cd720,TransmuteWitness,sizeof(TransmuteWitness),TransmuteUi,&OriginalTransmute))return false;
 C->LogInfo("MysteryChest 2.1.1-chronicle: 1390 equal-weight original rewards; Cube Transmute; transactional delivery inside cube.");return true;
}
D2RL_PLUGIN_EXPORT void __cdecl D2RLoaderUnloadPlugin() noexcept{
 AcquireSRWLockExclusive(&JobLock);auto* job=Jobs;Jobs=nullptr;ReleaseSRWLockExclusive(&JobLock);
 while(job){auto* next=job->next;HeapFree(GetProcessHeap(),0,job);job=next;}
}
