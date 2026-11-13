from pathlib import Path
import csv, io, json, re, hashlib, zipfile
from functools import lru_cache
from fractions import Fraction

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'work/baal_map_progression/package/mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel'
OUT=Path(__file__).with_name('package')/'mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel'
checks=[]; report=[]
def check(name,ok):
    assert ok,name
    checks.append(name)
def read(p):
    return list(csv.DictReader(io.StringIO(p.read_text(encoding='utf-8-sig')),delimiter='\t'))
def write(p,source,rows,insertions=None):

    raw=source.read_bytes(); lines=raw.decode('utf-8-sig').splitlines(keepends=True)
    fields=lines[0].rstrip('\r\n').split('\t'); by={r[fields[0]]:r for r in rows}
    result=[lines[0]]
    for line in lines[1:]:
        old=line.rstrip('\r\n').split('\t'); name=old[0]
        ending='\r\n' if line.endswith('\r\n') else '\n'
        for row in (insertions or {}).get(name,[]):
            result.append('\t'.join(row[k] for k in fields)+ending)
        r=by[name]
        if all(r[k]==v for k,v in zip(fields,old)):
            result.append(line)
        else: result.append('\t'.join(r[k] for k in fields)+ending)
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_bytes((b'\xef\xbb\xbf' if raw.startswith(b'\xef\xbb\xbf') else b'')+''.join(result).encode())

