#pragma once


namespace LegendNative {
using LeapUpdateFn=int32_t(__fastcall*)(void*,void*,void*);
using LeapCancelFn=int32_t(__fastcall*)(void*,void*,int32_t,int32_t);
using LeapRewindFn=void(__fastcall*)(void*,void*,int32_t);
using LeapPathFn=void(__fastcall*)(void*,int32_t);
bool LeapIdentity(CombatActions::State& s,void* unit) noexcept {
 uintptr_t path=0,row=0;int16_t id=-1;int32_t owner=0;
 return s.leapActive&&s.leapUnit==reinterpret_cast<uintptr_t>(unit)&&find&&
 Read(reinterpret_cast<uintptr_t>(unit)+0x38,path)&&path==s.leapPath&&
 find(unit,s.leapSkill,-1)==reinterpret_cast<void*>(s.leapNode)&&
 Read(s.leapNode,row)&&row&&Read(row,id)&&id==s.leapSkill&&
 Read(s.leapNode+0x4c,owner)&&owner==-1;
}
void LeapLog(const CombatRecord& r,void* unit,const char* event,const char* reason) noexcept {
 const auto& s=*r.state;uint32_t flags=0,type=0,foot=0,mask=0;
 Read(s.leapNode+0x14,flags);Read(s.leapPath+0x50,type);Read(s.leapPath+0x60,foot);Read(s.leapPath+0x64,mask);
 int32_t x=0,y=0;BossSkills::Position(unit,x,y);
 Text t;t.Add(event);t.Add(" uid=");t.Num(r.uid);t.Add(" role=");t.Add(r.sovereign?"sovereign":"dungeon_legend");
 t.Add(" skill=");t.Num(s.leapSkill);t.Add(" reason=");t.Add(reason);t.Add(" flags=");t.Num(flags);
 t.Add(" path_type=");t.Num(type);t.Add(" footprint_mask=");t.Num(foot);t.Add(" move_mask=");t.Num(mask);
 t.Add(" x=");t.Num(x);t.Add(" y=");t.Num(y);t.Add(" rewinds=");t.Num(s.leapRewinds);
 t.Add(" native_landing=1 player_skills_unchanged=1");Log(t.s);
}
void LeapObserve(void* game,void* unit,uint32_t id,int32_t result) noexcept {
 if(!result||Original(id)!=132)return;
 auto r=CombatActor(game,unit);if(!r.state||!r.spell)return;auto& s=*r.state;
 uintptr_t path=0;uint32_t mode=0,flags=0,type=0;void* node=find?find(unit,id,-1):nullptr;
 if(!node||BossSkills::UsedId(unit)!=id||!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||mode!=14||
 !Read(reinterpret_cast<uintptr_t>(unit)+0x38,path)||!path||!Read(path+0x50,type)||type!=9||
 !Read(reinterpret_cast<uintptr_t>(node)+0x14,flags)||!(flags&0x100))return;
 if(LeapIdentity(s,unit))return;
 s.leapUnit=reinterpret_cast<uintptr_t>(unit);s.leapPath=path;s.leapNode=reinterpret_cast<uintptr_t>(node);s.leapSkill=id;
 s.leapFrameValid=Read(reinterpret_cast<uintptr_t>(game)+0x170,s.leapFrame);
 s.leapRewinds=0;s.leapTickValid=false;s.leapBusy=false;s.leapActive=true;
 s.leapProgressValid=s.leapFrameValid&&BossSkills::Position(unit,s.leapX,s.leapY);s.leapProgressFrame=s.leapFrame;
 LeapLog(r,unit,"ES_LEAP_TAKEOFF","native_flight_started");
}
bool LeapExpired(const CombatActions::State& s,void* game) noexcept {
 uint32_t frame=0;return !s.leapFrameValid||!Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)||
 (static_cast<uint32_t>(frame-s.leapFrame)>=150&&static_cast<uint32_t>(frame-s.leapFrame)<0x80000000u);
}
bool LeapStalled(CombatActions::State& s,void* game,void* unit) noexcept {
 uint32_t frame=0;int32_t x=0,y=0;
 if(!Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)||!BossSkills::Position(unit,x,y))return false;
 if(!s.leapProgressValid||x!=s.leapX||y!=s.leapY){s.leapProgressValid=true;s.leapProgressFrame=frame;s.leapX=x;s.leapY=y;return false;}


 const uint32_t elapsed=frame-s.leapProgressFrame;
 return s.leapRewinds>=2&&elapsed>=25&&elapsed<0x80000000u;
}
void LeapCancel(CombatRecord& r,void* game,void* unit,const char* reason) noexcept {
 auto& s=*r.state;if(s.leapBusy)return;s.leapBusy=true;


 reinterpret_cast<LeapCancelFn>(ctx->exeBase+0x56a8c0)(game,unit,s.leapSkill,1);
 reinterpret_cast<LeapPathFn>(ctx->exeBase+0x342a70)(reinterpret_cast<void*>(s.leapPath),2);
 LeapLog(r,unit,"ES_LEAP_RECOVERY",reason);s.leapActive=false;s.leapBusy=false;
}
bool LeapUpdate(CombatRecord& r,void* game,void* unit,bool rewind) noexcept {
 auto& s=*r.state;uint32_t flags=0;if(!Read(s.leapNode+0x14,flags))return false;
 if(!(flags&0x100)){
  uint32_t type=0,foot=0,mask=0;
  if(Read(s.leapPath+0x50,type)&&type!=9&&Read(s.leapPath+0x60,foot)&&foot&&Read(s.leapPath+0x64,mask)&&mask){LeapLog(r,unit,"ES_LEAP_LANDED","native_handler");s.leapActive=false;return false;}
  LeapCancel(r,game,unit,"incomplete_native_exit");return false;
 }
 if(!PreserveCastSkill(unit,s.leapSkill)){LeapCancel(r,game,unit,"learned_node_lost");return false;}
 if(LeapExpired(s,game)){LeapCancel(r,game,unit,"flight_deadline");return false;}
 if(!(flags&2)&&LeapStalled(s,game,unit)){LeapCancel(r,game,unit,"flight_stalled");return false;}

 s.leapBusy=true;
 const int32_t landed=reinterpret_cast<LeapUpdateFn>(ctx->exeBase+0x56b5e0)(game,unit,reinterpret_cast<void*>(s.leapNode));
 if(landed){LeapLog(r,unit,"ES_LEAP_LANDED","native_path_arrived");s.leapActive=false;}
 else if(rewind){


  reinterpret_cast<LeapRewindFn>(ctx->exeBase+0x4914e0)(game,unit,0);
  if(++s.leapRewinds==1)LeapLog(r,unit,"ES_LEAP_FLIGHT_HELD","sequence_ended_before_landing");
 }
 s.leapBusy=false;return !landed;
}
bool LeapMode(void* game,void* unit,uint32_t requested) noexcept {
 if(requested==0||requested==12)return false;
 auto r=CombatActor(game,unit);if(!r.state)return false;auto& s=*r.state;
 if(s.leapBusy||!s.leapActive)return false;
 if(!LeapIdentity(s,unit)){s.leapActive=false;return false;}
 uint32_t mode=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode))return false;
 if(mode!=14){LeapCancel(r,game,unit,"animation_left_without_landing");return false;}
 uint32_t frame=0,flags=0;
 if(Read(s.leapNode+0x14,flags)&&(flags&0x100)&&!(flags&2)&&!LeapExpired(s,game)&&
 Read(reinterpret_cast<uintptr_t>(game)+0x170,frame)&&s.leapTickValid&&frame==s.leapLastFrame){
  if(PreserveCastSkill(unit,s.leapSkill))return true;
  LeapCancel(r,game,unit,"learned_node_lost");return false;
 }
 s.leapTickValid=Read(reinterpret_cast<uintptr_t>(game)+0x170,s.leapLastFrame);
 return LeapUpdate(r,game,unit,true);
}
void LeapWatch(void* game,void* unit) noexcept {
 auto r=CombatActor(game,unit);if(!r.state)return;auto& s=*r.state;
 if(s.leapBusy||!s.leapActive)return;
 if(!LeapIdentity(s,unit)){s.leapActive=false;return;}
 uint32_t mode=0,flags=0;if(!Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode)||!Read(s.leapNode+0x14,flags))return;
 const bool stalled=mode==14&&!(flags&2)&&LeapStalled(s,game,unit);
 if(mode!=14||LeapExpired(s,game)||stalled){
  LeapCancel(r,game,unit,mode!=14?"animation_left_without_landing":stalled?"flight_stalled":"flight_deadline");
  if(mode==14)move(game,unit,1,nullptr);return;
 }
 if(flags&2)LeapUpdate(r,game,unit,false);
}
}
