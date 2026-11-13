#pragma once
namespace SkillRoles {
enum class Role:uint32_t {Offensive,Summon,Support};
constexpr uint32_t Count=3;
bool IsCurse(const Pool::Skill& s) noexcept {
 return s.category==Pool::Category::Curse||s.id==17||s.id==146;
}
Role Get(const Pool::Skill& s) noexcept {
 if(s.category==Pool::Category::Summon||s.id==88)return Role::Summon;
 switch(s.id){case 40:case 50:case 60:case 68:case 132:case 235:case 258:case 267:case 278:return Role::Support;}
 if(s.category==Pool::Category::Aura||IsCurse(s))return Role::Support;
 return Role::Offensive;
}
bool ActiveAttack(const Pool::Skill& s) noexcept {
 return Get(s)==Role::Offensive&&!s.persistent&&s.category!=Pool::Category::Trap;
}

bool SpellOffense(const Pool::Skill& s) noexcept {
 return Get(s)==Role::Offensive&&s.category!=Pool::Category::Trap;
}
const char* Name(Role r) noexcept {constexpr const char* names[]={"offensive","summon","support"};return names[(uint32_t)r];}

uint64_t offensiveDraws[2][Pool::Count]={};
uint32_t FairSkill(const uint32_t* choices,uint32_t n,uint32_t profile) noexcept {
 if(!ES::cfg.sovereignFairOffense)return choices[ES::Rand(n)];
 if(profile>1)profile=0;uint64_t minimum=~0ULL;uint32_t ties[Pool::Count]={},count=0;
 for(uint32_t i=0;i<n;++i){const uint64_t used=offensiveDraws[profile][choices[i]];if(used<minimum){minimum=used;count=0;}if(used==minimum)ties[count++]=choices[i];}
 const uint32_t index=ties[ES::Rand(count)];++offensiveDraws[profile][index];return index;
}




bool Select(const uint32_t* eligible,uint32_t n,uint32_t count,uint32_t* out,uint32_t profile=0) noexcept {
 if(count>3||n<count)return false;if(!count)return true;
 if(count==1){uint32_t index=eligible[ES::Rand(n)];
  if(Get(Pool::Skills[index])==Role::Offensive){uint32_t matches[Pool::Count]={},m=0;for(uint32_t i=0;i<n;++i)if(Get(Pool::Skills[eligible[i]])==Role::Offensive)matches[m++]=eligible[i];index=FairSkill(matches,m,profile);}
  out[0]=index;return true;
 }
 uint32_t active[Pool::Count]={},offense[Pool::Count]={},summons[Pool::Count]={},support[Pool::Count]={};
 uint32_t a=0,o=0,s=0,b=0;
 for(uint32_t i=0;i<n;++i){const auto& skill=Pool::Skills[eligible[i]];if(ActiveAttack(skill))active[a++]=eligible[i];
  switch(Get(skill)){case Role::Offensive:offense[o++]=eligible[i];break;case Role::Summon:summons[s++]=eligible[i];break;case Role::Support:support[b++]=eligible[i];break;}
 }
 if(!a)return false;
 uint32_t weights[3]={};
 if(count==2){if(o>=2)weights[0]=50;if(s)weights[1]=25;if(b)weights[2]=25;}
 else {if(o<2)return false;if(s)weights[1]=50;if(b)weights[2]=50;}
 const uint32_t total=weights[0]+weights[1]+weights[2];if(!total)return false;
 const uint32_t roll=ES::Rand(total);const uint32_t plan=roll<weights[0]?0:roll<weights[0]+weights[1]?1:2;
 out[0]=FairSkill(active,a,profile);
 uint32_t slot=1;
 if(count==3||plan==0){uint32_t remaining[Pool::Count]={},m=0;for(uint32_t i=0;i<o;++i)if(offense[i]!=out[0])remaining[m++]=offense[i];out[slot++]=FairSkill(remaining,m,profile);}
 if(plan==1)out[slot++]=summons[ES::Rand(s)];else if(plan==2)out[slot++]=support[ES::Rand(b)];
 return slot==count;
}
}
