#pragma once


namespace SPD {
constexpr uint32_t AuditCapacity=64,AuditFirstAttempts=8,AuditFirstAdmitted=64;
struct AuditRow {
 uint64_t key=0,calls=0,admitted=0,seedChanged=0,unreadableSeed=0;
 uint64_t skillCalls=0,skillOk=0;uint32_t lastSkill=0,lastLevel=0,lastResult=0;
 uint32_t actorType=0,actorClass=0,actorId=0;
 bool occupied=false;
};
inline AuditRow auditRows[AuditCapacity]={};
inline SRWLOCK auditLock=SRWLOCK_INIT;
inline uint64_t auditOverflow=0;
struct AuditSeed {
 uintptr_t actor,target;uint64_t before=0;bool readable=false,modeReadable=false;uint32_t targetMode=0;
 explicit AuditSeed(void* p,void* t) noexcept:actor(reinterpret_cast<uintptr_t>(p)),target(reinterpret_cast<uintptr_t>(t)) {
  PreserveError preserve;readable=actor&&Copy(actor+0x28,&before,sizeof(before));
  modeReadable=target&&Copy(target+0xc,&targetMode,sizeof(targetMode));
 }
};
inline void AuditFields(Text& t,const AuditRow& row) noexcept {
 t.Add(" scope=since_plugin_load stat=");t.Num((row.key>>32)&0xffff);
 t.Add(" layer=");t.Num(uint32_t(row.key));t.Add(" key=");t.Hex(row.key);
 t.Add(" actor_type=");t.Num(row.actorType);t.Add(" actor_class=");t.Num(row.actorClass);
 t.Add(" actor_id=");t.Num(row.actorId);t.Add(" calls=");t.Num(row.calls);
 t.Add(" native_admitted=");t.Num(row.admitted);t.Add(" seed_changed=");t.Num(row.seedChanged);
 t.Add(" unreadable_seed=");t.Num(row.unreadableSeed);
 t.Add(" skill_calls=");t.Num(row.skillCalls);t.Add(" skill_ok=");t.Num(row.skillOk);
 t.Add(" last_skill=");t.Num(row.lastSkill);t.Add(" last_level=");t.Num(row.lastLevel);
 t.Add(" last_skill_result=");t.Num(row.lastResult);
}

inline AuditRow* AuditFind(const Record& record,uint64_t key) noexcept {
 AuditRow* row=nullptr;AuditRow* vacant=nullptr;
 for(auto& r:auditRows){
  if(!r.occupied){if(!vacant)vacant=&r;continue;}
  if(r.key==key&&r.actorType==record.actorType&&r.actorClass==record.actorClass&&r.actorId==record.actorId){row=&r;break;}
 }
 if(!row&&vacant){row=vacant;row->occupied=true;row->key=key;row->actorType=record.actorType;row->actorClass=record.actorClass;row->actorId=record.actorId;}
 return row;
}
inline void AuditSkill(const Record& record,uint64_t key,int result) noexcept {
 PreserveError preserve;AcquireSRWLockExclusive(&auditLock);
 if(auto* row=AuditFind(record,key)){
  ++row->skillCalls;if(result)++row->skillOk;row->lastSkill=record.skill;row->lastLevel=record.level;row->lastResult=uint32_t(result);
  if(row->skillCalls<=AuditFirstAdmitted){
   char storage[1024]={};Text t{storage,0,sizeof(storage)};t.Add("SP_PROC_SKILL_SAMPLE");AuditFields(t,*row);
   t.Add(" tick=");t.Num(GetTickCount64());Line(t.p);
  }
 }
 ReleaseSRWLockExclusive(&auditLock);
}
inline void AuditProc(const Record& record,int result,const AuditSeed& seed) noexcept {
 PreserveError preserve;uint64_t after=0;
 const bool readable=seed.readable&&Copy(seed.actor+0x28,&after,sizeof(after));
 AcquireSRWLockExclusive(&auditLock);
 auto* row=AuditFind(record,record.extra);
 if(!row){
  if(++auditOverflow==1)Line("SP_PROC_AUDIT_OVERFLOW capacity=64 gameplay_unchanged=1");
  ReleaseSRWLockExclusive(&auditLock);return;
 }
 ++row->calls;if(result)++row->admitted;
 if(!readable)++row->unreadableSeed;else if(after!=seed.before)++row->seedChanged;

 if(row->calls<=AuditFirstAttempts||(result&&row->admitted<=AuditFirstAdmitted)){
  char storage[1024]={};Text t{storage,0,sizeof(storage)};t.Add("SP_PROC_AUDIT_SAMPLE");AuditFields(t,*row);
  t.Add(" tick=");t.Num(GetTickCount64());t.Add(" result=");t.Num(uint32_t(result));
  t.Add(" target_type=");t.Num(record.targetType);t.Add(" target_class=");t.Num(record.targetClass);
  t.Add(" target_id=");t.Num(record.targetId);t.Add(" seed_readable=");t.Num(readable);
  uint32_t modeAfter=0;const bool modeReadable=seed.modeReadable&&Copy(seed.target+0xc,&modeAfter,sizeof(modeAfter));
  t.Add(" target_mode_readable=");t.Num(modeReadable);t.Add(" target_mode_before=");t.Num(seed.targetMode);
  t.Add(" target_mode_after=");t.Num(modeAfter);
  t.Add(" seed_before=");t.Hex(seed.before);t.Add(" seed_after=");t.Hex(after);Line(t.p);
 }
 ReleaseSRWLockExclusive(&auditLock);
}
inline void AuditSummary(const char* reason) noexcept {
 PreserveError preserve;AcquireSRWLockExclusive(&auditLock);
 char storage[1024]={};Text t{storage,0,sizeof(storage)};t.Add("SP_PROC_AUDIT_TOTALS reason=");t.Add(reason);
 t.Add(" scope=since_plugin_load overflow_calls=");t.Num(auditOverflow);Line(t.p);
 for(const auto& row:auditRows)if(row.occupied){
  char line[1024]={};Text x{line,0,sizeof(line)};x.Add("SP_PROC_AUDIT_TOTAL");AuditFields(x,row);Line(x.p);
 }
 ReleaseSRWLockExclusive(&auditLock);
}
}
