#pragma once

void SovereignPetHealth(Pet& p,void* unit,Boss& b) noexcept {
 void* owner=lookup(p.game,1,p.owner);uintptr_t game=0;uint32_t mode=0;
 if(!b.sovereign||b.healthSpawnDepth||!NativeRoot(b,owner)||NativePet(p.game,unit)!=&p||lookup(p.game,1,p.uid)!=unit||!Read(reinterpret_cast<uintptr_t>(unit)+0xd8,game)||game!=reinterpret_cast<uintptr_t>(p.game)||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||p.retire||!LegendNative::baseStat||!LegendNative::setStat)return;
 const uint32_t original=LegendNative::Original(p.skill);

 if(!cfg.sovereignSummonHealth||original==88||original==62||original==257||original==262||original==271||original==276){p.healthApplied=true;return;}
 const uint32_t life=stat(unit,6,0),maximum=stat(unit,7,0),ownerMaximum=stat(owner,7,0);
 constexpr uint32_t cap=0x7fffffffu;
 if(life<256||maximum<256||ownerMaximum<256||life>cap||maximum>cap||ownerMaximum>cap)return;
 const uint32_t baseMax=LegendNative::baseStat(unit,7,0),baseLife=LegendNative::baseStat(unit,6,0);
 if(baseMax>cap||baseLife>cap)return;
 const uint64_t requested=static_cast<uint64_t>(ownerMaximum)*cfg.sovereignSummonHealth/100;
 const uint32_t desired=requested>cap?cap:static_cast<uint32_t>(requested);
 const uint32_t target=desired>maximum?desired:maximum;
 uint32_t actualMax=maximum,actualLife=life;bool applied=target==maximum;
 p.healthApplied=true;
 if(target>maximum){
  const uint64_t increase=target-maximum;



  uint64_t step=(increase*(baseMax?baseMax:256)+maximum-1)/maximum;
  if(!step)step=1;uint32_t writtenBase=baseMax;
  for(uint32_t attempt=0;attempt<3;++attempt){
   const uint64_t next=static_cast<uint64_t>(baseMax)+step;
   const uint32_t proposed=next>cap?cap:static_cast<uint32_t>(next);
   if(proposed<=writtenBase)break;
   LegendNative::setStat(unit,7,static_cast<int32_t>(proposed),0);writtenBase=proposed;actualMax=stat(unit,7,0);
   if(actualMax>=target||actualMax<=maximum||actualMax>cap)break;
   const uint64_t gained=actualMax-maximum;
   step=(increase*(writtenBase-baseMax)+gained-1)/gained;
   if(step<=writtenBase-baseMax)step=static_cast<uint64_t>(writtenBase-baseMax)+1;
  }
  if(actualMax>maximum&&actualMax<=cap){
   const uint32_t current=life>maximum?maximum:life;
   const uint32_t targetLife=static_cast<uint64_t>(current)*actualMax/maximum;
   const uint64_t newLife=static_cast<uint64_t>(baseLife)+(targetLife>life?targetLife-life:0);
   if(targetLife>=life)LegendNative::setStat(unit,6,static_cast<int32_t>(newLife>cap?cap:newLife),0);
   actualLife=stat(unit,6,0);applied=actualMax>=target&&actualLife==targetLife;
  }
 }
 Text t;t.Add(applied?"ES_SOVEREIGN_SUMMON_HEALTH uid=":"ES_SOVEREIGN_SUMMON_HEALTH_PARTIAL uid=");t.Num(p.uid);t.Add(" owner=");t.Num(p.owner);t.Add(" skill=");t.Num(p.skill);t.Add(" owner_percent=");t.Num(cfg.sovereignSummonHealth);t.Add(" owner_max_hp=");t.Num(ownerMaximum/256);t.Add(" target_max_hp=");t.Num(target/256);t.Add(" old_max_hp=");t.Num(maximum/256);t.Add(" actual_max_hp=");t.Num(actualMax/256);t.Add(" actual_hp=");t.Num(actualLife/256);t.Add(" native_floor=");t.Num(desired<=maximum);t.Add(" apply_once=1 damage_fraction_preserved=1");Log(t.s,!applied);
}
