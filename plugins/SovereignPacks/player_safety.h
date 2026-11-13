#pragma once
#include "location_guard.h"


namespace Players {
using FindClientFn=void*(__fastcall*)(uint32_t);
FindClientFn findClient=nullptr;
struct Tracked {void* game=nullptr;uint32_t id=0;bool seen=false;};
Tracked clients[64];bool overflow=false;SRWLOCK lock=SRWLOCK_INIT;
struct Position {int32_t x,y;uint32_t level;};
struct Snapshot {Position points[64];uint32_t count=0;};
bool Bind() noexcept {
 constexpr uint8_t lookup[]={0xff,0x15,0x2a,0x63,0xee,0xff};
 constexpr uint8_t membership[]={0x48,0x39,0xb8,0xb0,0x02,0x00,0x00};
 constexpr uint8_t playerCheck[]={0x48,0x83,0xb8,0x78,0x02,0x00,0x00,0x00};
 using namespace TerrorLocation;
 if(!Bytes(coreBase+0x820aa0,lookup,sizeof(lookup))||!Bytes(coreBase+0x820aac,membership,sizeof(membership))||!Bytes(coreBase+0x820aca,playerCheck,sizeof(playerCheck))||!Bytes(coreBase+RoomTraversalRva,RoomTraversalBytes,sizeof(RoomTraversalBytes)))return false;
 findClient=reinterpret_cast<FindClientFn>(RipSlot(coreBase+0x820aa0));return findClient!=nullptr;
}
void Reset() noexcept {AcquireSRWLockExclusive(&lock);memset(clients,0,sizeof(clients));overflow=false;ReleaseSRWLockExclusive(&lock);}
bool Track(void* game,void* client) noexcept {
 uint32_t id=0;
 if(!game||!client||!findClient||!Read(reinterpret_cast<uintptr_t>(client),id))return false;
 void* live=findClient(id);
 if(live&&live!=client){AcquireSRWLockExclusive(&lock);overflow=true;ReleaseSRWLockExclusive(&lock);return false;}


 AcquireSRWLockExclusive(&lock);Tracked* empty=nullptr;
 for(auto& c:clients){if(c.game==game&&c.id==id){if(live)c.seen=true;ReleaseSRWLockExclusive(&lock);return true;}if(!c.game&&!empty)empty=&c;}
 if(empty)*empty={game,id,live!=nullptr};else overflow=true;ReleaseSRWLockExclusive(&lock);return empty!=nullptr;
}
bool Capture(void* game,Snapshot& out) noexcept {
 Tracked list[64];AcquireSRWLockShared(&lock);memcpy(list,clients,sizeof(list));const bool full=overflow;ReleaseSRWLockShared(&lock);
 if(full||!findClient)return false;uint32_t live=0;
 for(const auto& c:list){if(c.game!=game)continue;
  const uintptr_t client=reinterpret_cast<uintptr_t>(findClient(c.id));if(!client){if(!c.seen)return false;continue;}
  uintptr_t owner=0,unit=0,path=0,room=0,room2=0,level=0;uint32_t type=99,id=0;
  if(!Read(client+0x2b0,owner))return false;if(owner!=reinterpret_cast<uintptr_t>(game)){if(!c.seen)return false;continue;}
  if(!c.seen){AcquireSRWLockExclusive(&lock);for(auto& stored:clients)if(stored.game==game&&stored.id==c.id)stored.seen=true;ReleaseSRWLockExclusive(&lock);}
  ++live;

  if(!Read(client+0x278,unit)||!unit||!Read(unit,type)||type!=0||!Read(unit+0x38,path)||!path||!Readable(path,128)||!Read(path+0x20,room)||!room||!Read(room+0x18,room2)||!room2||!Read(room2+0x90,level)||!level||!Read(level+0x1f8,id)||id<1||id>=1024)return false;
  out.points[out.count++]={getX(reinterpret_cast<void*>(path)),getY(reinterpret_cast<void*>(path)),id};
 }
 return live>0;
}
bool Safe(const Snapshot& snapshot,uint32_t level,int32_t x,int32_t y,uint32_t& nearest) noexcept {
 nearest=0xffffffff;
 for(uint32_t n=0;n<snapshot.count;++n){const auto& p=snapshot.points[n];if(p.level!=level)continue;
  const uint32_t dx=x>p.x?x-p.x:p.x-x,dy=y>p.y?y-p.y:p.y-y,d=dx>dy?dx:dy;
  if(d<nearest)nearest=d;
 }
 return nearest>=cfg.playerDistance;
}
}
