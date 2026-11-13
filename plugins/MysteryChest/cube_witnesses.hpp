#pragma once
struct CubeWitness {uintptr_t rva;const unsigned char* data;unsigned size;};
inline constexpr unsigned char TransmuteWitness[]={76,139,220,85,83,72,141,108,36,136,72,129,236,120,1,0,0};
inline constexpr unsigned char CubeWitness1[]={72,185,152,201,127,16,95,96,12,250,72,59,193,116,40,72};
inline constexpr unsigned char CubeWitness2[]={232,33,240,255,255};
inline constexpr unsigned char CubeWitness3[]={233,205,6,0,0};
inline constexpr unsigned char CubeWitness4[]={72,185,152,201,127,16,95,96,12,250,72,59,193,116,22,72,141,76,36,48,72,137,84,36,48,232,75,227,255,255,132};
inline constexpr CubeWitness CubeWitnesses[]={{0x2cd720,TransmuteWitness,17},{0x2cd003,CubeWitness1,16},{0x2cd03a,CubeWitness2,5},{0x2cd04e,CubeWitness3,5},{0x23c217,CubeWitness4,31}};
