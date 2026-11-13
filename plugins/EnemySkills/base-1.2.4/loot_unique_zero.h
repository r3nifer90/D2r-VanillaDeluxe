#pragma once





namespace ES::LootUniqueZero {
#include "native_unique_zero_contract.inc"
uintptr_t owned=0;
bool Contract(bool fixed) noexcept {
 if(!ctx||!ctx->exeBase||!Readable(ctx->exeBase+Begin,sizeof(Original)))return false;
 const auto* p=(const uint8_t*)(ctx->exeBase+Begin);
 for(size_t i=0;i<sizeof(Original);++i)if(p[i]!=(fixed&&i==Branch-Begin?0x78:Original[i]))return false;
 return true;
}
bool Change(uint8_t before,uint8_t after) noexcept {
 auto* p=(volatile uint8_t*)(ctx->exeBase+Branch);if(*p!=before)return false;
 DWORD old=0,ignored=0;if(!VirtualProtect((void*)p,1,PAGE_EXECUTE_READWRITE,&old))return false;
 *p=after;
 const bool flushed=FlushInstructionCache(GetCurrentProcess(),(const void*)p,1)!=FALSE;
 if(!flushed){*p=before;FlushInstructionCache(GetCurrentProcess(),(const void*)p,1);}
 if(!VirtualProtect((void*)p,1,old,&ignored))Log("ES_LOOT_UNIQUE_ZERO_PROTECTION_RESTORE_FAILED",true);
 return flushed;
}
bool Install() noexcept {
 if(owned)return Contract(true);

 if(Contract(true)){Log("ES_LOOT_UNIQUE_ZERO_READY existing_fix=1 owned=0 unique_id_0=allowed missing_id_minus_1=rejected new_item_rows=0");return true;}
 if(!Contract(false)||!Change(0x7e,0x78))return false;
 owned=ctx->exeBase+Branch;
 Log("ES_LOOT_UNIQUE_ZERO_READY existing_fix=0 owned=1 unique_id_0=allowed missing_id_minus_1=rejected new_item_rows=0");return true;
}
void Remove() noexcept {
 if(!owned)return;
 if(Contract(true)){if(!Change(0x78,0x7e)){Log("ES_LOOT_UNIQUE_ZERO_RESTORE_FAILED",true);return;}}
 else Log("ES_LOOT_UNIQUE_ZERO_RESTORE_SKIPPED foreign_code_change=1",true);
 owned=0;
}
}
