#pragma once

namespace Visuals {
#include "visual_guard.h"
size_t Length(const char* s) noexcept {size_t n=0;while(s[n])++n;return n;}
struct Buffer {
    uint8_t* p=nullptr;size_t size=0,capacity=0;
    ~Buffer(){if(p)HeapFree(GetProcessHeap(),0,p);}
    bool Reserve(size_t n) noexcept {if(n<=capacity)return true;if(n>64*1024*1024)return false;size_t c=capacity?capacity:256;while(c<n)c*=2;void* q=p?HeapReAlloc(GetProcessHeap(),0,p,c):HeapAlloc(GetProcessHeap(),0,c);if(!q)return false;p=static_cast<uint8_t*>(q);capacity=c;return true;}
    bool Add(const void* a,size_t n) noexcept {if(!Reserve(size+n+1))return false;memcpy(p+size,a,n);size+=n;p[size]=0;return true;}
    bool Text(const char* a) noexcept {return Add(a,Length(a));}
};
wchar_t dataRoot[2048]={};
bool Join(const wchar_t* root,const char* relative,wchar_t* out,uint32_t cap) noexcept {
    uint32_t n=0;while(root[n]){if(n+2>=cap)return false;out[n]=root[n];++n;}if(n&&out[n-1]!=L'\\'&&out[n-1]!=L'/')out[n++]=L'\\';
    for(uint32_t i=0;relative[i];++i){if(n+1>=cap)return false;out[n++]=relative[i]=='/'?L'\\':static_cast<unsigned char>(relative[i]);}out[n]=0;return true;
}
bool Root() noexcept {
    const wchar_t* candidates[]={ctx->modDirectory,ctx->scopeRootDirectory};wchar_t path[2048]={};
    for(auto* p:candidates){if(!p)continue;if(Join(p,"VanillaDeluxe.mpq",path,2048)){const DWORD a=GetFileAttributesW(path);if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)){uint32_t i=0;do{dataRoot[i]=path[i];}while(path[i++]);return true;}}
        if(Join(p,"data",path,2048)){const DWORD a=GetFileAttributesW(path);if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)){uint32_t i=0;do{dataRoot[i]=p[i];}while(p[i++]);return true;}}}
    return false;
}
bool MkParents(wchar_t* path) noexcept {
    for(uint32_t i=3;path[i];++i)if(path[i]==L'\\'||path[i]==L'/'){const wchar_t c=path[i];path[i]=0;const bool ok=CreateDirectoryW(path,nullptr)||GetLastError()==ERROR_ALREADY_EXISTS;path[i]=c;if(!ok)return false;}return true;
}
bool Save(const char* relative,const void* bytes,size_t size,bool backup=false) noexcept {
    wchar_t path[2048]={},temp[2048]={},old[2048]={};if(!Join(dataRoot,relative,path,2048)||!MkParents(path))return false;
    uint32_t n=0;while(path[n]&&n<1950){temp[n]=old[n]=path[n];++n;}if(n>=1950)return false;
    constexpr wchar_t suffix[]=L".r3ni-tmp";for(uint32_t i=0;i<sizeof(suffix)/sizeof(wchar_t);++i)temp[n+i]=suffix[i];
    constexpr wchar_t previous[]=L".r3ni-before-sovereign-colors";for(uint32_t i=0;i<sizeof(previous)/sizeof(wchar_t);++i)old[n+i]=previous[i];
    if(backup&&GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(old)==INVALID_FILE_ATTRIBUTES&&!CopyFileW(path,old,TRUE))return false;
    HANDLE f=CreateFileW(temp,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;const bool ok=size<=0xffffffffULL&&WriteFile(f,bytes,static_cast<DWORD>(size),&written,nullptr)&&written==size&&FlushFileBuffers(f);CloseHandle(f);
    if(!ok){DeleteFileW(temp);return false;}if(!MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp);return false;}return true;
}

