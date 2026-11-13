import csv, collections, re
from pathlib import Path

def load(p):
 with p.open(encoding='utf-8-sig',newline='') as f:
  r=csv.DictReader(f,delimiter='\t');return r.fieldnames,list(r)
def children(r):
 return [(i,r[f'Item{i}'].split(',')[0]) for i in range(1,11) if r[f'Item{i}'] and int(r[f'Prob{i}'] or 0)]
def graph(rows,groups=True):
 d={r['Treasure Class']:r for r in rows if r['Treasure Class'] not in ('','Expansion')}
 edges={n:{c for _,c in children(r)} for n,r in d.items()}
 if groups:
  gp=collections.defaultdict(list)
  for n,r in d.items():
   if int(r['group'] or 0):gp[r['group']].append(n)
  for group in gp.values():
   group.sort(key=lambda n:int(d[n]['level'] or 0))
   for a,b in zip(group,group[1:]):edges[a].add(b)
 rev=collections.defaultdict(set)
 for n,cs in edges.items():
  for c in cs:rev[c].add(n)
 return d,edges,rev
def reach(edges,roots):
 seen=set(roots); todo=list(seen)
 while todo:
  for c in edges.get(todo.pop(),()):
   if c not in seen:seen.add(c);todo.append(c)
 return seen
def classify(p,d):
 _,m=load(p/'monstats.txt');_,su=load(p/'superuniques.txt')
 normal=set();protected=set(); edits=[]
 for row in m:
  boss=any(row.get(k)=='1' for k in ('boss','primeevil','SetBoss')) or row['Id'].startswith(('rmap_treasure','colossal','uber','diabloclone','r3tl'))
  for k,v in row.items():
   if not k.startswith('TreasureClass') or v not in d:continue
   if boss or 'Herald' in k:protected.add(v)
   else:normal.add(v);edits.append((row,k,v))
 protected|={v for r in su for k,v in r.items() if k.startswith('TC') and v in d}
 return m,normal,protected,edits

if __name__=='__main__':
 for p in (Path('fresh/excel'),Path('actual/excel/base')):
  _,t=load(p/'treasureclassex.txt')
  d,e,r=graph(t);_,direct,rev=graph(t,False)
  m,normal,special,edits=classify(p,d)
  special|={n for n in d if n not in normal and not rev.get(n)}
  special|={n for n in d if re.search('Warden|Sparkly|Sovereign|Mystery|Chest|^R3VA |^R3TTR |^R3TL ',n,re.I)}
  n=reach(e,normal); s=reach(e,special); aff=reach(r,{'rhm','eoe'})
  print(p, 'roots',len(normal),len(special),'affected',len(aff&n),'shared',len(aff&n&s),'affected roots shared',len(aff&normal&s))
  print('direct normal only',sorted((n-s)&aff&set(rev['rhm']|rev['eoe']))[:80])
  print('shared target',len(n&s&set(rev['rhm']|rev['eoe'])))
  print('shared grouped',sum(bool(int(d[k]['group'] or 0)) for k in aff&n&s&d.keys()))
  for a in ['VDML Normal Special','VDML Elite Special','VDML Warden Special']:
   print(a,a in n,a in s)

CURRENCIES={'rhm':'R3 Half Malus','eoe':'R3 Half Essence'}
REL=Path('mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel')
OUT=Path('fresh_out')
def put(path,h,rows,source):
 path.parent.mkdir(parents=True,exist_ok=True)
 encoding='utf-8-sig' if source.read_bytes().startswith(b'\xef\xbb\xbf') else 'utf-8'
 with path.open('w',encoding=encoding,newline='') as f:
  w=csv.DictWriter(f,h,delimiter='\t',lineterminator='\r\n');w.writeheader();w.writerows(rows)

