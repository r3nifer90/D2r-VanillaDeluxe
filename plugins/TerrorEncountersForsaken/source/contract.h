#pragma once


namespace CF {bool Contract() {
 if(!Readable(base,0x1000)||At<uint16_t>(0)!=0x5a4d)return false;
 uint32_t pe=At<uint32_t>(0x3c);if(pe>0x800||At<uint32_t>(pe)!=0x4550||At<uint16_t>(pe+4)!=0x8664)return false;
 if(!Readable(base+0x1000,0x45ec6))return false;
 uint64_t hash=14695981039346656037ULL;auto*p=(const uint8_t*)(base+0x1000);
 for(uint32_t n=0;n<0x45ec6;++n)hash=(hash^p[n])*1099511628211ULL;
 return hash==0xe52e15365e9b2fc7ULL;
}}
