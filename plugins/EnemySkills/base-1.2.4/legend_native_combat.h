#pragma once
namespace BossSkills {
void NativeCastMonitor(Boss& b,void* unit) noexcept {
 uint32_t frame=0,mode=0;
 if(!b.nativeStarted||b.busy||!NativeRoot(b,unit)||!b.castFrameValid||!Read(reinterpret_cast<uintptr_t>(b.game)+0x170,frame)||static_cast<uint32_t>(frame-b.castFrame)<150||static_cast<uint32_t>(frame-b.castFrame)>=0x80000000u||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode!=b.nativeMode||UsedId(unit)!=b.nativeSkill)return;
 const uint64_t now=GetTickCount64();
 for(uint32_t i=0;i<3;++i)if(b.skills[i].id==b.nativeSkill&&b.next[i]<now+5000)b.next[i]=now+5000;
 LegendNative::CombatCastTimeout(b.game,unit,b.nativeSkill,b.nativeMode,b.castFrame,true);
 b.nativeStarted=0;b.factoryDue=false;LegendNative::idle(b.game,unit,3);
 Text t;t.Add("ES_NATIVE_CAST_TIMEOUT uid=");t.Num(b.uid);t.Add(" skill=");t.Num(b.nativeSkill);t.Add(" source=host_monitor native_ai_resume=1 forced_mode_change=0 retry_backoff_ms=5000");Log(t.s,true);
}
bool NativeOpponentValid(const Boss& b,void* unit,void* target) noexcept {
 uint32_t type=99,mode=0,id=0;int32_t x=0,y=0,tx=0,ty=0;
 return target&&target!=unit&&Read(reinterpret_cast<uintptr_t>(target),type)&&(type==0||(type==1&&alignment(target)==2))&&Read(reinterpret_cast<uintptr_t>(target)+8,id)&&lookup(b.game,type,id)==target&&Read(reinterpret_cast<uintptr_t>(target)+0xc,mode)&&mode!=0&&mode!=12&&stat(target,6,0)>=256&&Level(unit)==Level(target)&&Position(unit,x,y)&&Position(target,tx,ty)&&Distance(x,y,tx,ty)<=40;
}
void AiDecisionLog(Boss& b,uint64_t now,uint32_t reason,const char* action) noexcept {
 return;
}
bool NativePursuit(Boss& b,void* unit,uint32_t mode,uint64_t now,void* target=nullptr) noexcept {


 if(!Legend(b.kind)||(mode!=1&&mode!=2&&mode!=15))return false;
 if(!NativeOpponentValid(b,unit,target))target=LegendNative::currentTarget?LegendNative::currentTarget(b.game,unit):nullptr;
 if(!NativeOpponentValid(b,unit,target)){Snapshot empty;target=Target(b,unit,empty);}
 int32_t x=0,y=0,tx=0,ty=0;
 if(!NativeOpponentValid(b,unit,target)||!Position(unit,x,y)||!Position(target,tx,ty)||!LegendNative::walk)return false;
 const uint32_t distance=Distance(x,y,tx,ty);const bool obstructed=LegendNative::collision&&LegendNative::collision(unit,target,4);
 if(distance<=ApproachRange(b)&&!obstructed)return false;

 if((mode==2||mode==15)&&b.progressSeen&&now-b.progressAt<750){LegendNative::idle(b.game,unit,1);return true;}
 if(now<b.nextMove)return false;
 const int32_t result=LegendNative::walk(b.game,unit,target,0);b.nextMove=now+(result?350:250);
 if(result){Read((uintptr_t)target,b.targetType);Read((uintptr_t)target+8,b.targetId);b.walkCommit=now+350;LegendNative::idle(b.game,unit,1);}

 if(now>=b.moveAt){b.moveAt=now+3000;Text t;t.Add("ES_NATIVE_PURSUIT uid=");t.Num(b.uid);t.Add(" distance=");t.Num(distance);t.Add(" returned=");t.Num(result);t.Add(" all_profiles=1 during_skill_cooldown=1 native_pathfinder=1 ai_turn=1 teleport=0");Log(t.s);}
 return result!=0;
}
bool NativeDecision(Boss& b,void* unit) noexcept {
 uintptr_t game=0;uint32_t mode=0;
 if(b.busy){LegendNative::idle(b.game,unit,3);return true;}
 if(lookup(b.game,1,b.uid)!=unit||!Read(reinterpret_cast<uintptr_t>(unit)+0xd8,game)||game!=reinterpret_cast<uintptr_t>(b.game)||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode==0||mode==12||stat(unit,6,0)<256)return false;
 b.aiFrameValid=Read(reinterpret_cast<uintptr_t>(b.game)+0x170,b.aiFrame);
 const uint64_t now=GetTickCount64();
 auto native=[&](uint32_t reason,const char* action)->bool {b.factoryDue=false;++b.nativeAiTurns;AiDecisionLog(b,now,reason,action);return false;};
 NativeLevels(b,unit);
 NativeAuraPoll(b,unit,now);

 void* target=b.targetId?lookup(b.game,b.targetType,b.targetId):nullptr;
 NativePoll(b,unit,target,now);
 if(b.nativeStarted&&!LegendNative::FreeMode(unit)&&mode==b.nativeMode&&UsedId(unit)==b.nativeSkill){
  uint32_t frame=0;const bool expired=b.castFrameValid&&Read(reinterpret_cast<uintptr_t>(b.game)+0x170,frame)&&static_cast<uint32_t>(frame-b.castFrame)>=150&&static_cast<uint32_t>(frame-b.castFrame)<0x80000000u;
  if(!expired){AiDecisionLog(b,now,1,"protect_skill_animation");LegendNative::idle(b.game,unit,3);return true;}
  for(uint32_t i=0;i<3;++i)if(b.skills[i].id==b.nativeSkill&&b.next[i]<now+5000)b.next[i]=now+5000;
  Text t;t.Add("ES_NATIVE_CAST_TIMEOUT uid=");t.Num(b.uid);t.Add(" skill=");t.Num(b.nativeSkill);t.Add(" game_frames=");t.Num(static_cast<uint32_t>(frame-b.castFrame));t.Add(" native_ai_resume=1 forced_mode_change=0 retry_backoff_ms=5000");Log(t.s,true);
  LegendNative::CombatCastTimeout(b.game,unit,b.nativeSkill,b.nativeMode,b.castFrame,b.castFrameValid);
  b.nativeStarted=0;b.factoryDue=false;return native(2,"native_after_cast_timeout");
 }

 if(!LegendNative::FreeMode(unit))return native(3,"native_active_action");


 target=LegendNative::currentTarget?LegendNative::currentTarget(b.game,unit):nullptr;
 const bool current=NativeOpponentValid(b,unit,target);
 if(!current){Snapshot empty;target=Target(b,unit,empty);}
 if(!NativeOpponentValid(b,unit,target)){b.targetId=0;b.nativeWalkAt=0;return native(6,"native_no_spell_target");}
 uint32_t type=0,id=0;Read(reinterpret_cast<uintptr_t>(target),type);Read(reinterpret_cast<uintptr_t>(target)+8,id);
 if(type!=b.targetType||id!=b.targetId){Text t;t.Add("ES_NATIVE_TARGET uid=");t.Num(b.uid);t.Add(current?" source=current_native type=":" source=engine_fallback type=");t.Num(type);t.Add(" id=");t.Num(id);t.Add(" time_ms=");t.Num(now);Log(t.s);}
 b.targetType=type;b.targetId=id;


 b.factoryDue=false;
 if(now<b.nextAny){if(NativePursuit(b,unit,mode,now,target))return true;return native(5,"native_during_skill_cooldown");}
 const bool walking=mode==2||mode==15;
 if(walking){
  if(!b.nativeWalkAt)b.nativeWalkAt=now;
  int32_t x=0,y=0,tx=0,ty=0;const bool distant=Position(unit,x,y)&&Position(target,tx,ty)&&Distance(x,y,tx,ty)>3;


 }else b.nativeWalkAt=0;
 const uint64_t prior=b.nextAny;uint32_t start=b.cursor;


 for(uint32_t n=0;n<3;++n){const uint32_t k=(b.cursor+n)%3;if(now>=b.next[k]&&RefillNeeded(b,b.skills[k].id)){start=k;break;}}
 for(uint32_t n=0;n<3;++n){const uint32_t k=(start+n)%3;NativeCast(b,unit,target,k,now);if(b.nextAny!=prior)break;}
 if(b.nativeStarted&&!LegendNative::FreeMode(unit)){
  b.nativeWalkAt=0;AiDecisionLog(b,now,8,"native_skill_started");LegendNative::idle(b.game,unit,3);return true;
 }

 if(NativePursuit(b,unit,mode,now,target)){AiDecisionLog(b,now,10,"native_stationary_pursuit");return true;}
 b.factoryDue=false;return native(9,b.nextAny!=prior?"native_after_rejected_skill":"native_no_eligible_skill");
}
}
