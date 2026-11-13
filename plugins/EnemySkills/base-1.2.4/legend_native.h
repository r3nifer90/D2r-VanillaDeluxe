#pragma once
#include "native_skill_data.h"
#include "native_model_modes.h"
#include "sovereign_model_modes.h"
#include "sovereign_skill_data.h"
#include "shared_offensive_skills.h"
namespace BossSkills {struct Boss;Boss* NativeBoss(void*,uint32_t,uint32_t) noexcept;bool NativeRoot(const Boss&,void*) noexcept;}
namespace SovereignNative {
void CombatPoll(void*) noexcept;
bool TrapCastGuard(void*,void*,uint32_t) noexcept;bool Decision(void*,void*) noexcept;bool Tracked(void*,void*) noexcept;
BossSkills::Boss* PetRoot(void*,uint32_t,uint32_t) noexcept;bool Repair(void*,void*) noexcept;void ModeResult(void*,void*,uint32_t,int32_t) noexcept;
int32_t GuardLevel(void*,void*,int32_t,int32_t) noexcept;
void Event(void*,void*,uint32_t,int32_t,bool) noexcept;
void Missile(void*,void*,void*) noexcept;
}
namespace LegendNative {
using namespace ES;
using UseFn=int32_t(__fastcall*)(void*,void*,uint8_t,int32_t,void*,int32_t,int32_t,uint8_t);
using FindFn=void*(__fastcall*)(void*,int32_t,int32_t);
using MoveFn=int32_t(__fastcall*)(void*,void*,int32_t,void*);
using WalkFn=int32_t(__fastcall*)(void*,void*,void*,uint16_t);
using WalkXYFn=int32_t(__fastcall*)(void*,void*,int32_t,int32_t);WalkXYFn walkXY=nullptr;
using TargetFn=void*(__fastcall*)(void*,void*,void*,int32_t*,int32_t*,uint8_t);
using CollisionFn=int32_t(__fastcall*)(void*,void*,uint32_t);
using CurrentTargetFn=void*(__fastcall*)(void*,void*);
using FaceFn=void(__fastcall*)(void*,int32_t,int32_t,int32_t);
using IdleFn=void(__fastcall*)(void*,void*,int32_t);
using KillFn=void(__fastcall*)(void*,void*,void*,int32_t);
using AlignFn=void(__fastcall*)(void*,uint8_t,int32_t);
using FlagFn=void(__fastcall*)(void*,int32_t);
using OwnerFn=void(__fastcall*)(void*,void*,int32_t,int32_t,int32_t,int32_t);
using AiFn=void(__fastcall*)(void*,void*,int32_t,int32_t);
using InitFn=void(__fastcall*)(void*,void*,void*,int32_t);
using SkillHandlerFn=int32_t(__fastcall*)(void*,void*,int32_t,int32_t,int32_t,int32_t,int32_t);
using MissileFn=void*(__fastcall*)(void*,void*);
using PanelFn=void(__fastcall*)(void*,void*);
using AssignFn=void(__fastcall*)(void*,int32_t,int32_t,int32_t);
using StateFn=int32_t(__fastcall*)(void*,int32_t);
using FormulaFn=int32_t(__fastcall*)(uint8_t,void*,uint32_t,int32_t,int32_t);
struct TrapArgs;using SpawnPetFn=void*(__fastcall*)(void*,const TrapArgs*);
SpawnPetFn priorSpawnPet=nullptr;
void* __fastcall CreatureSpawn(void*,const TrapArgs*) noexcept;
using TrapFn=void*(__fastcall*)(void*,void*,int32_t,int32_t,const void*,int32_t,int32_t,int32_t);
using ModeFn=int32_t(__fastcall*)(void*,void*,int32_t);
using UsedSkillFn=void(__fastcall*)(void*,void*,int32_t);
UsedSkillFn setUsed=nullptr;
bool PreserveCastSkill(void*,uint32_t) noexcept;
using RightFn=void*(__fastcall*)(void*);
UseFn nativeUse=nullptr,use=nullptr;FindFn find=nullptr;MoveFn move=nullptr;IdleFn idle=nullptr;KillFn kill=nullptr;AlignFn align=nullptr;FlagFn noLoot=nullptr;
WalkFn walk=nullptr;
TargetFn chooseTarget=nullptr;CollisionFn collision=nullptr;
FaceFn face=nullptr;CurrentTargetFn currentTarget=nullptr;
AssignFn assign=nullptr;StateFn state=nullptr;RightFn right=nullptr;
OwnerFn priorOwner=nullptr;AiFn priorAi=nullptr;InitFn priorInit=nullptr;SkillHandlerFn priorSkill=nullptr;MissileFn priorMissile=nullptr;
PanelFn priorPanel=nullptr;
using SetStatFn=void(__fastcall*)(void*,uint32_t,int32_t,uint32_t);
SetStatFn setStat=nullptr;StatFn baseStat=nullptr;
FormulaFn formula=nullptr,priorFormula=nullptr;
int32_t __fastcall PetFormula(uint8_t,void*,uint32_t,int32_t,int32_t) noexcept;
using CoordinatesFn=int32_t(__fastcall*)(void*,void*,int32_t*,int32_t*);CoordinatesFn priorCoordinates=nullptr;
int32_t __fastcall PetCoordinates(void*,void*,int32_t*,int32_t*) noexcept;
TrapFn priorTrap=nullptr;
void* __fastcall TrapSummon(void*,void*,int32_t,int32_t,const void*,int32_t,int32_t,int32_t) noexcept;
using DropFn=uint64_t(__fastcall*)(void*,void*,void*,uint16_t,int32_t,int32_t,int32_t,void**,int32_t*,int32_t);
DropFn priorDrop=nullptr;
using TcLookupFn=const void*(__fastcall*)(uint8_t,uint16_t,int32_t);
TcLookupFn priorTcLookup=nullptr;
const void* __fastcall CorruptedTcLookup(uint8_t,uint16_t,int32_t) noexcept;
using LootItemFn=void*(__fastcall*)(void*,void*,int32_t,int32_t,int32_t,int32_t);
LootItemFn priorLootItem=nullptr;
void* __fastcall LootItem(void*,void*,int32_t,int32_t,int32_t,int32_t) noexcept;
using DamageFn=uint64_t(__fastcall*)(void*,void*,void*,int32_t,void*);
DamageFn priorDamage=nullptr;
uint64_t __fastcall Damage(void*,void*,void*,int32_t,void*) noexcept;
void HurricaneCastEnd(void*,void*,uint32_t,int32_t,int32_t) noexcept;
uint64_t __fastcall Drop(void*,void*,void*,uint16_t,int32_t,int32_t,int32_t,void**,int32_t*,int32_t) noexcept;
ModeFn priorMode=nullptr;
int32_t __fastcall ChangeMode(void*,void*,int32_t) noexcept;
bool HitReaction(uint32_t mode) noexcept {return mode==3||mode==6||mode==13;}
bool CastGuardEnabled(uint32_t) noexcept;
bool TrapCastingSkill(uint32_t) noexcept;
bool ready=false,diagReady=false;constexpr uint32_t HookCapacity=33;uintptr_t targets[HookCapacity]={};uint32_t installedHooks=0;
bool CombatActive(void*,void*) noexcept;
void CombatYieldForSpell(void*,void*) noexcept;
void LeapObserve(void*,void*,uint32_t,int32_t) noexcept;
bool LeapMode(void*,void*,uint32_t) noexcept;
void LeapWatch(void*,void*) noexcept;
bool CombatFallback(void*,void*) noexcept;
void BasicEvent(void*,void*,int32_t) noexcept;
bool BasicDamageTrack(void*,void*) noexcept;
void BasicDamageResult(void*,void*,void*,uint32_t,uint32_t,uintptr_t) noexcept;
void CombatWatch(void*,void*) noexcept;
void CombatAiSeen(void*,void*) noexcept;
void CombatCastTimeout(void*,void*,uint32_t,uint32_t,uint32_t,bool) noexcept;
int32_t CombatMode(void*,void*,int32_t,bool&) noexcept;
void CompleteCreatureCast(void*,void*,uint32_t,uint32_t,uint64_t) noexcept;
bool LateCreatureHandler(void*,void*,uint32_t) noexcept;
void OffensiveResult(void*,void*,uint32_t,int32_t) noexcept;
void ContinuousPoll(void*) noexcept;
bool ContinuousMissile(void*,void*,void*) noexcept;
int32_t __fastcall UseWithVisualSignal(void*,void*,uint8_t,int32_t,void*,int32_t,int32_t,uint8_t) noexcept;
bool CastSignalContract() noexcept;
void __fastcall SignalWire(void*,uint8_t,uint32_t,uint8_t,uint32_t,uint16_t,uint16_t,uint16_t,uint16_t,uint8_t,int32_t) noexcept;
extern void(__fastcall* priorSignalWire)(void*,uint8_t,uint32_t,uint8_t,uint32_t,uint16_t,uint16_t,uint16_t,uint16_t,uint8_t,int32_t);
int32_t __fastcall ClientCastMode(void*,void*) noexcept;
void __fastcall SignalTarget(void*,int32_t,int32_t,int32_t,int32_t,uint8_t) noexcept;
void __fastcall SignalCoordinates(void*,int32_t,int32_t,uint16_t,uint16_t,uint8_t) noexcept;
extern int32_t(__fastcall* priorClientCastMode)(void*,void*);
extern void(__fastcall* priorSignalTarget)(void*,int32_t,int32_t,int32_t,int32_t,uint8_t);
extern void(__fastcall* priorSignalCoordinates)(void*,int32_t,int32_t,uint16_t,uint16_t,uint8_t);
bool VisualContract() noexcept;
bool ClientFlowContract() noexcept;
bool InstallPlayerVisual() noexcept;
void RemovePlayerVisual() noexcept;
inline bool playerVisualHook=false;
void VisualClientLearn(void*,void*,uint32_t,uint32_t) noexcept;
void MeteorClock(void*,uint32_t,const char*) noexcept;
void ProjectileState(void*,uint32_t,uint32_t,uint32_t,const char*) noexcept;
void ClientPrepareAction(void*,uint32_t,uintptr_t) noexcept;
int32_t __fastcall ClientStart(void*,int32_t,int32_t,int32_t,int32_t) noexcept;
int32_t __fastcall ClientDo(void*,int32_t,int32_t,int32_t,int32_t) noexcept;
void __fastcall ClientFrame(void*) noexcept;
void* __fastcall ClientMissile(void*,int32_t) noexcept;
extern int32_t(__fastcall* priorClientStart)(void*,int32_t,int32_t,int32_t,int32_t);
extern int32_t(__fastcall* priorClientDo)(void*,int32_t,int32_t,int32_t,int32_t);
extern void(__fastcall* priorClientFrame)(void*);
extern void*(__fastcall* priorClientMissile)(void*,int32_t);
void VisualMissile(void*,void*,void*,void*) noexcept;
void __fastcall VisualSync(void*,void*,void*,int32_t) noexcept;
void __fastcall VisualSend(void*,const void*) noexcept;
void __fastcall VisualInitial(void*,void*,void*) noexcept;
void __fastcall VisualPacket21(const void*) noexcept;
extern void(__fastcall* priorVisualSync)(void*,void*,void*,int32_t);
extern void(__fastcall* priorVisualSend)(void*,const void*);
extern void(__fastcall* priorVisualInitial)(void*,void*,void*);
extern void(__fastcall* priorVisualPacket21)(const void*);
bool ContinuousCoordinates(void*,void*,int32_t*,int32_t*) noexcept;
bool DispatchAi(void*,void*) noexcept;
bool Owned(void*,void*) noexcept;
bool CustomPetAi(void*,void*) noexcept;
bool PanelOwned(void*) noexcept;
void OwnerChanged(void*,void*,int32_t,int32_t) noexcept;
int32_t GuardLevel(void*,void*,int32_t,int32_t) noexcept;
void InitLevels(void*,void*) noexcept;
void* PetHealthStart(void*,void*,int32_t) noexcept;
void PetHealthEnd(void*) noexcept;
void SkillEvent(void*,void*,uint32_t,int32_t,bool) noexcept;
void MissileEvent(void*,void*,void*) noexcept;
const LegendNativeData::Skill* Meta(uint32_t id) noexcept {return id>=437&&id<497?&LegendNativeData::Skills[id-437]:id>=500&&id<533?&LegendNativeData::ExtraSkills[id-500]:SovereignNativeData::Meta(id);}
int32_t AuraState(uint32_t id) noexcept {if(id==438)return 49;if(id==473)return 33;if(id==477)return 35;const auto* meta=Meta(id);const auto* s=Pool::Get(meta?meta->original:id);return s&&s->category==Pool::Category::Aura?s->state:-1;}
uint32_t Original(uint32_t id) noexcept {const auto* s=Meta(id);return s?s->original:id;}
bool Legend(void* unit) noexcept {uint32_t id[3]={};return Read(reinterpret_cast<uintptr_t>(unit),id)&&id[0]==1&&((id[1]>=1779&&id[1]<2099)||(id[1]>=2102&&id[1]<2278));}
bool SovereignLeader(void* unit) noexcept {uint32_t id[3]={};return unit&&Read(reinterpret_cast<uintptr_t>(unit),id)&&id[0]==1&&SovereignNativeData::Leader(id[1]);}
void Mana(void* unit) noexcept {if(setStat&&stat(unit,8,0)<25600){setStat(unit,9,9999*256,0);setStat(unit,8,9999*256,0);}}
bool FreeMode(void* unit) noexcept {uint32_t mode=0;return Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)&&(mode==1||mode==2||mode==15);}
void __fastcall Ai(void* game,void* unit,int32_t a,int32_t b) noexcept {
 if(ready){LeapWatch(game,unit);CombatAiSeen(game,unit);CombatYieldForSpell(game,unit);if(CombatActive(game,unit)||DispatchAi(game,unit)||CombatFallback(game,unit))return;}
 priorAi(game,unit,a,b);
}
void __fastcall Owner(void* game,void* unit,int32_t id,int32_t type,int32_t a,int32_t b) noexcept {priorOwner(game,unit,id,type,a,b);if(ready)OwnerChanged(game,unit,id,type);}
void __fastcall Panel(void* client,void* unit) noexcept {




 if(ready&&!client&&PanelOwned(unit)){Log("ES_NATIVE_PET_PANEL_SKIPPED monster_owner=1 null_player_client=1 summon_preserved=1");return;}
 priorPanel(client,unit);
}
void __fastcall Init(void* game,void* unit,void* control,int32_t state) noexcept {
 if(ready&&CustomPetAi(game,unit)&&control&&Writable(reinterpret_cast<uintptr_t>(control),0x14)){


  *static_cast<uint32_t*>(control)=0;
  *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(control)+8)=ctx->exeBase+0x4a3680;
  *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(control)+0x10)=1;
  return;
 }
 priorInit(game,unit,control,state);
 if(ready)InitLevels(game,unit);
}
uint32_t* BalanceSkillField(void*,void*) noexcept;
void BalanceWeaponMissile(void*,void*,void*,void*) noexcept;
int32_t DungeonSummonBatch(void*,void*,int32_t,int32_t,int32_t,int32_t,int32_t) noexcept;
int32_t __fastcall SkillHandler(void* game,void* unit,int32_t id,int32_t level,int32_t pay,int32_t item,int32_t effect) noexcept {


 if(ready&&Original(id)==132){
  uint32_t identity[3]={};
  if(unit&&Read((uintptr_t)unit,identity)&&identity[0]==1){
   auto* root=BossSkills::NativeBoss(game,identity[2],identity[1]);
   if((root&&BossSkills::NativeRoot(*root,unit))||SovereignNative::Tracked(game,unit))return 0;
  }
 }
 if(ready&&LateCreatureHandler(game,unit,id))return 0;
 if(ready){level=SovereignNative::GuardLevel(game,unit,id,level);level=GuardLevel(game,unit,id,level);}
 const bool track=ready&&(Legend(unit)||Owned(game,unit));
 if(track)SkillEvent(game,unit,id,0,true);
 if(ready)SovereignNative::Event(game,unit,id,0,true);
 void* healthScope=ready?PetHealthStart(game,unit,id):nullptr;
 uint32_t* balanceField=ready&&SkillBalance::enabled?BalanceSkillField(game,unit):nullptr;const uint32_t oldBalance=balanceField?*balanceField:0;if(balanceField)*balanceField=id;
 const int32_t result=DungeonSummonBatch(game,unit,id,level,pay,item,effect);
 if(ready&&Original(id)==132)LeapObserve(game,unit,id,result);
 if(balanceField)*balanceField=oldBalance;
 if(ready&&id==0)BasicEvent(game,unit,result);
 if(healthScope)PetHealthEnd(healthScope);
 if(ready&&Original(id)==250)HurricaneCastEnd(game,unit,id,level,result);
 if(track)SkillEvent(game,unit,id,result,false);
 if(ready)SovereignNative::Event(game,unit,id,result,false);
 if(ready)OffensiveResult(game,unit,id,result);
 return result;
}
void* __fastcall Missile(void* game,void* params) noexcept {void* owner=nullptr;Read(reinterpret_cast<uintptr_t>(params)+8,owner);const bool track=ready&&owner&&(Legend(owner)||Owned(game,owner));void* result=priorMissile(game,params);if(ready)ArmageddonProbe::Missile(params,result,0);if(ready)VisualMissile(game,owner,params,result);const bool periodic=ready&&ContinuousMissile(game,owner,result);if(ready&&SkillBalance::enabled)BalanceWeaponMissile(game,owner,params,result);if(track&&!periodic)MissileEvent(game,owner,result);if(ready&&!periodic)SovereignNative::Missile(game,owner,result);return result;}
#include "shared_summon_contract.h"
bool Check(uint32_t rva,const uint8_t* bytes,uint32_t n) noexcept {
 if(Executable(ctx->exeBase+rva)&&Bytes(ctx->exeBase+rva,bytes,n))return true;
 if(SharedSummonContract::Accepted(rva,bytes,n))return true;
 Text t;t.Add("ES_NATIVE_CONTRACT_REJECT rva_decimal=");t.Num(rva);t.Add(" checked_bytes=");t.Num(n);t.Add(" unsupported_code_or_conflicting_hook=1 no_item_fallback=1");Log(t.s,true);return false;
}
#include "legend_native_bank.h"
bool CreateBalanceDamageHooks() noexcept;
bool Bind() noexcept {

 const uint8_t corpseEntry[]={0x40,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf9,0x48,0x85,0xc9};
 const uint8_t corpseGate[]={0xe8,0x41,0x09,0xdd,0xff};
 const uint8_t corpseReserve[]={0xba,0x76,0,0,0,0x48,0x8b,0xcf,0xe8,0xce,0xb5,0xdc,0xff};
 if(!Check(0x33a510,corpseEntry,sizeof(corpseEntry))||!Check(0x569bca,corpseGate,sizeof(corpseGate))||!Check(0x569bd5,corpseReserve,sizeof(corpseReserve)))return false;
 if(ready)return true;



 #include "native_contract_checks.inc"
 #include "native_walk_contract.inc"
 #include "native_target_contract.inc"
 #include "native_face_contract.inc"
 #include "native_aura_contract.inc"
 #include "native_combat_contract.inc"
 #include "native_summon_contract.inc"
 #include "native_level_side_contract.inc"
 #include "native_attack_contract.inc"
 #include "native_dungeon_tuning_contract.inc"
 #include "native_leap_contract.inc"
 #include "native_reaction_contract.inc"
 if(!CheckSharedBank())return false;
 #include "native_pet_fix_contract.inc"
 #include "native_hammer_contract.inc"
 const uint8_t handlerTail[]={0x44,0x89,0x4c,0x24,0x20,0x44,0x89,0x44,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
 if(!Check(0x43acb5,handlerTail,sizeof(handlerTail)))return false;
 if(reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(L"D2RCore.dll"),MAKEINTRESOURCEA(1249)))!=coreBase+TerrorBalance::SetStatRva||!Bytes(coreBase+TerrorBalance::SetStatRva,TerrorBalance::SetStatBytes,sizeof(TerrorBalance::SetStatBytes))){Log("ES_NATIVE_CONTRACT_REJECT core_stat_setter=1 no_item_fallback=1",true);return false;}
 if(reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(L"D2RCore.dll"),MAKEINTRESOURCEA(1242)))!=coreBase+TerrorBalance::BaseStatRva||!Bytes(coreBase+TerrorBalance::BaseStatRva,TerrorBalance::BaseStatBytes,sizeof(TerrorBalance::BaseStatBytes))){Log("ES_NATIVE_CONTRACT_REJECT core_base_stat_getter=1",true);return false;}
 baseStat=reinterpret_cast<StatFn>(coreBase+TerrorBalance::BaseStatRva);
 formula=reinterpret_cast<FormulaFn>(ctx->exeBase+0x3b5160);
 setStat=reinterpret_cast<SetStatFn>(coreBase+TerrorBalance::SetStatRva);
 nativeUse=reinterpret_cast<UseFn>(ctx->exeBase+0x4a7bc0);use=UseWithVisualSignal;find=reinterpret_cast<FindFn>(ctx->exeBase+0x33dcd0);
 setUsed=reinterpret_cast<UsedSkillFn>(ctx->exeBase+0x34f430);
 move=reinterpret_cast<MoveFn>(ctx->exeBase+0x4a78e0);idle=reinterpret_cast<IdleFn>(ctx->exeBase+0x4a6e50);
 walk=reinterpret_cast<WalkFn>(ctx->exeBase+0x4a8770);
 {const uint8_t entry[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xec,0x50};if(!Check(0x4a8880,entry,sizeof(entry)))return false;walkXY=reinterpret_cast<WalkXYFn>(ctx->exeBase+0x4a8880);}
 chooseTarget=reinterpret_cast<TargetFn>(ctx->exeBase+0x595750);collision=reinterpret_cast<CollisionFn>(ctx->exeBase+0x3502a0);
 currentTarget=reinterpret_cast<CurrentTargetFn>(ctx->exeBase+0x48fe20);
 face=reinterpret_cast<FaceFn>(ctx->exeBase+0x3416c0);
 assign=reinterpret_cast<AssignFn>(ctx->exeBase+0x438a70);state=reinterpret_cast<StateFn>(ctx->exeBase+0x3351b0);right=reinterpret_cast<RightFn>(ctx->exeBase+0x34b400);
 kill=reinterpret_cast<KillFn>(ctx->exeBase+0x44db20);align=reinterpret_cast<AlignFn>(ctx->exeBase+0x48e600);noLoot=reinterpret_cast<FlagFn>(ctx->exeBase+0x34f010);
 if(!initialized){const auto r=MH_Initialize();if(r!=MH_OK&&r!=MH_ERROR_ALREADY_INITIALIZED)return false;initialized=true;}
 const uintptr_t rvas[]={0x4a2a00,0x4a5800,0x4a2690,0x480ad0,0x56e6a0,0x43acb0,0x4475c0,0x4350e0,0x3b5160,0x432b70};void* hooks[]={reinterpret_cast<void*>(Ai),reinterpret_cast<void*>(Owner),reinterpret_cast<void*>(Init),reinterpret_cast<void*>(Panel),reinterpret_cast<void*>(TrapSummon),reinterpret_cast<void*>(SkillHandler),reinterpret_cast<void*>(ChangeMode),reinterpret_cast<void*>(CreatureSpawn),reinterpret_cast<void*>(PetFormula),reinterpret_cast<void*>(PetCoordinates)};void** originals[]={reinterpret_cast<void**>(&priorAi),reinterpret_cast<void**>(&priorOwner),reinterpret_cast<void**>(&priorInit),reinterpret_cast<void**>(&priorPanel),reinterpret_cast<void**>(&priorTrap),reinterpret_cast<void**>(&priorSkill),reinterpret_cast<void**>(&priorMode),reinterpret_cast<void**>(&priorSpawnPet),reinterpret_cast<void**>(&priorFormula),reinterpret_cast<void**>(&priorCoordinates)};
 for(uint32_t i=0;i<10;++i){auto* target=reinterpret_cast<void*>(ctx->exeBase+rvas[i]);if(installedHooks>=HookCapacity||MH_CreateHook(target,hooks[i],originals[i])!=MH_OK||!(*originals[i]))goto fail;targets[installedHooks++]=reinterpret_cast<uintptr_t>(target);}
 if(!CreateBalanceDamageHooks())goto fail;
 ready=true;
 for(uint32_t i=0;i<installedHooks;++i){const auto status=MH_EnableHook(reinterpret_cast<void*>(targets[i]));if(status!=MH_OK&&status!=MH_ERROR_ENABLED)goto fail;}
 Log("ES_CREATURE_SUMMON_READY petmax_callers_verified=1 shared_native_groups=1 sovereign_and_dungeon_batch=1 missing_animation_event_completion=1 native_pathfinder=1 player_limits_unchanged=1");
 Log("ES_NATIVE_TRAP_RESOLVER_ABI pointer_bits=64 optional_xy=null native_resolver=1 sovereign_and_dungeon=1");
 Log("ES_NATIVE_REACTION_CONTEXT mode_info_clears_used_skill=1 scoped_native_restore=1 original_damage_and_death=1");
 Log("ES_LEAP_LANDING_READY native_path_arrival=1 airborne_sequence_rewind=1 collision_restore=1 registered_dungeon_sovereign_only=1 player_leap_unchanged=1");
 Log("ES_LEGEND_NATIVE_READY version=1 learned_skills=1 native_ai_use_skill=1 native_animation=1 monster_owned_pet_ai=1 monster_pet_panel_guard=1 configured_level_handler_guard=1 all_profile_attack_guard=1 player_skill_rows_unchanged=1");return true;
 fail:ready=false;for(uint32_t i=0;i<installedHooks;++i){MH_DisableHook(reinterpret_cast<void*>(targets[i]));MH_RemoveHook(reinterpret_cast<void*>(targets[i]));}installedHooks=0;Log("ES_LEGEND_NATIVE_REJECT hook_or_byte_contract=1 no_item_cast_fallback=1",true);return false;
}
bool Diagnostics() noexcept {
 if(diagReady)return true;


 const uint8_t missileTail[]={0x48,0x8d,0x6c,0x24,0xc0,0x48,0x81,0xec,0x40,0x01,0x00,0x00};
 if(!Check(0x5371a5,missileTail,sizeof(missileTail)))return false;
 const uint8_t dropEntry[]={0x40,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
 if(!Check(0x441300,dropEntry,sizeof(dropEntry)))return false;


 const uint8_t damageEntry[]={0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0xe8,0xfe,0xff,0xff,0x48,0x81,0xec,0x18,0x02,0,0};
 if(!Check(0x44ce80,damageEntry,sizeof(damageEntry)))return false;
 const uint8_t itemEntry[]={0x4c,0x8b,0xdc,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8d,0x6c,0x24,0xa0,0x48,0x81,0xec,0x60,0x01,0x00,0x00};
 if(!Check(0x444680,itemEntry,sizeof(itemEntry)))return false;
 const uint8_t tcEntry[]={0x48,0x89,0x5c,0x24,0x08,0x55,0x56,0x57,0x48,0x83,0xec,0x30,0x41,0x8b,0xd8,0x0f,0xb7,0xea};
 if(!Check(0x397290,tcEntry,sizeof(tcEntry)))return false;
 const uint8_t targetAction[]={0xe8,0xd4,0xa0,0x19,0};const uint8_t coordinateAction[]={0xe8,0xe7,0x9f,0x19,0};
 const uint8_t packetSkill[]={0x89,0x7b,0x08,0x40,0x0f,0xb6,0xcd,0xc6,0x43,0x0c,0x01};
 if(!Check(0x1a5697,targetAction,sizeof(targetAction))||!Check(0x1a5784,coordinateAction,sizeof(coordinateAction))||!Check(0x437d4e,packetSkill,sizeof(packetSkill)))return false;
 if(!VisualContract()||!ClientFlowContract()||!CastSignalContract())return false;
 const uintptr_t rvas[]={0x5371a0,0x441300,0x44ce80,0x444680,0x397290,0x537bc0,0x4784d0,0x538690,0x12b9b0,0x218030,0x216d20,0x1a7a80,0x1b7760,0x33f770,0x5390d0,0x5391a0,0x47f7a0};void* hooks[]={reinterpret_cast<void*>(Missile),reinterpret_cast<void*>(Drop),reinterpret_cast<void*>(Damage),reinterpret_cast<void*>(LootItem),reinterpret_cast<void*>(CorruptedTcLookup),reinterpret_cast<void*>(VisualSync),reinterpret_cast<void*>(VisualSend),reinterpret_cast<void*>(VisualInitial),reinterpret_cast<void*>(VisualPacket21),reinterpret_cast<void*>(ClientStart),reinterpret_cast<void*>(ClientDo),reinterpret_cast<void*>(ClientFrame),reinterpret_cast<void*>(ClientMissile),reinterpret_cast<void*>(ClientCastMode),reinterpret_cast<void*>(SignalTarget),reinterpret_cast<void*>(SignalCoordinates),reinterpret_cast<void*>(SignalWire)};void** originals[]={reinterpret_cast<void**>(&priorMissile),reinterpret_cast<void**>(&priorDrop),reinterpret_cast<void**>(&priorDamage),reinterpret_cast<void**>(&priorLootItem),reinterpret_cast<void**>(&priorTcLookup),reinterpret_cast<void**>(&priorVisualSync),reinterpret_cast<void**>(&priorVisualSend),reinterpret_cast<void**>(&priorVisualInitial),reinterpret_cast<void**>(&priorVisualPacket21),reinterpret_cast<void**>(&priorClientStart),reinterpret_cast<void**>(&priorClientDo),reinterpret_cast<void**>(&priorClientFrame),reinterpret_cast<void**>(&priorClientMissile),reinterpret_cast<void**>(&priorClientCastMode),reinterpret_cast<void**>(&priorSignalTarget),reinterpret_cast<void**>(&priorSignalCoordinates),reinterpret_cast<void**>(&priorSignalWire)};
 const uint32_t before=installedHooks;
 for(uint32_t i=0;i<17;++i){if(i==12&&playerVisualHook)continue;auto* target=reinterpret_cast<void*>(ctx->exeBase+rvas[i]);if(installedHooks>=HookCapacity||MH_CreateHook(target,hooks[i],originals[i])!=MH_OK||!(*originals[i]))goto fail;targets[installedHooks++]=reinterpret_cast<uintptr_t>(target);}
 for(uint32_t i=before;i<installedHooks;++i)if(MH_EnableHook(reinterpret_cast<void*>(targets[i]))!=MH_OK)goto fail;
 diagReady=true;MiniProbe::Line("CLIENT_HOOK_READY=1");Log("ES_SKILL_VISUAL_SYNC_READY native_cast_signal=1 sovereign_unit_target_wire=1 client_target_binding_order=1 native_packet_skill_damage=1 projectile_state_probe=1 native_client_action_modes=1 native_client_action_completion=1 zero_meteor_clock=1 damage_application_probe=1 native_packet73=1 hammer_override=1 learned_node_packet21=1 client_packet21_modes=1 solo_offline_path=1 no_extra_server_effects=1 client_render_not_observed=1");Log("ES_LEGEND_DIAGNOSTICS_READY skill_handler=1 server_missile_creation=1 pet_owner_creation=1 animation_transition=1 native_drop_result=1 native_item_attempt=1 hurricane_health_delta=1");return true;
 fail:for(uint32_t i=before;i<installedHooks;++i){MH_DisableHook(reinterpret_cast<void*>(targets[i]));MH_RemoveHook(reinterpret_cast<void*>(targets[i]));}installedHooks=before;Log("ES_LEGEND_DIAGNOSTICS_PARTIAL native_casts_still_observed=1",true);return false;
}
void Remove() noexcept {ready=diagReady=false;RemovePlayerVisual();for(uint32_t i=installedHooks;i-->0;){MH_DisableHook(reinterpret_cast<void*>(targets[i]));MH_RemoveHook(reinterpret_cast<void*>(targets[i]));}installedHooks=0;}
}