def repair(bank):
 p=Path('fresh/excel')/bank
 h,t=load(p/'treasureclassex.txt')
 d,e,r=graph(t,False)
 six={c for n in ('Talic','Madawc','Korlic') for _,c in children(d[f'VDTZ Uber {n}'])}
 weapons={c for _,c in children(d['R3VA Weapons'])}
 restricted=six|weapons|{'Annihilus','Hellfire Torch'}
 allowed={x:set() for x in restricted}
 for name,ancient in zip(('Talic','Madawc','Korlic'),('RHM1153','RHM1154','RHM1155')):
  for _,x in children(d[f'VDTZ Uber {name}']):
   allowed[x].update((f'VDTZ Uber {name}',f'Uber {name}',ancient))
 for x in weapons:allowed[x].add('R3VA Weapons')
 allowed['Annihilus'].update(('ROP','ROP (N)','ROP (H)','VDTZ Annihilus','RHM1055','RHM1345'))
 allowed['Hellfire Torch'].update(('VDTZ Hellfire Torch','RHM1346'))
 assert len(six)==6 and len(weapons)==8
 def write_items(row,items):
  for i in range(1,11):
   item,w=items[i-1] if i<=len(items) else ('','')
   row[f'Item{i}']=item;row[f'Prob{i}']=str(w)
 def weight(row):return sum(int(row[f'Prob{i}']) for i,_ in children(row))
 empty='R3ET Empty'
 assert d[empty]['Picks']=='1' and d[empty]['NoDrop']=='1' and not children(d[empty])
 removed=[]
 for n,row in d.items():
  bad={i for i,c in children(row) if c in restricted and n not in allowed[c]}
  if not bad:continue
  removed.extend((n,c) for i,c in children(row) if i in bad)
  if n.startswith(('R3EL Unique L','R3TL AllUL','R3TL BestUL')):
   assert row['Picks']=='1'
   items=[(row[f'Item{i}'],row[f'Prob{i}']) for i,_ in children(row) if i not in bad]
   assert items
   write_items(row,items)
  else:
   for i in bad:
    assert list(d).index(empty)<list(d).index(n),n
    row[f'Item{i}']=empty

 for n,row in d.items():
  if n.startswith(('R3EL Unique G','R3TL AllUG','R3TL BestUG')) or n in ('R3EL Unique','R3TL AllUTree','R3TL BestUTree'):
   for i,c in children(row):
    assert c in d
    row[f'Prob{i}']=str(weight(d[c]))


 for n in set.union(*(allowed[x] for x in six)):
  d[n]['group']=''
 uh,us=load(p/'uniqueitems.txt')
 assert {u['index'] for u in us if u['index'] in restricted}==restricted
 for u in us:
  if u['index'] in restricted and u['spawnable']=='1':u['spawnable']='0'
 put(OUT/REL/bank/'uniqueitems.txt',uh,us,p/'uniqueitems.txt')

 d,e,r=graph(t)
 _,direct,_=graph(t,False)
 m,normal,special,refs=classify(p,d)
 aff=reach(r,CURRENCIES)
 needed=reach(e,normal)&aff&d.keys()


 groups={d[n]['group'] for n in needed if int(d[n]['group'] or 0)}
 needed|={n for n,row in d.items() if row['group'] in groups}
 maximum=max(int(x['group'] or 0) for x in d.values())
 newgroups={g:str(i) for i,g in enumerate(sorted(groups,key=int),maximum+1)}
 assert maximum+len(newgroups)<32768
 names={n:f'R3 Half {i:04d}' for i,n in enumerate(n for n in d if n in needed)}
 assert not (set(names.values())|set(CURRENCIES.values())) & d.keys()
 def newrow(n):
  row={k:'' for k in h};row.update({'Treasure Class':n,'Picks':'1','NoDrop':'0','*eol':'0'});return row
 def with_suffix(value,new):
  parts=value.split(',',1)
  return new+(','+parts[1] if len(parts)>1 else '')
 newrows=[]
 for c,n in CURRENCIES.items():
  row=newrow(n);write_items(row,[(c,1),(empty,1)]);newrows.append(row)
 complete=set();stack=set()
 def emit(n):
  if n in complete:return
  assert n not in stack,n
  stack.add(n)
  row=d[n].copy();row['Treasure Class']=names[n]
  if row['group'] in newgroups:row['group']=newgroups[row['group']]
  for i,c in children(row):
   if c in names:
    emit(c);row[f'Item{i}']=with_suffix(row[f'Item{i}'],names[c])
   elif c in CURRENCIES:row[f'Item{i}']=with_suffix(row[f'Item{i}'],CURRENCIES[c])
  newrows.append(row);stack.remove(n);complete.add(n)
 for n in names:emit(n)
 for row,k,v in refs:
  if v in names:row[k]=names[v]

 positions={x['Treasure Class']:i for i,x in enumerate(newrows)}
 for g in groups:
  original=[n for n,x in d.items() if x['group']==g]
  assert original==sorted(original,key=lambda n:positions[names[n]]),g
 t.extend(newrows)
 nd,ne,nr=graph(t)
 for row in newrows:
  for _,c in children(row):
   if c in positions:assert positions[c]<positions[row['Treasure Class']]
 for root in normal:
  assert (root in aff)==bool(set(CURRENCIES.values())&reach(ne,[names.get(root,root)])),root
 for root in special:
  assert not set(CURRENCIES.values())&reach(ne,[root]),root
 for term,roots in allowed.items():
  assert set(nr[term])==roots&r[term],(term,nr[term],roots)
 assert not restricted&reach(ne,['R3TL Root','R3EL Unique'])
 for name in ('Talic','Madawc','Korlic'):
  assert {c for _,c in children(d[f'VDTZ Uber {name}'])}<=reach(ne,[f'R3VA Final {name}'])
 mh,_=load(p/'monstats.txt')
 put(OUT/REL/bank/'monstats.txt',mh,m,p/'monstats.txt')
 put(OUT/REL/bank/'treasureclassex.txt',h,t,p/'treasureclassex.txt')
 print(bank or 'main',len(removed),'restricted references removed;',len(names),'cloned TC;',len(groups),'cloned groups;',sum(v in names for _,_,v in refs),'monster references')
 return restricted

if __name__=='__main__':
 assert repair('')==repair('base')
