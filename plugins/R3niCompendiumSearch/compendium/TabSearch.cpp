#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <D2RLPlugin/api.h>
#include "tab_catalog.h"
extern "C" void* memset(void* p,int v,size_t n){auto b=(volatile unsigned char*)p;while(n--)*b++=(unsigned char)v;return p;}
using namespace D2RL;
static const PluginContext* ctx;
static const WidgetService* w;
static const SharedEventService* events;
static SharedEvents::ListenerHandle listener;
static char queries[68][384]={};
static int last[68]={};
static bool eq(const char*a,const char*b){if(!a||!b)return false;while(*a&&*a==*b){++a;++b;}return *a==*b;}
static bool prefix(const char*a,const char*b){if(!a||!b)return false;while(*b)if(*a++!=*b++)return false;return true;}
static char lower(char c){return c>='A'&&c<='Z'?char(c+32):c;}
static bool contains(const char*a,const char*b){if(!*b)return false;for(;*a;++a){unsigned i=0;while(b[i]&&a[i]&&lower(a[i])==lower(b[i]))++i;if(!b[i])return true;}return false;}
static int match(const PageSpec&p,const char*q,int after){if(!*q)return -1;for(unsigned n=0;n<p.count;++n){int i=(after+1+n)%p.count;if(contains(p.cards[i].en,q)||contains(p.cards[i].pl,q))return i;}return -1;}
static Widgets::WidgetHandle find(Widgets::WidgetHandle p,const char*n){Widgets::WidgetHandle h=0;if(p)w->findWidget(ctx,p,n,&h);return h;}
static Widgets::WidgetHandle root(const char* name){Widgets::WidgetHandle h=0;if(prefix(name,"r3misc")){w->findPanel(ctx,"r3misc",&h);return find(h,name);}w->findPanel(ctx,name,&h);return h;}
static void visible(Widgets::WidgetHandle p,const char*n,bool v){auto h=find(p,n);if(h){w->setWidgetVisible(ctx,h,v);w->setWidgetEnabled(ctx,h,v);}}
static void copy(char*out,const char*in){unsigned n=0;while(n<383&&in[n]){out[n]=in[n];++n;}out[n]=0;}
static void search(unsigned page){
 const auto&p=pages[page];auto r=root(p.name);auto input=find(r,"SearchInput");char q[384]{};uint32_t needed=0;
 if(!input||w->getInputText(ctx,input,q,sizeof(q),&needed)!=Widgets::Result::Success)return;
 int selected=match(p,q,eq(queries[page],q)?last[page]:-1);last[page]=selected;copy(queries[page],q);
 visible(r,"ScrollView",!*q);visible(r,"ScrollBarBackground",!*q);
 visible(r,"SearchResults",*q&&selected>=0);visible(r,"SearchScrollBarBackground",*q&&selected>=0);visible(r,"SearchNoMatch",*q&&selected<0);
 auto results=find(r,"SearchResults");
 for(unsigned i=0;i<p.count;++i){char name[256]="Found_";unsigned k=6;for(unsigned c=0;p.cards[i].name[c]&&k<255;++c)name[k++]=p.cards[i].name[c];name[k]=0;visible(results,name,int(i)==selected);}
}
static SharedEvents::UiMessageAction message(const PluginContext*,const SharedEvents::UiMessageEvent*e,void*) noexcept{
 if(!e||!eq(e->target,"PanelManager"))return SharedEvents::UiMessageAction::Continue;
 if(eq(e->command,"CompendiumTab")&&e->text){
  Widgets::WidgetHandle panel=0;w->findPanel(ctx,"r3misc",&panel);
  const char* names[]={"maps","materials","scrolls","dyes"};const char* pn[]={"r3miscmaps","r3miscmaterials","r3miscscrolls","r3miscdyes"};
  for(unsigned i=0;i<4;++i)visible(panel,pn[i],eq(e->text,names[i]));
  return SharedEvents::UiMessageAction::Consume;
 }
 if(eq(e->command,"CompendiumFind")&&e->text){for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);++i)if(eq(pages[i].name,e->text)){search(i);break;}return SharedEvents::UiMessageAction::Consume;}
 return SharedEvents::UiMessageAction::Continue;
}
static constexpr PluginInfo info{.infoSize=PluginInfoSize,.abiVersion=D2RL_PLUGIN_ABI_VERSION,.id="r3ni-compendium",.name="R3ni Item Compendium Search",.version="0.7.0",.author="r3ni",.description="Per-tab name lookup. Compatible with the legacy Widget service. UI only.",.flags=PluginFlags::ModScopedOnly|PluginFlags::Client};
D2RL_PLUGIN_EXPORT const PluginInfo* D2RLoaderGetPluginInfo() noexcept{return &info;}
D2RL_PLUGIN_EXPORT bool D2RLoaderLoadPlugin(const PluginContext*c) noexcept{
 ctx=c;if(!ctx)return false;
 if(ctx->QueryService(&w)!=ServiceQueryResult::Success||ctx->QueryService(&events)!=ServiceQueryResult::Success)return false;
 if(!HasWidgetServiceField(w,WidgetServiceRequiredSize)||!w->findPanel||!w->findWidget||!w->getInputText||!w->setWidgetVisible||!w->setWidgetEnabled||!events||events->serviceSize<SharedEventServiceRequiredSize||!events->registerUiMessageListener||!events->unregisterUiMessageListener)return false;
 for(unsigned i=0;i<68;++i)last[i]=-1;
 SharedEvents::UiMessageListener l{.structSize=SharedEvents::UiMessageListenerSize,.priority=2000,.callback=message};return events->registerUiMessageListener(ctx,&l,&listener)==SharedEvents::Result::Success;
}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept{if(events&&listener)events->unregisterUiMessageListener(ctx,listener);listener=0;ctx=nullptr;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
