#include "pvp_ward.hpp"
#include "native_contract.hpp"
#include "core/services.hpp"
#include "core/hook_registry.hpp"
#include "core/stat_read_bus.hpp"
#include "core/stat_list_post_bus.hpp"
#include "core/buff_display_bus.hpp"
#include <windows.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>


namespace BuffPanel::Systems::PvpWard {
namespace {
using namespace D2RL;
const PluginContext* C{};
const NetworkService* Net{};
const ThreadService* Threads{};
const LifecycleService* Life{};
const OverlayService* Overlay{};
Network::ChannelHandle Channel{};
Lifecycle::ListenerHandle Listeners[3]{};
Overlay::CallbackHandle OverlayHandle{};
std::atomic<bool> Ready{},HostReady{},QueuedUi{},QueuedGame{},Connected{};
std::atomic<std::uint64_t> Session{};
std::atomic<std::uint32_t> LocalId{};
std::atomic<bool> LocalKnown{};
std::atomic<std::uintptr_t> NativeGame{};
std::mutex StateMutex;
Network::PeerHandle Peers[MaximumPeers]{};
std::size_t PeerCount{};
std::atomic<std::size_t> ActivePeerCount{};
std::mutex WoundMutex;
std::uint32_t PlayerIds[MaximumPlayers]{};
std::size_t PlayerCount{};
std::uint32_t Sequence{},Reduction{75};
bool Enabled{true};
bool GuiActive{};
std::uint32_t GuiReduction{};
std::uint64_t LastServerMessage{},LastUiPoll{},LastPing{};
bool HudVisible{};
Widgets::Rect HudGrid{},HudSlot{};
Packet Received{};
std::uint64_t ReceivedAt{};

using ToggleFn=void(__fastcall*)(void*,void*,void*,std::uint32_t,std::int32_t) noexcept;
using FinalFn=void(__fastcall*)(void*,void*,void*,void*) noexcept;
using CrushingFn=std::int32_t(__fastcall*)(void*,void*,void*,void*) noexcept;
using LookupFn=void*(__fastcall*)(void*,std::int32_t,std::int32_t) noexcept;
using OwnerFn=void*(__fastcall*)(void*,void*) noexcept;
using SetUnitFn=void(__fastcall*)(void*,std::int32_t,std::int32_t,std::int32_t) noexcept;
using SetListFn=std::int32_t(__fastcall*)(std::uint8_t,void*,std::int32_t,std::int32_t,std::int32_t) noexcept;
using GetListFn=std::int32_t(__fastcall*)(std::uint8_t,void*,std::int32_t,std::int32_t) noexcept;
ToggleFn OriginalToggle{}; FinalFn OriginalFinal{}; CrushingFn OriginalCrushing{};
SetUnitFn OriginalSetUnit{};SetListFn OriginalSetList{};
LookupFn Lookup{};OwnerFn Owner{};GetListFn GetList{};

bool Range(const void* p,std::size_t bytes,bool write=false) noexcept {
    auto at=reinterpret_cast<std::uintptr_t>(p);if(!at || at+bytes<at)return false;
    const auto end=at+bytes;
    while(at<end){ MEMORY_BASIC_INFORMATION m{};if(VirtualQuery(reinterpret_cast<void*>(at),&m,sizeof(m))!=sizeof(m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        const auto protection=m.Protect&255u;
        if(write && protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return false;
        const auto next=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=at)return false;at=next;
    }return true;
}
template<class T> bool Read(const void* p,std::size_t off,T& value) noexcept {const auto* q=static_cast<const std::uint8_t*>(p)+off;if(!p||!Range(q,sizeof(T)))return false;std::memcpy(&value,q,sizeof(T));return true;}
template<class T> bool Write(void* p,std::size_t off,T value) noexcept {auto* q=static_cast<std::uint8_t*>(p)+off;if(!p||!Range(q,sizeof(T),true))return false;std::memcpy(q,&value,sizeof(T));return true;}



bool Player(void* unit,std::uint32_t* uid=nullptr) noexcept {
    std::uint32_t type=99,id=0;void* data{};
    if(!Read(unit,0,type)||type!=0||!Read(unit,8,id)||!Read(unit,0x10,data)||!data)return false;
    if(uid)*uid=id;return true;
}
bool SameGame(void* game,void* unit) noexcept {void* current{};return Read(unit,0xd8,current)&&current==game;}
std::uint32_t Flags(void* player,std::uint32_t other) noexcept {
    void* data{};void* node{};if(!Read(player,0x10,data)||!Read(data,0x88,node))return 0;
    for(unsigned i=0;node&&i<256;++i){std::uint32_t id{},flags{};void* next{};if(!Read(node,0,id)||!Read(node,4,flags)||!Read(node,0x18,next))return 0;if(id==other)return flags;node=next;}return 0;
}
void* PlayerOwner(void* game,void* unit) noexcept {
    for(unsigned i=0;unit&&i<8;++i){std::uint32_t uid{};if(Player(unit,&uid))return SameGame(game,unit)&&Lookup(game,0,static_cast<std::int32_t>(uid))==unit?unit:nullptr;std::uint32_t type=99;if(!Read(unit,0,type)||(type!=1&&type!=3)||!SameGame(game,unit))return nullptr;auto* next=Owner(game,unit);if(next==unit)return nullptr;unit=next;}return nullptr;
}



bool MultiplayerHost() noexcept {return Ready.load()&&HostReady.load();}
bool Eligible(void* game,void* attacker,void* victim) noexcept {
    if(!Enabled||!MultiplayerHost()||reinterpret_cast<std::uintptr_t>(game)!=NativeGame.load()||!Player(victim)||!SameGame(game,victim))return false;
    std::uint32_t victimId{};if(!Player(victim,&victimId)||Lookup(game,0,static_cast<std::int32_t>(victimId))!=victim)return false;
    auto* source=PlayerOwner(game,attacker);if(!source||source==victim)return false;
    std::uint32_t a{},b{};return Player(source,&a)&&Player(victim,&b)&&Hostile(Flags(source,b),Flags(victim,a));
}
void Track(void* game,void* a,void* b) noexcept {
    std::uint32_t first{},second{};if(a==b||!Player(a,&first)||!Player(b,&second)||!SameGame(game,a)||!SameGame(game,b)||Lookup(game,0,static_cast<std::int32_t>(first))!=a||Lookup(game,0,static_cast<std::int32_t>(second))!=b)return;
    std::lock_guard lock(StateMutex);
    const auto key=reinterpret_cast<std::uintptr_t>(game);
    if(NativeGame.load()!=key){PlayerCount=0;NativeGame.store(key);}
    for(auto id:{first,second}){bool found=false;for(std::size_t i=0;i<PlayerCount;++i)found|=PlayerIds[i]==id;if(!found&&PlayerCount<MaximumPlayers)PlayerIds[PlayerCount++]=id;}
}
void ScaleField(void* damage,std::size_t offset) noexcept {std::int32_t value{};if(Read(damage,offset,value)&&value>0)(void)Write(damage,offset,Scale(value,Reduction));}
void __fastcall Final(void* game,void* attacker,void* victim,void* damage) noexcept {
    OriginalFinal(game,attacker,victim,damage);
    if(!Eligible(game,attacker,victim)||!Range(damage,0x180,true))return;


    for(auto offset:{0x18u,0x20u,0x24u,0x2cu,0x30u,0x34u,0x38u,0x134u})ScaleField(damage,offset);


    std::int32_t absorb{};auto getter=Core::StatReads().RawGetter();
    if(getter&&Read(damage,0x130,absorb)&&absorb>0){const auto hp=getter(victim,6,0),max=getter(victim,7,0);const auto room=std::max<std::int64_t>(0,static_cast<std::int64_t>(max)-hp);absorb=static_cast<std::int32_t>(std::min<std::int64_t>(absorb,room));(void)Write(damage,0x130,Scale(absorb,Reduction));}


    void* entries{};std::uint64_t count{};
    if(Read(damage,0x40,entries)&&Read(damage,0x48,count)&&count&&count<=1024&&Range(entries,static_cast<std::size_t>(count)*12,true))
        for(std::size_t i=0;i<count;++i)ScaleField(entries,i*12+4);
}
std::int32_t __fastcall Crushing(void* game,void* attacker,void* victim,void* info) noexcept {




    const bool eligible=Eligible(game,attacker,victim);
    const auto getter=Core::StatReads().RawGetter();
    const auto before=eligible&&getter?getter(victim,6,0):0;
    void* damage{};std::uint16_t priorFlags{};
    const bool flagsKnown=eligible&&Read(info,0,damage)&&Read(damage,4,priorFlags);
    const auto result=OriginalCrushing(game,attacker,victim,info);
    if(eligible&&getter&&before>0){
        const auto after=getter(victim,6,0);
        if(after>=0&&after<before){
            const auto adjusted=before-Scale(before-after,Reduction);
            if(adjusted!=after){
                OriginalSetUnit(victim,6,adjusted,0);


                if(after==0&&adjusted>0&&flagsKnown){std::uint16_t flags{};if(Read(damage,4,flags))(void)Write(damage,4,static_cast<std::uint16_t>((flags&~2u)|(priorFlags&2u)));}
            }
        }
    }
    return result;
}
struct ListWitness {void* list{};std::uint32_t victim{},owner{},expiry{};std::int32_t scaled{};};
ListWitness Wounds[256]{};std::size_t WoundCursor{};
bool WoundOrigin(void* list,void* victim,void*& game,std::uint32_t& ownerId,std::uint32_t& expiry) noexcept {
    std::uint32_t state{},ownerType{};
    if(!Read(list,0x20,state)||state!=62||!Read(list,8,ownerType)||!Read(list,12,ownerId)||!Read(list,0x24,expiry)||!Read(victim,0xd8,game)||ownerType>3)return false;
    return Eligible(game,Lookup(game,static_cast<std::int32_t>(ownerType),static_cast<std::int32_t>(ownerId)),victim);
}
void RememberWound(void* list,void* victim,std::uint32_t owner,std::uint32_t expiry,std::int32_t value) noexcept {
    std::uint32_t id{};if(!Player(victim,&id))return;
    std::lock_guard lock(WoundMutex);
    for(auto& x:Wounds)if(x.list==list){x={list,id,owner,expiry,value};return;}
    Wounds[WoundCursor++%std::size(Wounds)]={list,id,owner,expiry,value};
}
std::int32_t __fastcall SetList(std::uint8_t bank,void* list,std::int32_t stat,std::int32_t value,std::int32_t layer) noexcept {
    void* victim{};void* game{};std::uint32_t owner{},expiry{};


    if(stat==74&&layer==0){std::lock_guard lock(WoundMutex);for(auto& x:Wounds)if(x.list==list)x={};}


    if(MultiplayerHost()&&stat==74&&value<0&&layer==0&&Read(list,0,victim)&&Player(victim)&&WoundOrigin(list,victim,game,owner,expiry)){
        value=ScaleNegative(value,Reduction);RememberWound(list,victim,owner,expiry,value);
    }return OriginalSetList(bank,list,stat,value,layer);
}
void OnPost(const Core::StatListPostEvent& event,Core::StatListPostPhase phase,void*) noexcept {
    if(phase!=Core::StatListPostPhase::BeforeNative||!Player(event.unit))return;
    void* game{};std::uint32_t owner{},expiry{};if(!WoundOrigin(event.statList,event.unit,game,owner,expiry))return;
    std::uint8_t bank{};std::uint32_t id{};if(!Read(game,0x106,bank)||!Player(event.unit,&id))return;
    const auto value=GetList(bank,event.statList,74,0);if(value>=0)return;
    {std::lock_guard lock(WoundMutex);for(const auto& x:Wounds)if(x.list==event.statList&&x.victim==id&&x.owner==owner&&x.expiry==expiry&&x.scaled==value)return;}
    const auto scaled=ScaleNegative(value,Reduction);RememberWound(event.statList,event.unit,owner,expiry,scaled);OriginalSetList(bank,event.statList,74,scaled,0);
}
void QueueGame() noexcept;
void __fastcall Toggle(void* game,void* a,void* b,std::uint32_t flags,std::int32_t enabled) noexcept {
    OriginalToggle(game,a,b,flags,enabled);if(Ready.load()&&(flags&HostilityFlag)){Track(game,a,b);QueueGame();}
}
void PublishGui(bool active,std::uint32_t percent) noexcept {
    GuiActive=active;GuiReduction=percent;
    if(!active){(void)Core::BuffDisplays().Remove(BuffKey);return;}
    Core::BuffDisplayEntry entry{};entry.key=BuffKey;entry.iconAtlas=Core::BuffIconAtlas::Paladin;entry.iconFrame=42;
    entry.displayMode=Core::BuffDisplayMode::Passive;entry.currentValue=static_cast<std::int32_t>(percent);entry.priority=200;
    (void)Core::BuffDisplays().Upsert(entry);
}
void __cdecl GameTick(const PluginContext* ctx,void*) noexcept {
    QueuedGame.store(false);if(ctx!=C||!Ready.load()||!Session.load())return;


    HostReady.store(true);
    std::array<Network::PeerHandle,MaximumPeers> peers{};std::array<std::uint32_t,MaximumPlayers> ids{};
    std::size_t pc{},n{};
    {std::lock_guard lock(StateMutex);pc=PeerCount;n=PlayerCount;std::copy_n(Peers,pc,peers.begin());std::copy_n(PlayerIds,n,ids.begin());}
    auto* game=reinterpret_cast<void*>(NativeGame.load());
    std::array<PlayerState,MaximumPlayers> states{};
    for(std::size_t i=0;i<n;++i){states[i].id=ids[i];auto* a=game?Lookup(game,0,static_cast<std::int32_t>(ids[i])):nullptr;if(!Player(a)||!SameGame(game,a))continue;
        for(std::size_t j=0;j<n;++j){if(i==j)continue;auto* b=Lookup(game,0,static_cast<std::int32_t>(ids[j]));if(Player(b)&&SameGame(game,b)&&Hostile(Flags(a,ids[j]),Flags(b,ids[i]))){states[i].active=1;break;}}
    }
    ++Sequence;
    for(std::size_t offset=0;offset<n||offset==0;offset+=PlayersPerPacket){Packet p{};p.sequence=Sequence;p.reduction=Reduction;p.count=static_cast<std::uint32_t>(std::min<std::size_t>(PlayersPerPacket,n-offset));std::copy_n(states.begin()+offset,p.count,p.players);

        if(LocalKnown.load())for(std::uint32_t i=0;i<p.count;++i)if(p.players[i].id==LocalId.load()){std::lock_guard lock(StateMutex);Received=p;ReceivedAt=GetTickCount64();break;}
        for(std::size_t i=0;i<pc;++i)(void)Net->sendToClient(C,Channel,peers[i],2,&p,16+p.count*sizeof(PlayerState));if(n==0)break;
    }
}
void QueueGame() noexcept {if(!Threads||!Session.load()||QueuedGame.exchange(true))return;if(Threads->runOnGameThread(C,GameTick,nullptr)!=Threads::Result::Success)QueuedGame.store(false);}
void __cdecl HostMessage(const PluginContext* ctx,Network::ChannelHandle channel,Network::PeerHandle peer,std::uint16_t id,const void*,std::uint32_t size,void*) noexcept {


    if(ctx!=C||channel!=Channel||id!=1||size!=0||!peer)return;
    {std::lock_guard lock(StateMutex);bool found=false;for(std::size_t i=0;i<PeerCount;++i)found|=Peers[i]==peer;if(!found&&PeerCount<MaximumPeers)Peers[PeerCount++]=peer;ActivePeerCount.store(PeerCount);}
    QueueGame();
}
void __cdecl ClientMessage(const PluginContext* ctx,Network::ChannelHandle channel,std::uint16_t id,const void* data,std::uint32_t size,void*) noexcept {
    if(ctx!=C||channel!=Channel||id!=2||!data||size<16||size>sizeof(Packet))return;Packet p{};std::memcpy(&p,data,size);if(!ValidPacket(p,size))return;
    const auto uid=LocalId.load();const bool known=LocalKnown.load();
    if(p.count!=0){bool found=false;for(std::uint32_t i=0;i<p.count;++i)found|=known&&p.players[i].id==uid;if(!found)return;}
    std::lock_guard lock(StateMutex);Received=p;ReceivedAt=GetTickCount64();
}
void __cdecl Connection(const PluginContext* ctx,const Network::ConnectionEvent* event,void*) noexcept {
    if(ctx!=C||!event||event->channel!=Channel)return;
    if(event->state==Network::ConnectionState::Connected)Connected.store(true);
    else if(event->state==Network::ConnectionState::Disconnected||event->state==Network::ConnectionState::Rejected){std::lock_guard lock(StateMutex);for(std::size_t i=0;i<PeerCount;++i)if(Peers[i]==event->peer){Peers[i]=Peers[--PeerCount];break;}ActivePeerCount.store(PeerCount);ReceivedAt=0;}
}
void QueueUi() noexcept;
void __cdecl UiTick(const PluginContext* ctx,void*) noexcept {
    QueuedUi.store(false);if(ctx!=C||!Ready.load()||!Session.load())return;const auto now=GetTickCount64();
    if(now-LastPing>=1000){LastPing=now;(void)Net->connectToHost(C,Channel);(void)Net->sendToHost(C,Channel,1,nullptr,0);}
    if(now-LastUiPoll>=250){LastUiPoll=now;QueueGame();Packet packet{};std::uint64_t at{};{std::lock_guard lock(StateMutex);packet=Received;at=ReceivedAt;}
        bool active=false;if(LocalKnown.load()&&at&&now-at<3000)for(std::uint32_t i=0;i<packet.count;++i)if(packet.players[i].id==LocalId.load())active=packet.players[i].active!=0;
        PublishGui(active,packet.reduction);
    }QueueUi();
}
void QueueUi() noexcept {if(!Threads||!Session.load()||QueuedUi.exchange(true))return;if(Threads->runOnUiThread(C,UiTick,nullptr)!=Threads::Result::Success)QueuedUi.store(false);}
void Reset() noexcept {
    HostReady.store(false);NativeGame.store(0);Connected.store(false);LocalKnown.store(false);QueuedGame.store(false);QueuedUi.store(false);LastPing=LastUiPoll=0;
    {std::lock_guard lock(StateMutex);PeerCount=PlayerCount=0;ActivePeerCount.store(0);Received={};ReceivedAt=0;}
    {std::lock_guard lock(WoundMutex);std::fill(std::begin(Wounds),std::end(Wounds),ListWitness{});WoundCursor=0;}
    PublishGui(false,0);HudVisible=false;
}
void __cdecl Gameplay(const PluginContext* ctx,const Lifecycle::GameplayEvent* event,void*) noexcept {
    if(ctx!=C||!event)return;
    if(event->kind==Lifecycle::GameplayEventKind::GameLeft){Session.store(0);Reset();}
    else if(event->kind==Lifecycle::GameplayEventKind::GameJoined){Reset();Session.store(event->sessionGeneration);QueueUi();}
    else if(event->kind==Lifecycle::GameplayEventKind::LocalPlayerReady){LocalId.store(event->playerId);LocalKnown.store(true);Session.store(event->sessionGeneration);QueueUi();}
}
void __cdecl DrawHover(const PluginContext* ctx,const Overlay::Frame* frame,void*) noexcept {
    if(ctx!=C||!frame||!HudVisible||!GuiActive||!Session.load())return;
    HWND window=GetForegroundWindow();DWORD process{};if(!window||!GetWindowThreadProcessId(window,&process)||process!=GetCurrentProcessId())return;
    POINT mouse{};RECT client{};if(!GetCursorPos(&mouse)||!ScreenToClient(window,&mouse)||!GetClientRect(window,&client)||client.right<=0||client.bottom<=0)return;
    const float mx=mouse.x*frame->screenWidth/client.right,my=mouse.y*frame->screenHeight/client.bottom,s=frame->screenHeight/2160.f;
    const float left=frame->screenWidth*.5f+(HudGrid.x+HudSlot.x)*s,top=frame->screenHeight+(HudGrid.y+HudSlot.y)*s;
    if(mx<left||mx>left+HudSlot.width*s||my<top||my>top+HudSlot.height*s)return;
    char text[160]{};std::snprintf(text,sizeof(text),"Duel Ward\nPvP damage reduced by %u%%\nActive while hostile. Host controlled.",GuiReduction);
    const float size=std::max(12.f,frame->defaultTextSize);Overlay::MeasureTextRequest measure{};measure.structSize=sizeof(measure);measure.canvas=frame->canvas;measure.textSize=size;measure.text=text;measure.textLength=static_cast<std::uint32_t>(std::strlen(text));Overlay::TextMetrics metrics{};metrics.structSize=sizeof(metrics);
    if(Overlay->measureText(C,&measure,&metrics)!=Overlay::Result::Success)return;
    const auto x=std::clamp(left,4.f,std::max(4.f,frame->screenWidth-metrics.width-20));const auto y=std::max(4.f,top-metrics.height-24);
    Overlay::FilledRectangleRequest bg{};bg.structSize=sizeof(bg);bg.canvas=frame->canvas;bg.rect={x,y,x+metrics.width+16,y+metrics.height+16};bg.color={0,0,0,.94f};(void)Overlay->drawFilledRectangle(C,&bg);
    Overlay::TextRequest draw{};draw.structSize=sizeof(draw);draw.canvas=frame->canvas;draw.position={x+8,y+8};draw.color={.95f,.85f,.55f,1};draw.textSize=size;draw.text=text;draw.textLength=measure.textLength;(void)Overlay->drawText(C,&draw);
}
template<std::size_t N>bool Check(std::uintptr_t rva,const std::uint8_t(&expected)[N]) noexcept {const auto* p=reinterpret_cast<const void*>(C->exeBase+rva);return Range(p,N)&&std::memcmp(p,expected,N)==0;}
void Configure() noexcept {
    wchar_t path[32768]{};const auto* directory=C->pluginDirectory;if(!directory)return;
    std::swprintf(path,std::size(path),L"%ls\\PvpWard.ini",directory);Enabled=GetPrivateProfileIntW(L"PvpWard",L"Enabled",1,path)!=0;
    const auto value=static_cast<std::int32_t>(GetPrivateProfileIntW(L"PvpWard",L"DamageReductionPercent",75,path));Reduction=static_cast<std::uint32_t>(std::clamp(value,0,100));
}
}
bool Initialize(const D2RL::PluginContext* ctx) noexcept {
    C=ctx;Configure();if(!Enabled)return true;
    if(ctx->QueryService(&Net)!=ServiceQueryResult::Success||!HasNetworkServiceField(Net,NetworkServiceRequiredSize)||ctx->QueryService(&Threads)!=ServiceQueryResult::Success||!HasThreadServiceField(Threads,ThreadServiceRequiredSize)||ctx->QueryService(&Life)!=ServiceQueryResult::Success||!HasLifecycleServiceField(Life,LifecycleServiceRequiredSize)||ctx->QueryService(&Overlay)!=ServiceQueryResult::Success||!HasOverlayServiceField(Overlay,OverlayServiceRequiredSize))return false;
    if(!Check(Native::ToggleHostilityRva,Native::ToggleHostilityBytes)||!Check(Native::FinalDamageRva,Native::FinalDamageBytes)||!Check(Native::CrushingBlowRva,Native::CrushingBlowBytes)||!Check(Native::UnitLookupRva,Native::UnitLookupBytes)||!Check(Native::OwnerLookupRva,Native::OwnerLookupBytes)||!Check(Native::PlayerDataRva,Native::PlayerDataBytes)||!Check(Native::PlayerListRva,Native::PlayerListBytes)||!Check(Native::StatListOwnerRva,Native::StatListOwnerBytes)||!Check(Native::FinalDamageCallRva,Native::FinalDamageCallBytes)||!Check(Native::TotalDamageStoreRva,Native::TotalDamageStoreBytes)||!Check(Native::PoisonRateRva,Native::PoisonRateBytes)||!Check(Native::CrushingSetHPRva,Native::CrushingSetHPBytes)||!Check(Native::OpenWoundsStateRva,Native::OpenWoundsStateBytes)||!Check(Native::CurseListOwnerRva,Native::CurseListOwnerBytes)||!Check(Native::CurseListPostRva,Native::CurseListPostBytes))return false;
    Lookup=reinterpret_cast<LookupFn>(C->exeBase+Native::UnitLookupRva);Owner=reinterpret_cast<OwnerFn>(C->exeBase+Native::OwnerLookupRva);Core::RipIndirectBridgeResolution listBridge{};
    if(!Core::ResolveRipIndirectBridge(C,Native::GetListStatBridgeRva,std::to_array(Native::GetListStatBridgeBytes),0x3e2a208,&listBridge))return false;
    GetList=reinterpret_cast<GetListFn>(listBridge.targetAddress);
    Core::RipIndirectBridgeResolution unitBridge{};
    if(!Core::ResolveRipIndirectBridge(C,Native::SetUnitStatBridgeRva,std::to_array(Native::SetUnitStatBridgeBytes),0x3e2a248,&unitBridge))return false;
    OriginalSetUnit=reinterpret_cast<SetUnitFn>(unitBridge.targetAddress);
    if(!Core::Hooks().InstallRipIndirectChain("PvP Open Wounds rates",Native::SetListStatBridgeRva,std::to_array(Native::SetListStatBridgeBytes),0x3e2a230,SetList,&OriginalSetList)
        ||!Core::StatListPosts().Register({"PvP Open Wounds",-100,OnPost,nullptr})
        ||!ctx->InstallInlineHook(Native::ToggleHostilityRva,Native::ToggleHostilityBytes,sizeof(Native::ToggleHostilityBytes),Toggle,&OriginalToggle)
        ||!ctx->InstallInlineHook(Native::FinalDamageRva,Native::FinalDamageBytes,sizeof(Native::FinalDamageBytes),Final,&OriginalFinal)
        ||!ctx->InstallInlineHook(Native::CrushingBlowRva,Native::CrushingBlowBytes,sizeof(Native::CrushingBlowBytes),Crushing,&OriginalCrushing))return false;
    Network::ChannelRegistration registration{};registration.structSize=sizeof(registration);registration.localChannelId=1;registration.compatibilityToken=CompatibilityToken;registration.hostMessage=HostMessage;registration.clientMessage=ClientMessage;registration.connectionState=Connection;
    if(Net->registerChannel(C,&registration,&Channel)!=Network::Result::Success)return false;
    unsigned index=0;for(const auto kind:{Lifecycle::GameplayEventKind::GameJoined,Lifecycle::GameplayEventKind::LocalPlayerReady,Lifecycle::GameplayEventKind::GameLeft}){
        Lifecycle::GameplayEventListener listener{};listener.structSize=sizeof(listener);listener.kind=kind;listener.callback=Gameplay;if(Life->registerGameplayEventListener(C,&listener,&Listeners[index++])!=Lifecycle::Result::Success)return false;
    }
    Overlay::CallbackRegistration overlay{};overlay.structSize=sizeof(overlay);overlay.phase=Overlay::Phase::AfterGameUi;overlay.callback=DrawHover;if(Overlay->registerFrameCallback(C,&overlay,&OverlayHandle)!=Overlay::Result::Success)return false;
    Ready.store(true);return true;
}
void Shutdown() noexcept {
    Ready.store(false);Session.store(0);Reset();
    if(C&&Net&&Channel)(void)Net->unregisterChannel(C,Channel);Channel=0;
    if(C&&Life)for(auto h:Listeners)if(h)(void)Life->unregisterGameplayEventListener(C,h);
    if(C&&Overlay&&OverlayHandle)(void)Overlay->unregisterFrameCallback(C,OverlayHandle);OverlayHandle=0;
}
void ClearHudRect() noexcept {HudVisible=false;}
void SetHudRect(D2RL::Widgets::Rect grid,D2RL::Widgets::Rect slot) noexcept {HudGrid=grid;HudSlot=slot;HudVisible=true;}
}
