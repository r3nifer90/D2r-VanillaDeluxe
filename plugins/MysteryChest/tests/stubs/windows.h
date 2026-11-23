#pragma once
#include <cstdlib>
#include <mutex>
#define __cdecl
#define __fastcall
#define __declspec(x)
using SRWLOCK=std::mutex;
#define SRWLOCK_INIT {}
inline void AcquireSRWLockExclusive(SRWLOCK* x){x->lock();}
inline void ReleaseSRWLockExclusive(SRWLOCK* x){x->unlock();}
inline void* GetProcessHeap(){return nullptr;}
constexpr int HEAP_ZERO_MEMORY=8;
inline void* HeapAlloc(void*,int,size_t size){return std::calloc(1,size);}
inline bool HeapFree(void*,int,void* p){std::free(p);return true;}

#include <cstdint>
#include <cstring>
using SIZE_T=size_t;
struct FILETIME {uint32_t dwLowDateTime,dwHighDateTime;};
inline void* GetCurrentProcess(){return nullptr;}
inline bool ReadProcessMemory(void*,const void* p,void* out,size_t n,SIZE_T* used){if(!p)return false;std::memcpy(out,p,n);*used=n;return true;}
inline void GetSystemTimeAsFileTime(FILETIME* t){uint64_t n=116444736000000000ULL+29920000ULL*600000000ULL;t->dwLowDateTime=uint32_t(n);t->dwHighDateTime=uint32_t(n>>32);}
inline void AcquireSRWLockShared(SRWLOCK* x){x->lock();}
inline void ReleaseSRWLockShared(SRWLOCK* x){x->unlock();}

inline void* mockLootFrame=nullptr;
inline unsigned short CaptureStackBackTrace(unsigned,unsigned count,void** out,unsigned long*) {if(mockLootFrame&&count){out[0]=mockLootFrame;return 1;}return 0;}