for bank in ['', 'base/']:
    label=bank or 'main'; ms=read(SRC/bank/'monstats.txt'); tc=read(SRC/bank/'treasureclassex.txt')
    oldms={r['Id']:dict(r) for r in ms}; oldtc={r['Treasure Class']:dict(r) for r in tc}; td={r['Treasure Class']:r for r in tc}
    hp=[]; wardens=[]
    for r in ms:
        if not r['Id'].startswith('rmap_'): continue
        if re.fullmatch(r'rmap_m[a-z][1-6]_boss',r['Id']):
            wardens.append(r['Id']); continue
        if r['killable']!='1' or r['npc']=='1':continue
        for k in ['minHP','maxHP','MinHP(N)','MaxHP(N)','MinHP(H)','MaxHP(H)']:
            if r[k]:r[k]=str(max(1,(int(r[k])*3+2)//4))
        hp.append(r['Id'])
    check(label+' all 60 Wardens identified',len(wardens)==60)
    check(label+' 1094 map combat templates reduced',len(hp)==1094)
    check(label+' Warden records fully unchanged',all(next(r for r in ms if r['Id']==n)==oldms[n] for n in wardens))
    check(label+' only map HP cells changed',all((r==oldms[r['Id']]) if r['Id'] not in hp else all(v==oldms[r['Id']][k] for k,v in r.items() if k not in ['minHP','maxHP','MinHP(N)','MaxHP(N)','MinHP(H)','MaxHP(H)']) for r in ms))

    misc=read(ROOT/'work/loader_archive/excel'/bank/'misc.txt')
    mapcodes={r['code'] for r in misc if r['type']=='mapi'}
    @lru_cache(None)
    def map_leaves(n):
        if n in mapcodes:return frozenset([n])
        if n not in oldtc:return frozenset()
        return frozenset().union(*(map_leaves(oldtc[n]['Item'+str(i)]) for i in range(1,11) if oldtc[n]['Item'+str(i)]))
    changed_quality=[]


    for r in tc:
        n=r['Treasure Class']
        eligible=(n=='VDML Map Normal Gear' or bool(re.fullmatch(r'RMap T[1-6] Loot',n)) or (n.startswith('VDML Clean') and not map_leaves(n) and not r['ConditionCalc'] and int(r['Picks'] or 0)!=0))
        if not eligible:continue
        for k in ['Unique','Set']:
            q=int(r[k] or 0)
            r[k]=str(1024-max(1,((1024-q)*4+2)//5)) if q<1024 else str(q)
        changed_quality.append(n)


    def wrapper(name,target,q):
        r={k:'' for k in tc[0]};r.update({'Treasure Class':name,'Picks':'-1','Unique':str(q),'Set':str(q),'NoDrop':'0','Item1':target,'Prob1':'1','*eol':'0'});return r
    added=[wrapper('VDMB Elite Gear','R3DB O0284',845),wrapper('VDMB Warden Gear','R3DB O0465',1004)]
    for r in tc:
        n=r['Treasure Class']
        if n.startswith('RMap ') and n.endswith(' Elite') and r['Item1']=='R3DB O0284':r['Item1']='VDMB Elite Gear'
        if re.fullmatch(r'RMap T[1-6] Boss',n):
            check(label+' boss gear original '+n,r['Item3']=='R3DB O0465')
            r['Item3']='VDMB Warden Gear'
    for name,weight in [('VDML Normal Special',50000),('VDML Elite Special',250000),('VDML Warden Special',5000000)]:
        r=td[name]; empty=next(i for i in range(1,11) if r['Item'+str(i)]=='VDML Empty')
        slot=next(i for i in range(1,11) if not r['Item'+str(i)])
        r['Prob'+str(empty)]=str(int(r['Prob'+str(empty)])-weight)
        r['Item'+str(slot)]='eoe';r['Prob'+str(slot)]=str(weight)
        check(label+' '+name+' total weight conserved',sum(int(r['Prob'+str(i)] or 0) for i in range(1,11))==100000000)
        check(label+' '+name+' previous real material weights unchanged',all(r['Prob'+str(i)]==v for i in range(1,11) if (v:=oldtc[name]['Prob'+str(i)]) and oldtc[name]['Item'+str(i)]!='VDML Empty'))
    dest=OUT/bank
    write(dest/'monstats.txt',SRC/bank/'monstats.txt',ms)
    write(dest/'treasureclassex.txt',SRC/bank/'treasureclassex.txt',tc,{'VDML Map Normal Gear':added})
    result=read(dest/'treasureclassex.txt'); new={r['Treasure Class']:r for r in result};idx={r['Treasure Class']:i for i,r in enumerate(result)}
    check(label+' exactly two reference wrappers added',len(result)==len(tc)+2)
    check(label+' existing row order retained',[r['Treasure Class'] for r in result if r['Treasure Class'] not in {a['Treasure Class'] for a in added}]==[r['Treasure Class'] for r in tc])
    check(label+' all TC references resolve backwards',all(idx[r['Item'+str(i)]]<idx[r['Treasure Class']] for r in result for i in range(1,11) if r['Item'+str(i)] in idx))
    old_refs={o['Item'+str(j)] for o in tc for j in range(1,11)}
    check(label+' every new item reference valid',all(r['Item'+str(i)] in new or r['Item'+str(i)]=='eoe' for r in result for i in range(1,11) if r['Item'+str(i)] and r['Item'+str(i)] not in old_refs))
    check(label+' no gear wrapper contains map drops',all(not map_leaves(a['Item1']) for a in added))
    check(label+' all sustain currency and tier pools unchanged',all(new[n]==r for n,r in oldtc.items() if n.startswith('RMap ') and any(x in n for x in ['Sustain','Currency','Tier ','Gilded'])))
    check(label+' Baal and Herald correction retained',all(new[n]==r for n,r in oldtc.items() if n in ['R3Map Baal Test','R3CAS N5 W24','R3DB 0203','VD Sovereign Maps','R3TL Root'] or n.startswith('R3HM ') or 'item_strength_percentC' in r['ConditionCalc']))
    @lru_cache(None)
    def reachable(n):
        if n not in new:return frozenset([n])
        r=new[n]
        if not int(r['Picks'] or 0):return frozenset()
        return frozenset().union(*(reachable(r['Item'+str(i)]) for i in range(1,11) if r['Item'+str(i)] and int(r['Prob'+str(i)] or 0)>0))
    roots={v for r in ms if r['Id'].startswith('rmap_') for k,v in r.items() if k.startswith('TreasureClass') and k.endswith('(H)') and v}
    check(label+' all populated map Hell loot paths can drop both materials',all({'eoe','rhm'}<=reachable(n) for n in roots))
    check(label+' exactly 66 map elite gear routes',sum(r['Item1']=='VDMB Elite Gear' for r in result)==66)
    check(label+' exactly six Warden gear routes',sum(r['Item3']=='VDMB Warden Gear' for r in result)==6)
    changes=[{'row':n,'cells':{k:[oldtc[n][k],v] for k,v in r.items() if oldtc[n][k]!=v}} for n,r in new.items() if n in oldtc and r!=oldtc[n]]
    allowed=set(changed_quality)|{'VDML Normal Special','VDML Elite Special','VDML Warden Special'}|{n for n in oldtc if (n.startswith('RMap ') and n.endswith(' Elite')) or re.fullmatch(r'RMap T[1-6] Boss',n)}
    check(label+' no unrelated TC rows changed',all(x['row'] in allowed for x in changes))
    report.append({'bank':label,'hp_templates':len(hp),'wardens_unchanged':len(wardens),'loot_roots_checked':len(roots),'quality_rows':len(changed_quality),'tc_changes':changes,'added_wrappers':added,'hp_ids':hp,'intentional_no_loot_helpers_unchanged':[r['Id'] for r in ms if r['Id'] in hp and not r['TreasureClass(H)']]})

summary={'checks_passed':len(checks),'checks':checks,'banks':report,'limitations':['Static table and graph validation only; native game not available.','67 templates without native loot (event guards/waves and suicide minion) retain their intentional no-loot settings.','Sovereign/plugin-spawned units using non-map MonStats records are outside this table-only HP reduction.'],'quality_coefficients':{'ordinary':[0,205],'elite':[800,845],'warden':[983,1004]},'essence_percent':{'normal':0.05,'elite':0.25,'warden':5}}
Path(__file__).with_name('validation.json').write_text(json.dumps(summary,indent=2))
print(json.dumps({k:summary[k] for k in ['checks_passed','limitations','quality_coefficients','essence_percent']},indent=2))
