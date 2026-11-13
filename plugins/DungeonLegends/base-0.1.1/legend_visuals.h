#pragma once
namespace Assets {
using Visuals::Buffer;using Visuals::Span;
uint16_t masks[TLData::Count]={};bool ready=false;
bool TextValue(const Buffer& b,Span s,char* out,size_t cap) noexcept {if(s.end-s.begin<2||b.p[s.begin]!='"'||b.p[s.end-1]!='"'||s.end-s.begin-2>=cap)return false;size_t n=0;for(size_t p=s.begin+1;p<s.end-1;++p){if(b.p[p]<32||b.p[p]=='\\')return false;out[n++]=b.p[p];}out[n]=0;return true;}
bool RootVariant(const Buffer& b,char* name,size_t cap) noexcept {
 Span comp;if(!Visuals::RootComponents(b,comp))return false;size_t p=comp.begin+1;Visuals::WS(b,p);uint32_t found=0;
 while(p<comp.end&&b.p[p]!=']'){Span v={p,0},type,path;if(!Visuals::Value(b,p))return false;v.end=p;
  if(Visuals::Member(b,v,"type",type)&&Visuals::Equal(b,type,"VariantDefinitionComponent")){if(!Visuals::Member(b,v,"filename",path)||!TextValue(b,path,name,cap))return false;++found;}
  Visuals::WS(b,p);if(b.p[p]==','){++p;Visuals::WS(b,p);}else break;}return found==1;
}
uint32_t Entries(const Buffer& b,Span* rows,uint32_t max) noexcept {
 Span doc,entries;if(!Visuals::Document(b,doc)||!Visuals::Member(b,doc,"entries",entries)||b.p[entries.begin]!='[')return 0;
 size_t p=entries.begin+1;Visuals::WS(b,p);uint32_t count=0;
 while(p<entries.end&&b.p[p]!=']'){if(count>=max)return 0;rows[count].begin=p;if(!Visuals::Value(b,p))return 0;rows[count++].end=p;Visuals::WS(b,p);if(b.p[p]==','){++p;Visuals::WS(b,p);}else break;}return count;
}
bool Palette(const Buffer& src,Span* entries,uint32_t count,uint32_t selected,Buffer& out) noexcept {
 Span selectedTransforms;if(!Visuals::Member(src,entries[selected],"transforms",selectedTransforms))return false;
 size_t previous=0;for(uint32_t n=2;n<count;++n){Span transforms;if(!Visuals::Member(src,entries[n],"transforms",transforms)||transforms.begin<previous)return false;
  if(!out.Add(src.p+previous,transforms.begin-previous)||!out.Add(src.p+selectedTransforms.begin,selectedTransforms.end-selectedTransforms.begin))return false;previous=transforms.end;}
 return out.Add(src.p+previous,src.size-previous);
}
bool Relink(const Buffer& src,Buffer& out,const char* from,const char* to) noexcept {
 size_t p=0,previous=0;uint32_t changed=0;
 while(p<src.size){if(src.p[p]!='"'){++p;continue;}Span s={p,0};if(!Visuals::String(src,p))return false;s.end=p;
  if(Visuals::Equal(src,s,from)){if(!out.Add(src.p+previous,s.begin-previous)||!out.Text("\"")||!out.Text(to)||!out.Text("\""))return false;previous=s.end;++changed;}}
 return changed>=2&&out.Add(src.p+previous,src.size-previous);
}
bool Mapping(const Buffer& original,const Buffer& extra,Buffer& out) noexcept {
 Span doc;if(!Visuals::Document(original,doc)||original.p[doc.begin]!='{')return false;
 if(!out.Text("{"))return false;size_t p=doc.begin+1;Visuals::WS(original,p);bool any=false;
 while(p<doc.end&&original.p[p]!='}'){const size_t begin=p;Span key={p,0};if(!Visuals::String(original,p))return false;key.end=p;Visuals::WS(original,p);if(original.p[p++]!=':')return false;if(!Visuals::Value(original,p))return false;
  if(!Addon::Starts(original.p+key.begin+1,key.end-key.begin-2,"r3tl")){if(any&&!out.Text(","))return false;if(!out.Add(original.p+begin,p-begin))return false;any=true;}
  Visuals::WS(original,p);if(original.p[p]==','){++p;Visuals::WS(original,p);}else break;}
 if(extra.size){if(any&&!out.Text(","))return false;if(!out.Add(extra.p,extra.size))return false;}return out.Text("}\n");
}
bool Prepare() noexcept {
 if(ready)return true;if(!Visuals::archiveReady)return false;if(!Visuals::Root())return false;Buffer mapping;Span doc;
 if(!Visuals::GameFile("data/hd/character/monsters.json",mapping)||!Visuals::Document(mapping,doc))return false;
 Buffer entries;uint32_t models=0;memset(masks,0,sizeof(masks));
 for(uint32_t i=0;i<TLData::Count;++i){const auto& profile=TLData::Profiles[i];Span link;char source[256]={};Buffer path,original,enlarged;
  if(!Visuals::Member(mapping,doc,profile.source,link)||!TextValue(mapping,link,source,sizeof(source))||!path.Text("data/hd/character/enemy/")||!path.Text(source)||!path.Text(".json")||!Visuals::GameFile(reinterpret_cast<char*>(path.p),original)){Text t;t.Add("TL_MODEL_MISSING profile=");t.Add(profile.key);Log(t.s,true);continue;}
  const char* why=nullptr;if(!Visuals::Enlarge(original,enlarged,bosses[i].size,why)){Text t;t.Add("TL_MODEL_REJECT profile=");t.Add(profile.key);t.Add(" reason=");t.Add(why);Log(t.s,true);continue;}
  char nativeVariant[256]={};Buffer palette;Span parts[18];uint32_t count=0;
  if(cfg.colors&&RootVariant(original,nativeVariant,sizeof(nativeVariant))&&Visuals::GameFile(nativeVariant,palette))count=Entries(palette,parts,18);
  const uint32_t variants=count>=3?count-2:1;
  for(uint32_t v=0;v<variants&&v<16;++v){Buffer model,variant,target;Text key;key.Add("r3tl");key.Num(i);key.Add("v");key.Num(v);
   if(count>=3){Text filename;filename.Add("data/hd/character/enemy/r3tl_palette_");filename.Num(i);filename.Add("_");filename.Num(v);filename.Add(".json");
    if(!Palette(palette,parts,count,v+2,variant)||!Visuals::Save(filename.s,variant.p,variant.size)||!Relink(enlarged,model,nativeVariant,filename.s))continue;
   }else if(!model.Add(enlarged.p,enlarged.size))continue;
   Text filename;filename.Add("data/hd/character/enemy/");filename.Add(key.s);filename.Add(".json");if(!Visuals::Save(filename.s,model.p,model.size))continue;
   if(entries.size&&!entries.Text(","))return false;if(!entries.Text("\"")||!entries.Text(key.s)||!entries.Text("\":\"")||!entries.Text(key.s)||!entries.Text("\""))return false;
   masks[i]|=static_cast<uint16_t>(1u<<v);++models;
  }
  Text t;t.Add("TL_MODEL_READY profile=");t.Add(profile.key);t.Add(" variants=");t.Num(variants);t.Add(" materialized_mask=");t.Num(masks[i]);t.Add(" size_percent=");t.Num(bosses[i].size);Log(t.s,!masks[i]);
 }
 if(!models)return false;Buffer merged;if(!Mapping(mapping,entries,merged)||!Visuals::Save("data/hd/character/monsters.json",merged.p,merged.size,true))return false;
 ready=true;Log("TL_VISUALS_READY: private scaled native models/palettes; stock models and status recolors preserved.");return true;
}
}