bool Loose(const char* name,Buffer& out) noexcept {
    wchar_t path[2048]={};if(!Join(dataRoot,name,path,2048))return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length={};DWORD received=0;
    const bool loaded=GetFileSizeEx(file,&length)&&length.QuadPart>0&&length.QuadPart<16*1024*1024&&out.Reserve(static_cast<size_t>(length.QuadPart)+1)&&ReadFile(file,out.p,static_cast<DWORD>(length.QuadPart),&received,nullptr)&&received==length.QuadPart;
    CloseHandle(file);if(!loaded)return false;out.size=received;out.p[out.size]=0;return true;
}
bool GameFile(const char* name,Buffer& out) noexcept {

    wchar_t path[2048]={};if(!Join(dataRoot,name,path,2048))return false;
    if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return Loose(name,out);
    using namespace TerrorVisual;
    if(!Bytes(coreBase+FileLoaderRva,FileLoaderBytes,sizeof(FileLoaderBytes)))return false;
    using OpenFn=void*(__fastcall*)(const char*);using SizeFn=uint64_t(__fastcall*)(void*);
    using ReadFn=uint8_t(__fastcall*)(void*,void*,uint64_t,uint64_t*);using CloseFn=void(__fastcall*)(void*);
    auto open=reinterpret_cast<OpenFn>(RipSlot(coreBase+0x29e9a6));auto size=reinterpret_cast<SizeFn>(RipSlot(coreBase+0x29e9b8));
    auto read=reinterpret_cast<ReadFn>(RipSlot(coreBase+0x29ea4a));auto close=reinterpret_cast<CloseFn>(RipSlot(coreBase+0x29ea6c));
    if(!open||!size||!read||!close)return false;
    void* file=open(name);if(!file)return false;const uint64_t length=size(file);uint64_t received=0;
    const bool loaded=length>0&&length<16*1024*1024&&out.Reserve(static_cast<size_t>(length)+1)&&read(file,out.p,length,&received)&&received==length;
    close(file);if(!loaded)return false;out.size=static_cast<size_t>(length);out.p[out.size]=0;return true;
}



struct Span {size_t begin=0,end=0;};
bool White(uint8_t c) noexcept {return c==' '||c=='\t'||c=='\r'||c=='\n';}
void WS(const Buffer& b,size_t& i) noexcept {while(i<b.size&&White(b.p[i]))++i;}
bool String(const Buffer& b,size_t& i) noexcept {
    if(i>=b.size||b.p[i++]!='"')return false;
    while(i<b.size){uint8_t c=b.p[i++];if(c=='"')return true;if(c<32)return false;
        if(c=='\\'){if(i>=b.size)return false;c=b.p[i++];if(c=='u'){for(uint32_t n=0;n<4;++n){if(i>=b.size)return false;c=b.p[i++];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}}
            else if(c!='"'&&c!='\\'&&c!='/'&&c!='b'&&c!='f'&&c!='n'&&c!='r'&&c!='t')return false;}}
    return false;
}
bool Digit(uint8_t c) noexcept {return c>='0'&&c<='9';}
bool Number(const Buffer& b,size_t& i) noexcept {
    if(i<b.size&&b.p[i]=='-')++i;if(i>=b.size||!Digit(b.p[i]))return false;
    if(b.p[i]=='0')++i;else while(i<b.size&&Digit(b.p[i]))++i;
    if(i<b.size&&b.p[i]=='.'){++i;if(i>=b.size||!Digit(b.p[i]))return false;while(i<b.size&&Digit(b.p[i]))++i;}
    if(i<b.size&&(b.p[i]=='e'||b.p[i]=='E')){++i;if(i<b.size&&(b.p[i]=='+'||b.p[i]=='-'))++i;if(i>=b.size||!Digit(b.p[i]))return false;while(i<b.size&&Digit(b.p[i]))++i;}
    return true;
}
bool Value(const Buffer& b,size_t& i,uint32_t depth=0) noexcept {
    WS(b,i);if(i>=b.size||depth>128)return false;const uint8_t c=b.p[i];
    if(c=='"')return String(b,i);
    if(c=='{'||c=='['){++i;WS(b,i);const uint8_t end=c=='{'?'}':']';if(i<b.size&&b.p[i]==end){++i;return true;}
        for(;;){if(c=='{'){if(!String(b,i))return false;WS(b,i);if(i>=b.size||b.p[i++]!=':')return false;}
            if(!Value(b,i,depth+1))return false;WS(b,i);if(i>=b.size)return false;if(b.p[i]==end){++i;return true;}if(b.p[i++]!=',')return false;WS(b,i);}}
    const char* literal=c=='t'?"true":c=='f'?"false":c=='n'?"null":nullptr;
    if(literal){for(size_t n=0;literal[n];++n)if(i>=b.size||b.p[i++]!=literal[n])return false;return true;}
    return Number(b,i);
}
bool Equal(const Buffer& b,Span s,const char* t) noexcept {
    const size_t n=Length(t);if(s.end-s.begin!=n+2||b.p[s.begin]!='"'||b.p[s.end-1]!='"')return false;
    for(size_t i=0;i<n;++i)if(b.p[s.begin+1+i]!=static_cast<uint8_t>(t[i]))return false;return true;
}
int MemberStatus(const Buffer& b,Span object,const char* key,Span& out) noexcept {
    size_t i=object.begin;if(i>=object.end||b.p[i++]!='{')return -1;WS(b,i);bool found=false;
    while(i<object.end&&b.p[i]!='}'){const size_t first=i;if(!String(b,i))return -1;const Span name={first,i};WS(b,i);if(i>=object.end||b.p[i++]!=':')return -1;
        WS(b,i);Span value={i,0};if(!Value(b,i))return -1;value.end=i;if(Equal(b,name,key)){if(found)return -1;found=true;out=value;}
        WS(b,i);if(i<object.end&&b.p[i]==','){++i;WS(b,i);}else break;}
    if(i+1!=object.end||b.p[i]!='}')return -1;return found?1:0;
}
bool Member(const Buffer& b,Span object,const char* key,Span& out) noexcept {return MemberStatus(b,object,key,out)==1;}
bool Document(const Buffer& b,Span& root) noexcept {
    size_t i=b.size>=3&&b.p[0]==0xef&&b.p[1]==0xbb&&b.p[2]==0xbf?3:0;WS(b,i);root.begin=i;
    if(!Value(b,i))return false;root.end=i;WS(b,i);return i==b.size;
}


