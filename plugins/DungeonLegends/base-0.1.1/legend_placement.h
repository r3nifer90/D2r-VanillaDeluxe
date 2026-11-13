#pragma once


namespace Placement {
struct Rect {int32_t x,y,w,h;};
struct Pending {uint64_t epoch=0;void* game=nullptr;uint32_t level=0,profile=0,variant=0;Rect box={};bool waiting=false,busy=false;uint64_t next=0,logAt=0;uint32_t failures=0;};
Pending plans[1024];uint32_t waitingCount=0;
struct Anchor {uint64_t epoch;void* game;uint32_t level;Rect box;int32_t x,y;};Anchor anchors[256];uint32_t anchorCursor=0;
using PopulateFn=void(__fastcall*)(void*,const void*);PopulateFn priorPopulation=nullptr;void** populationSlot=nullptr;
AllocateFn priorAllocation=nullptr;void** allocationSlot=nullptr;
struct Position {int32_t x,y;uint32_t level;};struct Snapshot {Position points[64];uint32_t count=0;bool invalid=false;};
void __fastcall Capture(void* client,void* data) noexcept {
 auto& out=*static_cast<Snapshot*>(data);uintptr_t unit=0,path=0;uint32_t type=99;
 if(out.count>=64||!Read(reinterpret_cast<uintptr_t>(client)+0x278,unit)||!unit||!Read(unit,type)||type!=0||!Read(unit+0x38,path)||!path||!Readable(path,128)){out.invalid=true;return;}
 const int32_t id=getLevel(reinterpret_cast<void*>(unit));if(id<1||id>=1024){out.invalid=true;return;}
 out.points[out.count++]={getX(reinterpret_cast<void*>(path)),getY(reinterpret_cast<void*>(path)),static_cast<uint32_t>(id)};
}
bool Players(void* game,Snapshot& out) noexcept {auto each=reinterpret_cast<EachClientFn>(RipSlot(coreBase+0x825806));if(!each)return false;each(game,Capture,&out,0);return out.count&&!out.invalid;}
uint32_t Distance(int32_t x,int32_t y,int32_t a,int32_t b) noexcept {uint32_t dx=x>a?x-a:a-x,dy=y>b?y-b:b-y;return dx>dy?dx:dy;}
bool Safe(const Snapshot& s,uint32_t level,int32_t x,int32_t y) noexcept {for(uint32_t n=0;n<s.count;++n)if(s.points[n].level==level&&Distance(x,y,s.points[n].x,s.points[n].y)<cfg.distance)return false;return true;}
bool Geometry(uintptr_t room,uintptr_t parent,Rect& r) noexcept {uintptr_t level=0;int32_t x=0,y=0,w=0,h=0;if(!Read(room+0x90,level)||level!=parent||!Read(room+0x60,x)||!Read(room+0x64,y)||!Read(room+0x68,w)||!Read(room+0x6c,h)||x<0||y<0||w<2||h<2||x>13107||y>13107||w>13107-x||h>13107-y)return false;r={x*5,y*5,w*5,h*5};return true;}
bool RoomLevel(const void* active,uintptr_t& room,uintptr_t& level,uint32_t& id) noexcept {return Read(reinterpret_cast<uintptr_t>(active)+0x18,room)&&room&&Read(room+0x90,level)&&level&&Read(level+0x1f8,id)&&id>0&&id<1024;}
bool Same(const Rect& a,const Rect& b) noexcept {return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
bool Eligible(void* game,uint32_t level) noexcept {uint8_t difficulty=255,bank=0;return IsDungeon(level)&&Read(reinterpret_cast<uintptr_t>(game)+0x104,difficulty)&&difficulty==2&&Read(reinterpret_cast<uintptr_t>(game)+0x106,bank)&&bank>=2&&bank<=3;}
void Reset() noexcept {memset(plans,0,sizeof(plans));memset(anchors,0,sizeof(anchors));waitingCount=anchorCursor=0;}
void Materialize(Pending& p,const void* active) noexcept {
 const uint64_t now=GetTickCount64();if(!p.waiting||p.busy||now<p.next||p.epoch!=serverGeneration||p.epoch!=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0)))return;
 if(!Eligible(p.game,p.level)){p.waiting=false;if(waitingCount)--waitingCount;Log("TL_LOCATION_CANCEL ineligible_dungeon=1");return;}
 Snapshot players;if(!Players(p.game,players)){p.next=now+2000;return;}
 uint32_t candidates[32]={},available=0;for(uint32_t n=0;n<256&&available<32;++n){const auto& a=anchors[n];if(a.epoch==p.epoch&&a.game==p.game&&a.level==p.level&&Same(a.box,p.box))candidates[available++]=n;}
 p.busy=true;p.next=now+2500;uint32_t calls=0,blocked=0;bool created=false;
 for(uint32_t trial=0;trial<64&&calls<8;++trial){int32_t x=p.box.x+2+Rand(p.box.w-4),y=p.box.y+2+Rand(p.box.h-4);if(available&&trial<32){const auto& a=anchors[candidates[Rand(available)]];x=a.x+static_cast<int32_t>(Rand(17))-8;y=a.y+static_cast<int32_t>(Rand(17))-8;if(x<p.box.x+2||y<p.box.y+2||x>=p.box.x+p.box.w-2||y>=p.box.y+p.box.h-2)continue;}if(!Safe(players,p.level,x,y)){++blocked;continue;}

  Snapshot live;if(!Players(p.game,live)||!Safe(live,p.level,x,y)){++blocked;continue;}
  SpawnRequest r={};r.game=p.game;r.room=active;r.x=x;r.y=y;r.mode=1;r.flags=3;++calls;
  if(Spawn(r,p.profile,p.variant,p.level)){created=true;break;}
 }
 p.waiting=!created;if(created&&waitingCount)--waitingCount;p.busy=false;if(!created&&calls)++p.failures;
 if(created||calls||now>=p.logAt){p.logAt=now+30000;Text t;t.Add("TL_LOCATION_ATTEMPT level=");t.Num(p.level);t.Add(" profile=");t.Add(TLData::Profiles[p.profile].key);t.Add(" created=");t.Num(created);t.Add(" native_calls=");t.Num(calls);t.Add(" player_distance_skips=");t.Num(blocked);t.Add(" waiting_same_room=");t.Num(p.waiting);Log(t.s);}

