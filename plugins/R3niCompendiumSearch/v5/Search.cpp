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
static Panels::RegistrationHandle panelHandles[sizeof(resources)/sizeof(resources[0])]{};
static Resources::RegistrationHandle resourceHandles[sizeof(resources)/sizeof(resources[0])]{};
static SharedEvents::ListenerHandle listener;
static char previous[sizeof(pages)/sizeof(pages[0])][384]{};
static int last[sizeof(pages)/sizeof(pages[0])]{};
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
static bool present(unsigned index,unsigned slot){
 if(index>=sizeof(resources)/sizeof(resources[0]))return false;
 if(opened>=0){panels->closePanel(ctx,panelHandles[opened]);opened=-1;}
 if(panels->openPanel(ctx,panelHandles[index])!=Panels::Result::Success){error("Compendium: could not open the result viewer.");return false;}
 bool okay=true;
 if(resources[index].cards){
  char name[96]="r3ni-compendium/";copy(name+16,resources[index].local,sizeof(name)-16);
  Widgets::WidgetHandle root=0;
  if(w->findPanel(ctx,name,&root)!=Widgets::Result::Success||!root)okay=false;
  auto view=find(root,"ResultView");if(!view)okay=false;


  for(unsigned i=0;i<resources[index].cards&&okay;++i){
   char n[40]="ResultCard";unsigned v=i,k=10;if(v>=10)n[k++]=char('0'+v/10);n[k++]=char('0'+v%10);n[k]=0;
   auto h=find(view,n);bool selected=i==slot;
   if(!h||w->setWidgetVisible(ctx,h,selected)!=Widgets::Result::Success||w->setWidgetEnabled(ctx,h,selected)!=Widgets::Result::Success)okay=false;
  }
 }
 if(!okay){panels->closePanel(ctx,panelHandles[index]);error("Compendium: result widget initialization failed; viewer closed before drawing.");return false;}
 opened=int(index);return true;
}
static void executeSearch(const PluginContext*,void* data) noexcept{
 unsigned index=unsigned(reinterpret_cast<uintptr_t>(data));if(index>=sizeof(pages)/sizeof(pages[0]))return;
 const auto&p=pages[index];Widgets::WidgetHandle root=0;w->findPanel(ctx,eq(p.name,"r3miscmaps")&&eq(activeMisc,"r3misc")?"r3misc":p.name,&root);
 if(!root&&eq(p.name,"r3miscmaps"))w->findPanel(ctx,"r3misc",&root);
 auto input=find(root,"SearchInput");char q[384]{};uint32_t required=0;
 if(!input||w->getInputText(ctx,input,q,sizeof(q),&required)!=Widgets::Result::Success){error("Compendium: the native search input could not be read.");return;}
 int hit=match(globalPage,q,eq(q,previous[index])?last[index]:-1);last[index]=hit;copy(previous[index],q,sizeof(previous[index]));
 if(!*q)return;
 if(hit<0){present(unsigned(sizeof(resources)/sizeof(resources[0])-1),0);return;}
 unsigned item=globalPage.cards[hit].result;present(item/24,item%24);
}
static bool misc(const char*s){return eq(s,"r3misc")||eq(s,"r3miscmaps")||eq(s,"r3miscmaterials")||eq(s,"r3miscscrolls")||eq(s,"r3miscdyes");}
static SharedEvents::UiMessageAction message(const PluginContext*,const SharedEvents::UiMessageEvent*e,void*) noexcept{
 if(!e||!eq(e->target,"PanelManager"))return SharedEvents::UiMessageAction::Continue;

 if(eq(e->command,"OpenPanel")&&e->text){
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
 if(eq(e->command,"ClosePanel")&&e->text){if(eq(e->text,activeMisc))activeMisc[0]=0;}
 return SharedEvents::UiMessageAction::Continue;
}
static constexpr PluginInfo info{.infoSize=PluginInfoSize,.abiVersion=D2RL_PLUGIN_ABI_VERSION,.id="r3ni-compendium",.name="R3ni Item Compendium Search",.version="0.7.2",.author="r3ni",.description="Global name lookup from every tab with isolated result viewers. UI only.",.flags=PluginFlags::ModScopedOnly|PluginFlags::Client};
D2RL_PLUGIN_EXPORT const PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &info;}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept{
 if(events&&listener)events->unregisterUiMessageListener(ctx,listener);listener=0;
 if(panels)for(auto &h:panelHandles)if(h){panels->unregisterPanel(ctx,h);h=0;}
 if(resourceService)for(auto &h:resourceHandles)if(h){resourceService->unregisterResource(ctx,h);h=0;}
 opened=-1;activeMisc[0]=0;ctx=nullptr;
}
D2RL_PLUGIN_EXPORT bool D2RLoaderLoadPlugin(const PluginContext*c) noexcept{
 ctx=c;if(!ctx)return false;
 if(ctx->QueryService(&w)!=ServiceQueryResult::Success||ctx->QueryService(&events)!=ServiceQueryResult::Success||ctx->QueryService(&panels)!=ServiceQueryResult::Success||ctx->QueryService(&resourceService)!=ServiceQueryResult::Success||ctx->QueryService(&threads)!=ServiceQueryResult::Success)return false;
 if(!w->findPanel||!w->findWidget||!w->setWidgetVisible||!w->setWidgetEnabled||!w->dispatchUiAction||!w->getInputText||!panels->registerPanel||!panels->unregisterPanel||!panels->openPanel||!panels->closePanel||!resourceService->registerResource||!resourceService->unregisterResource||!events->registerUiMessageListener||!events->unregisterUiMessageListener||!threads->runOnUiThread||!HasWidgetServiceField(w,WidgetServiceRequiredSize)||!HasPanelServiceField(panels,PanelServiceClosePanelFieldEnd)||!HasResourceServiceField(resourceService,ResourceServiceRequiredSize)||!HasThreadServiceField(threads,ThreadServiceRequiredSize)||events->serviceSize<SharedEventServiceRequiredSize)return false;
 for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);++i)last[i]=-1;
 for(unsigned i=0;i<sizeof(resources)/sizeof(resources[0]);++i){
  Resources::ResourceRegistration r{.structSize=Resources::ResourceRegistrationSize,.path=resources[i].path,.bytes=resources[i].json,.byteCount=resources[i].bytes};
  Panels::PanelRegistration p{.structSize=Panels::PanelRegistrationSize,.flags=Panels::PanelFlags::CloseOnEscape,.localId=resources[i].local};
  if(resourceService->registerResource(ctx,&r,&resourceHandles[i])!=Resources::Result::Success||panels->registerPanel(ctx,&p,&panelHandles[i])!=Panels::Result::Success){D2RLoaderUnloadPlugin();return false;}
 }
 SharedEvents::UiMessageListener l{.structSize=SharedEvents::UiMessageListenerSize,.priority=2000,.callback=message};
 if(events->registerUiMessageListener(ctx,&l,&listener)!=SharedEvents::Result::Success){D2RLoaderUnloadPlugin();return false;}return true;
}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
