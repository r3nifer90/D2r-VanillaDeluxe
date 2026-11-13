struct Type {uint32_t code,parent1,parent2,rare;};Type types[512];uint32_t typeCount=0;
struct Affix {uint32_t level,maxlevel,required;uint32_t include[7],exclude[5];};Affix affixes[2048];uint32_t affixCount=0;
bool ReadExtra(const wchar_t* root,const wchar_t* name,unsigned mode){
 wchar_t path[2048]{};if(!Join(root,name,path))return false;
 HANDLE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};bool ok=GetFileSizeEx(h,&size)&&size.QuadPart>0&&size.QuadPart<=16*1024*1024;
 char* b=ok?(char*)HeapAlloc(GetProcessHeap(),0,(size_t)size.QuadPart):nullptr;DWORD got=0;ok=b&&ReadFile(h,b,(DWORD)size.QuadPart,&got,nullptr)&&got==size.QuadPart;CloseHandle(h);if(!ok){if(b)HeapFree(GetProcessHeap(),0,b);return false;}
 Field header[512],row[512];size_t start=(got>=3&&(uint8_t)b[0]==239&&(uint8_t)b[1]==187&&(uint8_t)b[2]==191)?3:0,end=start;while(end<got&&b[end]!='\n')++end;size_t len=end-start;if(len&&b[start+len-1]=='\r')--len;size_t columns=Fields(b+start,len,header);
 int code=Column(header,columns,mode==0?"item":"Code"),level=Column(header,columns,"lvl"),req=Column(header,columns,mode==2?"levelreq":"lvl req"),spawn=Column(header,columns,"spawnable"),disabled=Column(header,columns,"disabled"),cond=Column(header,columns,"DropConditionCalc"),p1=Column(header,columns,"Equiv1"),p2=Column(header,columns,"Equiv2"),rare=Column(header,columns,mode==1?"Rare":"rare"),alvl=Column(header,columns,"level"),maxlvl=Column(header,columns,"maxlevel");
 int include[7],exclude[5];for(int i=0;i<7;++i){char n[]="itype1";n[5]+=i;include[i]=Column(header,columns,n);}for(int i=0;i<5;++i){char n[]="etype1";n[5]+=i;exclude[i]=Column(header,columns,n);}
 ok=mode==0?(code>=0&&level>=0&&req>=0&&spawn>=0&&disabled>=0&&cond>=0):mode==1?(code>=0&&p1>=0&&p2>=0&&rare>=0):(req>=0&&rare>=0&&spawn>=0&&alvl>=0&&maxlvl>=0&&include[0]>=0);
 start=end+1;while(ok&&start<got){end=start;while(end<got&&b[end]!='\n')++end;len=end-start;if(len&&b[start+len-1]=='\r')--len;size_t n=Fields(b+start,len,row);start=end+1;if(n!=columns)continue;
  if(mode==0){
   if(!row[spawn].Eq("1")||row[disabled].Eq("1"))continue;uint32_t c=row[code].Code();if(!c)continue;
   for(uint32_t i=0;i<baseCount;++i)if(bases[i].code==c){if(setCount>=2048){ok=false;break;}uint32_t l=row[level].Num(),r=row[req].Num();sets[setCount++]={c,l>bases[i].level?l:bases[i].level,r>bases[i].required?r:bases[i].required,row[cond].n?0u:1u};break;}
  }else if(mode==1){uint32_t c=row[code].Code();if(!c)continue;if(typeCount>=512){ok=false;break;}types[typeCount++]={c,row[p1].Code(),row[p2].Code(),row[rare].Eq("1")?1u:0u};
  }else{
   if(!row[rare].Eq("1")||!row[spawn].Eq("1"))continue;if(affixCount>=2048){ok=false;break;}auto& a=affixes[affixCount++];a={};a.level=row[alvl].Num();a.maxlevel=row[maxlvl].Num();a.required=row[req].Num();
   for(unsigned i=0;i<7;++i)if(include[i]>=0)a.include[i]=row[include[i]].Code();for(unsigned i=0;i<5;++i)if(exclude[i]>=0)a.exclude[i]=row[exclude[i]].Code();
  }
 }
 HeapFree(GetProcessHeap(),0,b);return ok&&(mode==0?setCount>0:mode==1?typeCount>0:affixCount>0);
}
bool IsType(uint32_t type,uint32_t target,unsigned depth=0){
 if(!type||!target||depth>=16)return false;if(type==target)return true;
 for(unsigned i=0;i<typeCount;++i)if(types[i].code==type)return IsType(types[i].parent1,target,depth+1)||IsType(types[i].parent2,target,depth+1);
 return false;
}
bool RareType(uint32_t type){for(unsigned i=0;i<typeCount;++i)if(types[i].code==type)return types[i].rare!=0;return false;}
bool Applies(const Base& b,const Affix& a){
 bool included=false;for(auto c:a.include)if(IsType(b.type,c)||IsType(b.type2,c))included=true;if(!included)return false;
 for(auto c:a.exclude)if(IsType(b.type,c)||IsType(b.type2,c))return false;return true;
}
bool SafeRareBase(const Base& b,uint32_t chestLevel,uint32_t playerLevel,uint32_t cap){
 if(!b.code||b.required>cap||b.level>chestLevel||b.level>playerLevel||(!RareType(b.type)&&!RareType(b.type2)))return false;
 uint32_t ilvl=playerLevel>b.level?playerLevel:b.level;if(ilvl>99)ilvl=99;
 uint32_t alvl=b.magic?ilvl+b.magic:ilvl<99-b.level/2?ilvl-b.level/2:ilvl*2-99;if(alvl>99)alvl=99;
 for(unsigned i=0;i<affixCount;++i){const auto& a=affixes[i];if(a.required>cap&&a.level<=alvl&&(!a.maxlevel||alvl<=a.maxlevel)&&Applies(b,a))return false;}
 return true;
}
