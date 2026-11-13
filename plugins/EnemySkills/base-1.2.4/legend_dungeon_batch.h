#pragma once
namespace LegendNative {


using BatchTargetFn=void(__fastcall*)(void*,void*);
void* BatchCorpse(BossSkills::Boss& b,void* unit,const uint32_t* used,uint32_t n) noexcept {
 using namespace BossSkills;int32_t x=0,y=0;if(!Position(unit,x,y))return nullptr;const int32_t level=Level(unit);void* best=nullptr;uint32_t distance=21;
 for(uint32_t k=0;k<32;++k){const auto& c=corpses[(corpseCursor-1-k)&255];if(c.epoch!=b.epoch||c.level!=static_cast<uint32_t>(level))continue;
  bool seen=false;for(uint32_t i=0;i<n;++i)if(used[i]==c.uid)seen=true;if(seen)continue;
  void* corpse=lookup(b.game,1,c.uid);uint32_t id[3]={},mode=0;int32_t cx=0,cy=0;
  if(!corpse||!Read(reinterpret_cast<uintptr_t>(corpse),id)||id[0]!=1||id[1]!=c.kind||id[2]!=c.uid||!Read(reinterpret_cast<uintptr_t>(corpse)+0xc,mode)||mode!=12||stat(corpse,6,0)>0||Level(corpse)!=level||!Position(corpse,cx,cy))continue;
  if(b.sovereign&&!PoisonCorpseAvailable(corpse))continue;
  const uint32_t d=Distance(x,y,cx,cy);if(d<distance){distance=d;best=corpse;}
 }return best;
}
int32_t DungeonSummonBatch(void* game,void* unit,int32_t id,int32_t level,int32_t pay,int32_t item,int32_t effect) noexcept {
 using namespace BossSkills;uint32_t identity[3]={};Boss* b=nullptr;
 if(ready&&unit&&Read(reinterpret_cast<uintptr_t>(unit),identity)&&identity[0]==1&&(BossSkills::Legend(identity[1])||SovereignNativeData::Leader(identity[1])))b=NativeBoss(game,identity[2],identity[1]);
 const bool trap=b&&b->sovereign&&TrapSkill(id);
 if(!b||!NativeRoot(*b,unit)||!b->nativeStarted||b->nativeSkill!=static_cast<uint32_t>(id)||UsedId(unit)!=static_cast<uint32_t>(id)||(!CreatureSummon(id)&&!trap)||b->summonBatchDepth||(b->summonBatchCast==b->nativeStarted&&b->summonBatchSkill==static_cast<uint32_t>(id)))return priorSkill(game,unit,id,level,pay,item,effect);
 const uint32_t before=PetCount(*b,id),limit=PetLimit(*b,id);uint32_t requested=trap?cfg.sovereignTrapBatch:b->sovereign?SummonTuning::SovereignBatch(id):SummonTuning::Batch(id);if(before>=limit||!requested)return priorSkill(game,unit,id,level,pay,item,effect);if(requested>limit-before)requested=limit-before;
 b->summonBatchCast=b->nativeStarted;b->summonBatchSkill=id;++b->summonBatchDepth;
 const uint64_t cast=b->nativeStarted;const uint32_t original=Original(id);const bool corpseSkill=original==70||original==80;
 uintptr_t path=0;Read(reinterpret_cast<uintptr_t>(unit)+0x38,path);uint32_t used[8]={},usedCount=0;bool retargeted=false;
 if(corpseSkill){void* corpse=currentTarget(game,unit);uint32_t mode=0,uid=0;if(corpse&&Read(reinterpret_cast<uintptr_t>(corpse)+0xc,mode)&&mode==12&&Read(reinterpret_cast<uintptr_t>(corpse)+8,uid))used[usedCount++]=uid;}
 const int32_t result=priorSkill(game,unit,id,level,pay,item,effect);uint32_t calls=1,previous=before,alive=PetCount(*b,id);int32_t last=result;const char* stop="batch_complete";
 while(calls<requested&&alive<limit){
  if(!last||alive<=previous){stop="native_failure_or_no_creation";break;}
  if(!NativeRoot(*b,unit)||b->nativeStarted!=cast||b->nativeSkill!=static_cast<uint32_t>(id)||UsedId(unit)!=static_cast<uint32_t>(id)){stop="owner_or_cast_changed";break;}
  if(corpseSkill){
   uintptr_t livePath=0;void* corpse=BatchCorpse(*b,unit,used,usedCount);uint32_t uid=0;
   if(!corpse||!path||!Read(reinterpret_cast<uintptr_t>(unit)+0x38,livePath)||path!=livePath||!Read(reinterpret_cast<uintptr_t>(corpse)+8,uid)){stop="no_further_corpse";break;}
   used[usedCount++]=uid;reinterpret_cast<BatchTargetFn>(ctx->exeBase+0x342920)(reinterpret_cast<void*>(path),corpse);retargeted=true;
  }
  previous=alive;last=priorSkill(game,unit,id,level,pay,item,effect);++calls;alive=PetCount(*b,id);
 }
 if(!last||alive<=previous)stop="native_failure_or_no_creation";
 if(retargeted&&NativeRoot(*b,unit)){uintptr_t livePath=0;if(Read(reinterpret_cast<uintptr_t>(unit)+0x38,livePath)&&livePath==path){void* opponent=b->targetId?lookup(game,b->targetType,b->targetId):nullptr;if(!NativeOpponentValid(*b,unit,opponent))opponent=nullptr;reinterpret_cast<BatchTargetFn>(ctx->exeBase+0x342920)(reinterpret_cast<void*>(path),opponent);}}
 --b->summonBatchDepth;
 Text t;t.Add(trap?"ES_SOVEREIGN_TRAP_BATCH uid=":b->sovereign?"ES_SOVEREIGN_SUMMON_BATCH uid=":"ES_DUNGEON_SUMMON_BATCH uid=");t.Num(b->uid);t.Add(" skill=");t.Num(original);t.Add(" private_skill=");t.Num(id);t.Add(" requested=");t.Num(requested);t.Add(" native_calls=");t.Num(calls);t.Add(" alive_before=");t.Num(before);t.Add(" alive_after=");t.Num(alive);t.Add(" limit=");t.Num(limit);t.Add(" stop=");t.Add(stop);t.Add(" one_animation=1 native_handlers=1 corpse_rules_preserved=1");Log(t.s,!result);
 return result;
}
}
