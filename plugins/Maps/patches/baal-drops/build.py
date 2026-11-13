from pathlib import Path
import csv,io,json,hashlib
from fractions import Fraction
ROOT=Path(__file__).resolve().parents[2]
SRC_TC=ROOT/'work/herald_native_tables/package/mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel'
SRC_MS=ROOT/'work/loader_archive/excel'
DEST=Path(__file__).with_name('package')/'mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel'
checks=[];changes=[]
def check(name,ok):
 assert ok,name
 checks.append(name)
def read(data):return list(csv.DictReader(io.StringIO(data.decode('utf-8-sig')),delimiter='\t'))
def edit(source,target,updates,bank):
 raw=source.read_bytes();before=read(raw);lines=raw.decode('utf-8-sig').splitlines(keepends=True);head=lines[0].rstrip('\r\n').split('\t');ix={k:i for i,k in enumerate(head)};seen=set();diff=[]
 for n,line in enumerate(lines[1:],1):
  fields=line.rstrip('\r\n').split('\t');name=fields[0]
  if name not in updates:continue
  seen.add(name)
  for col,(expected,replacement) in updates[name].items():
   check(bank+' '+source.name+' original '+name+' '+col,fields[ix[col]]==expected)
   fields[ix[col]]=replacement;diff.append({'row':name,'column':col,'old':expected,'new':replacement})
  lines[n]='\t'.join(fields)+ ('\r\n' if line.endswith('\r\n') else '\n')
 check(bank+' '+source.name+' edited exact rows',seen==set(updates))
 data=''.join(lines).encode();data=(b'\xef\xbb\xbf'+data) if raw.startswith(b'\xef\xbb\xbf') else data
 after=read(data)
 check(bank+' '+source.name+' row count unchanged',len(before)==len(after))
 check(bank+' '+source.name+' names and order unchanged',[r[head[0]] for r in before]==[r[head[0]] for r in after])
 actual=[(a[head[0]],k,a[k],b[k]) for a,b in zip(before,after) for k in a if a[k]!=b[k]]
 check(bank+' '+source.name+' no unrelated edits',set(actual)=={(d['row'],d['column'],d['old'],d['new']) for d in diff})
 target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 changes.append({'bank':bank,'file':source.name,'edits':diff,'before_sha256':hashlib.sha256(raw).hexdigest(),'after_sha256':hashlib.sha256(data).hexdigest()})
 return before,after
