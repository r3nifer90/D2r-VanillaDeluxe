#pragma once

#include "names_generated.h"
namespace Names {
bool Bind() noexcept {
 const uint8_t flag[]={0xf6,0x41,0x1a,0x02};
 const uint8_t index[]={0x0f,0xb7,0x49,0x2a,0x48,0x39,0x88,0x80,0x10,0x00,0x00};


 return Bytes(coreBase+0x2a53f3,flag,sizeof(flag))&&Bytes(coreBase+0x2a5440,index,sizeof(index));
}
bool Validate(uint32_t bank) noexcept {
 DataTables::TableView view={.structSize=DataTables::TableViewSize};
 const auto b=static_cast<DataTables::Bank>(bank);
 if(tables->getTable(ctx,b,DataTables::TableId::SuperUniques,&view)!=DataTables::Result::Success||!view.rows||view.rowSize!=60||view.rowCount!=First+Count){Log("SP_NAMES_REJECT: run d2rl compile VanillaDeluxe outside a game, then restart; expected 100 SuperUniques records.",true);return false;}
 for(uint32_t n=0;n<SPData::Count;++n){
  DataTables::RowView mon={.structSize=DataTables::RowViewSize},named={.structSize=DataTables::RowViewSize};uint16_t a=0,bname=0;
  if(tables->getRow(ctx,b,DataTables::TableId::MonStats,SPData::Kinds[n].leader,&mon)!=DataTables::Result::Success||mon.rowSize!=508||tables->getRow(ctx,b,DataTables::TableId::SuperUniques,Ids[n],&named)!=DataTables::Result::Success||named.rowSize!=60||!Read(reinterpret_cast<uintptr_t>(mon.row)+0x60,a)||!Read(reinterpret_cast<uintptr_t>(named.row)+4,bname)||a!=bname)return false;
 }
 Log("SP_NAMES_READY: private super-unique names; native modifiers and stock elite names retained.");return true;
}
bool Apply(void* unit) noexcept {
 const uintptr_t u=reinterpret_cast<uintptr_t>(unit);uint32_t type=99,kind=0;uintptr_t data=0;uint16_t flags=0;
 if(!Read(u,type)||type!=1||!Read(u+4,kind)||kind<815||kind>=SPData::TotalRows||(kind-815)%2||!Read(u+0x10,data)||!data||!Writable(data+0x1a,2)||!Writable(data+0x2a,2)||!Read(data+0x1a,flags))return false;
 *reinterpret_cast<uint16_t*>(data+0x2a)=Ids[(kind-815)/2];


 *reinterpret_cast<uint16_t*>(data+0x1a)=flags|2|8;return true;
}
}
