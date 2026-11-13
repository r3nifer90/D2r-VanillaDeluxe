#pragma once
namespace CombatActions {
struct State {
 uint64_t nextBasic=0,logAt=0,watchLog=0,damageLog=0;
 uint32_t basicMode=0,basicFrame=0,watchFrame=0,watchMode=0,aiFrame=0,attempts=0,handlers=0,damageEvents=0;
 uint32_t staleSkill=0,staleMode=0,staleFrame=0;bool staleValid=false;
 bool basicActive=false,basicFrameValid=false,watching=false,aiFrameValid=false,issuing=false;
 uintptr_t leapUnit=0,leapPath=0,leapNode=0;
 uint32_t leapSkill=0,leapFrame=0,leapRewinds=0,leapLastFrame=0;
 bool leapActive=false,leapBusy=false,leapFrameValid=false,leapTickValid=false;
 uint32_t leapProgressFrame=0;int32_t leapX=0,leapY=0;bool leapProgressValid=false;
};
}