for bank in ['', 'base/']:
 label=bank or 'main'
 tc_updates={

  'R3Map Baal Test':{'Item1':('R3CAS N5 W23','VD Sovereign Maps'),'Item2':('R3Map Starter','R3CAS N5 W23')},
  'R3CAS N5 W24':{'Picks':('-2','-3'),'Item1':('R3DB 0202','R3HM T1 Themes'),'Item2':('R3CAS N5 Roll','R3DB 0202'),'Item3':('','R3CAS N5 Roll'),'Prob3':('','1')},
  'R3DB 0203':{'Item1':('R3HM T1 Themes','VD Sovereign Maps')}
 }
 old,rows=edit(SRC_TC/bank/'treasureclassex.txt',DEST/bank/'treasureclassex.txt',tc_updates,label)
 _,ms=edit(SRC_MS/bank/'monstats.txt',DEST/bank/'monstats.txt',{
  'baalcrab':{'TreasureClassChamp(H)':('R3CAS N5 W23','R3Map Baal Test'),'TreasureClassUnique(H)':('R3CAS N5 W23','R3Map Baal Test')}
 },label)
 tc={r['Treasure Class']:r for r in rows};oldtc={r['Treasure Class']:r for r in old};baal=next(r for r in ms if r['Id']=='baalcrab')


 index={r['Treasure Class']:i for i,r in enumerate(rows)}
 forward=[(i,r['Treasure Class'],f'Item{j}',r[f'Item{j}'],index[r[f'Item{j}']]) for i,r in enumerate(rows) for j in range(1,11) if r[f'Item{j}'] in index and index[r[f'Item{j}']]>=i]
 check(label+' every treasure-class reference precedes its caller',not forward)
 check(label+' replacement T1 pool identical to starter pool',{k:v for k,v in tc['R3HM T1 Themes'].items() if k!='Treasure Class'}=={k:v for k,v in tc['R3Map Starter'].items() if k!='Treasure Class'})
 check(label+' quest uses earlier T1 pool',index['R3HM T1 Themes']<index['R3CAS N5 W24'])

 check(label+' repeat paths share same map wrapper',all(baal[k]=='R3Map Baal Test' for k in ['TreasureClass(H)','TreasureClassChamp(H)','TreasureClassUnique(H)']))
 check(label+' native quest route retained',baal['TreasureClassQuest(H)']=='R3CAS N5 W24')
 check(label+' all three TZ routes retained',all(baal[k]=='R3DB 0203' for k in ['TreasureClassDesecrated(H)','TreasureClassDesecratedChamp(H)','TreasureClassDesecratedUnique(H)']))
 def slots(n):
  r=tc[n];return [(r['Item'+str(i)],r['Prob'+str(i)]) for i in range(1,11) if r['Item'+str(i)]]
 check(label+' repeat map rolled before unchanged normal boss loot',tc['R3Map Baal Test']['Picks']=='-2' and slots('R3Map Baal Test')==[('VD Sovereign Maps','1'),('R3CAS N5 W23','1')])
 check(label+' quest starter first, original quest loot rolls retained',tc['R3CAS N5 W24']['Picks']=='-3' and slots('R3CAS N5 W24')==[('R3HM T1 Themes','1'),('R3DB 0202','1'),('R3CAS N5 Roll','1')])
 check(label+' TZ lottery replaces only prior guaranteed map',tc['R3DB 0203']['Picks']=='-3' and slots('R3DB 0203')==[('VD Sovereign Maps','1'),('R3DB 0203 R','1'),('R3DB O0463','1')])
 check(label+' shared Sovereign lottery untouched',tc['VD Sovereign Maps']==oldtc['VD Sovereign Maps'])
 lottery=tc['VD Sovereign Maps'];weights=[int(lottery['Prob'+str(i)]) for i in range(1,7)];total=sum(weights)
 check(label+' exact five Sovereign tier probabilities', [Fraction(w,total) for w in weights[:5]]==[Fraction(1,200),Fraction(1,400),Fraction(1,1000),Fraction(1,2000),Fraction(1,4000)])
 check(label+' any map probability exactly 37/4000',Fraction(sum(weights[:5]),total)==Fraction(37,4000))
 check(label+' no-map branch 3963/4000',weights[5]==3963 and total==4000 and lottery['Item6']=='R3HM No Map')
 check(label+' all Herald predicates from successful correction untouched',all(tc[n]==r for n,r in oldtc.items() if 'item_strength_percentC' in r['ConditionCalc'] or n.startswith('R3HM ') or n=='R3TL Root'))

 before_refs={r['Item'+str(i)] for r in old for i in range(1,11)}
 new_refs={r['Item'+str(i)] for r in rows for i in range(1,11)}
 check(label+' no new unresolved reference',all(n in tc for n in new_refs-before_refs))
 def paths_to(root,target):
  def visit(n,anc):
   if n==target:return 1
   if n not in tc:return 0
   assert n not in anc,(n,anc)
   return sum(visit(child,anc|{n}) for child,weight in slots(n))
  return visit(root,set())
 for root in ['R3Map Baal Test','R3DB 0203']:
  check(label+' '+root+' has exactly one Sovereign map roll',paths_to(root,'VD Sovereign Maps')==1)
  check(label+' '+root+' no guaranteed starter remains',paths_to(root,'R3Map Starter')==0)
 check(label+' quest exactly one guaranteed starter',paths_to('R3CAS N5 W24','R3HM T1 Themes')==1)
 check(label+' quest has no additional Sovereign map lottery',paths_to('R3CAS N5 W24','VD Sovereign Maps')==0)

 for tier in range(1,6):
  r=tc['R3HM T'+str(tier)+' Themes'];check(label+f' map tier {tier} theme pool picks one',r['Picks']=='1')
report={'checks_passed':len(checks),'checks':checks,'changes':changes,'probabilities_percent':{'quest_starter':100,'repeat_T1':0.5,'repeat_T2':0.25,'repeat_T3':0.1,'repeat_T4':0.05,'repeat_T5':0.025,'repeat_any':0.925},'limits':['Static table/link/probability regression only. Actual native quest selection and in-game rolls not tested locally.','Native quest drop eligibility is preserved; no per-character quest tracking plugin added.']}
Path(__file__).with_name('validation.json').write_text(json.dumps(report,indent=2));print('checks passed',len(checks))
