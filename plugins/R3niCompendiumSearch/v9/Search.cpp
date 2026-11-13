#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <D2RLPlugin/api.h>
#include "catalog.h"
extern "C" void* memset(void*p,int v,size_t n){auto b=(volatile unsigned char*)p;while(n--)*b++=(unsigned char)v;return p;}
using namespace D2RL;
static const PluginContext* ctx;
static const WidgetService* w;
static const PanelService* panels;
static const ResourceService* resourceService;
static const SharedEventService* events;
static const ThreadService* threads;
static HINSTANCE instance;
static HHOOK keyboardHook;
static volatile LONG armedSearch=-1;
static void arm(int page){InterlockedExchange(&armedSearch,page);}
static int armed(){return int(InterlockedCompareExchange(&armedSearch,0,0));}
static bool consumedReturn=false;
static void executeSearch(const PluginContext*,void*) noexcept;



static LRESULT CALLBACK keyboardMessage(int code,WPARAM remove,LPARAM data){
 if(code>=0&&remove==PM_REMOVE&&data){
  auto*m=reinterpret_cast<MSG*>(data);
  if(m->message==WM_KEYDOWN&&m->wParam==VK_ESCAPE)arm(-1);
  if(m->wParam==VK_RETURN){
   if(m->message==WM_KEYUP&&consumedReturn){consumedReturn=false;m->message=WM_NULL;return 0;}
   if(m->message==WM_KEYDOWN||m->message==WM_CHAR){
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    if(process==GetCurrentProcessId()&&!(GetKeyState(VK_MENU)&0x8000)&&!(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_SHIFT)&0x8000)&&(armed()>=0||consumedReturn)){
     if(!consumedReturn){
      unsigned page=unsigned(armed());arm(-1);consumedReturn=true;
      if(threads->runOnUiThread(ctx,executeSearch,reinterpret_cast<void*>(uintptr_t(page)))!=Threads::Result::Success)consumedReturn=false;
     }
     if(consumedReturn){m->message=WM_NULL;return 0;}
    }
   }
  }
 }
 return CallNextHookEx(keyboardHook,code,remove,data);
}
static bool ensureKeyboardHook(){
 if(keyboardHook)return true;
 DWORD process=0;DWORD thread=GetWindowThreadProcessId(GetForegroundWindow(),&process);
 if(!thread||process!=GetCurrentProcessId())return false;
 keyboardHook=SetWindowsHookExW(WH_GETMESSAGE,keyboardMessage,instance,thread);
 return keyboardHook!=nullptr;
}
static Panels::RegistrationHandle panelHandles[2]{};
static Resources::RegistrationHandle resourceHandles[2]{};
static bool seen[2]{};
static unsigned nextSlot=0;
static SharedEvents::ListenerHandle listener;
static char activeMisc[32]{};
static int opened=-1;
static bool eq(const char*a,const char*b){if(!a||!b)return false;while(*a&&*a==*b){++a;++b;}return *a==*b;}
static bool starts(const char*a,const char*b){if(!a||!b)return false;while(*b)if(*a++!=*b++)return false;return true;}
static char lower(char c){return c>='A'&&c<='Z'?char(c+32):c;}
static bool contains(const char*a,const char*b){if(!*b)return false;for(;*a;++a){unsigned i=0;while(b[i]&&a[i]&&lower(a[i])==lower(b[i]))++i;if(!b[i])return true;}return false;}
static void copy(char*out,const char*in,unsigned capacity){unsigned i=0;while(i+1<capacity&&in[i]){out[i]=in[i];++i;}out[i]=0;}
static int match(const PageSpec&p,const char*q,int after){if(!*q)return -1;for(unsigned n=0;n<p.count;++n){unsigned i=(unsigned(after+1)+n)%p.count;if(contains(p.cards[i].en,q)||contains(p.cards[i].pl,q))return int(i);}return -1;}
static Widgets::WidgetHandle find(Widgets::WidgetHandle h,const char*n){Widgets::WidgetHandle out=0;if(h)w->findWidget(ctx,h,n,&out);return out;}
static void error(const char*s){if(ctx)ctx->LogError(s);}

