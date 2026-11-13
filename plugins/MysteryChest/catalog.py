from pathlib import Path
import csv,json,hashlib,re
root=Path(__file__).parent;tables=root.parent/'tables/excel'
def rows(fn):return list(csv.DictReader(open(tables/(fn+'.txt'),encoding='utf-8-sig'),delimiter='\t'))
base={r['code']:r for fn in ['weapons','armor','misc'] for r in rows(fn) if r.get('code')}
names={r['Key']:r.get('enUS',r['Key']) for r in json.loads((root.parent/'strings/strings/item-names.json').read_text(encoding='utf-8-sig'))}
def clean(s):return re.sub('ÿc.','',s).replace('\n',' ')
rewards=[];excluded=[]
for fn,quality in [('uniqueitems',7),('setitems',5)]:
 for idx,r in enumerate(x for x in rows(fn) if x['index']!='Expansion'):
  n=r['index'];code=r.get('code',r.get('item',''))
  if not code or code not in base:continue
  if n.startswith(('R3MCU','R3MCS','R3ABU','R3ABS','Crafted ','PreCrafted ','Precrafted ')) or r.get('disabled')=='1':excluded.append(n);continue
  rewards.append(dict(code=code,quality=quality,id=idx,name=clean(names.get(n,n)),key=n,category='unique' if quality==7 else 'set'))
p=next((root.parent/'unpacked/D2R_Vizjerei_Prism_Materials_Stash_Source_Checkpoint/work/prism_stash/package').rglob('bankexpansionlayouthd.json'));layout=json.loads(p.read_text(encoding='utf-8-sig'))
material=[]
def codes(x):
 if isinstance(x,dict):
  if 'itemCode' in x.get('fields',{}):material.append(x['fields']['itemCode'])
  for v in x.values():codes(v)
 elif isinstance(x,list):
  for v in x:codes(v)
def find(x):
 if isinstance(x,dict):
  if x.get('name')=='advancedstash_materials':codes(x);return
  for v in x.values():find(v)
 elif isinstance(x,list):
  for v in x:find(v)
find(layout)
runes={f'r{i:02}' for i in range(1,34)}
gems={r['code'] for r in rows('misc') if r.get('type','').startswith('gem') and r.get('type')!='gemx'}
maps={r['code'] for r in rows('misc') if re.fullmatch(r'RMap .+ T[1-6] V2',r.get('name',''))}
plain=set(material)|runes|gems|maps;unique_codes={r['code'] for r in rewards if r['quality']==7}
for code in sorted(plain):
 assert code in base,code
 if code in unique_codes and base[code].get('unique')=='1':continue
 rewards.append(dict(code=code,quality=4 if code in maps else 2,id=0xFFFFFFFF,name=clean(names.get(base[code].get('namestr',''),base[code]['name'])),key=base[code]['name'],category='map' if code in maps else 'misc'))
assert len({(r['code'],r['quality'],r['id']) for r in rewards})==len(rewards)
assert len(maps)==60 and len(runes)==33 and len(gems)==35
assert all(any(r['key']==n for r in rewards) for n in ['Annihilus','Hellfire Torch','Prism of Omniscience','Shard of Mastery'])
for c in material:assert any(r['code']==c for r in rewards),c
payload=json.dumps(rewards,sort_keys=True).encode();token=int.from_bytes(hashlib.sha256(payload).digest()[:8],'little')
lines=['#pragma once','#include <cstdint>','struct Reward {uint32_t code,quality,record;};','inline constexpr Reward Rewards[] = {']
for r in rewards:lines.append('{%du,%du,%du}, // %s'%(int.from_bytes(r['code'].ljust(4,' ').encode(),'little'),r['quality'],r['id'],r['key']))
lines+=['};','inline constexpr uint64_t CatalogToken=%duLL;'%token]
(root/'catalog.hpp').write_text('\n'.join(lines)+'\n');(root/'catalog.json').write_text(json.dumps(rewards,ensure_ascii=False,indent=2))
m={'count':len(rewards),'counts':{c:sum(r['category']==c for r in rewards) for c in ['unique','set','misc','map']},'chance_percent':100/len(rewards),'runes':len(runes),'gems':len(gems),'maps':len(maps),'materials':material,'excluded':excluded,'sha256':hashlib.sha256(payload).hexdigest()}
(root/'catalog_manifest.json').write_text(json.dumps(m,indent=2));print({k:v for k,v in m.items() if k not in ['excluded','materials']})
