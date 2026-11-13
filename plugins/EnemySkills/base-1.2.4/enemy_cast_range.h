#pragma once


namespace BossSkills {
uint64_t CastDistanceSquared(int32_t x,int32_t y,int32_t xx,int32_t yy) noexcept {
 const int64_t dx=int64_t(x)-xx,dy=int64_t(y)-yy;
 return uint64_t(dx*dx+dy*dy);
}
bool CastWithin(void* unit,void* target,uint32_t radiusTenths) noexcept {
 int32_t x=0,y=0,tx=0,ty=0;
 return Position(unit,x,y)&&Position(target,tx,ty)&&
 CastDistanceSquared(x,y,tx,ty)*100<=uint64_t(radiusTenths)*radiusTenths;
}
bool CastMelee(uint32_t id,uint32_t kind) noexcept {
 const uint32_t original=LegendNative::Original(id);
 return kind==4||original==106||original==111||original==126||original==147||original==152||original==388;
}
bool CastEnemyEnabled(uint32_t id) noexcept {return LegendNative::Original(id)!=132;}

uint32_t CastRadius(uint32_t id,uint32_t kind,uint32_t level) noexcept {
 const uint32_t original=LegendNative::Original(id);
 const auto* s=Pool::Get(original);
 if(s&&s->category==Pool::Category::Trap)return 120;
 switch(original){
  case 44:return 118;
  case 48:return 114;
  case 92:return 170;
  case 146:return 50;
  case 154:return 70;
  case 394:return 100;
  case 401:return level>=20?170:level>=10?150:130;
  case 390:return 120;
 }


 if(kind==5||kind==6||kind==7||(s&&s->persistent)||original==68||original==249||original==250||original==277)return 600;
 return 180;
}
bool CastNativeMelee(void* game,void* unit,void* target) noexcept {
 if(!LegendNative::chooseTarget)return false;
 uintptr_t monster=0,control=0;int32_t distance=0,combat=0;
 if(!Read((uintptr_t)unit+0x10,monster)||!monster||!Read(monster+0x30,control)||!control||!Readable(control,0x3a))return false;
 void* picked=LegendNative::chooseTarget(game,unit,(void*)control,&distance,&combat,0);
 return combat&&picked==target;
}
bool CastInRange(void* game,void* unit,void* target,uint32_t id,uint32_t kind,uint32_t level) noexcept {
 if(!CastEnemyEnabled(id)||!target)return false;
 if(CastMelee(id,kind))return CastNativeMelee(game,unit,target);
 const uint32_t original=LegendNative::Original(id);
 if(original==107&&CastWithin(unit,target,39))return false;
 return CastWithin(unit,target,CastRadius(id,kind,level));
}
}
