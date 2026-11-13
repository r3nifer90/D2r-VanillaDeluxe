#pragma once


namespace NovaRouteTrace {
using namespace ES;
bool captured=false,bossCaptured=false,baselineCaptured=false;
void Dump(const char* role,uintptr_t address,uint32_t size,bool code) noexcept {
 MEMORY_BASIC_INFORMATION m={};
 if(size>2048||!size||!Readable(address,size)||(code&&(!ctx||!ctx->exeBase||!VirtualQuery(reinterpret_cast<void*>(address),&m,sizeof(m))||reinterpret_cast<uintptr_t>(m.AllocationBase)!=ctx->exeBase||!Executable(address)))){
  Text t;t.Add("ES_NOVA_CAPTURE_SKIP role=");t.Add(role);Log(t.s);return;
 }
 Text head;head.Add(code?"ES_NOVA_NATIVE role=":"ES_NOVA_DATA role=");head.Add(role);if(code){head.Add(" rva=");head.Num(address-ctx->exeBase);}head.Add(" bytes=");head.Num(size);Log(head.s);
 constexpr char hex[]="0123456789abcdef";
 for(uint32_t offset=0;offset<size;offset+=64){Text line;line.Add(code?"ES_NOVA_CODE role=":"ES_NOVA_RAW role=");line.Add(role);line.Add(" offset=");line.Num(offset);line.Add(" hex=");
  const uint32_t count=size-offset<64?size-offset:64;
  for(uint32_t i=0;i<count;++i){const uint8_t value=*reinterpret_cast<const uint8_t*>(address+offset+i);char part[]={hex[value>>4],hex[value&15],0};line.Add(part);}Log(line.s);
 }
}
void Function(const char* role,uintptr_t address) noexcept {Dump(role,address,512,true);}
uintptr_t DirectCall(uintptr_t function,uint32_t offset) noexcept {
 uint8_t opcode=0;int32_t displacement=0;
 if(!ctx||!ctx->exeBase||!Executable(function)||!Read(function+offset,opcode)||opcode!=0xe8||!Read(function+offset+1,displacement))return 0;
 const uintptr_t target=function+offset+5+displacement;MEMORY_BASIC_INFORMATION m={};
 if(!Executable(target)||!VirtualQuery(reinterpret_cast<void*>(target),&m,sizeof(m))||reinterpret_cast<uintptr_t>(m.AllocationBase)!=ctx->exeBase)return 0;
 return target;
}
void Once() noexcept {
 if(captured)return;captured=true;
 const uintptr_t unit=reinterpret_cast<uintptr_t>(cast),coordinate=reinterpret_cast<uintptr_t>(coordinateCast);
 Function("unit",unit);Function("coordinate",coordinate);


 const uint8_t unitPrefix[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x70};
 const uint8_t coordPrefix[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x70};
 if(!Bytes(unit,unitPrefix,sizeof(unitPrefix))||!Bytes(coordinate,coordPrefix,sizeof(coordPrefix))){Log("ES_NOVA_INNER_CAPTURE_SKIP reason=wrapper_prefix_differs");return;}
 const uintptr_t innerUnit=DirectCall(unit,0xc2),innerCoordinate=DirectCall(coordinate,0xb6);
 if(innerUnit&&innerUnit==innerCoordinate)Dump("resolver",innerUnit,2048,true);
 else Log("ES_NOVA_INNER_CAPTURE_SKIP reason=shared_callee_differs");
 const uintptr_t notifyUnit=DirectCall(unit,0x111),notifyCoordinate=DirectCall(coordinate,0xe9);
 if(notifyUnit)Dump("unit_tail",notifyUnit,1024,true);
 if(notifyCoordinate)Dump("coordinate_tail",notifyCoordinate,1024,true);
}
void SkillRow(void* game) noexcept {
 uint8_t bank=0;Read(reinterpret_cast<uintptr_t>(game)+0x106,bank);
 const uintptr_t row=reinterpret_cast<uintptr_t>(skill(bank,92));uint16_t a=0,c=0;
 if(!row||!Readable(row,748)||!Read(row+0x20a,a)||!Read(row+0x20c,c)){Log("ES_NOVA_LOADED_ROW_UNREADABLE",true);return;}
 Text t;t.Add("ES_NOVA_LOADED_SKILL id=92 bank=");t.Num(bank);t.Add(" row_bytes=748 raw_word_020a=");t.Num(a);t.Add(" raw_word_020c=");t.Num(c);Log(t.s);Dump("skill92",row,748,false);
}
void Baseline(void* game) noexcept {if(baselineCaptured)return;baselineCaptured=true;Log("ES_NOVA_BASELINE_CAPTURE trigger=first_sovereign_registration no_andariel_required=1");SkillRow(game);Once();}
void Inputs(void* game,void* unit) noexcept {
 uint8_t bank=0,difficulty=0;uint32_t uid=0,kind=0,mode=0,flags=0;
 Read(reinterpret_cast<uintptr_t>(game)+0x106,bank);Read(reinterpret_cast<uintptr_t>(game)+0x104,difficulty);
 Read(reinterpret_cast<uintptr_t>(unit)+8,uid);Read(reinterpret_cast<uintptr_t>(unit)+4,kind);Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode);Read(reinterpret_cast<uintptr_t>(unit)+0x128,flags);
 Text t;t.Add("ES_NOVA_INPUT uid=");t.Num(uid);t.Add(" class=");t.Num(kind);t.Add(" bank=");t.Num(bank);t.Add(" difficulty=");t.Num(difficulty);t.Add(" mode=");t.Num(mode);t.Add(" flags=");t.Num(flags);Log(t.s);


 const uintptr_t row=reinterpret_cast<uintptr_t>(skill(bank,92));uint16_t word20a=0,word20c=0;
 if(!row||!Readable(row,748)||!Read(row+0x20a,word20a)||!Read(row+0x20c,word20c)){Log("ES_NOVA_LOADED_ROW_UNREADABLE",true);return;}
 Text loaded;loaded.Add("ES_NOVA_LOADED_SKILL id=92 bank=");loaded.Num(bank);loaded.Add(" row_bytes=748 raw_word_020a=");loaded.Num(word20a);loaded.Add(" raw_word_020c=");loaded.Num(word20c);Log(loaded.s);
 Dump("skill92",row,748,false);
 Dump("caster_before",reinterpret_cast<uintptr_t>(unit),336,false);
}
void After(void* unit,int32_t result) noexcept {
 uint32_t mode=0,flags=0;Read(reinterpret_cast<uintptr_t>(unit)+0xc,mode);Read(reinterpret_cast<uintptr_t>(unit)+0x128,flags);
 Text t;t.Add("ES_NOVA_AFTER mode=");t.Num(mode);t.Add(" flags=");t.Num(flags);t.Add(" wrapper_return=");t.Num(static_cast<uint32_t>(result));t.Add(" effect_unconfirmed=1");Log(t.s);
 Dump("caster_after",reinterpret_cast<uintptr_t>(unit),336,false);
}
}
