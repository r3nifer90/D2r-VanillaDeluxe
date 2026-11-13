#pragma once
namespace SummonTuning {

struct Entry {uint32_t id;const wchar_t* key;uint32_t defaultLimit;};
constexpr Entry Entries[]={{32,L"Valkyrie",0},{70,L"RaiseSkeleton",8},{75,L"ClayGolem",0},{80,L"RaiseSkeletalMage",8},{94,L"FireGolem",0},{221,L"Raven",8},{226,L"OakSage",0},{227,L"SummonSpiritWolf",0},{231,L"CarrionVine",0},{246,L"SpiritofBarbs",0},{247,L"SummonGrizzly",0},{373,L"SummonGoatman",0},{376,L"SummonTainted",0},{377,L"SummonDefiler",0}};
constexpr uint32_t Count=sizeof(Entries)/sizeof(Entries[0]);
uint32_t limits[Count]={},batch[Count]={},sovereignBatch[Count]={};
int32_t Index(uint32_t id) noexcept {id=SkillBalance::Canon(id);for(uint32_t i=0;i<Count;++i)if(Entries[i].id==id)return i;return -1;}
uint32_t Limit(uint32_t id,uint32_t fallback) noexcept {const int32_t i=Index(id);return i>=0&&limits[i]?limits[i]:fallback;}
uint32_t Batch(uint32_t id) noexcept {const int32_t i=Index(id);return i>=0&&batch[i]?batch[i]:cfg.legendSummonBatch;}
uint32_t SovereignBatch(uint32_t id) noexcept {const int32_t i=Index(id);return i>=0&&sovereignBatch[i]?sovereignBatch[i]:cfg.sovereignSummonBatch;}
bool Load(const wchar_t* path) noexcept {
 for(uint32_t i=0;i<Count;++i){wchar_t key[96]={};uint32_t n=0;for(;Entries[i].key[n];++n)key[n]=Entries[i].key[n];
  const wchar_t* suffix=L".max_summons";uint32_t j=0;do{key[n+j]=suffix[j];}while(suffix[j++]);
  if(!SkillBalance::Number(path,L"Summons",key,Entries[i].defaultLimit,8,limits[i]))return false;
  suffix=L".dungeon_per_cast";j=0;do{key[n+j]=suffix[j];}while(suffix[j++]);if(!SkillBalance::Number(path,L"Summons",key,0,8,batch[i]))return false;
  suffix=L".sovereign_per_cast";j=0;do{key[n+j]=suffix[j];}while(suffix[j++]);if(!SkillBalance::Number(path,L"Summons",key,0,8,sovereignBatch[i]))return false;
 }
 Text t;t.Add("ES_SUMMON_TUNING_CONFIG shared_limits=1 dungeon_attack_speed_bonus=");t.Num(cfg.legendAttackSpeed);t.Add(" movement_speed_bonus=");t.Num(cfg.legendMoveSpeed);t.Add(" summon_batch=");t.Num(cfg.legendSummonBatch);t.Add(" summon_interval_ms=");t.Num(cfg.legendSummonInterval);t.Add(" sovereign_summon_batch=");t.Num(cfg.sovereignSummonBatch);t.Add(" continuous_hammer=");t.Num(cfg.continuousHammer);t.Add(" hammer_interval_ms=");t.Num(cfg.hammerInterval);t.Add(" per_skill_controls=1 player_sovereign_stats_unchanged=1");Log(t.s);return true;
}
}
