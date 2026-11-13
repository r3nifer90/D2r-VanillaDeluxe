#pragma once
namespace BossSkills {
void NativeAuraPoll(Boss& b,void* unit,uint64_t now) noexcept {
 if(!b.active||!Legend(b.kind)||!LegendNative::ready||!LegendNative::assign||!LegendNative::state||!LegendNative::right||now<b.nextAura||b.thread!=GetCurrentThreadId()||b.epoch!=static_cast<uint64_t>(InterlockedCompareExchange64(&generation,0,0)))return;
 uint32_t identity[3]={},mode=0;
 if(!unit||!Read(reinterpret_cast<uintptr_t>(unit),identity)||identity[0]!=1||identity[1]!=b.kind||identity[2]!=b.uid||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||stat(unit,6,0)<256)return;
 uintptr_t unitGame=0;if(lookup(b.game,1,b.uid)!=unit||!Read(reinterpret_cast<uintptr_t>(unit)+0xd8,unitGame)||unitGame!=reinterpret_cast<uintptr_t>(b.game))return;
 NativeLevels(b,unit);
 b.nextAura=now+300;
 for(uint32_t i=0;i<3;++i){const auto& s=b.skills[i];const int32_t state=LegendNative::AuraState(s.id);if(s.kind!=7||state<0)continue;
  void* learned=LegendNative::find(unit,s.id,-1);uintptr_t row=0;uint32_t level=0;uint8_t flags=0;int16_t nativeState=-1;
  if(!learned||!Read(reinterpret_cast<uintptr_t>(learned),row)||!row||!Read(row+0x24,flags)||!(flags&0x20)||!Read(row+0xa0,nativeState)||nativeState!=state||!Read(reinterpret_cast<uintptr_t>(learned)+0x40,level)||!level){
   if(now>=b.nextAuraLog){b.nextAuraLog=now+10000;Text t;t.Add("ES_NATIVE_AURA_REJECT uid=");t.Num(b.uid);t.Add(" skill=");t.Num(s.id);t.Add(" learned_aura_flags_or_state_mismatch=1 player_skills_untouched=1");Log(t.s,true);}return;
  }
  bool selected=LegendNative::right(unit)==learned;
  bool active=LegendNative::state(unit,state)!=0;
  if((!b.auraAttempts||!selected||!active)&&now>=b.nextAuraRetry){
   b.nextAuraRetry=now+5000;++b.auraAttempts;
   Text t;t.Add("ES_NATIVE_AURA_SELECT uid=");t.Num(b.uid);t.Add(" skill=");t.Num(s.id);t.Add(" state=");t.Num(state);t.Add(" attempt=");t.Num(b.auraAttempts);t.Add(" time_ms=");t.Num(now);t.Add(" native_right_skill=1 native_periodic_event=1 attack_animation_untouched=1");Log(t.s);
   LegendNative::Mana(unit);



   LegendNative::assign(unit,0,static_cast<int32_t>(s.id),-1);
   selected=LegendNative::right(unit)==learned;active=LegendNative::state(unit,state)!=0;
  }
  const bool healthy=selected&&active;
  if(!b.auraSeen||healthy!=b.auraHealthy||now>=b.nextAuraLog){b.auraSeen=true;b.auraHealthy=healthy;b.nextAuraLog=now+10000;
   Text t;t.Add("ES_NATIVE_AURA_STATE uid=");t.Num(b.uid);t.Add(" skill=");t.Num(s.id);t.Add(" state=");t.Num(state);t.Add(" configured_level=");t.Num(s.level);t.Add(" learned_level=");t.Num(level);t.Add(" right_selected=");t.Num(selected);t.Add(" state_active=");t.Num(active);t.Add(" attempts=");t.Num(b.auraAttempts);t.Add(" time_ms=");t.Num(now);t.Add(" original_state_overlays=1 rendering_requires_game_observation=1");Log(t.s,!healthy);
  }
  return;
 }
}
}
