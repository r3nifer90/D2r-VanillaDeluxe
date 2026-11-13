#pragma once
#include "location_guard.h"
#include "visited.h"
namespace Placement {
struct Rect {int32_t x,y,w,h;};
struct Pending {uint64_t generation=0;void* game=nullptr;uint32_t level=0,kind=0,minions=0,spawned=0,minimum=0;Rect box={};int32_t x=0,y=0;bool waiting=false,busy=false;uint64_t nextAttempt=0;uint32_t misses=0;uint64_t nextSafetyLog=0,plannedAt=0,nextRelocate=0;uint32_t relocations=0;int32_t homeX=0,homeY=0;uint32_t radius=0;};
Pending plans[1024*15];uint8_t totals[1024]={};
using PopulateFn=void(__fastcall*)(void*,const void*);PopulateFn originalPopulation=nullptr;void** populationSlot=nullptr;
AllocateFn previousAllocate=nullptr;void** allocationSlot=nullptr;
uint64_t failureWindow=0;uint32_t failedCalls=0;
struct Anchor {void* game=nullptr;uint32_t level=0;Rect box={};int32_t x=0,y=0;};
Anchor anchors[1024];uint32_t anchorCount=0,anchorNext=0;
struct LoadedRoom {const void* active=nullptr;Rect box={};};
void Reset() noexcept {Visits::Reset();anchorCount=anchorNext=0;memset(anchors,0,sizeof(anchors));failureWindow=0;failedCalls=0;Affixes::Reset();memset(plans,0,sizeof(plans));memset(totals,0,sizeof(totals));}
bool CapturePositions(void* game,Players::Snapshot& snapshot) noexcept {if(!Players::Capture(game,snapshot))return false;Visits::Remember(snapshot);return true;}
bool Geometry(uintptr_t room2,uintptr_t level,Rect& r) noexcept {uintptr_t parent=0;int32_t x=0,y=0,w=0,h=0;
 if(!Read(room2+0x90,parent)||parent!=level||!Read(room2+0x60,x)||!Read(room2+0x64,y)||!Read(room2+0x68,w)||!Read(room2+0x6c,h)||x<0||y<0||w<2||h<2||x>13107||y>13107||w>13107-x||h>13107-y)return false;r={x*5,y*5,w*5,h*5};return true;}
bool RoomLevel(const void* active,uintptr_t& room2,uintptr_t& level,uint32_t& id) noexcept {return Read(reinterpret_cast<uintptr_t>(active)+0x18,room2)&&room2&&Read(room2+0x90,level)&&level&&Read(level+0x1f8,id)&&id>0&&id<1024;}
bool Same(const Rect& a,const Rect& b) noexcept {return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
uint32_t Distance(int32_t x,int32_t y,int32_t a,int32_t b) noexcept {uint32_t dx=x>a?x-a:a-x,dy=y>b?y-b:b-y;return dx>dy?dx:dy;}
bool Apart(const Pending& p,int32_t x,int32_t y) noexcept {for(uint32_t n=0;n<totals[p.level];++n){const auto& other=plans[p.level*15+n];if(&other!=&p&&other.generation==serverGeneration&&Distance(x,y,other.x,other.y)<p.minimum)return false;}return true;}


bool Territory(const Pending& p,int32_t x,int32_t y) noexcept {
 const uint32_t own=Distance(x,y,p.homeX,p.homeY);
 for(uint32_t n=0;n<totals[p.level];++n){const auto& other=plans[p.level*15+n];if(&other==&p||other.generation!=p.generation)continue;
  const uint32_t d=Distance(x,y,other.homeX,other.homeY);if(d<own||(d==own && &other<&p))return false;
 }return true;
}


void AnchorAccepted(const SpawnRequest& request,void* result) noexcept {
 if(!result||!serverGeneration||serverGeneration!=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0)))return;
 uint32_t type=99,kind=0;uintptr_t room2=0,level=0;uint32_t id=0;Rect box={};const uintptr_t unit=reinterpret_cast<uintptr_t>(result);
 if(!Read(unit,type)||type!=1||!Read(unit+4,kind)||kind>=815||!RoomLevel(request.room,room2,level,id)||!totals[id]||!Geometry(room2,level,box))return;
 int32_t x=request.x,y=request.y;uintptr_t path=0;
 if(Read(unit+0x38,path)&&path&&Readable(path,128)){x=getX(reinterpret_cast<void*>(path));y=getY(reinterpret_cast<void*>(path));}
 if(x<box.x+2||y<box.y+2||x>=box.x+box.w-2||y>=box.y+box.h-2)return;
 for(uint32_t n=0;n<anchorCount;++n){auto& a=anchors[n];if(a.game==request.game&&a.level==id&&Same(a.box,box)&&Distance(x,y,a.x,a.y)<8)return;}
 const uint32_t index=anchorCount<1024?anchorCount++:anchorNext++%1024;anchors[index]={request.game,id,box,x,y};
}
void Materialize(Pending& p,const void* active) noexcept {
 if(!p.waiting||p.busy||p.game==nullptr||p.generation!=serverGeneration||serverGeneration!=static_cast<uint64_t>(InterlockedCompareExchange64(&expectedGeneration,0,0)))return;
 uint8_t difficulty=0;if(!cfg.enabled||!Read(reinterpret_cast<uintptr_t>(p.game)+0x104,difficulty)||difficulty!=2||!EligibleGame(p.game))return;
 const uint64_t now=GetTickCount64();if(now<p.nextAttempt)return;
 if(now>=failureWindow+250){failureWindow=now;failedCalls=0;}



 if(failedCalls>=8)return;
 Players::Snapshot players={};if(!CapturePositions(p.game,players)){p.nextAttempt=now+2000;if(now>=p.nextSafetyLog){p.nextSafetyLog=now+30000;Log("SP_PLAYER_SAFETY_WAIT: authoritative player positions unavailable; no native allocation.");}return;}
 uint32_t anchorIndices[32]={},available=0;
 for(uint32_t n=0;n<anchorCount&&available<32;++n)if(anchors[n].game==p.game&&anchors[n].level==p.level&&Same(anchors[n].box,p.box)&&Territory(p,anchors[n].x,anchors[n].y))anchorIndices[available++]=n;
 p.busy=true;uint32_t safetySkips=0,visitedSkips=0,anchorCalls=0,nearestAtSpawn=0xffffffff;const auto& k=SPData::Kinds[p.kind];uint32_t calls=0,failures=0;const uint32_t before=p.spawned;

 for(uint32_t tries=0;tries<96&&calls<16&&failedCalls<8&&p.spawned<=p.minions;++tries){const bool leader=p.spawned==0;
  int32_t x=0,y=0;


  const bool anchored=leader&&available&&tries<48&&(p.misses<8||tries%2==0);
  if(anchored){const auto& a=anchors[anchorIndices[Rand(available)]];x=a.x+static_cast<int32_t>(Rand(17))-8;y=a.y+static_cast<int32_t>(Rand(17))-8;}
  else if(leader&&(tries>=32||p.misses>=8)){x=p.box.x+2+Rand(p.box.w-4);y=p.box.y+2+Rand(p.box.h-4);}
  else {x=p.x+(leader?static_cast<int32_t>(Rand(17))-8:static_cast<int32_t>(Rand(13))-6);y=p.y+(leader?static_cast<int32_t>(Rand(17))-8:static_cast<int32_t>(Rand(13))-6);}
  if(x<p.box.x+2||y<p.box.y+2||x>=p.box.x+p.box.w-2||y>=p.box.y+p.box.h-2||(leader&&(!Territory(p,x,y)||!Apart(p,x,y))))continue;
  players={};if(!CapturePositions(p.game,players)){++safetySkips;break;}
  uint32_t nearest=0;if(!Players::Safe(players,p.level,x,y,nearest)){++safetySkips;continue;}
  if(Visits::Has(p.level,x,y)){++visitedSkips;continue;}
  if(nearest<nearestAtSpawn)nearestAtSpawn=nearest;
  SpawnRequest r={p.game,active,nullptr,static_cast<int32_t>(leader?k.leader:k.minion),1,0,x,y,3,0,0};
  ++calls;if(anchored)++anchorCalls;void* unit=Allocate(r,leader);
  if(!unit){++failures;++failedCalls;if(leader)++p.misses;continue;}
  ++p.spawned;if(leader){p.x=x;p.y=y;}

 }
 p.waiting=p.spawned<=p.minions;p.busy=false;
 if(p.waiting)p.nextAttempt=now+(p.spawned==before?5000:2000);
 if(visitedSkips&&now>=p.nextSafetyLog){p.nextSafetyLog=now+30000;Text v;v.Add("SP_VISITED_WAIT level=");v.Num(p.level);v.Add(" leader_class=");v.Num(k.leader);v.Add(" skipped=");v.Num(visitedSkips);v.Add(" members=");v.Num(p.spawned);v.Add(" history_preserved=1");Log(v.s);}
 if(safetySkips&&now>=p.nextSafetyLog){p.nextSafetyLog=now+30000;Text safety;safety.Add("SP_PLAYER_SAFETY_WAIT level=");safety.Num(p.level);safety.Add(" blocked_candidates=");safety.Num(safetySkips);safety.Add(" minimum_distance=");safety.Num(cfg.playerDistance);safety.Add(" rerolled=0");Log(safety.s);}
 if(!calls)return;
 if(anchorCalls){Text a;a.Add("SP_PACK_ANCHOR level=");a.Num(p.level);a.Add(" leader_class=");a.Num(k.leader);a.Add(" available=");a.Num(available);a.Add(" allocation_calls=");a.Num(anchorCalls);a.Add(" created_leader=");a.Num(before==0&&p.spawned>0);Log(a.s);}

 Text t;t.Add("SP_PACK_SPAWN level=");t.Num(p.level);t.Add(" leader_class=");t.Num(k.leader);t.Add(" members=");t.Num(p.spawned);t.Add(" target_members=");t.Num(p.minions+1);t.Add(" retry_missing_only=");t.Num(p.waiting);t.Add(" allocation_calls=");t.Num(calls);t.Add(" player_distance=");t.Num(nearestAtSpawn);t.Add(" safety_skips=");t.Num(safetySkips);t.Add(" failures=");t.Num(failures);t.Add(" retry_after_ms=");t.Num(p.waiting?(p.spawned==before?5000:2000):0);Log(t.s,p.waiting);
}