bool RootComponents(const Buffer& b,Span& out) noexcept {
    Span doc,entities,type;if(!Document(b,doc)||!Member(b,doc,"type",type)||!Equal(b,type,"UnitDefinition")||!Member(b,doc,"entities",entities)||b.p[entities.begin]!='[')return false;
    uint32_t unitRoots=0,namedRoots=0;Span unitComponents={},namedComponents={};size_t i=entities.begin+1;WS(b,i);
    while(i<entities.end&&b.p[i]!=']'){
        Span entity={i,0},components,name;if(!Value(b,i))return false;entity.end=i;
        if(!Member(b,entity,"type",type)||!Equal(b,type,"Entity")||!Member(b,entity,"components",components)||b.p[components.begin]!='[')return false;
        const bool namedRoot=Member(b,entity,"name",name)&&Equal(b,name,"entity_root");uint32_t unitComponentsCount=0;size_t c=components.begin+1;WS(b,c);
        while(c<components.end&&b.p[c]!=']'){Span component={c,0};if(!Value(b,c))return false;component.end=c;
            const int status=MemberStatus(b,component,"type",type);if(status<0)return false;if(status==1&&Equal(b,type,"UnitRootComponent"))++unitComponentsCount;
            WS(b,c);if(b.p[c]==','){++c;WS(b,c);}else break;}
        if(unitComponentsCount>1)return false;if(unitComponentsCount){++unitRoots;unitComponents=components;}
        if(namedRoot){++namedRoots;namedComponents=components;}
        WS(b,i);if(b.p[i]==','){++i;WS(b,i);}else break;
    }if(unitRoots==1){out=unitComponents;return true;}if(unitRoots>1||namedRoots!=1)return false;out=namedComponents;return true;
}
int RootTransform(const Buffer& b,Span components,Span& out) noexcept {
    size_t i=components.begin+1;WS(b,i);bool found=false;
    while(i<components.end&&b.p[i]!=']'){Span component={i,0},type;if(!Value(b,i))return -1;component.end=i;
        const int status=MemberStatus(b,component,"type",type);if(status<0)return -1;
        if(status==1&&Equal(b,type,"TransformDefinitionComponent")){if(found)return -1;found=true;out=component;}
        WS(b,i);if(b.p[i]==','){++i;WS(b,i);}else break;
    }return found?1:0;
}
bool ScaleNumber(const Buffer& b,Span span,Buffer& out,uint32_t percent) noexcept {
    size_t i=span.begin,end=i;if(!Number(b,end)||end!=span.end||end-i>32||b.p[i]=='-')return false;
    double v=0;while(i<end&&Digit(b.p[i])){v=v*10+(b.p[i++]-'0');}
    if(i<end&&b.p[i]=='.'){++i;double place=0.1;while(i<end&&Digit(b.p[i])){v+=(b.p[i++]-'0')*place;place*=0.1;}}
    if(i<end){++i;bool negative=false;if(b.p[i]=='+'||b.p[i]=='-')negative=b.p[i++]=='-';uint32_t exponent=0;while(i<end){exponent=exponent*10+b.p[i++]-'0';if(exponent>32)return false;}while(exponent--)v*=negative?0.1:10.0;}
    if(!(v>=0.000001&&v<=1000.0))return false;const uint64_t fixed=static_cast<uint64_t>(v*(percent*10000.0)+0.5);
    char s[32]={},reverse[24];uint32_t n=0,k=0;uint64_t whole=fixed/1000000;do{reverse[k++]=static_cast<char>('0'+whole%10);whole/=10;}while(whole);while(k)s[n++]=reverse[--k];s[n++]='.';
    uint64_t fraction=fixed%1000000;for(uint64_t place=100000;place;place/=10){s[n++]=static_cast<char>('0'+fraction/place);fraction%=place;}return out.Add(s,n);
}
bool DefaultScale(Buffer& out,uint32_t percent) noexcept {
    Buffer one;if(!one.Text("1"))return false;
    return out.Text("\"scale\":{\"x\":")&&ScaleNumber(one,{0,1},out,percent)&&out.Text(",\"y\":")&&ScaleNumber(one,{0,1},out,percent)&&out.Text(",\"z\":")&&ScaleNumber(one,{0,1},out,percent)&&out.Text("}");
}
bool Enlarge(const Buffer& original,Buffer& output,uint32_t percent,const char*& reason) noexcept {
    reason="invalid_json";Span doc;if(!Document(original,doc))return false;
    Span components,transform,scale,axes[3];reason="missing_or_ambiguous_root_entity";if(!RootComponents(original,components))return false;
    reason="ambiguous_root_transform";const int transformStatus=RootTransform(original,components,transform);if(transformStatus<0)return false;
    if(!transformStatus){


        size_t i=components.begin+1;WS(original,i);const size_t at=components.end-1;
        reason="allocation_failed";
        return output.Add(original.p,at)&&(original.p[i]==']'||output.Text(","))&&output.Text("{\"type\":\"TransformDefinitionComponent\",\"name\":\"r3sp_root_TransformDefinition\",\"position\":{\"x\":0,\"y\":0,\"z\":0},\"orientation\":{\"x\":0,\"y\":0,\"z\":0,\"w\":1},")&&DefaultScale(output,percent)&&output.Text(",\"inheritOnlyPosition\":false}")&&output.Add(original.p+at,original.size-at);
    }
    reason="duplicate_or_invalid_scale";const int scaleStatus=MemberStatus(original,transform,"scale",scale);if(scaleStatus<0)return false;
    if(!scaleStatus){const size_t at=transform.end-1;reason="allocation_failed";return output.Add(original.p,at)&&output.Text(",")&&DefaultScale(output,percent)&&output.Add(original.p+at,original.size-at);}
    reason="invalid_scale_axes";const char* names[]={"x","y","z"};for(uint32_t n=0;n<3;++n)if(!Member(original,scale,names[n],axes[n]))return false;
    for(uint32_t a=0;a<3;++a)for(uint32_t b=a+1;b<3;++b)if(axes[b].begin<axes[a].begin){const Span s=axes[a];axes[a]=axes[b];axes[b]=s;}
    reason="invalid_scale_number";size_t previous=0;for(const auto& axis:axes){if(axis.begin<previous||!output.Add(original.p+previous,axis.begin-previous)||!ScaleNumber(original,axis,output,percent))return false;previous=axis.end;}
    return output.Add(original.p+previous,original.size-previous);
}