 if(p.failures>=12){p.waiting=false;if(waitingCount)--waitingCount;Log("TL_LOCATION_FAILED: 96 bounded native allocation failures in selected room; no announcement, no reroll.",true);}
}
void Observe(void* game,const void* active) noexcept {if(!cfg.enabled||!serverGeneration||!waitingCount)return;uintptr_t room=0,level=0;uint32_t id=0;Rect box={};if(!RoomLevel(active,room,level,id)||!Geometry(room,level,box))return;auto& p=plans[id];if(p.waiting&&p.game==game&&Same(p.box,box))Materialize(p,active);}
void __fastcall Populate(void* game,const void* active) noexcept {priorPopulation(game,active);Observe(game,active);}
void* __fastcall Allocation(const void* raw) noexcept {
 void* result=priorAllocation(raw);SpawnRequest r={};uint32_t type=99,kind=0;
 if(result&&Read(reinterpret_cast<uintptr_t>(result),type)&&type==1&&Read(reinterpret_cast<uintptr_t>(result)+4,kind)&&raw&&Read(reinterpret_cast<uintptr_t>(raw),r)){
  if(kind>=TLData::FirstMon&&kind<TLData::TotalMon)Affixes::Restore(r.game,result);
  if(waitingCount){uintptr_t room=0,level=0,path=0;uint32_t id=0;Rect box={};
   if(kind<TLData::FirstMon&&RoomLevel(r.room,room,level,id)&&plans[id].waiting&&plans[id].game==r.game&&Geometry(room,level,box)&&Read(reinterpret_cast<uintptr_t>(result)+0x38,path)&&path){int32_t x=getX(reinterpret_cast<void*>(path)),y=getY(reinterpret_cast<void*>(path));if(x>=box.x+2&&y>=box.y+2&&x<box.x+box.w-2&&y<box.y+box.h-2)anchors[anchorCursor++&255]={serverGeneration,r.game,id,box,x,y};}
   Observe(r.game,r.room);
  }
 }return result;
}
bool Install() noexcept {
 if(populationSlot&&allocationSlot)return true;using namespace TerrorLocation;
 if(!Bytes(coreBase+RoomTraversalRva,RoomTraversalBytes,sizeof(RoomTraversalBytes))||!Bytes(coreBase+RoomCoordinatesRva,RoomCoordinatesBytes,sizeof(RoomCoordinatesBytes))||!Bytes(coreBase+PopulateWrapperRva,PopulateWrapperBytes,sizeof(PopulateWrapperBytes)))return false;
 auto* pop=reinterpret_cast<void**>(coreBase+PopulateOriginalSlotRva);auto* alloc=reinterpret_cast<void**>(coreBase+0x707f38);uintptr_t p=0,a=0;
 if(!Read(reinterpret_cast<uintptr_t>(pop),p)||!Read(reinterpret_cast<uintptr_t>(alloc),a)||!Executable(p)||!Executable(a)||!Writable(reinterpret_cast<uintptr_t>(pop),8)||!Writable(reinterpret_cast<uintptr_t>(alloc),8))return false;
 priorPopulation=reinterpret_cast<PopulateFn>(p);priorAllocation=reinterpret_cast<AllocateFn>(a);
 if(InterlockedCompareExchangePointer(pop,reinterpret_cast<void*>(Populate),reinterpret_cast<void*>(p))!=reinterpret_cast<void*>(p))return false;populationSlot=pop;
 if(InterlockedCompareExchangePointer(alloc,reinterpret_cast<void*>(Allocation),reinterpret_cast<void*>(a))!=reinterpret_cast<void*>(a)){InterlockedCompareExchangePointer(pop,reinterpret_cast<void*>(p),reinterpret_cast<void*>(Populate));populationSlot=nullptr;return false;}
 allocationSlot=alloc;allocateOne=priorAllocation;Log("TL_ROOM_DELEGATES_READY originals_forwarded_once=1 host_only=1");return true;
}
void UnlinkKnown(const wchar_t* dll,const char* symbol,void* observer,void* replacement) noexcept {using Fn=bool(__cdecl*)(void*,void*) noexcept;HMODULE h=GetModuleHandleW(dll);if(h){auto f=reinterpret_cast<Fn>(GetProcAddress(h,symbol));if(f)f(observer,replacement);}}
void Remove() noexcept {
 if(populationSlot&&priorPopulation&&InterlockedCompareExchangePointer(populationSlot,reinterpret_cast<void*>(priorPopulation),reinterpret_cast<void*>(Populate))!=reinterpret_cast<void*>(Populate)){
  UnlinkKnown(L"SovereignPacks.dll","R3niSovereignUnlinkPopulation",reinterpret_cast<void*>(Populate),reinterpret_cast<void*>(priorPopulation));UnlinkKnown(L"TerrorEncounters.dll","R3niTerrorUnlinkPopulation",reinterpret_cast<void*>(Populate),reinterpret_cast<void*>(priorPopulation));}
 if(allocationSlot&&priorAllocation&&InterlockedCompareExchangePointer(allocationSlot,reinterpret_cast<void*>(priorAllocation),reinterpret_cast<void*>(Allocation))!=reinterpret_cast<void*>(Allocation))UnlinkKnown(L"SovereignPacks.dll","R3niSovereignUnlinkAllocation",reinterpret_cast<void*>(Allocation),reinterpret_cast<void*>(priorAllocation));populationSlot=allocationSlot=nullptr;
}
void Poll(const SpawnRequest& request) noexcept {
 uintptr_t room=0,level=0;uint32_t id=0;if(!RoomLevel(request.room,room,level,id))return;auto& p=plans[id];if(!p.waiting||p.game!=request.game)return;
 uint32_t offset=0,matches=0;for(uint32_t off=0;off<=0x58;off+=8){uintptr_t active=0;if(Read(room+off,active)&&active==reinterpret_cast<uintptr_t>(request.room)){offset=off;++matches;}}if(matches!=1)return;
 uintptr_t current=0;if(!Read(level+0x10,current))return;for(uint32_t n=0;current&&n<2048;++n){Rect box={};uintptr_t next=0,active=0,back=0;if(!Geometry(current,level,box)||!Read(current+0x48,next)||next==current)return;if(Same(box,p.box)&&Read(current+offset,active)&&active&&Read(active+0x18,back)&&back==current){Materialize(p,reinterpret_cast<void*>(active));return;}current=next;}
}
bool Plan(const SpawnRequest& request,uint32_t id,uint32_t profile,uint32_t variant) noexcept {
 uintptr_t room=0,level=0;uint32_t actual=0;if(!RoomLevel(request.room,room,level,actual)||actual!=id)return false;Snapshot players;if(!Players(request.game,players))return false;
 uintptr_t current=0;if(!Read(level+0x10,current)||!current)return false;Rect chosen={};uint32_t count=0,eligible=0;
 for(;current&&count<2048;++count){Rect r={};uintptr_t next=0;if(!Geometry(current,level,r)||!Read(current+0x48,next)||next==current)return false;
  bool safe=false;const int32_t xs[]={r.x+2,r.x+r.w/2,r.x+r.w-3},ys[]={r.y+2,r.y+r.h/2,r.y+r.h-3};for(int32_t x:xs)for(int32_t y:ys)if(Safe(players,id,x,y))safe=true;
  if(safe&&Rand(++eligible)==0)chosen=r;current=next;
 }
 if(current||!eligible){Log("TL_LOCATION_SKIP: no safely distant room; no near-player fallback.",true);return false;}
 plans[id]={serverGeneration,request.game,id,profile,variant,chosen,true,false};++waitingCount;Text t;t.Add("TL_LOCATION_PLAN level=");t.Num(id);t.Add(" profile=");t.Add(TLData::Profiles[profile].key);t.Add(" rooms=");t.Num(count);t.Add(" eligible_rooms=");t.Num(eligible);t.Add(" room_x=");t.Int(chosen.x);t.Add(" room_y=");t.Int(chosen.y);t.Add(" min_player_distance=");t.Num(cfg.distance);t.Add(" announce_after_confirmed_spawn=1");Log(t.s);Poll(request);return true;
}
}
