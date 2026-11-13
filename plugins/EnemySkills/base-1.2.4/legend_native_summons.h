#pragma once
namespace LegendNative {


struct TrapArgs {uint64_t flags;void* owner;uint16_t kind,reserved;uint32_t unknown,mode;int32_t x,y,petType,petMax,aiState;};
static_assert(sizeof(TrapArgs)==48&&offsetof(TrapArgs,mode)==24&&offsetof(TrapArgs,petMax)==40);
void* __fastcall CreatureSpawn(void* game,const TrapArgs* input) noexcept {
 using namespace BossSkills;
 TrapArgs args={};uint32_t identity[3]={};Boss* root=nullptr;
 if(ready&&input&&Read(reinterpret_cast<uintptr_t>(input),args)&&args.owner&&Read(reinterpret_cast<uintptr_t>(args.owner),identity)&&identity[0]==1)
  root=NativeBoss(game,identity[2],identity[1]);



 if(!root||!NativeRoot(*root,args.owner)||!root->nativeStarted||!CreatureSummon(root->nativeSkill)||UsedId(args.owner)!=root->nativeSkill)
  return priorSpawnPet(game,input);
 const uint32_t skill=root->nativeSkill,alive=PetCount(*root,skill),limit=PetLimit(*root,skill);
 if(!limit||alive>=limit)return nullptr;
 const int32_t nativeMax=args.petMax;args.petMax=static_cast<int32_t>(NativeGroupLimit(*root,skill));
 uint32_t attempts=1;void* result=priorSpawnPet(game,&args);int32_t ox=0,oy=0;



 if(!result&&Position(args.owner,ox,oy)){
  constexpr int32_t dx[]={1,1,0,-1,-1,-1,0,1},dy[]={0,1,1,1,0,-1,-1,-1};
  const uint32_t first=(root->uid+alive)%8;
  for(uint32_t n=0;n<16&&!result;++n){const uint32_t direction=(first+n)%8;const int32_t radius=n<8?3:5;
   args.x=ox+dx[direction]*radius;args.y=oy+dy[direction]*radius;
   if(args.x<1||args.x>65535||args.y<1||args.y>65535)continue;
   args.flags|=1;++attempts;result=priorSpawnPet(game,&args);
  }
 }
 Text t;t.Add("ES_CREATURE_SUMMON_SPAWN uid=");t.Num(root->uid);t.Add(" role=");t.Add(root->sovereign?"sovereign":"dungeon");t.Add(" skill=");t.Num(Original(skill));t.Add(" private_skill=");t.Num(skill);t.Add(" alive_before=");t.Num(alive);t.Add(" target_limit=");t.Num(limit);t.Add(" native_group_limit=");t.Num(args.petMax);t.Add(" native_argument_limit=");t.Num(nativeMax>0?static_cast<uint32_t>(nativeMax):0);t.Add(" native_attempts=");t.Num(attempts);t.Add(" created=");t.Num(result!=nullptr);t.Add(" placement_retry=");t.Num(attempts>1);t.Add(" limit_source=ini collision_preserved=1 player_limits_unchanged=1");Log(t.s,!result);
 return result;
}
void* __fastcall TrapSummon(void* game,void* unit,int32_t x,int32_t y,const void* data,int32_t id,int32_t level,int32_t unitLevel) noexcept {
 using namespace BossSkills;
 uint32_t ident[3]={};Boss* b=nullptr;
 if(ready&&unit&&Read(reinterpret_cast<uintptr_t>(unit),ident)&&ident[0]==1&&TrapSkill(id))b=NativeBoss(game,ident[2],ident[1]);
 if(!b)return priorTrap(game,unit,x,y,data,id,level,unitLevel);
 uintptr_t unitGame=0,row=0;void* learned=find(unit,id,-1);int16_t rowId=-1;int8_t petType=-1;uint8_t bank=0;
 if(lookup(game,1,b->uid)!=unit||!Read(reinterpret_cast<uintptr_t>(unit)+0xd8,unitGame)||unitGame!=reinterpret_cast<uintptr_t>(game)||stat(unit,6,0)<256||!b->nativeStarted||b->nativeSkill!=static_cast<uint32_t>(id)||UsedId(unit)!=static_cast<uint32_t>(id)||!learned||!Read(reinterpret_cast<uintptr_t>(learned),row)||row!=reinterpret_cast<uintptr_t>(data)||!Read(row,rowId)||rowId!=id||level<1||level>99||!Read(row+0x112,petType)||!Read(reinterpret_cast<uintptr_t>(game)+0x106,bank))return nullptr;
 const uint32_t limit=PetLimit(*b,id);if(!limit||PetCount(*b,id)>=limit)return nullptr;



 using SummonIdFn=int32_t(__fastcall*)(void*,int32_t,int32_t,int32_t,int32_t*,int32_t*,int32_t*);
 using TablesFn=const void*(__fastcall*)(uint8_t);
 using CountFn=int32_t(__fastcall*)(uint8_t);
 int32_t mode=0;const int32_t kind=reinterpret_cast<SummonIdFn>(ctx->exeBase+0x433800)(unit,0,id,level,&mode,nullptr,nullptr);
 uint64_t monCount=0;const uintptr_t tables=reinterpret_cast<uintptr_t>(reinterpret_cast<TablesFn>(ctx->exeBase+0x300a90)(bank));
 if(!tables||!Read(tables+0xf60,monCount)||kind<0||static_cast<uint64_t>(kind)>=monCount||kind>65535||mode<0||mode>=16)return nullptr;
 const int32_t types=reinterpret_cast<CountFn>(ctx->exeBase+0x2b4420)(bank);if(petType<0||petType>=types)petType=0;
 if((!x||!y)&&!Position(unit,x,y))return nullptr;
 using RoomFn=void*(__fastcall*)(void*);using RoomAtFn=void*(__fastcall*)(void*,int32_t,int32_t);using TownFn=int32_t(__fastcall*)(void*);
 struct Point {int32_t x,y;};Point occupied[64]={};uint32_t occupiedCount=0;
 if(b->sovereign)for(uint32_t i=0;i<petHigh;++i){const auto& p=pets[i];
  if(!p.active||p.epoch!=b->epoch||p.game!=game||p.thread!=b->thread||!TrapSkill(p.skill))continue;
  void* other=lookup(game,1,p.uid);uint32_t otherKind=0,otherMode=0;int32_t px=0,py=0;
  if(!other||!Read(reinterpret_cast<uintptr_t>(other)+4,otherKind)||otherKind!=p.kind||!Read(reinterpret_cast<uintptr_t>(other)+0xc,otherMode)||otherMode==0||otherMode==12||stat(other,6,0)<256||Level(other)!=Level(unit)||!Position(other,px,py)||Distance(x,y,px,py)>8)continue;
  if(occupiedCount>=64){Log("ES_NATIVE_TRAP_PLACEMENT_WAIT nearby_trap_density=1 bounded_scan=1 no_overlap=1");return nullptr;}
  occupied[occupiedCount++]={px,py};
 }


 TrapArgs a={b->sovereign?5u:1u,unit,static_cast<uint16_t>(kind),0,0,static_cast<uint32_t>(mode),x,y,petType,static_cast<int32_t>(limit),0};
 using SpawnFn=void*(__fastcall*)(void*,const TrapArgs*);
 void* pet=nullptr;uint32_t attempts=0;
 constexpr int32_t dx[]={1,-1,0,0,1,-1,1,-1},dy[]={0,0,1,-1,1,1,-1,-1};


 for(uint32_t n=0;n<(b->sovereign?17u:1u)&&!pet;++n){
  a.x=x;a.y=y;if(n){const uint32_t direction=(n-1)%8;const int32_t radius=n<=8?3:5;a.x+=dx[direction]*radius;a.y+=dy[direction]*radius;}
  if(a.x<1||a.x>65535||a.y<1||a.y>65535)continue;
  bool clear=true;for(uint32_t j=0;j<occupiedCount;++j)if(Distance(a.x,a.y,occupied[j].x,occupied[j].y)<2){clear=false;break;}if(!clear)continue;
  void* room=reinterpret_cast<RoomAtFn>(ctx->exeBase+0x38dfb0)(reinterpret_cast<RoomFn>(ctx->exeBase+0x34b440)(unit),a.x,a.y);
  if(!room||reinterpret_cast<TownFn>(ctx->exeBase+0x2f0750)(room))continue;
  ++attempts;pet=reinterpret_cast<SpawnFn>(ctx->exeBase+0x4350e0)(game,&a);
 }
 Text t;t.Add("ES_NATIVE_TRAP_CREATE uid=");t.Num(b->uid);t.Add(" skill=");t.Num(id);t.Add(" summon_class=");t.Num(kind);t.Add(" created=");t.Num(pet!=nullptr);t.Add(" alive_after=");t.Num(PetCount(*b,id));t.Add(" group_limit=");t.Num(limit);t.Add(" x=");t.Num(a.x);t.Add(" y=");t.Num(a.y);t.Add(" native_placement_attempts=");t.Num(attempts);t.Add(" nearby_traps=");t.Num(occupiedCount);t.Add(" minimum_trap_distance=2 native_spawn_stats_mode=1");Log(t.s,!pet);
 if(!pet)return nullptr;
 uint32_t petId[3]={};if(pet==unit||Legend(pet)||!Read(reinterpret_cast<uintptr_t>(pet),petId)||petId[0]!=1||SovereignNativeData::Leader(petId[1])||petId[1]!=static_cast<uint32_t>(kind)){Log("ES_NATIVE_TRAP_REJECT unexpected_spawn_identity=1 boss_drop_untouched=1",true);return nullptr;}


 OwnerChanged(game,pet,static_cast<int32_t>(b->uid),1);
 if(!Owned(game,pet)){Log("ES_NATIVE_TRAP_REJECT untracked_spawn=1",true);noLoot(pet,1);kill(game,pet,nullptr,0);return nullptr;}
 if(b->sovereign){int32_t px=0,py=0;bool clear=Position(pet,px,py);
  for(uint32_t j=0;j<occupiedCount&&clear;++j)if(Distance(px,py,occupied[j].x,occupied[j].y)<2)clear=false;
  if(!clear){Log("ES_NATIVE_TRAP_REJECT actual_position_not_separated=1 native_cleanup=1 no_loot=1",true);noLoot(pet,1);kill(game,pet,nullptr,0);return nullptr;}
 }
 using BaseFn=void(__fastcall*)(void*,void*,void*,int32_t,int32_t);
 using PassiveFn=void(__fastcall*)(void*,void*,void*,int32_t,int32_t,int32_t,int32_t);
 reinterpret_cast<BaseFn>(ctx->exeBase+0x55fb50)(game,unit,pet,0,level);
 reinterpret_cast<PassiveFn>(ctx->exeBase+0x55ef80)(game,unit,pet,id,level,unitLevel,0);
 if(auto* tracked=NativePet(game,pet))NativePetHostile(*tracked,pet);
 alignas(8) uint8_t change[48]={};
 using InfoFn=void(__fastcall*)(void*,int32_t,void*);using ModeFn=int32_t(__fastcall*)(void*,void*,int32_t);
 reinterpret_cast<InfoFn>(ctx->exeBase+0x4471e0)(pet,mode,change);
 reinterpret_cast<ModeFn>(ctx->exeBase+0x4475c0)(game,change,1);
 return pet;
}
}
