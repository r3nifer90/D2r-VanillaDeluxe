#pragma once

namespace SkillBalance {
struct Entry {uint32_t id;const wchar_t* key;};
constexpr Entry Entries[]={{17,L"SlowMissiles"},{26,L"Strafe"},{32,L"Valkyrie"},{36,L"FireBolt"},{38,L"ChargedBolt"},{39,L"IceBolt"},{40,L"FrozenArmor"},{44,L"FrostNova"},{45,L"IceBlast"},{47,L"FireBall"},{48,L"Nova"},{49,L"Lightning"},{50,L"ShiverArmor"},{51,L"FireWall"},{53,L"ChainLightning"},{55,L"GlacialSpike"},{56,L"Meteor"},{57,L"ThunderStorm"},{59,L"Blizzard"},{60,L"ChillingArmor"},{62,L"Hydra"},{64,L"FrozenOrb"},{66,L"AmplifyDamage"},{67,L"Teeth"},{68,L"BoneArmor"},{70,L"RaiseSkeleton"},{72,L"Weaken"},{75,L"ClayGolem"},{80,L"RaiseSkeletalMage"},{82,L"LifeTap"},{83,L"PoisonExplosion"},{84,L"BoneSpear"},{87,L"Decrepify"},{88,L"BonePrison"},{91,L"LowerResist"},{92,L"PoisonNova"},{93,L"BoneSpirit"},{94,L"FireGolem"},{98,L"Might"},{102,L"HolyFire"},{104,L"Defiance"},{106,L"Zeal"},{107,L"Charge"},{111,L"Vengeance"},{112,L"BlessedHammer"},{113,L"Concentration"},{114,L"HolyFreeze"},{118,L"HolyShock"},{121,L"FistoftheHeavens"},{122,L"Fanaticism"},{123,L"Conviction"},{126,L"Bash"},{132,L"Leap"},{146,L"BattleCry"},{147,L"Frenzy"},{152,L"Berserk"},{154,L"WarCry"},{221,L"Raven"},{225,L"Firestorm"},{226,L"OakSage"},{227,L"SummonSpiritWolf"},{229,L"MoltenBoulder"},{231,L"CarrionVine"},{234,L"Fissure"},{235,L"CycloneArmor"},{240,L"Twister"},{244,L"Volcano"},{245,L"Tornado"},{246,L"SpiritofBarbs"},{247,L"SummonGrizzly"},{249,L"Armageddon"},{250,L"Hurricane"},{256,L"ShockField"},{257,L"BladeSentinel"},{258,L"BurstofSpeed"},{262,L"WakeofFire"},{267,L"Fade"},{271,L"LightningSentry"},{276,L"DeathSentry"},{277,L"BladeShield"},{278,L"Venom"},{373,L"SummonGoatman"},{376,L"SummonTainted"},{377,L"SummonDefiler"},{390,L"BladeWarp"},{394,L"RingofFire"},{395,L"MiasmaBolt"},{398,L"FlameWave"},{399,L"MiasmaChains"},{388,L"EchoingStrike"},{401,L"Apocalypse"},{402,L"Abyss"}};
constexpr uint32_t Count=sizeof(Entries)/sizeof(Entries[0]);
struct Profile {uint32_t level=20,damage=100,useLevels=1,useDamage=1;uint32_t levels[Count]={},damageScale[Count]={};};
Profile profiles[3];uint32_t enabled=0,logDamage=1;
uint32_t Canon(uint32_t id) noexcept {return id>=437&&id<497?LegendNativeData::Skills[id-437].original:id>=500&&id<533?LegendNativeData::ExtraSkills[id-500].original:id;}
int32_t Index(uint32_t id) noexcept {id=Canon(id);for(uint32_t i=0;i<Count;++i)if(Entries[i].id==id)return i;return -1;}
bool Number(const wchar_t* path,const wchar_t* section,const wchar_t* key,uint32_t fallback,uint32_t maximum,uint32_t& out) noexcept {
 Text n;n.Num(fallback);wchar_t def[32]={},v[32]={};for(uint32_t i=0;n.s[i];++i)def[i]=n.s[i];
 GetPrivateProfileStringW(section,key,def,v,32,path);uint32_t value=0,i=0;
 for(;v[i];++i){if(i>8||v[i]<'0'||v[i]>'9')return false;const uint32_t digit=v[i]-'0';if(value>maximum/10||(value==maximum/10&&digit>maximum%10))return false;value=value*10+digit;}
 if(!i)return false;out=value;return true;
}
bool Load(const wchar_t* path) noexcept {
 if(!Number(path,L"SkillBalance",L"enabled",0,1,enabled))return false;if(!enabled)return true;if(!Number(path,L"SkillBalance",L"log_damage_values",1,1,logDamage))return false;
 const wchar_t* sections[]={L"Skills.Sovereign",L"Skills.Corrupted",L"Skills.Dungeon"};
 for(uint32_t p=0;p<3;++p){auto& cfg=profiles[p];cfg.level=p?30:20;cfg.damage=100;cfg.useLevels=cfg.useDamage=1;memset(cfg.levels,0,sizeof(cfg.levels));for(auto& v:cfg.damageScale)v=100;
  if(!Number(path,sections[p],L"default_level",cfg.level,99,cfg.level)||!cfg.level||!Number(path,sections[p],L"damage_percent",100,10000,cfg.damage)||!Number(path,sections[p],L"use_skill_levels",1,1,cfg.useLevels)||!Number(path,sections[p],L"use_skill_damage",1,1,cfg.useDamage))return false;
  for(uint32_t i=0;i<Count;++i){wchar_t key[96]={};uint32_t n=0;for(;Entries[i].key[n];++n)key[n]=Entries[i].key[n];const wchar_t suffix[]=L".level";for(uint32_t j=0;j<sizeof(suffix)/2;++j)key[n+j]=suffix[j];if(!Number(path,sections[p],key,0,99,cfg.levels[i]))return false;
   if(Entries[i].id==276)continue;const wchar_t dmg[]=L".damage_percent";for(uint32_t j=0;j<sizeof(dmg)/2;++j)key[n+j]=dmg[j];if(!Number(path,sections[p],key,100,10000,cfg.damageScale[i]))return false;
  }
 }
 Text t;t.Add("ES_SKILL_BALANCE_CONFIG enabled=");t.Num(enabled);t.Add(" profiles=3 independent_levels=1 death_sentry_level_only=1 global_rows_unchanged=1");Log(t.s);return true;
}
uint32_t Level(uint32_t profile,uint32_t id,uint32_t legacy) noexcept {
 if(!enabled||profile>2)return legacy;const auto& p=profiles[profile];const int32_t i=Index(id);return p.useLevels&&i>=0&&p.levels[i]?p.levels[i]:p.level;
}
uint32_t DamageBasisPoints(uint32_t profile,uint32_t id) noexcept {
 if(!enabled||profile>2||!id||Canon(id)==276)return 10000;
 const auto& p=profiles[profile];const int32_t i=Index(id);if(i<0)return 10000;
 return p.damage*(p.useDamage?p.damageScale[i]:100);
}
}
