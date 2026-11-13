#pragma once
namespace TLData {
constexpr uint32_t FirstMon=1779, Variants=16, TotalMon=2099, FirstSkill=437, TotalSkill=497, FirstSU=100, TotalSU=120, Count=20;
struct Profile {const char* key;const wchar_t* section;const char* name;const char* source;const char* message;uint32_t sourceClass;uint16_t areas[16];uint8_t kinds[3];uint16_t skills[3];bool refresh[3];};
constexpr Profile Profiles[]={
{"izual",L"izual","Veyrath, the Fallen Seraph","izual","Veyrath breaks his oath; a fallen seraph descends into hatred.",256,{146,150,151,153,154,155,156,159,161,162,163,164},{4,7,3},{437,438,439},{false,true,false}},
{"nihlathak",L"nihlathak","Mordrath, Warden of Bone","nihlathakboss","Mordrath stirs beneath the monastery; the dead will not rest.",526,{146,150,151,153,154,155,156,159,161,162,163,164},{1,0,0},{440,441,442},{true,false,false}},
{"blood_raven",L"blood_raven","Vespara, the Crimson Huntress","bloodraven","Vespara draws her bow; the crimson hunt has begun.",267,{146,150,151,153,154,155,156,159,161,162,163,164},{0,5,0},{443,444,445},{false,true,false}},
{"griswold",L"griswold","Gorath, the Forsaken Smith","griswold","Gorath rises from the dust; his hammer remembers every betrayal.",365,{146,150,151,153,154,155,156,159,161,162,163,164},{4,3,1},{446,447,448},{false,false,false}},
{"smith",L"smith","Kharvok, the Ashen Butcher","smith","Kharvok leaves the furnace; the ashes demand fresh blood.",402,{146,150,151,153,154,155,156,159,161,162,163,164},{4,4,1},{449,450,451},{true,true,false}},
{"radament",L"radament","Azharak, Keeper of the Dead","radament","Azharak opens the sealed tomb; a thousand bones answer his call.",229,{146,150,151,153,154,155,156,159,161,162,163,164},{6,6,0},{452,453,454},{true,true,false}},
{"fire_eye",L"fire_eye","Zharun, the Venomblade","sandraider3","Zharun slips through the jungle; venom gleams upon his blades.",31,{146,150,151,153,154,155,156,159,161,162,163,164},{1,1,1},{455,456,457},{true,true,true}},
{"summoner",L"summoner","Malachar, the Starless Seer","summoner","Malachar tears open the starless sky; ruin follows his prophecy.",250,{146,150,151,153,154,155,156,159,161,162,163,164},{1,5,5},{458,459,460},{true,true,true}},
{"stormtree",L"stormtree","Thornagar, the Withered Heart","thornhulk3","Thornagar wakes among the withered roots; the storm has found a heart.",129,{146,150,151,153,154,155,156,159,161,162,163,164},{1,0,5},{461,462,463},{true,false,true}},
{"lord_de_seis",L"lord_de_seis","Vaelthos, the Riftbinder","doomknight3","Vaelthos unseals the rift; flame and miasma cross the veil.",312,{146,150,151,153,154,155,156,159,161,162,163,164},{0,0,0},{464,465,466},{false,false,false}},
{"hephasto",L"hephasto","Vorgath, the Infernal Forger","hephasto","Vorgath strikes the infernal anvil; burning chains reach for Sanctuary.",409,{146,150,151,153,154,155,156,159,161,162,163,164},{0,5,0},{467,468,469},{false,true,false}},
{"infector_of_souls",L"infector_of_souls","Nharzul, the Plague Maw","megademon3","Nharzul exhales beneath Kurast; the air itself begins to rot.",362,{146,150,151,153,154,155,156,159,161,162,163,164},{1,2,0},{470,471,472},{false,false,false}},
{"shenk",L"shenk","Kargor, the Siege Tyrant","overseer1","Kargor raises the siege standard; his hammers will shatter your refuge.",479,{146,150,151,153,154,155,156,159,161,162,163,164},{7,0,5},{473,474,475},{true,false,true}},
{"sharptooth_slayer",L"sharptooth_slayer","Drazhul, the Ashen Shepherd","overseer3","Drazhul calls his flock from the embers; the mountain will burn.",481,{146,150,151,153,154,155,156,159,161,162,163,164},{5,7,1},{476,477,478},{true,true,true}},
{"frozenstein",L"frozenstein","Hrimgor, the Frozen Colossus","snowyeti4","Hrimgor breaks the ancient ice; winter has taken flesh.",449,{146,150,151,153,154,155,156,159,161,162,163,164},{1,0,0},{479,480,481},{true,false,false}},
{"thresh_socket",L"thresh_socket","Kharzug, the Molten Breaker","siegebeast3","Kharzug shakes the frozen earth; molten blood rises from its wounds.",443,{146,150,151,153,154,155,156,159,161,162,163,164},{0,0,0},{482,483,484},{false,false,false}},
{"pindleskin",L"pindleskin","Valdrak, the Oathless Revenant","reanimatedhorde5","Valdrak returns without an oath; black wings mark the road to vengeance.",440,{146,150,151,153,154,155,156,159,161,162,163,164},{5,3,4},{485,486,487},{true,false,false}},
{"snapchip_shatter",L"snapchip_shatter","Skarnath, the Rimeborn","frozenhorror1","Skarnath gathers the killing frost; every breath becomes a blade.",501,{146,150,151,153,154,155,156,159,161,162,163,164},{1,0,0},{488,489,490},{true,false,false}},
{"lister_the_tormentor",L"lister_the_tormentor","Rhazgor, the Tempest Maw","baalminion1","Rhazgor howls before the Worldstone; the tempest hungers for the living.",571,{146,150,151,153,154,155,156,159,161,162,163,164},{1,0,5},{491,492,493},{true,false,true}},
{"countess",L"countess","Velisara, the Blade Matron","corruptrogue3","Velisara claims the ruined cloisters; hidden blades await your blood.",45,{146,150,151,153,154,155,156,159,161,162,163,164},{0,1,0},{494,495,496},{false,true,false}},
};
}
