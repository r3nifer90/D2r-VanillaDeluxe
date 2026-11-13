#pragma once


struct Ref {uint32_t slot;const char* name;};
struct Plan {const char* name;uint32_t count;uint32_t weights[10];uint32_t refs;Ref ref[2];};
constexpr Plan plans[2][3]={
{
{"R3C Unique",2,{1262,12},2,{{0,"R3EG Legacy Unique"},{1,"R3EG Mid"}}},
{"R3C Uniqueg999_25",10,{5,1,1,3,1,1,7,1,5,5},1,{{6,"R3EG Endgame"}}},
{"R3C Uniqueg999_71",10,{7,1,5,5,5,5,5,5,6,5},1,{{0,"R3EG Endgame Boss"}}},
},
{
{"R3C Unique",2,{1260,12},2,{{0,"R3EG Legacy Unique"},{1,"R3EG Mid"}}},
{"R3C Uniqueg997_25",10,{5,1,1,3,1,1,7,1,5,5},1,{{6,"R3EG Endgame"}}},
{"R3C Uniqueg997_70",10,{1,1,1,5,1,1,3,1,1,7},1,{{9,"R3EG Endgame Boss"}}},
},
};
