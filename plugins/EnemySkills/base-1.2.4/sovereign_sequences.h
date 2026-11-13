#pragma once
namespace SovereignNative {
using SequenceFn=int32_t(__fastcall*)(void*,void*);
SequenceFn priorSequence=nullptr;
bool sequenceReady=false;
int32_t TemplateSequence(uint8_t bank,uint32_t id) noexcept {
 if(!sequenceReady||bank<2||bank>3||(id!=107&&id!=132))return 0;
 using MonFn=const void*(__fastcall*)(uint8_t,int32_t);
 using SeqFn=const void*(__fastcall*)(uint8_t,int32_t);
 const uint32_t cls=id==107?1779:1827,slot=id==107?2:1,expected=id==107?439:447;
 const uintptr_t row=reinterpret_cast<uintptr_t>(reinterpret_cast<MonFn>(ctx->exeBase+0x976e0)(bank,cls));
 int16_t skill=-1,index=0;uintptr_t data=0;int32_t length=0;
 if(!row||!Read(row+0x1c4+2*slot,skill)||skill!=expected||!Read(row+0x1dc+2*slot,index)||index<=0)return 0;
 const uintptr_t seq=reinterpret_cast<uintptr_t>(reinterpret_cast<SeqFn>(ctx->exeBase+0x396fe0)(bank,index));
 if(!seq||!Read(seq,data)||!data||!Read(seq+0xc,length)||length<=0||length>4096||!Readable(data,4))return 0;
 return index;
}
int32_t __fastcall Sequence(void* unit,void* node) noexcept {

 const int32_t native=priorSequence(unit,node);if(native>0)return native;
 uint32_t ident[3]={};uintptr_t row=0;int16_t id=-1;int32_t owner=0;uint8_t bank=0;
 if(!unit||!node||!Read(reinterpret_cast<uintptr_t>(unit),ident)||ident[0]!=1||!SovereignNativeData::Leader(ident[1])||!Read(reinterpret_cast<uintptr_t>(node),row)||!row||!Read(row,id)||(id!=107&&id!=132)||!Read(reinterpret_cast<uintptr_t>(node)+0x4c,owner)||owner!=-1||CastContext::getUsed(unit)!=node||!Read(reinterpret_cast<uintptr_t>(unit)+0x1bd,bank))return native;
 const auto* m=SovereignNativeData::Get(ident[1]);if(!m||!(m->mask&(1u<<4))||!(m->mask&(1u<<2)))return native;


 const int32_t index=TemplateSequence(bank,id);return index>0?index:native;
}
bool InstallSequence() noexcept {
 if(sequenceReady)return true;
 const uint8_t entry[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xec,0x20};
 const uint8_t lookup[]={0x48,0x8d,0x90,0xc4,0x01,0,0,0x66,0x39,0x1a};
 const uint8_t seqBounds[]={0x48,0x3b,0x98,0x80,0x11,0,0,0x73,0x46};
 const uint8_t result[]={0x41,0x0f,0xbf,0x84,0x41,0xdc,0x01,0,0};

 if(!LegendNative::Check(0x396ff5,seqBounds,sizeof(seqBounds))||!LegendNative::Check(0x33dbc0,entry,sizeof(entry))||!LegendNative::Check(0x33dc8c,lookup,sizeof(lookup))||!LegendNative::Check(0x33dcc2,result,sizeof(result)))return false;
 auto* target=reinterpret_cast<void*>(ctx->exeBase+0x33dbc0);
 if(LegendNative::installedHooks>=LegendNative::HookCapacity||MH_CreateHook(target,reinterpret_cast<void*>(Sequence),reinterpret_cast<void**>(&priorSequence))!=MH_OK||!priorSequence)return false;
 if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);return false;}
 LegendNative::targets[LegendNative::installedHooks++]=reinterpret_cast<uintptr_t>(target);sequenceReady=true;
 Log("ES_SOVEREIGN_SEQUENCE_READY charge_leap=1 existing_dungeon_templates=1 shared_tables_unchanged=1 native_sequences_preserved=1");return true;
}
bool InitializeAnimation() noexcept {
 if(!ES::initialized){const auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;ES::initialized=true;}
 return InstallSequence();
}
}