struct JsonBuffer{
 char bytes[8*1024*1024]{};unsigned used=0;bool okay=true;
 void reset(){used=0;okay=true;}
 void append(const char*s){while(*s){if(used+1>=sizeof(bytes)){okay=false;return;}bytes[used++]=*s++;}bytes[used]=0;}
 void number(unsigned n){char d[16];unsigned count=0;do{d[count++]=char('0'+n%10);n/=10;}while(n);while(count){char c[2]={d[--count],0};append(c);}}
 void text(const char*s,unsigned slot,unsigned count){while(*s){if(starts(s,"__SLOT__")){number(slot);s+=8;}else if(starts(s,"__COUNT__")){number(count);s+=9;}else{char c[2]={*s++,0};append(c);}}}
};
static JsonBuffer output;
static unsigned resultCount=0,resultHeight=0;
static void buildResults(const char*q,unsigned slot){
 output.reset();resultCount=0;resultHeight=0;
 for(const auto&c:globalCards)if(contains(c.en,q)||contains(c.pl,q))++resultCount;
 output.text(resultPrefix,slot,resultCount);output.append("[");unsigned added=0;
 for(const auto&c:globalCards)if(contains(c.en,q)||contains(c.pl,q)){
  const auto&t=templates[c.result];if(added++)output.append(",");output.append(t.prefix);output.number(resultHeight);output.append(t.suffix);resultHeight+=t.height;
 }
 if(!added)output.append(R"({"type":"TextBoxWidget","name":"NoMatch","fields":{"rect":{"x":10,"y":10,"width":2100,"height":100},"text":"No matching item.","style":{"fontFace":"Formal","pointSize":32,"fontColor":"$FontColorLightGold"}}})");
 output.append("]");output.text(resultSuffix,slot,resultCount);
}
static const char*localNames[]={"Matches0","Matches1"};
static const char*fullNames[]={"r3ni-compendium/Matches0","r3ni-compendium/Matches1"};
static const char*paths[]={"data/global/ui/layouts/r3ni-compendium/Matches0hd.json","data/global/ui/layouts/r3ni-compendium/Matches1hd.json"};
static bool showResults(const char*q){
 unsigned slot=nextSlot;buildResults(q,slot);if(!output.okay){error("Compendium: search result layout exceeds its safe buffer.");return false;}
 if(opened>=0){panels->closePanel(ctx,panelHandles[opened]);opened=-1;}


 if(seen[slot]){
  Widgets::UiAction unload{.structSize=Widgets::UiActionSize,.target="PanelManager",.command="UnloadPanel",.text=fullNames[slot]};
  if(w->dispatchUiAction(ctx,&unload)!=Widgets::Result::Success){error("Compendium: old result view could not be unloaded.");return false;}seen[slot]=false;
 }
 if(resourceHandles[slot]){
  if(resourceService->unregisterResource(ctx,resourceHandles[slot])!=Resources::Result::Success){error("Compendium: old result resource could not be released.");return false;}resourceHandles[slot]=0;
 }
 Resources::ResourceRegistration r{.structSize=Resources::ResourceRegistrationSize,.path=paths[slot],.bytes=output.bytes,.byteCount=output.used};
 if(resourceService->registerResource(ctx,&r,&resourceHandles[slot])!=Resources::Result::Success){error("Compendium: result layout could not be registered.");return false;}
 if(panels->openPanel(ctx,panelHandles[slot])!=Panels::Result::Success){error("Compendium: result list could not be opened.");return false;}
 opened=int(slot);seen[slot]=true;nextSlot=1-slot;return true;
}
static void executeSearch(const PluginContext*,void* data) noexcept{
 arm(-1);unsigned index=unsigned(reinterpret_cast<uintptr_t>(data));if(index>=sizeof(pages)/sizeof(pages[0]))return;
 const auto&p=pages[index];Widgets::WidgetHandle root=0;w->findPanel(ctx,eq(p.name,"r3miscmaps")&&eq(activeMisc,"r3misc")?"r3misc":p.name,&root);
 if(!root&&eq(p.name,"r3miscmaps"))w->findPanel(ctx,"r3misc",&root);
 auto input=find(root,"SearchInput");char q[384]{};uint32_t required=0;
 if(!input||w->getInputText(ctx,input,q,sizeof(q),&required)!=Widgets::Result::Success){error("Compendium: the native search input could not be read.");return;}
 if(*q)showResults(q);
}
static bool misc(const char*s){return eq(s,"r3misc")||eq(s,"r3miscmaps")||eq(s,"r3miscmaterials")||eq(s,"r3miscscrolls")||eq(s,"r3miscdyes");}
static bool catalogPanel(const char*s){
 if(misc(s)||eq(s,"runewords")||eq(s,"r3runewords")||eq(s,"r3compendium")||eq(s,"r3allitems")||eq(s,"r3uniques")||eq(s,"r3sets")||eq(s,"r3charms")||eq(s,"r3weapons"))return true;
 for(const auto&p:pages)if(eq(s,p.name))return true;
 return false;
}
static SharedEvents::UiMessageAction message(const PluginContext*,const SharedEvents::UiMessageEvent*e,void*) noexcept{
 if(e&&eq(e->target,"SettingsPanelMessage")&&eq(e->command,"ToggleChildPanel")&&catalogPanel(e->text))arm(-1);
 if(!e||!eq(e->target,"PanelManager"))return SharedEvents::UiMessageAction::Continue;
 if(eq(e->command,"UnloadAll")||eq(e->command,"OpenExclusivePanel"))arm(-1);

 if(eq(e->command,"OpenPanel")&&e->text){
  constexpr char editPrefix[]="r3ni_compendium_edit_";
  if(starts(e->text,editPrefix)){
   arm(-1);
   const char*page=e->text+sizeof(editPrefix)-1;
   for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);++i)if(eq(page,pages[i].name)){
    Widgets::WidgetHandle root=0;w->findPanel(ctx,eq(page,"r3miscmaps")&&eq(activeMisc,"r3misc")?"r3misc":page,&root);
    auto input=find(root,"SearchInput");char text[384]{};uint32_t needed=0;
    if(input&&w->getInputText(ctx,input,text,sizeof(text),&needed)==Widgets::Result::Success&&*text){
     if(ensureKeyboardHook())arm(int(i));
     else error("Compendium: Search keyboard listener could not attach to the game window.");
    }
    break;
   }
   return SharedEvents::UiMessageAction::Consume;
  }


  if(catalogPanel(e->text))arm(-1);
  constexpr char prefix[]="r3ni_compendium_find_";
  if(starts(e->text,prefix)){
   const char* page=e->text+sizeof(prefix)-1;
   for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);++i)if(eq(page,pages[i].name)){
    if(threads->runOnUiThread(ctx,executeSearch,reinterpret_cast<void*>(uintptr_t(i)))!=Threads::Result::Success)error("Compendium: search UI request could not be queued.");break;
   }
   return SharedEvents::UiMessageAction::Consume;
  }
  if(misc(e->text)){
   if(*activeMisc&&!eq(activeMisc,e->text)){
    char closing[32]{};copy(closing,activeMisc,sizeof(closing));activeMisc[0]=0;
    Widgets::UiAction close{.structSize=Widgets::UiActionSize,.target="PanelManager",.command="ClosePanel",.text=closing};w->dispatchUiAction(ctx,&close);
   }
   copy(activeMisc,e->text,sizeof(activeMisc));
  }
 }
 if(eq(e->command,"ClosePanel")){if(catalogPanel(e->text))arm(-1);if(e->text&&eq(e->text,activeMisc))activeMisc[0]=0;}
 return SharedEvents::UiMessageAction::Continue;
}
static constexpr PluginInfo info{.infoSize=PluginInfoSize,.abiVersion=D2RL_PLUGIN_ABI_VERSION,.id="r3ni-compendium",.name="R3ni Item Compendium Search",.version="0.7.5",.author="r3ni",.description="Global name lookup with all matching cards in one scrollable list. UI only.",.flags=PluginFlags::ModScopedOnly|PluginFlags::Client};
D2RL_PLUGIN_EXPORT const PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &info;}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept{
 if(keyboardHook){UnhookWindowsHookEx(keyboardHook);keyboardHook=nullptr;}
 arm(-1);consumedReturn=false;
 if(events&&listener)events->unregisterUiMessageListener(ctx,listener);listener=0;
 if(panels)for(auto &h:panelHandles)if(h){panels->unregisterPanel(ctx,h);h=0;}
 if(resourceService)for(auto &h:resourceHandles)if(h){resourceService->unregisterResource(ctx,h);h=0;}
 opened=-1;activeMisc[0]=0;ctx=nullptr;
}
D2RL_PLUGIN_EXPORT bool D2RLoaderLoadPlugin(const PluginContext*c) noexcept{
 ctx=c;if(!ctx)return false;
 if(ctx->QueryService(&w)!=ServiceQueryResult::Success||ctx->QueryService(&events)!=ServiceQueryResult::Success||ctx->QueryService(&panels)!=ServiceQueryResult::Success||ctx->QueryService(&resourceService)!=ServiceQueryResult::Success||ctx->QueryService(&threads)!=ServiceQueryResult::Success)return false;
 if(!w->findPanel||!w->findWidget||!w->setWidgetVisible||!w->setWidgetEnabled||!w->dispatchUiAction||!w->getInputText||!panels->registerPanel||!panels->unregisterPanel||!panels->openPanel||!panels->closePanel||!resourceService->registerResource||!resourceService->unregisterResource||!events->registerUiMessageListener||!events->unregisterUiMessageListener||!threads->runOnUiThread||!HasWidgetServiceField(w,WidgetServiceRequiredSize)||!HasPanelServiceField(panels,PanelServiceClosePanelFieldEnd)||!HasResourceServiceField(resourceService,ResourceServiceRequiredSize)||!HasThreadServiceField(threads,ThreadServiceRequiredSize)||events->serviceSize<SharedEventServiceRequiredSize)return false;
 for(unsigned i=0;i<2;++i){
  buildResults("",i);
  Resources::ResourceRegistration r{.structSize=Resources::ResourceRegistrationSize,.path=paths[i],.bytes=output.bytes,.byteCount=output.used};
  Panels::PanelRegistration p{.structSize=Panels::PanelRegistrationSize,.flags=Panels::PanelFlags::CloseOnEscape,.localId=localNames[i]};
  if(resourceService->registerResource(ctx,&r,&resourceHandles[i])!=Resources::Result::Success||panels->registerPanel(ctx,&p,&panelHandles[i])!=Panels::Result::Success){D2RLoaderUnloadPlugin();return false;}
 }
 SharedEvents::UiMessageListener l{.structSize=SharedEvents::UiMessageListenerSize,.priority=2000,.callback=message};
 if(events->registerUiMessageListener(ctx,&l,&listener)!=SharedEvents::Result::Success){D2RLoaderUnloadPlugin();return false;}return true;
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)instance=h;return TRUE;}
