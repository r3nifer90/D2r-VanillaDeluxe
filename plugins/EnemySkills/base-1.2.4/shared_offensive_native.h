#pragma once
namespace LegendNative {
uint32_t SharedMode(uint32_t kind) noexcept {
 const uint16_t mask=LegendNativeData::Modes(kind);
 constexpr uint8_t modes[]={7,8,9,10,4,5};
 for(uint8_t mode:modes)if(mask&(1u<<mode))return mode;
 return 0;
}


bool PrepareShared(void* game,void* unit,const R3niBossSkills::Skill& s) noexcept {
 if(!ready||!SharedOffensive::Supported(s.id)||!Legend(unit)||!game)return false;
 uint32_t kind=0,mode=0;uintptr_t actualGame=0;uint8_t bank=0,difficulty=255;
 if(!Read((uintptr_t)unit+4,kind)||!SharedMode(kind)||!Read((uintptr_t)unit+0xd8,actualGame)||actualGame!=(uintptr_t)game||!Read((uintptr_t)game+0x106,bank)||bank<2||bank>3||!Read((uintptr_t)game+0x104,difficulty)||difficulty!=2||!Read((uintptr_t)unit+0xc,mode)||mode==0||mode==12||stat(unit,6,0)<256||alignment(unit)==2)return false;
 const auto* meta=Meta(s.id);if(!meta||s.kind!=meta->kind)return false;
 void* node=find(unit,s.id,-1);
 if(!node){
  const uint8_t entry[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20,0x8b,0xea,0x48,0x8b,0xf9};
  const uint8_t owner[]={0x89,0x43,0x10,0xc7,0x43,0x40,0x01,0,0,0,0xc7,0x43,0x4c,0xff,0xff,0xff,0xff};
  if(!Check(0x33a010,entry,sizeof(entry))||!Check(0x33a182,owner,sizeof(owner)))return false;
  using LearnFn=void*(__fastcall*)(void*,int32_t);node=((LearnFn)(ctx->exeBase+0x33a010))(unit,s.id);
 }
 uintptr_t row=0;int16_t id=-1;int32_t owner=0;
 if(!node||!Read((uintptr_t)node,row)||!row||!Read(row,id)||id!=s.id||!Read((uintptr_t)node+0x4c,owner)||owner!=-1||!Writable((uintptr_t)node+0x40,4)||!Writable((uintptr_t)node+0x10,4))return false;
 *(uint32_t*)((uintptr_t)node+0x40)=s.level;*(uint32_t*)((uintptr_t)node+0x10)=SharedMode(kind);return true;
}
}
