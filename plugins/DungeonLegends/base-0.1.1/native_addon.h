#pragma once

namespace Addon {
using Visuals::Buffer;using Visuals::Span;
bool Starts(const uint8_t* p,size_t n,const char* prefix) noexcept {for(size_t i=0;prefix[i];++i){if(i>=n)return false;uint8_t a=p[i],b=prefix[i];if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)return false;}return true;}
size_t Bom(const Buffer& b) noexcept {return b.size>=3&&b.p[0]==0xef&&b.p[1]==0xbb&&b.p[2]==0xbf?3:0;}
Span Line(const Buffer& b,size_t& p) noexcept {Span s={p,p};while(p<b.size&&b.p[p]!='\n'&&b.p[p]!='\r')++p;s.end=p;if(p<b.size&&b.p[p]=='\r')++p;if(p<b.size&&b.p[p]=='\n')++p;return s;}
Span Cell(const Buffer& b,Span row,uint32_t index) noexcept {size_t p=row.begin;while(index&&p<row.end){if(b.p[p++]=='\t')--index;}if(index)return {};const size_t start=p;while(p<row.end&&b.p[p]!='\t')++p;return {start,p};}
uint32_t Column(const Buffer& b,Span row,const char* key) noexcept {for(uint32_t i=0;i<512;++i){const Span s=Cell(b,row,i);if(!s.begin&&!s.end)break;if(s.end-s.begin==Visuals::Length(key)){bool eq=true;for(size_t n=0;key[n];++n)if(b.p[s.begin+n]!=key[n])eq=false;if(eq)return i;}}return 0xffffffff;}
bool Number(const Buffer& b,Span s,uint32_t& value) noexcept {if(s.end==s.begin)return false;value=0;for(size_t p=s.begin;p<s.end;++p){if(!Visuals::Digit(b.p[p])||value>1000000)return false;value=value*10+b.p[p]-'0';}return true;}
bool Resource(uint32_t id,Buffer& out) noexcept {auto r=FindResourceW(module,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));auto loaded=r?LoadResource(module,r):nullptr;const void* p=loaded?LockResource(loaded):nullptr;const DWORD n=r?SizeofResource(module,r):0;return p&&n&&out.Add(p,n);}
bool Tsv(const Buffer& old,const Buffer& fragment,Buffer& out,const char* index,uint32_t first) noexcept {
 size_t p=Bom(old),f=Bom(fragment);const Span header=Line(old,p),wanted=Line(fragment,f);
 if(header.end-header.begin!=wanted.end-wanted.begin||!Bytes(reinterpret_cast<uintptr_t>(old.p+header.begin),fragment.p+wanted.begin,header.end-header.begin))return false;
 const uint32_t col=index?Column(old,header,index):0xffffffff;if(index&&col==0xffffffff)return false;
 if(!out.Add(old.p,p))return false;uint32_t maximum=0;bool indexed=false;
 while(p<old.size){const size_t begin=p;const Span row=Line(old,p),key=Cell(old,row,0);if(Starts(old.p+key.begin,key.end-key.begin,"r3tl"))continue;
  if(index){uint32_t n=0;if(Number(old,Cell(old,row,col),n)){if(n>=first)return false;indexed=true;if(n>maximum)maximum=n;}}
  if(!out.Add(old.p+begin,p-begin))return false;}
 if(index&&(!indexed||maximum+1!=first))return false;
 if(out.size&&out.p[out.size-1]!='\n'&&!out.Text("\r\n"))return false;
 return out.Add(fragment.p+f,fragment.size-f);
}
bool Strings(const Buffer& original,const Buffer& fragment,Buffer& out) noexcept {
 Span doc,fd;if(!Visuals::Document(original,doc)||original.p[doc.begin]!='['||!Visuals::Document(fragment,fd)||fragment.p[fd.begin]!='[')return false;
 if(!out.Text("["))return false;size_t p=doc.begin+1;Visuals::WS(original,p);bool written=false;
 while(p<doc.end&&original.p[p]!=']'){Span row={p,0},key,id;if(!Visuals::Value(original,p))return false;row.end=p;
  if(!Visuals::Member(original,row,"Key",key)||key.end-key.begin<2)return false;
  const bool own=Starts(original.p+key.begin+1,key.end-key.begin-2,"r3tl_name_");
  if(!own){uint32_t number=0;if(Visuals::Member(original,row,"id",id)&&Number(original,id,number)&&number>=62000&&number<62020)return false;
   if(written&&!out.Text(","))return false;if(!out.Add(original.p+row.begin,row.end-row.begin))return false;written=true;}
  Visuals::WS(original,p);if(p<doc.end&&original.p[p]==','){++p;Visuals::WS(original,p);}else break;}
 if(written&&!out.Text(","))return false;return out.Add(fragment.p+fd.begin+1,fd.end-fd.begin-2)&&out.Text("]\n");
}
bool LocalizationIdsFree() noexcept {
 wchar_t pattern[2048]={};if(!Visuals::Join(Visuals::dataRoot,"data/local/lng/strings/*.json",pattern,2048))return false;
 WIN32_FIND_DATAW entry={};HANDLE find=FindFirstFileW(pattern,&entry);if(find==INVALID_HANDLE_VALUE)return false;bool ok=true;
 do {if(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;char relative[512]="data/local/lng/strings/";size_t n=Visuals::Length(relative);for(uint32_t i=0;entry.cFileName[i];++i){if(n>=510||entry.cFileName[i]>127){ok=false;break;}relative[n++]=static_cast<char>(entry.cFileName[i]);}relative[n]=0;if(!ok)break;
  Buffer b;Span doc;if(!Visuals::Loose(relative,b)||!Visuals::Document(b,doc)||b.p[doc.begin]!='['){ok=false;break;}size_t p=doc.begin+1;Visuals::WS(b,p);
  while(p<doc.end&&b.p[p]!=']'){Span row={p,0},id,key;if(!Visuals::Value(b,p)){ok=false;break;}row.end=p;uint32_t number=0;if(Visuals::Member(b,row,"id",id)&&Number(b,id,number)&&number>=62000&&number<62020){const bool own=Visuals::Member(b,row,"Key",key)&&key.end-key.begin>=2&&Starts(b.p+key.begin+1,key.end-key.begin-2,"r3tl_name_");if(!own){Text t;t.Add("DL_LOCALIZATION_ID_COLLISION file=");t.Add(relative);t.Add(" id=");t.Num(number);Log(t.s,true);ok=false;break;}}
   Visuals::WS(b,p);if(b.p[p]==','){++p;Visuals::WS(b,p);}else break;
  }
 }while(ok&&FindNextFileW(find,&entry));FindClose(find);return ok;
}
struct File {const char* name=nullptr;Buffer previous,result;bool changed=false,moved=false;};
bool Stage() noexcept {
 if(!Visuals::Root()||!LocalizationIdsFree())return false;
 const char* names[]={"data/global/excel/monstats.txt","data/global/excel/skills.txt","data/global/excel/superuniques.txt","data/global/excel/treasureclassex.txt","data/global/excel/base/monstats.txt","data/global/excel/base/skills.txt","data/global/excel/base/superuniques.txt","data/global/excel/base/treasureclassex.txt","data/local/lng/strings/monsters.json"};
 const char* indices[]={"*hcIdx","*Id","hcIdx",nullptr};const uint32_t first[]={1779,437,100,0};
 auto* files=static_cast<File*>(HeapAlloc(GetProcessHeap(),0,sizeof(File)*9));if(!files)return false;for(uint32_t i=0;i<9;++i)new(files+i) File;
 bool ok=true,changed=false;
 for(uint32_t i=0;i<9&&ok;++i){auto& f=files[i];f.name=names[i];Buffer fragment;
  ok=Visuals::Loose(f.name,f.previous)&&Resource(1100+i,fragment)&&(i==8?Strings(f.previous,fragment,f.result):Tsv(f.previous,fragment,f.result,indices[i%4],first[i%4]));
  if(ok){f.changed=f.previous.size!=f.result.size||!Bytes(reinterpret_cast<uintptr_t>(f.previous.p),f.result.p,f.result.size);changed=changed||f.changed;}
  if(!ok){Text t;t.Add("TL_SOURCE_REJECT file=");t.Add(f.name);t.Add(" source header/index or localization collision; no tables committed");Log(t.s,true);}}
 for(uint32_t i=0;i<9&&ok;++i)if(files[i].changed){ok=Visuals::Save(files[i].name,files[i].result.p,files[i].result.size,true);files[i].moved=ok;}
 if(!ok){for(uint32_t i=9;i-->0;)if(files[i].moved)if(!Visuals::Save(files[i].name,files[i].previous.p,files[i].previous.size))Log("TL_SOURCE_ROLLBACK_FAILED: restore .r3ni-before-terror-legends files.",true);}
 for(uint32_t i=0;i<9;++i)files[i].~File();HeapFree(GetProcessHeap(),0,files);
 if(ok)Log(changed?"TL_SOURCE_STAGED: private boss/skill/loot records appended; compile VanillaDeluxe at the main menu, fully exit and restart.":"TL_SOURCE_READY: owned source records already staged; loaded BINs validated separately.");return ok;
}
}
