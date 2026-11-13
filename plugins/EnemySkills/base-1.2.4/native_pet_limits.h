#pragma once
namespace LegendNative {


bool PetMaxCaller(uintptr_t pc) noexcept {
 const uint32_t sites[]={0x33bfdb,0x500804,0x51f249,0x533527,0x533562,0x5542fe,0x5566aa,0x558156,0x55c303,0x55c6e4,0x55d36d,0x55e737,0x55ebf7,0x55fef9,0x56e7f4,0x57120f,0x575b19,0x575d25,0x5770d0,0x5d5f5c};
 for(auto rva:sites)if(pc==ctx->exeBase+rva)return true;return false;
}
int32_t __fastcall PetFormula(uint8_t bank,void* unit,uint32_t expression,int32_t id,int32_t level) noexcept {
 using namespace BossSkills;
 const uintptr_t caller=reinterpret_cast<uintptr_t>(__builtin_return_address(0));
 if(ready&&id>0&&PetMaxCaller(caller)&&CreatureSummon(id)){
  uint32_t ident[3]={};uintptr_t game=0,row=0;uint8_t actualBank=0;uint32_t formulaId=0;
  if(unit&&Read((uintptr_t)unit,ident)&&ident[0]==1&&Read((uintptr_t)unit+0xd8,game)&&game&&Read(game+0x106,actualBank)&&actualBank==bank){
   auto* b=NativeBoss((void*)game,ident[2],ident[1]);bool assigned=false;if(b)for(const auto& s:b->skills)assigned|=s.id==uint32_t(id);
   void* node=assigned?find(unit,id,-1):nullptr;
   if(b&&NativeRoot(*b,unit)&&node&&Read((uintptr_t)node,row)&&row&&Read(row+0x114,formulaId)&&formulaId==expression){
    const uint32_t limit=NativeGroupLimit(*b,id);if(limit)return int32_t(limit);
   }
  }
 }
 return priorFormula(bank,unit,expression,id,level);
}



int32_t __fastcall PetCoordinates(void* game,void* unit,int32_t* x,int32_t* y) noexcept {
 using namespace BossSkills;uint32_t ident[3]={};
 if(ready&&reinterpret_cast<uintptr_t>(__builtin_return_address(0))==ctx->exeBase+0x561fed&&ContinuousCoordinates(game,unit,x,y))return 1;
 if(ready&&unit&&x&&y&&Read((uintptr_t)unit,ident)&&ident[0]==1){
  auto* b=NativeBoss(game,ident[2],ident[1]);
  if(b&&NativeRoot(*b,unit)&&b->nativeStarted&&UsedId(unit)==b->nativeSkill&&CreatureSummon(b->nativeSkill)){
   const uint32_t original=Original(b->nativeSkill);
   if(original!=70&&original!=80){int32_t ox=0,oy=0;const uint32_t alive=PetCount(*b,b->nativeSkill),limit=PetLimit(*b,b->nativeSkill);
    if(alive<limit&&Position(unit,ox,oy)){
     constexpr int32_t dx[]={1,1,0,-1,-1,-1,0,1},dy[]={0,1,1,1,0,-1,-1,-1};const uint32_t k=(b->uid+alive)%8;
     *x=ox+dx[k]*3;*y=oy+dy[k]*3;return 1;
    }
   }
  }
 }
 return priorCoordinates(game,unit,x,y);
}
}
