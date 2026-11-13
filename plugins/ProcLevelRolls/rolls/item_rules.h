#pragma once


namespace PLR {
struct ItemRule {const char* name; const char* alias; int32_t skill,stat; Range level,chance;};
inline constexpr ItemRule ItemRules[]={
    {"R3EHU001","R3EHU001",240,198,{35,35},{12,12}},
    {"R3EPU001","R3MCU1846",240,195,{40,50},{8,8}},
    {"R3EPU002","R3MCU1847",398,198,{40,50},{8,8}},
    {"R3EPU003","R3MCU1848",394,201,{30,40},{15,15}},
    {"R3EPU004","R3MCU1849",385,198,{20,20},{3,5}},
    {"R3EPU005","R3MCU1850",247,195,{30,30},{4,4}},
    {"R3EPU006","R3MCU1851",387,201,{35,40},{5,5}},
    {"terror-charms:Yellow","R3MCU0514",53,195,{40,50},{10,10}},
    {"terror-charms:Red","R3MCU0515",47,195,{40,50},{10,10}},
    {"terror-charms:Blue","R3MCU0516",64,195,{40,50},{10,10}},
    {"terror-charms:Green","R3MCU0517",92,195,{40,50},{10,10}},
    {"terror-charms:White","R3MCU0518",84,195,{40,50},{10,10}},
    {"terror-charms:Brown","R3MCU0520",247,195,{40,50},{5,5}},
    {"terror-charms:Apocalypse","R3MCU0522",401,195,{40,50},{10,10}},
    {"R3DSU008","R3MCU1461",244,195,{30,30},{10,10}},
    {"R3NSU034","R3MCU1531",68,201,{30,30},{4,4}},
    {"R3NSU038","R3MCU1535",68,201,{30,30},{2,2}},
    {"R3NSU043","R3MCU1540",68,201,{30,30},{2,2}},
    {"R3SRU009","R3SRU009",101,198,{35,45},{15,15}},
    {"R3SRU009","R3SRU009",121,198,{30,35},{4,8}},
    {"R3SRU010","R3SRU010",387,201,{35,35},{4,4}},
    {"R3SRU011","R3SRU011",385,198,{15,20},{1,2}},
    {"R3SRU012","R3SRU012",277,195,{25,30},{4,6}},
    {"R3SRU012","R3SRU012",264,201,{25,30},{2,4}},
};
}
