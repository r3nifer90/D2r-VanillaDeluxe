#pragma once
#include "native_bank_bytes.inc"
bool bankStartupProven=false;
void ObserveSharedBank() noexcept {

 bankStartupProven=Executable(ctx->exeBase+0x300a90)&&Bytes(ctx->exeBase+0x300a90,bankBytes,sizeof(bankBytes));
 Log(bankStartupProven?"ES_NATIVE_BANK_STARTUP original_body_verified=1 deferred_hooks_allowed=1":"ES_NATIVE_BANK_STARTUP original_body_verified=0 native_legends_require_supported_getter=1",!bankStartupProven);
}
bool CheckSharedBank() noexcept {
 const uintptr_t entry=ctx->exeBase+0x300a90;
 if(Executable(entry)&&Bytes(entry,bankBytes,sizeof(bankBytes)))return true;



 uint8_t op=0;int32_t disp=0;
 if(bankStartupProven&&Executable(entry)&&Bytes(entry+5,bankBytes+5,sizeof(bankBytes)-5)&&Read(entry,op)&&op==0xe9&&Read(entry+1,disp)){
  uintptr_t destination=entry+5+static_cast<int64_t>(disp);uint8_t jump[2]={};
  if(destination!=entry&&Executable(destination)&&Read(destination,jump)){
   if(jump[0]==0xff&&jump[1]==0x25){int32_t indirect=0;uintptr_t target=0;
    if(!Read(destination+2,indirect)||!Read(destination+6+static_cast<int64_t>(indirect),target)||target==entry||target==destination||!Executable(target))destination=0;
   }else destination=0;
   if(destination){Log("ES_NATIVE_BANK_SHARED_HOOK accepted=1 startup_body_verified=1 live_suffix_verified=1 existing_getter_chain_preserved=1");return true;}
  }
 }
 Text t;t.Add("ES_NATIVE_CONTRACT_REJECT rva_decimal=3148432 shared_getter_contract=1 startup_verified=");t.Num(bankStartupProven);t.Add(" unsupported_entry_or_body=1 no_item_fallback=1");Log(t.s,true);return false;
}