bool Rescue(Pending& p,const LoadedRoom* rooms,uint32_t count) noexcept {
 const uint64_t now=GetTickCount64();
 if(!count||!p.waiting||p.spawned||p.busy||now<p.plannedAt+8000||(now<p.nextAttempt&&p.misses<16)||now<p.nextRelocate)return false;
 Players::Snapshot players={};if(!CapturePositions(p.game,players))return false;
 uint32_t best=0,bestScore=0;int32_t bestX=0,bestY=0;bool chosen=false,usedAnchor=false;
 const uint32_t separation=p.minimum>cfg.separation?p.minimum:cfg.separation;
 uint32_t anchoredRooms[2048]={},anchoredRoomCount=0;
 for(uint32_t r=0;r<count;++r)for(uint32_t a=0;a<anchorCount;++a)if(anchors[a].game==p.game&&anchors[a].level==p.level&&Same(anchors[a].box,rooms[r].box)&&Territory(p,anchors[a].x,anchors[a].y)){anchoredRooms[anchoredRoomCount++]=r;break;}
 for(uint32_t trial=0;trial<64;++trial){const uint32_t roomIndex=trial<32&&anchoredRoomCount?anchoredRooms[Rand(anchoredRoomCount)]:Rand(count);const auto& room=rooms[roomIndex];int32_t x=0,y=0;
  uint32_t anchorIndices[32]={},available=0;
  for(uint32_t n=0;n<anchorCount&&available<32;++n)if(anchors[n].game==p.game&&anchors[n].level==p.level&&Same(anchors[n].box,room.box)&&Territory(p,anchors[n].x,anchors[n].y))anchorIndices[available++]=n;
  const bool anchored=available>0&&trial<48;
  if(anchored){const auto& a=anchors[anchorIndices[Rand(available)]];x=a.x+static_cast<int32_t>(Rand(17))-8;y=a.y+static_cast<int32_t>(Rand(17))-8;}
  else {x=room.box.x+2+Rand(room.box.w-4);y=room.box.y+2+Rand(room.box.h-4);}
  if(x<room.box.x+2||y<room.box.y+2||x>=room.box.x+room.box.w-2||y>=room.box.y+room.box.h-2)continue;
  uint32_t nearestPlayer=0;if(!Territory(p,x,y)||!Players::Safe(players,p.level,x,y,nearestPlayer)||Visits::Has(p.level,x,y))continue;


  if(p.misses>=8&&Same(p.box,room.box)&&trial<48)continue;
  uint32_t score=0x7fffffff;for(uint32_t n=0;n<totals[p.level];++n){const auto& other=plans[p.level*15+n];if(&other==&p||other.generation!=p.generation)continue;const uint32_t d=Distance(x,y,other.x,other.y);if(d<score)score=d;}
  if(score<separation)continue;


  if(!chosen||(anchored&&!usedAnchor)||(anchored==usedAnchor&&score>bestScore)){chosen=true;usedAnchor=anchored;bestScore=score;best=roomIndex;bestX=x;bestY=y;}
 }
 p.nextRelocate=now+10000;if(!chosen)return false;
 const int32_t oldX=p.x,oldY=p.y;p.box=rooms[best].box;p.x=bestX;p.y=bestY;p.minimum=separation;p.misses=0;p.nextAttempt=0;++p.relocations;
 Text text;text.Add("SP_PACK_RELOCATE level=");text.Num(p.level);text.Add(" leader_class=");text.Num(SPData::Kinds[p.kind].leader);text.Add(" old_x=");text.Int(oldX);text.Add(" old_y=");text.Int(oldY);text.Add(" new_x=");text.Int(p.x);text.Add(" new_y=");text.Int(p.y);text.Add(" anchor=");text.Num(usedAnchor);text.Add(" partition=voronoi unvisited=1 attempt=");text.Num(p.relocations);text.Add(" spawned_before=0 model_and_count_preserved=1");Log(text.s);
 Materialize(p,rooms[best].active);return true;
}
void Observe(void* game,const void* active) noexcept {
 if(!cfg.enabled||!expectedGeneration||!serverGeneration)return;uintptr_t room2=0,level=0;uint32_t id=0;Rect box={};if(!RoomLevel(active,room2,level,id)||!Geometry(room2,level,box)||!totals[id])return;
 Players::Snapshot positions={};if(!CapturePositions(game,positions))return;
 for(uint32_t n=0;n<totals[id];++n){auto& p=plans[id*15+n];if(p.game==game&&Same(p.box,box))Materialize(p,active);}


}
void __fastcall Populate(void* game,const void* room) noexcept {originalPopulation(game,room);Observe(game,room);}
void* __fastcall Allocation(const SpawnRequest* request) noexcept {
 void* result=previousAllocate(request);


 SpawnRequest copy={};if(result&&request&&Readable(reinterpret_cast<uintptr_t>(request),sizeof(copy))){memcpy(&copy,request,sizeof(copy));AnchorAccepted(copy,result);Affixes::Restore(copy.game,result);NotifyEnemySkills(copy.game,result);Observe(copy.game,copy.room);}return result;
}
bool Install() noexcept {
 if(populationSlot&&allocationSlot)return true;using namespace TerrorLocation;
 if(!Bytes(coreBase+RoomTraversalRva,RoomTraversalBytes,sizeof(RoomTraversalBytes))||!Bytes(coreBase+RoomCoordinatesRva,RoomCoordinatesBytes,sizeof(RoomCoordinatesBytes))||!Bytes(coreBase+PopulateWrapperRva,PopulateWrapperBytes,sizeof(PopulateWrapperBytes)))return false;
 auto* pop=reinterpret_cast<void**>(coreBase+PopulateOriginalSlotRva);auto* alloc=reinterpret_cast<void**>(coreBase+0x707f38);uintptr_t a=0,p=0;
 if(!Read(reinterpret_cast<uintptr_t>(pop),p)||!Read(reinterpret_cast<uintptr_t>(alloc),a)||!Executable(a)||!Executable(p)||!Writable(reinterpret_cast<uintptr_t>(pop),8)||!Writable(reinterpret_cast<uintptr_t>(alloc),8))return false;
 originalPopulation=reinterpret_cast<PopulateFn>(p);previousAllocate=reinterpret_cast<AllocateFn>(a);
 if(InterlockedCompareExchangePointer(pop,reinterpret_cast<void*>(Populate),reinterpret_cast<void*>(p))!=reinterpret_cast<void*>(p))return false;populationSlot=pop;
 if(InterlockedCompareExchangePointer(alloc,reinterpret_cast<void*>(Allocation),reinterpret_cast<void*>(a))!=reinterpret_cast<void*>(a)){InterlockedCompareExchangePointer(pop,reinterpret_cast<void*>(p),reinterpret_cast<void*>(Populate));populationSlot=nullptr;return false;}
 allocationSlot=alloc;allocateOne=previousAllocate;Log("SP_ROOM_OBSERVERS_READY: chained delegates, original population/allocation called once.");return true;
}
void Remove() noexcept {if(allocationSlot&&previousAllocate)InterlockedCompareExchangePointer(allocationSlot,reinterpret_cast<void*>(previousAllocate),reinterpret_cast<void*>(Allocation));if(populationSlot&&originalPopulation&&InterlockedCompareExchangePointer(populationSlot,reinterpret_cast<void*>(originalPopulation),reinterpret_cast<void*>(Populate))!=reinterpret_cast<void*>(Populate)){
 HMODULE te=GetModuleHandleW(L"TerrorEncounters.dll");using UnlinkFn=bool(__cdecl*)(void*,void*) noexcept;if(te){auto unlink=reinterpret_cast<UnlinkFn>(GetProcAddress(te,"R3niTerrorUnlinkPopulation"));if(unlink)unlink(reinterpret_cast<void*>(Populate),reinterpret_cast<void*>(originalPopulation));}}
allocationSlot=populationSlot=nullptr;}
void ScanLoaded(const SpawnRequest& request,bool report) noexcept {
 Players::Snapshot positions={};if(!CapturePositions(request.game,positions))return;
 uintptr_t entry=0,level=0;uint32_t id=0;if(!RoomLevel(request.room,entry,level,id)||!totals[id])return;
 uint32_t offset=0,matches=0;for(uint32_t off=0;off<=0x58;off+=8){uintptr_t active=0;if(Read(entry+off,active)&&active==reinterpret_cast<uintptr_t>(request.room)){offset=off;++matches;}}if(matches!=1)return;
 const uint32_t before=totals[id];uint32_t completed=0,loaded=0,waiting=0,liveCount=0;LoadedRoom live[2048];uintptr_t room2=0;if(!Read(level+0x10,room2))return;
 for(uint32_t rooms=0;room2&&rooms<2048;++rooms){Rect box={};uintptr_t next=0,active=0,back=0;
  if(!Geometry(room2,level,box)||!Read(room2+0x48,next)||next==room2)return;
  if(Read(room2+offset,active)&&active&&Read(active+0x18,back)&&back==room2){
   live[liveCount++]={reinterpret_cast<const void*>(active),box};
   for(uint32_t n=0;n<before;++n){auto& p=plans[id*15+n];if(p.waiting&&p.game==request.game&&Same(p.box,box)){++loaded;Materialize(p,reinterpret_cast<const void*>(active));if(!p.waiting)++completed;}}
  }room2=next;
 }
 for(uint32_t n=0;n<before;++n){auto& p=plans[id*15+n];if(p.game==request.game&&p.waiting&&!p.spawned){if(Rescue(p,live,liveCount)&&!p.waiting)++completed;}}
 for(uint32_t n=0;n<before;++n)if(plans[id*15+n].waiting)++waiting;

 static uint64_t lastGeneration=0;static uint32_t lastLevel=0,lastWaiting=0xffffffff;static uint64_t nextSummary=0;
 if(report&&(lastGeneration!=serverGeneration||lastLevel!=id||lastWaiting!=waiting||completed||(waiting&&GetTickCount64()>=nextSummary))){
  Text t;t.Add("SP_PENDING_SCAN level=");t.Num(id);t.Add(" total=");t.Num(before);t.Add(" complete=");t.Num(before-waiting);t.Add(" waiting=");t.Num(waiting);t.Add(" loaded_pending=");t.Num(loaded);t.Add(" loaded_rooms=");t.Num(liveCount);t.Add(" created_now=");t.Num(completed);t.Add(" rerolled=0");Log(t.s);
  lastGeneration=serverGeneration;lastLevel=id;lastWaiting=waiting;nextSummary=GetTickCount64()+30000;
 }
}
bool Plan(const SpawnRequest& request,uint32_t id,uint32_t requested,bool terror) noexcept {
 Players::Snapshot positions={};if(!CapturePositions(request.game,positions))return false;
 uintptr_t entry=0,level=0;uint32_t actual=0;if(!RoomLevel(request.room,entry,level,actual)||actual!=id)return false;
 uint32_t activeOffset=0,matches=0;for(uint32_t off=0;off<=0x58;off+=8){uintptr_t candidate=0;if(Read(entry+off,candidate)&&candidate==reinterpret_cast<uintptr_t>(request.room)){activeOffset=off;++matches;}}if(matches!=1)return false;
 Rect boxes[2048];uintptr_t room2s[2048];uint32_t rooms=0;uintptr_t current=0;if(!Read(level+0x10,current)||!current)return false;
 for(;current&&rooms<2048;++rooms){room2s[rooms]=current;uintptr_t next=0;if(!Geometry(current,level,boxes[rooms])||!Read(current+0x48,next)||next==current)return false;current=next;}
 if(current||!rooms)return false;uint32_t count=totals[id];const uint32_t initial=count;if(requested>15)requested=15;
 for(;count<requested;++count){int32_t bestX=0,bestY=0;uint32_t bestRoom=0,bestDistance=0;bool chosen=false;
  for(uint32_t trial=0;trial<512;++trial){const uint32_t room=Rand(rooms);const auto& r=boxes[room];int32_t x=r.x+2+Rand(r.w-4),y=r.y+2+Rand(r.h-4);uint32_t nearest=0x7fffffff;
   if(Visits::Has(id,x,y))continue;
   for(uint32_t n=0;n<count;++n){const auto& other=plans[id*15+n];uint32_t d=Distance(x,y,other.x,other.y);if(d<nearest)nearest=d;}
   if(!count){nearest=Distance(x,y,request.x,request.y);if(trial>16&&chosen)break;}
   if(!chosen||nearest>bestDistance){chosen=true;bestDistance=nearest;bestX=x;bestY=y;bestRoom=room;}
  }
  if(!chosen){Log("SP_PLACEMENT_LIMIT: no unvisited candidate; previously explored ground remains excluded.",true);break;}
  const uint32_t minimum=count? (bestDistance<cfg.separation?bestDistance:cfg.separation):cfg.separation;


  if(count&&minimum<12){Log("SP_PLACEMENT_LIMIT: map too small for another separated pack.",true);break;}
  auto& p=plans[id*15+count];p={serverGeneration,request.game,id,Visuals::PickKind(),cfg.minMinions+Rand(cfg.maxMinions-cfg.minMinions+1),0,minimum,boxes[bestRoom],bestX,bestY,true,false};p.plannedAt=GetTickCount64();p.homeX=bestX;p.homeY=bestY;totals[id]=count+1;
  uintptr_t active=0,back=0;const bool loaded=Read(room2s[bestRoom]+activeOffset,active)&&active&&Read(active+0x18,back)&&back==room2s[bestRoom];
  Text t;t.Add("SP_PACK_PLAN level=");t.Num(id);t.Add(" terror=");t.Num(terror);t.Add(" number=");t.Num(count+1);t.Add(" kind=");t.Num(p.kind);t.Add(" x=");t.Int(p.x);t.Add(" y=");t.Int(p.y);t.Add(" separation=");t.Num(minimum);t.Add(" deferred=");t.Num(!loaded);Log(t.s);


 }
 totals[id]=static_cast<uint8_t>(count);
 for(uint32_t n=initial;n<count;++n){auto& p=plans[id*15+n];uint32_t nearest=0x7fffffff;
  for(uint32_t j=0;j<count;++j)if(j!=n){const auto& other=plans[id*15+j];const uint32_t d=Distance(p.homeX,p.homeY,other.homeX,other.homeY);if(d<nearest)nearest=d;}
  p.radius=count>1?nearest*45/100:static_cast<uint32_t>((p.box.w>p.box.h?p.box.w:p.box.h)/2);if(p.radius<16)p.radius=16;
  Text region;region.Add("SP_PACK_TERRITORY level=");region.Num(id);region.Add(" number=");region.Num(n+1);region.Add(" home_x=");region.Int(p.homeX);region.Add(" home_y=");region.Int(p.homeY);region.Add(" radius=");region.Num(p.radius);region.Add(" partition=voronoi unvisited_only=1 unloaded_sector_waits=1");Log(region.s);
 }
 for(uint32_t n=initial;n<count;++n){auto& p=plans[id*15+n];for(uint32_t r=0;r<rooms;++r)if(Same(p.box,boxes[r])){uintptr_t active=0,back=0;if(Read(room2s[r]+activeOffset,active)&&active&&Read(active+0x18,back)&&back==room2s[r])Materialize(p,reinterpret_cast<void*>(active));break;}}
 Text t;t.Add("SP_AREA_PLAN level=");t.Num(id);t.Add(" terror=");t.Num(terror);t.Add(" requested_total=");t.Num(requested);t.Add(" planned_total=");t.Num(count);t.Add(" existing_elites_unchanged=1");Log(t.s,count<requested);return true;
}
}
