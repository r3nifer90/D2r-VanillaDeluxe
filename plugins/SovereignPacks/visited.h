#pragma once



namespace Visits {
constexpr uint32_t Capacity=262144,Cell=8,Radius=32;
uint64_t cells[Capacity]={},last[64]={};uint32_t used=0;bool full=false;
void Reset() noexcept {volatile uint64_t* clear=cells;for(uint32_t n=0;n<Capacity;++n)clear[n]=0;memset(last,0,sizeof(last));used=0;full=false;}
uint64_t Key(uint32_t level,uint32_t x,uint32_t y) noexcept {return (static_cast<uint64_t>(level)<<26)|(static_cast<uint64_t>(x/Cell)<<13)|(y/Cell);}
uint32_t Slot(uint64_t key) noexcept {key^=key>>30;key*=0xbf58476d1ce4e5b9ULL;key^=key>>27;key*=0x94d049bb133111ebULL;return static_cast<uint32_t>(key^(key>>31))&(Capacity-1);}
bool Has(uint32_t level,int32_t x,int32_t y) noexcept {
 if(full||x<0||y<0||x>65535||y>65535)return true;
 const uint64_t key=Key(level,x,y);uint32_t at=Slot(key);
 for(uint32_t n=0;n<Capacity;++n,at=(at+1)&(Capacity-1)){if(!cells[at])return false;if(cells[at]==key)return true;}return true;
}
void Insert(uint64_t key) noexcept {
 if(full)return;uint32_t at=Slot(key);
 for(uint32_t n=0;n<Capacity;++n,at=(at+1)&(Capacity-1)){
  if(cells[at]==key)return;
  if(!cells[at]){if(used>=Capacity*3/4){full=true;Log("SP_VISIT_LIMIT: history retained; new pack creation paused until next game.",true);return;}cells[at]=key;++used;return;}
 }
 full=true;
}
void Remember(const Players::Snapshot& snapshot) noexcept {
 for(uint32_t n=0;n<snapshot.count;++n){const auto& p=snapshot.points[n];
  if(p.x<0||p.y<0||p.x>65535||p.y>65535||!p.level||p.level>=1024){full=true;continue;}
  const uint64_t key=Key(p.level,p.x,p.y);if(last[n]==key)continue;last[n]=key;

  const int32_t loX=p.x>static_cast<int32_t>(Radius)?p.x-Radius:0,loY=p.y>static_cast<int32_t>(Radius)?p.y-Radius:0;
  const int32_t hiX=p.x+Radius<65536?p.x+Radius:65535,hiY=p.y+Radius<65536?p.y+Radius:65535;
  for(int32_t x=loX/Cell;x<=hiX/static_cast<int32_t>(Cell);++x)for(int32_t y=loY/Cell;y<=hiY/static_cast<int32_t>(Cell);++y)Insert(Key(p.level,x*Cell,y*Cell));
 }
}
}