bool Colorize(const Buffer& original,Buffer& output,const SPColors::Profile& profile,const char*& reason) noexcept {
 reason="palette_link_invalid";
 if(!profile.target[0])return output.Add(original.p,original.size);
 Span components;if(!RootComponents(original,components))return false;
 size_t i=components.begin+1;WS(original,i);uint32_t variants=0;
 while(i<components.end&&original.p[i]!=']'){
  Span component={i,0},type,filename;if(!Value(original,i))return false;component.end=i;
  if(Member(original,component,"type",type)&&Equal(original,type,"VariantDefinitionComponent")){
   if(!Member(original,component,"filename",filename)||!Equal(original,filename,profile.original))return false;++variants;
  }
  WS(original,i);if(original.p[i]==','){++i;WS(original,i);}else break;
 }
 if(variants!=1)return false;
 size_t previous=0;uint32_t references=0;i=0;
 while(i<original.size){
  if(original.p[i]!='"'){++i;continue;}
  Span value={i,0};if(!String(original,i))return false;value.end=i;
  if(Equal(original,value,profile.original)){
   if(!output.Add(original.p+previous,value.begin-previous)||!output.Text("\"")||!output.Text(profile.target)||!output.Text("\""))return false;
   previous=value.end;++references;
  }
 }
 if(references<2)return false;
 reason="palette_allocation_failed";
 return output.Add(original.p+previous,original.size-previous);
}
bool ready=false;uint16_t eligible[SPData::Count]={};uint32_t eligibleCount=0;
uint32_t PickKind() noexcept {return eligible[Rand(eligibleCount)];}
void ModelIssue(const SPData::Kind& kind,const char* stage,const char* detail,const Buffer* original=nullptr) noexcept {
 Text t;t.Add("SP_MODEL_SKIP model=");t.Add(kind.model);t.Add(" stage=");t.Add(stage);t.Add(" reason=");t.Add(detail);
 if(original){Buffer diagnostic;if(diagnostic.Text("r3ni-diagnostics/SovereignPacks/")&&diagnostic.Text(kind.model)&&diagnostic.Text(".json")&&Save(reinterpret_cast<char*>(diagnostic.p),original->p,original->size)){t.Add(" diagnostic=");t.Add(reinterpret_cast<char*>(diagnostic.p));}else t.Add(" diagnostic_write_failed=1");}
 t.Add(" other_models_continue=1");Log(t.s,true);
}
bool Prepare() noexcept {
 if(ready)return true;if(!ctx||!Root())return false;
 Buffer mapping;Span root;if(!GameFile("data/hd/character/monsters.json",mapping)||!Document(mapping,root)||mapping.p[root.begin]!='{')return false;
 bool changed=false;Buffer upgraded;
 struct MappingChange {Span value;uint16_t kind;};MappingChange mappingChanges[SPData::Count*2]={};uint32_t mappingChangeCount=0;

 uint16_t indices[793]={};bool seen[SPData::Count*2]={};for(uint32_t k=0;k<SPData::Count;++k)indices[SPData::Kinds[k].stock]=static_cast<uint16_t>(k+1);
 size_t cursor=root.begin+1;WS(mapping,cursor);
 while(cursor<root.end&&mapping.p[cursor]!='}'){
  Span name={cursor,0};if(!String(mapping,cursor))return false;name.end=cursor;WS(mapping,cursor);if(mapping.p[cursor++]!=':')return false;WS(mapping,cursor);Span value={cursor,0};if(!Value(mapping,cursor))return false;value.end=cursor;
  const auto* text=mapping.p+name.begin+1;const size_t bytes=name.end-name.begin-2;
  if(bytes>=6&&text[0]=='r'&&text[1]=='3'&&text[2]=='s'&&text[3]=='p'){
   uint32_t stock=0;size_t n=4;for(;n+1<bytes&&Digit(text[n]);++n){stock=stock*10+text[n]-'0';if(stock>=793)return false;}
   if(n+1!=bytes||!indices[stock]||(text[n]!='l'&&text[n]!='m'))return false;const uint32_t k=indices[stock]-1,which=k*2+(text[n]=='m');
   if(seen[which])return false;
   const auto& profile=SPColors::Profiles[SPColors::KindProfiles[k]];
   if(!Equal(mapping,value,SPData::Kinds[k].hd)){


    if(!Equal(mapping,value,profile.oldHd))return false;
    mappingChanges[mappingChangeCount++]={value,static_cast<uint16_t>(k)};
   }
   seen[which]=true;
  }
  WS(mapping,cursor);if(mapping.p[cursor]==','){++cursor;WS(mapping,cursor);}else break;
 }
 for(bool found:seen)if(!found){Log("SP_VISUALS_REJECT: companion HD mappings differ; spawning disabled.",true);return false;}
 bool completed[SPData::Count]={},supported[SPData::Count]={};eligibleCount=0;uint32_t modelsReady=0,modelsSkipped=0;
 for(uint32_t n=0;n<SPData::Count;++n){if(completed[n])continue;const auto& kind=SPData::Kinds[n];
  for(uint32_t j=n;j<SPData::Count;++j){const char* a=kind.hd;const char* b=SPData::Kinds[j].hd;while(*a&&*a==*b){++a;++b;}if(!*a&&!*b)completed[j]=true;}
  Buffer path,original,enlarged,colored,target,existing;
  if(!path.Text("data/hd/character/enemy/")||!path.Text(kind.model)||!path.Text(".json"))return false;
  if(!GameFile(reinterpret_cast<char*>(path.p),original)){ModelIssue(kind,"read",reinterpret_cast<char*>(path.p));++modelsSkipped;continue;}
  const char* reason=nullptr;if(!Enlarge(original,enlarged,cfg.size,reason)){ModelIssue(kind,"transform",reason,&original);++modelsSkipped;continue;}
  const auto& profile=SPColors::Profiles[SPColors::KindProfiles[n]];
  if(!Colorize(enlarged,colored,profile,reason)){ModelIssue(kind,"color",reason,&original);++modelsSkipped;continue;}
  if(profile.target[0]){
   const size_t bytes=Length(profile.json);Buffer oldPalette;
   if(!(Loose(profile.target,oldPalette)&&oldPalette.size==bytes&&Bytes(reinterpret_cast<uintptr_t>(oldPalette.p),reinterpret_cast<const uint8_t*>(profile.json),bytes))){
    if(!Save(profile.target,profile.json,bytes)){ModelIssue(kind,"palette_write",profile.target);++modelsSkipped;continue;}changed=true;
   }
  }
  if(!target.Text("data/hd/character/enemy/")||!target.Text(kind.hd)||!target.Text(".json"))return false;
  const bool same=Loose(reinterpret_cast<char*>(target.p),existing)&&existing.size==colored.size&&Bytes(reinterpret_cast<uintptr_t>(existing.p),colored.p,colored.size);
  if(!same){if(!Save(reinterpret_cast<char*>(target.p),colored.p,colored.size)){ModelIssue(kind,"write",reinterpret_cast<char*>(target.p));++modelsSkipped;continue;}changed=true;}
  ++modelsReady;for(uint32_t j=n;j<SPData::Count;++j){const char* a=kind.hd;const char* b=SPData::Kinds[j].hd;while(*a&&*a==*b){++a;++b;}if(!*a&&!*b)supported[j]=true;}
 }
 for(uint32_t n=0;n<SPData::Count;++n)if(supported[n])eligible[eligibleCount++]=static_cast<uint16_t>(n);
 Text summary;summary.Add("SP_VISUALS_SUMMARY ready_models=");summary.Num(modelsReady);summary.Add(" skipped_models=");summary.Num(modelsSkipped);summary.Add(" eligible_types=");summary.Num(eligibleCount);summary.Add(" total_types=");summary.Num(SPData::Count);Log(summary.s,true);
 if(!eligibleCount){Log("SP_VISUALS_REJECT: no usable HD model; native spawning remains disabled.",true);return false;}
 size_t previousMapping=0;
 for(uint32_t n=0;n<mappingChangeCount;++n){const auto& change=mappingChanges[n];if(!supported[change.kind])continue;
  if(!upgraded.Add(mapping.p+previousMapping,change.value.begin-previousMapping)||!upgraded.Text("\"")||!upgraded.Text(SPData::Kinds[change.kind].hd)||!upgraded.Text("\""))return false;
  previousMapping=change.value.end;
 }
 if(previousMapping){


  if(!upgraded.Add(mapping.p+previousMapping,mapping.size-previousMapping))return false;
  if(!Save("data/hd/character/monsters.json",upgraded.p,upgraded.size,true)){Log("SP_COLORS_REJECT: mapping backup/write failed; spawning disabled.",true);eligibleCount=0;return false;}
  changed=true;Log("SP_COLORS_MAPPING_UPGRADED: only Sovereign entries changed; previous mapping backed up.");
 }
 Log("SP_COLORS_READY: private native-variant banks; ordinary slots fixed per family variant; cold/poison/global palettes preserved. Visual verification pending.");
 ready=true;Log(changed?"SP_VISUALS_READY: restart once to refresh private HD resources.":"SP_VISUALS_READY: private HD resources already installed.");return true;
}
}
