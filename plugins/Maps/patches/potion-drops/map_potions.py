from pathlib import Path
from fractions import Fraction
from zipfile import ZipFile, ZIP_DEFLATED
import re
import balance as b

roots=('VDML Map Normal Gear','VDMB Elite Gear','VDMB Warden Gear')
is_pot=lambda x:bool(re.fullmatch(r'[hm]p[1-5]',x))
output='D2R_Maps_T1-T6_Potion_Balance_MODS.zip'
with ZipFile(output,'w',ZIP_DEFLATED) as archive:
 for bank in ('','base'):
  path=Path('fresh_out')/b.REL/bank/'treasureclassex.txt'
  raw=path.read_bytes()
  header,rows=b.load(path)
  d,e,r=b.graph(rows,False)
  _,full_edges,full_reverse=b.graph(rows)
  pots={x for x in r if is_pot(x)}
  impact=b.reach(r,pots)&b.reach(e,roots)&d.keys()
  assert impact==b.reach(full_reverse,pots)&b.reach(full_edges,roots)&d.keys()
  originals={n:d[n].copy() for n in d}
  replacements={}
  first=min(i for i,x in enumerate(rows) if x['Treasure Class'] in roots)
  added=[]
  for n,row in d.items():
   if n not in impact:continue
   assert all(c not in impact or c in replacements for _,c in b.children(row))
   name=n if n in roots else f'R3 Map Potion {len(added)+1:02d}'
   assert name==n or name not in d
   new=row.copy()
   new['Treasure Class']=name
   if n not in roots and row['group']:

    assert not full_edges[n]-e[n]
    new['group']=''
   for i,c in b.children(row):
    if c in replacements:
     assert new[f'Item{i}']==c
     new[f'Item{i}']=replacements[c]
   if any(is_pot(c) for _,c in b.children(row)):
    assert new['Picks']=='1'
    entries=[(new[f'Item{i}'],int(new[f'Prob{i}'])) for i in range(1,11) if new[f'Item{i}']]
    weights=[w for _,w in entries]
    converted=0
    for family in ('hp','mp'):
     indexes=[i for i,(x,w) in enumerate(entries) if is_pot(x) and x.startswith(family)]
     target=(sum(weights[i] for i in indexes)+1)//3
     assert sum(weights[i]-1 for i in indexes)>=target
     for _ in range(target):
      i=max((i for i in indexes if weights[i]>1),key=lambda i:Fraction(weights[i],entries[i][1]))
      weights[i]-=1
     converted+=target
    regular=(3*converted+2)//5
    full=converted-regular
    revised=list(zip((x for x,_ in entries),weights))
    for code,extra in [('rvs',regular),('rvl',full)]:
     if not extra:continue
     for i,(item,w) in enumerate(revised):
      if item==code:
       revised[i]=(code,w+extra)
       break
     else:revised.append((code,extra))
    assert len(revised)<=10 and sum(w for _,w in revised)==sum(w for _,w in entries)
    for i in range(1,11):
     item,weight=revised[i-1] if i<=len(revised) else ('','')
     new[f'Item{i}']=item
     new[f'Prob{i}']=str(weight)
   replacements[n]=name
   if n in roots:d[n].update(new)
   else:added.append(new)

  assert all(x in roots or x.startswith(('RMap ','VDML ','VD Herald Map','R3 Half ')) for x in b.reach(r,roots))
  _,monsters=b.load(Path('fresh_out')/b.REL/bank/'monstats.txt')
  touched_roots=b.reach(r,roots)
  for row in monsters:
   if any(v in touched_roots for k,v in row.items() if k.startswith('TreasureClass')):
    assert re.fullmatch(r'rmap_(?:v2_)?m[dkcfwshtei][1-6]_.+',row['Id']),row['Id']

  lines=raw.splitlines(keepends=True)
  assert len(lines)==len(rows)+1
  def encode(row):
   return ('\t'.join(row[k] for k in header)+'\r\n').encode()
  for i,row in enumerate(rows,1):
   if row['Treasure Class'] in roots:lines[i]=encode(row)
  lines[first+1:first+1]=[encode(x) for x in added]
  final=rows[:first]+added+rows[first:]
  idx={row['Treasure Class']:i for i,row in enumerate(final)}
  for row in added+[d[n] for n in roots]:
   assert all(c not in idx or idx[c]<idx[row['Treasure Class']] for _,c in b.children(row)),row['Treasure Class']
  assert all(d[n]==row for n,row in originals.items() if n not in roots)
  for tier in range(1,7):
   for kind,root in [('Normal',roots[0]),('Elite',roots[1]),('Boss',roots[2])]:
    assert root in b.reach(e,[f'RMap T{tier} {kind}'])
  archive.writestr((b.REL/bank/'treasureclassex.txt').as_posix(),b''.join(lines))
  print(bank or 'main',len(added),'private TC rows',len(roots),'existing map roots')
with ZipFile(output) as archive:assert archive.testzip() is None
