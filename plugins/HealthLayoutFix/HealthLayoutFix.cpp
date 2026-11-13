#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <D2RLPlugin/context.h>
#include "MinHook.h"
#include "native_contracts.h"
#include "boss_frame.h"
#ifdef HEALTH_LAYOUT_FIXTURE
unsigned fixtureCreated=0,fixtureEnabled=0,fixtureRemoved=0;
MH_STATUS FixtureInit(){return MH_OK;}
MH_STATUS FixtureCreate(void* target,void*,void** prior){++fixtureCreated;*prior=target;return MH_OK;}
MH_STATUS FixtureEnable(void*){++fixtureEnabled;return MH_OK;}
MH_STATUS FixtureDisable(void*){return MH_OK;}
MH_STATUS FixtureRemove(void*){++fixtureRemoved;return MH_OK;}
#define MH_Initialize FixtureInit
#define MH_CreateHook FixtureCreate
#define MH_EnableHook FixtureEnable
#define MH_DisableHook FixtureDisable
#define MH_RemoveHook FixtureRemove
#endif
#include "item_graphics.h"







namespace HealthLayout {
struct Size {int32_t width,height;};
using SetSize=void(__fastcall*)(void*,const Size*);
using SetScalar=void(__fastcall*)(void*,int32_t);
SetSize originalSize=nullptr;
SetScalar originalWidth=nullptr,originalX=nullptr;
void* targets[5]{};
unsigned installed=0;

bool Name(const void* widget,const char* expected,unsigned length){
 const auto address=reinterpret_cast<uintptr_t>(widget);
 if(!address)return false;


 const auto text=*reinterpret_cast<const char* const*>(address+8);
 if(!text||*reinterpret_cast<const uint64_t*>(address+0x10)!=length)return false;
 for(unsigned i=0;i<length;++i)if(text[i]!=expected[i])return false;
 return text[length]==0;
}
bool ParentSizedHealth(void* widget){
 const auto address=reinterpret_cast<uintptr_t>(widget);
 if(!address||!*reinterpret_cast<const unsigned char*>(address+0x52)||!Name(widget,"Health",6))return false;
 const void* parent=*reinterpret_cast<void* const*>(address+0x30);
 return Name(parent,"HUDMonsterHealth",16);
}
void __fastcall SizeHook(void* widget,const Size* size){if(!ParentSizedHealth(widget))originalSize(widget,size);}
void __fastcall WidthHook(void* widget,int32_t width){if(!ParentSizedHealth(widget))originalWidth(widget,width);}
void __fastcall XHook(void* widget,int32_t x){if(!ParentSizedHealth(widget))originalX(widget,x);}

bool Contract(uintptr_t address,const unsigned char* expected,unsigned size){
 unsigned char actual[32]{};SIZE_T read=0;
 if(size>sizeof(actual)||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),actual,size,&read)||read!=size)return false;
 for(unsigned i=0;i<size;++i)if(actual[i]!=expected[i])return false;
 return true;
}
void Stop(){
 for(unsigned i=0;i<installed;++i)if(targets[i]){MH_DisableHook(targets[i]);MH_RemoveHook(targets[i]);targets[i]=nullptr;}
 installed=0;
}
constexpr D2RL::PluginInfo info{sizeof(D2RL::PluginInfo),4,"r3ni-monster-health-layout","Monster Health and Item Graphics","0.1.7","r3ni","Preserves centered monster health bars and corrects asset routing for Sacred Bow, Visceratuant, Iros Torch, Blacktongue, Darkglow, Jade Tan Do and Iceblink families.",D2RL::PluginFlags::ModScopedOnly|D2RL::PluginFlags::NativeHooks|D2RL::PluginFlags::Client,{}};
static_assert(D2RL::HasValidPluginRole(info.flags)&&D2RL::HasOnlyKnownPluginFlags(info.flags));
}
extern "C" __declspec(dllexport) const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &HealthLayout::info;}
extern "C" __declspec(dllexport) bool D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept{
 using namespace HealthLayout;
 if(!D2RL::HasContext(context)||!context->activeMod||!context->exeBase)return false;
 const char* mod="VanillaDeluxe";unsigned j=0;for(;mod[j]&&context->activeMod[j]==mod[j];++j){}if(mod[j]||context->activeMod[j])return false;
 const uintptr_t base=context->exeBase; BossFrame::base=base;
 if(!BossFrame::Contracts()||!Contract(base+SizeRva,SizeBytes,sizeof(SizeBytes))||!Contract(base+WidthRva,WidthBytes,sizeof(WidthBytes))||!Contract(base+XRva,XBytes,sizeof(XBytes))||!Contract(base+ItemGraphics::Rva,ItemGraphics::Bytes,sizeof(ItemGraphics::Bytes)))return false;
 const auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
 void* replacements[]={reinterpret_cast<void*>(SizeHook),reinterpret_cast<void*>(WidthHook),reinterpret_cast<void*>(XHook),reinterpret_cast<void*>(ItemGraphics::Select),reinterpret_cast<void*>(BossFrame::Unique)};
 void** originals[]={reinterpret_cast<void**>(&originalSize),reinterpret_cast<void**>(&originalWidth),reinterpret_cast<void**>(&originalX),reinterpret_cast<void**>(&ItemGraphics::original),reinterpret_cast<void**>(&BossFrame::original)};
 const uintptr_t rvas[]={SizeRva,WidthRva,XRva,ItemGraphics::Rva,BossFrame::Rva};
 constexpr unsigned hookCount=sizeof(rvas)/sizeof(*rvas);
 static_assert(hookCount==sizeof(targets)/sizeof(*targets));
 static_assert(hookCount==sizeof(replacements)/sizeof(*replacements));
 static_assert(hookCount==sizeof(originals)/sizeof(*originals));
 for(unsigned i=0;i<hookCount;++i){
  targets[i]=reinterpret_cast<void*>(base+rvas[i]);
  if(MH_CreateHook(targets[i],replacements[i],originals[i])!=MH_OK){targets[i]=nullptr;Stop();return false;}
  ++installed;
 }
 for(unsigned i=0;i<hookCount;++i)if(MH_EnableHook(targets[i])!=MH_OK){Stop();return false;}
 return true;
}
extern "C" __declspec(dllexport) void D2RLoaderUnloadPlugin() noexcept{HealthLayout::Stop();}
#ifdef HEALTH_LAYOUT_FIXTURE
extern "C" __declspec(dllexport) unsigned HealthFixtureLoadState(unsigned index){switch(index){case 0:return fixtureCreated;case 1:return fixtureEnabled;case 2:return HealthLayout::installed;case 3:return fixtureRemoved;default:return sizeof(D2RL::PluginContext);}}
extern "C" __declspec(dllexport) void HealthFixtureOriginals(HealthLayout::SetSize size,HealthLayout::SetScalar width,HealthLayout::SetScalar x){HealthLayout::originalSize=size;HealthLayout::originalWidth=width;HealthLayout::originalX=x;}
extern "C" __declspec(dllexport) void HealthFixtureSize(void* widget,const HealthLayout::Size* size){HealthLayout::SizeHook(widget,size);}
extern "C" __declspec(dllexport) void HealthFixtureWidth(void* widget,int32_t width){HealthLayout::WidthHook(widget,width);}
extern "C" __declspec(dllexport) void HealthFixtureX(void* widget,int32_t x){HealthLayout::XHook(widget,x);}
extern "C" __declspec(dllexport) void GraphicsFixtureOriginal(void* fn){ItemGraphics::original=reinterpret_cast<ItemGraphics::Selector>(fn);}
extern "C" __declspec(dllexport) const char* GraphicsFixtureSelect(void* manager,uint32_t code,int32_t quality,int32_t record,uint8_t identified){return ItemGraphics::Select(manager,code,quality,record,identified);}
#endif
extern "C" BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
