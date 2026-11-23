#include "../chronicle_fix.h"
#include <cassert>
#include <iostream>
using namespace ChronicleIntegration;
static uint8_t row[0x15c]{};
static int record=0,lookups=0;
static void* __fastcall getter(uint8_t,int) {++lookups;return row;}
static int __fastcall generate(void*,void*,void*) {return GetUniqueRecord(3,record)?1:0;}
int main(){
 D2RL::PluginContext ctx{};ctx.exeBase=0x140000000;Context=&ctx;Ready=true;OriginalUnique=generate;OriginalGetUniqueRecord=getter;
 uint8_t game[0x108]{};game[0x106]=3;int item=1;mockLootFrame=reinterpret_cast<void*>(ctx.exeBase+0x440aaa);
 size_t checked=0;
 for(auto id:SelectedLoot::Records){record=id;mockRandom=0;assert(Unique(game,&item,nullptr)==0);mockRandom=1;assert(Unique(game,&item,nullptr)==1);++checked;}
 record=SelectedLoot::Records[0];
 Capture capture;Begin(capture);mockRandom=0;assert(Unique(game,&item,nullptr)==1&&mockRandom==0);End();
 mockLootFrame=nullptr;mockRandom=0;assert(Unique(game,&item,nullptr)==1&&mockRandom==0);
 mockLootFrame=reinterpret_cast<void*>(ctx.exeBase+0x440aaa);record=0;assert(!SelectedLoot::targeted(record));mockRandom=0;assert(Unique(game,&item,nullptr)==1&&mockRandom==0);
 record=SelectedLoot::Records[0];mockRngFails=true;assert(Unique(game,&item,nullptr)==1);mockRngFails=false;
 assert(!LootScope&&!LootDecisionMade&&UniqueItem==nullptr);
 assert(!SelectedLoot::nativeLootFrame(ctx.exeBase+0x43cd50,ctx.exeBase));assert(!SelectedLoot::nativeLootFrame(ctx.exeBase+0x443910,ctx.exeBase));
 assert(SelectedLoot::nativeLootFrame(ctx.exeBase+0x441500,ctx.exeBase));assert(!SelectedLoot::nativeLootFrame(ctx.exeBase+0x441c00,ctx.exeBase));
 unsigned rejected=0;for(unsigned i=0;i<1000000;++i) rejected+=SelectedLoot::reject(true,false,true,record,true,i);
 assert(rejected==500000);
 std::cout<<"PASS: "<<checked<<" selected identities, both 50% outcomes, Mystery Chest bypass, non-loot bypass, non-target bypass, RNG fail-open, scope restoration, native frame bounds and 1000000 deterministic draws.\n";
}
