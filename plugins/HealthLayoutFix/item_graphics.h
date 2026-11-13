#pragma once
namespace ItemGraphics {
using Selector=const char*(__fastcall*)(void*,uint32_t,int32_t,int32_t,uint8_t);
Selector original=nullptr;
constexpr uintptr_t Rva=0x662700;
constexpr unsigned char Bytes[]={0x48,0x89,0x5c,0x24,0x18,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x48,0x8b,0xec,0x48,0x83,0xec,0x60,0x48,0x8b,0xf9,0x45,0x85};
constexpr uint32_t Code(const char* text){return uint32_t(text[0])|(uint32_t(text[1])<<8)|(uint32_t(text[2])<<16)|(uint32_t(' ')<<24);}
struct Family {int32_t records[3];uint32_t codes[3];const char* asset;};
constexpr Family Families[]={
 {{443,933,1398},{Code("cbw"),Code("8cb"),Code("6cb")},"bow/short_battle_bow"},
 {{224,749,1204},{Code("buc"),Code("xuc"),Code("uuc")},"shield/vd_visceratuant"},
 {{10,543,998},{Code("wnd"),Code("9wn"),Code("7wn")},"wand/refreshed_iros_torch"},
 {{36,569,1024},{Code("bsw"),Code("9b9"),Code("7b7")},"sword/refreshed_blacktongue_nineteen"},

 {{83,615,1070},{Code("rng"),Code("xng"),Code("ung")},"armor/item43_darkglow"},
 {{83,616,1071},{Code("rng"),Code("xng"),Code("ung")},"armor/item43_darkglow"},

 {{41,573,1028},{Code("kri"),Code("9kr"),Code("7kr")},"knife/refreshed_the_jade_tan_do"},
 {{41,574,1029},{Code("kri"),Code("9kr"),Code("7kr")},"knife/refreshed_the_jade_tan_do"},

 {{87,619,1074},{Code("spl"),Code("xpl"),Code("upl")},"armor/item42_iceblink"},
 {{87,620,1075},{Code("spl"),Code("xpl"),Code("upl")},"armor/item42_iceblink"}
};
const char* __fastcall Select(void* manager,uint32_t code,int32_t quality,int32_t record,uint8_t identified){
 const char* result=original(manager,code,quality,record,identified);
 const DWORD error=GetLastError();
 if(manager&&quality==7&&identified){
  for(const auto& family:Families){bool knownRecord=false,knownCode=false;
   for(int32_t id:family.records)if(id==record)knownRecord=true;
   for(uint32_t base:family.codes)if(base==code)knownCode=true;
   if(knownRecord&&knownCode){result=family.asset;break;}
  }
 }
 SetLastError(error);return result;
}
}
