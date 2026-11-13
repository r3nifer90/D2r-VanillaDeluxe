#pragma once
#include <tlhelp32.h>
#include <cstddef>
#include "combat_observer_api.h"



namespace SPD {
R3niDiagnostics::Sink externalSink=nullptr;HMODULE observerModule=nullptr;
constexpr uint32_t Capacity=32768,HeaderBytes=4096,RecordBytes=128;
constexpr uint64_t FileBytes=HeaderBytes+uint64_t(Capacity)*RecordBytes;
enum Kind:uint16_t {Skill=1,Create=2,MissileDo=3,MissileHit=4,Report=5,Dispatch=6,Filter=7,Effect=8,Session=9,Exception=10};
enum Phase:uint16_t {Begin=1,NativeBegin=2,NativeEnd=3,End=4,Decision=5};
struct Record {
 volatile LONG64 commit=0;
 uint64_t tick=0,call=0,parent=0,game=0,actor=0,target=0,data=0,value=0,extra=0;
 uint32_t thread=0;uint16_t kind=0,phase=0;
 uint32_t actorType=0xffffffff,actorClass=0,actorId=0,targetType=0xffffffff,targetClass=0,targetId=0,skill=0,level=0,flags=0,frame=0;
};
struct Header {
 char magic[8];uint32_t version,headerBytes,recordBytes,capacity,pid,pointerBytes;
 uint64_t exeBase,coreBase,dllBase,startedFileTime;
 volatile LONG64 issued,dropped,calls;
};
static_assert(sizeof(Record)==128);static_assert(offsetof(Header,issued)==64);
HMODULE module=nullptr;HANDLE file=INVALID_HANDLE_VALUE,mapping=nullptr,log=INVALID_HANDLE_VALUE;
Header* header=nullptr;Record* ring=nullptr;volatile LONG locks[Capacity]={};
DWORD tls=TLS_OUT_OF_INDEXES;void* handler=nullptr;
volatile LONG exceptionBusy=0,exceptions=0;volatile LONG64 fallbackCalls=0;
struct Text {
 char* p;size_t n=0,cap;
 void Add(const char* s) noexcept {while(*s&&n+1<cap)p[n++]=*s++;p[n]=0;}
 void Num(uint64_t v) noexcept {char b[24];uint32_t n=0;do{b[n++]=char('0'+v%10);v/=10;}while(v);while(n){char c[]={b[--n],0};Add(c);}}
 void Hex(uint64_t v) noexcept {Add("0x");const char* d="0123456789ABCDEF";for(int n=60;n>=0;n-=4){char c[]={d[(v>>n)&15],0};Add(c);}}
};
void Line(const char* s) noexcept {
 if(log==INVALID_HANDLE_VALUE)return;DWORD n=0,w=0;while(s[n])++n;
 WriteFile(log,s,n,&w,nullptr);WriteFile(log,"\r\n",2,&w,nullptr);
}
struct PreserveError {DWORD value=GetLastError();~PreserveError() noexcept {SetLastError(value);}};
bool Copy(uintptr_t p,void* out,size_t n) noexcept {PreserveError preserve;SIZE_T got=0;return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(p),out,n,&got)&&got==n;}
void Identity(void* p,uint32_t& type,uint32_t& kind,uint32_t& id) noexcept {
 uint32_t v[3]={};if(Copy(reinterpret_cast<uintptr_t>(p),v,sizeof(v))){type=v[0];kind=v[1];id=v[2];}
}
void Write(Record r,uint16_t phase) noexcept {
 r.tick=GetTickCount64();r.thread=GetCurrentThreadId();r.phase=phase;
 if(externalSink&&r.kind>=Skill&&r.kind<=Effect)externalSink(&r,sizeof(r),phase);
 if(!header||!ring)return;
 const LONG64 ticket=InterlockedIncrement64(&header->issued);const uint32_t i=uint32_t(ticket-1)&(Capacity-1);
 if(InterlockedCompareExchange(&locks[i],1,0)){InterlockedIncrement64(&header->dropped);return;}
 r.tick=GetTickCount64();r.thread=GetCurrentThreadId();r.phase=phase;
 InterlockedExchange64(&ring[i].commit,0);
 memcpy(reinterpret_cast<uint8_t*>(ring+i)+8,reinterpret_cast<const uint8_t*>(&r)+8,sizeof(r)-8);
 InterlockedExchange64(&ring[i].commit,ticket);InterlockedExchange(&locks[i],0);
}
struct Span {
 Record record;Span* previous=nullptr;
 Span(uint16_t kind,void* game=nullptr,void* actor=nullptr,void* target=nullptr,void* data=nullptr,uint32_t skill=0,uint32_t level=0,uint64_t extra=0) noexcept {
  PreserveError preserve;previous=tls==TLS_OUT_OF_INDEXES?nullptr:static_cast<Span*>(TlsGetValue(tls));
  record.call=header?InterlockedIncrement64(&header->calls):InterlockedIncrement64(&fallbackCalls);
  uint64_t parent=0;if(previous)Copy(reinterpret_cast<uintptr_t>(previous)+offsetof(Record,call),&parent,sizeof(parent));
  record.parent=parent;record.thread=GetCurrentThreadId();record.kind=kind;record.game=reinterpret_cast<uintptr_t>(game);record.actor=reinterpret_cast<uintptr_t>(actor);record.target=reinterpret_cast<uintptr_t>(target);record.data=reinterpret_cast<uintptr_t>(data);record.skill=skill;record.level=level;record.extra=extra;

  Identity(actor,record.actorType,record.actorClass,record.actorId);Identity(target,record.targetType,record.targetClass,record.targetId);
  if(tls!=TLS_OUT_OF_INDEXES)TlsSetValue(tls,this);Write(record,Begin);
 }
 void Mark(uint16_t phase,uint64_t value=0) noexcept {PreserveError preserve;record.value=value;Write(record,phase);}
 ~Span() noexcept {PreserveError preserve;Write(record,End);if(tls!=TLS_OUT_OF_INDEXES)TlsSetValue(tls,previous);}
};
bool Path(wchar_t* out,const wchar_t* name) noexcept {
 const DWORD n=GetModuleFileNameW(module,out,2048);if(!n||n>=2048)return false;
 DWORD end=n;while(end&&out[end-1]!=L'\\'&&out[end-1]!=L'/')--end;
 for(DWORD i=0;;++i){if(end+i>=2048)return false;out[end+i]=name[i];if(!name[i])return true;}
}
void Rotate(const wchar_t* name,const wchar_t* old) noexcept {wchar_t a[2048]={},b[2048]={};if(Path(a,name)&&Path(b,old))MoveFileExW(a,b,MOVEFILE_REPLACE_EXISTING);}
void Modules() noexcept {
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());if(snapshot==INVALID_HANDLE_VALUE){Line("MODULE_LIST_UNAVAILABLE");return;}
 MODULEENTRY32W e={};e.dwSize=sizeof(e);
 if(Module32FirstW(snapshot,&e))do{char storage[768]={};Text t{storage,0,sizeof(storage)};t.Add("MODULE base=");t.Hex(reinterpret_cast<uintptr_t>(e.modBaseAddr));t.Add(" size=");t.Hex(e.modBaseSize);t.Add(" name=");for(uint32_t i=0;e.szModule[i];++i){char c[]={e.szModule[i]<128?char(e.szModule[i]):'?',0};t.Add(c);}Line(t.p);}while(Module32NextW(snapshot,&e));
 CloseHandle(snapshot);
}
char exceptionBuffer[16384]={};
LONG CALLBACK OnException(EXCEPTION_POINTERS* e) noexcept {
 if(!e||!e->ExceptionRecord||!e->ContextRecord)return EXCEPTION_CONTINUE_SEARCH;
 const DWORD c=e->ExceptionRecord->ExceptionCode;
 if(c!=0xc0000005&&c!=0xc0000006&&c!=0xc000001d&&c!=0xc0000094&&c!=0xc0000095&&c!=0xc00000fd&&c!=0xc0000409&&c!=0xc0000374)return EXCEPTION_CONTINUE_SEARCH;
 PreserveError preserve;if(InterlockedCompareExchange(&exceptionBusy,1,0))return EXCEPTION_CONTINUE_SEARCH;
 const LONG n=InterlockedIncrement(&exceptions);
 if(n<=64){
  auto& x=*e->ContextRecord;Text t{exceptionBuffer,0,sizeof(exceptionBuffer)};
  t.Add("FIRST_CHANCE index=");t.Num(n);t.Add(" code=");t.Hex(c);t.Add(" tick=");t.Num(GetTickCount64());t.Add(" thread=");t.Num(GetCurrentThreadId());t.Add(" RIP=");t.Hex(x.Rip);t.Add(" RSP=");t.Hex(x.Rsp);t.Add(" RBP=");t.Hex(x.Rbp);
  t.Add(" RAX=");t.Hex(x.Rax);t.Add(" RBX=");t.Hex(x.Rbx);t.Add(" RCX=");t.Hex(x.Rcx);t.Add(" RDX=");t.Hex(x.Rdx);t.Add(" RSI=");t.Hex(x.Rsi);t.Add(" RDI=");t.Hex(x.Rdi);t.Add(" R8=");t.Hex(x.R8);t.Add(" R9=");t.Hex(x.R9);t.Add(" R10=");t.Hex(x.R10);t.Add(" R11=");t.Hex(x.R11);t.Add(" R12=");t.Hex(x.R12);t.Add(" R13=");t.Hex(x.R13);t.Add(" R14=");t.Hex(x.R14);t.Add(" R15=");t.Hex(x.R15);
  Span* span=tls==TLS_OUT_OF_INDEXES?nullptr:static_cast<Span*>(TlsGetValue(tls));Record active;
  const bool hasActive=span&&Copy(reinterpret_cast<uintptr_t>(span),&active,sizeof(active))&&active.thread==GetCurrentThreadId()&&active.call&&active.kind>=Skill&&active.kind<=Exception;
  t.Add(" active_call=");t.Num(hasActive?active.call:0);t.Add(" active_kind=");t.Num(hasActive?active.kind:0);t.Add(" trace_issued=");t.Num(header?header->issued:0);
  for(DWORD i=0;i<e->ExceptionRecord->NumberParameters&&i<15;++i){t.Add(" param");t.Num(i);t.Add("=");t.Hex(e->ExceptionRecord->ExceptionInformation[i]);}
  t.Add("\r\nNOTE first_chance=1 may_be_handled_by_other_code=1 handler_always_continues_search=1");
  uint64_t stack[64]={};if(Copy(x.Rsp,stack,sizeof(stack))){t.Add("\r\nSTACK_QWORDS");for(uint32_t i=0;i<64;++i){t.Add(" ");t.Hex(stack[i]);}}
  uint8_t code[128]={};const uint64_t address=x.Rip>=32?x.Rip-32:x.Rip;
  if(Copy(address,code,sizeof(code))){t.Add("\r\nCODE address=");t.Hex(address);t.Add(" bytes=");const char* d="0123456789ABCDEF";for(uint8_t b:code){char h[]={d[b>>4],d[b&15],0};t.Add(h);}}
  Line(t.p);if(log!=INVALID_HANDLE_VALUE)FlushFileBuffers(log);
  Record r;r.kind=Exception;r.call=hasActive?active.call:0;r.value=c;r.actor=x.Rip;r.target=x.Rsp;r.extra=GetCurrentThreadId();Write(r,Decision);
  if(header)FlushViewOfFile(header,0);
 }
 InterlockedExchange(&exceptionBusy,0);return EXCEPTION_CONTINUE_SEARCH;
}
void Initialize(uintptr_t exe) noexcept {
 Rotate(L"R3niSpellProcs.diag.log",L"R3niSpellProcs.diag.previous.log");Rotate(L"R3niSpellProcs.trace.bin",L"R3niSpellProcs.trace.previous.bin");
 wchar_t p[2048]={};
 if(Path(p,L"R3niSpellProcs.diag.log"))log=CreateFileW(p,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 Line("SPELL_PROCS_DIAGNOSTICS version=0.1.7-proc-audit ring_schema=1 record_bytes=128 capacity=32768 filter_data=callback filter_flags=reject_reason");
 tls=TlsAlloc();
 if(Path(p,L"R3niSpellProcs.trace.bin"))file=CreateFileW(p,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file!=INVALID_HANDLE_VALUE)mapping=CreateFileMappingW(file,nullptr,PAGE_READWRITE,0,DWORD(FileBytes),nullptr);
 if(mapping)header=static_cast<Header*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(FileBytes)));
 if(header){memset(header,0,HeaderBytes);memcpy(header->magic,"R3NISPT1",8);header->version=1;header->headerBytes=HeaderBytes;header->recordBytes=RecordBytes;header->capacity=Capacity;header->pid=GetCurrentProcessId();header->pointerBytes=8;header->exeBase=exe;header->dllBase=reinterpret_cast<uintptr_t>(module);FILETIME ft={};GetSystemTimeAsFileTime(&ft);header->startedFileTime=(uint64_t(ft.dwHighDateTime)<<32)|ft.dwLowDateTime;ring=reinterpret_cast<Record*>(reinterpret_cast<uint8_t*>(header)+HeaderBytes);Line("TRACE_READY mapped_ring=1 per_event_flush=0 history_overwrites_oldest=1");}
 else Line("TRACE_UNAVAILABLE check_directory_write_access=1");
 Modules();handler=AddVectoredExceptionHandler(1,OnException);Line(handler?"EXCEPTION_OBSERVER_READY first_chance=1":"EXCEPTION_OBSERVER_UNAVAILABLE");if(log!=INVALID_HANDLE_VALUE)FlushFileBuffers(log);
}
void Ready(uintptr_t core) noexcept {if(header)header->coreBase=core;Line("NATIVE_CONTRACT_READY modules_after_all_plugins_loaded=1");Modules();}
void Event(uint32_t kind,uint64_t generation) noexcept {Span s(Session,nullptr,nullptr,nullptr,nullptr,0,0,generation);s.Mark(Decision,kind);}
void Flush() noexcept {if(header)FlushViewOfFile(header,0);if(log!=INVALID_HANDLE_VALUE)FlushFileBuffers(log);}
void Close() noexcept {
 if(handler){RemoveVectoredExceptionHandler(handler);handler=nullptr;}Flush();
 if(tls!=TLS_OUT_OF_INDEXES){TlsFree(tls);tls=TLS_OUT_OF_INDEXES;}
 ring=nullptr;if(header){UnmapViewOfFile(header);header=nullptr;}if(mapping){CloseHandle(mapping);mapping=nullptr;}if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);file=INVALID_HANDLE_VALUE;}if(log!=INVALID_HANDLE_VALUE){CloseHandle(log);log=INVALID_HANDLE_VALUE;}
}
}
