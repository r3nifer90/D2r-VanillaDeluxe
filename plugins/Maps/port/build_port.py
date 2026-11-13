from pathlib import Path
import csv, io, json, re, struct, hashlib, shutil, collections, sys
import pefile, capstone

ROOT = Path(__file__).resolve().parents[1]
DONOR = ROOT / 'donor_work'
TARGET = ROOT / 'target_original/mods/VanillaDeluxe/VanillaDeluxe.mpq'
PLUGIN = ROOT/'inspect_new/d2rloader/plugins/d2rl-maps.dll'
CONFIG = ROOT/'inspect_new/d2rloader/config/maps.toml'
OUT = ROOT / 'port_build'
MPQ = OUT / 'mods/VanillaDeluxe/VanillaDeluxe.mpq'
DEX = DONOR / 'data/global/excel'
TEX = TARGET / 'data/global/excel'
LEVEL_SHIFT = 9
WORLD_Y_SHIFT = 4000
AUDIT = {'binary_patches': [], 'tables': {}, 'warnings': [], 'maps': []}

class Table:
    def __init__(self, path):
        self.path = Path(path)
        data = list(csv.reader(io.StringIO(self.path.read_text(encoding='utf-8-sig')), delimiter='\t'))
        self.header, self.rows = data[0], data[1:]
        self._columns={}
        for i,h in enumerate(self.header):self._columns.setdefault(h.lower(),i)
        self._by_cache={}
        self.rows = [r + ['']*(len(self.header)-len(r)) for r in self.rows if r]
        assert all(len(r)==len(self.header) for r in self.rows), str(path)
    def col(self, k): return self._columns[k.lower()]
    def has(self,k): return k.lower() in self._columns
    def get(self, r, k, default=''): return r[self.col(k)] if self.has(k) else default
    def set(self, r, **fields):
        for k,v in fields.items(): r[self.col(k)] = str(v)
        self._by_cache.clear()
        return r
    def blank(self): return ['']*len(self.header)
    def dictionary(self, r): return dict(zip(self.header, r))
    def records(self): return [self.dictionary(r) for r in self.rows]
    def by(self, key):
        cached=self._by_cache.get(key)
        if cached is None or cached[0]!=len(self.rows):
            index=self.col(key);cached=(len(self.rows),{r[index]:r for r in self.rows if r[index]});self._by_cache[key]=cached
        return cached[1]
    def append_from(self, other, r):
        out=[]
        for i,k in enumerate(self.header):
            occurrence=sum(h.lower()==k.lower() for h in self.header[:i])
            cols=[j for j,h in enumerate(other.header) if h.lower()==k.lower()]
            out.append(r[cols[occurrence]] if len(cols)>occurrence else '')
        self.rows.append(out)
        return out
    def save(self, name=None):
        name = name or self.path.name
        b=('\r\n'.join('\t'.join(r) for r in [self.header]+self.rows)+'\r\n').encode('utf-8')
        dest=MPQ/'data/global/excel'/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(b)
        base=dest.parent/'base'/name;base.parent.mkdir(parents=True,exist_ok=True)
        if name=='objpreset.txt':
            bank=BASE_OBJECT_PRESETS
        elif (TEX/'base'/name).exists():
            bank=Table(TEX/'base'/name);original=Table(TEX/name)
            if name!='treasureclassex.txt':
                assert len(bank.rows)<=len(original.rows),(name,'base has extra indexed records')
                for r in original.rows[len(bank.rows):]:bank.append_from(original,r)
            for r in self.rows[len(original.rows):]:bank.append_from(self,r)
            if name=='monstats.txt':bank.set(bank.by('Id')['baalcrab'],**{'TreasureClass(H)':'R3Map Baal Test'})
        else:
            bank=self
        basebytes=('\r\n'.join('\t'.join(r) for r in [bank.header]+bank.rows)+'\r\n').encode('utf-8')
        base.write_bytes(basebytes)
        AUDIT['tables'][name]={'rows':len(self.rows),'sha256':hashlib.sha256(b).hexdigest()}
        AUDIT['tables'][name]['base_rows']=len(bank.rows)

def native(table): return [r for r in table.rows if not any(v.strip()=='Expansion' for v in r)]
def indices(table,key): return {table.get(r,key):i for i,r in enumerate(native(table))}
def read(root,name): return Table(root/(name+'.txt'))
def num(x): return int(x or 0)

D={}; T={}; M={}
BASE_OBJECT_PRESETS=None
def tables(name):
    if name not in D:D[name]=read(DEX,name)
    if name not in T:T[name]=read(TEX,name)
    return D[name],T[name]

def tagged(name,key,predicate):
    d,t=tables(name); assert d.has(key) and t.has(key),(name,key)
    names=t.by(key); added=[]
    for r in d.rows:
        k=d.get(r,key)
        if predicate(k,r):
            assert k not in names, (name,'collision',k)
            new=t.append_from(d,r); names[k]=new;added.append(new)
    M[name]=t
    return d,t,added

def preserve_indexed(name,key,idcol,first,predicate,last_count=None):
    d,t=tables(name)
    assert len(t.rows)<=first, (name,len(t.rows),first)
    while len(t.rows)<first:
        r=t.blank();t.set(r,**{key:f'r3map_reserved_{name}_{len(t.rows)}'})
        if idcol in t.header:t.set(r,**{idcol:len(t.rows)})
        if '*eol' in t.header:t.set(r,**{'*eol':'0'})
        t.rows.append(r)
    for i,r in enumerate(d.rows):
        if predicate(d.get(r,key)):
            assert len(t.rows)==i, (name,'source indices not contiguous',i,len(t.rows))
            t.append_from(d,r)
    if last_count:
        while len(t.rows)<last_count:
            r=t.blank();t.set(r,**{key:f'r3map_reserved_{name}_{len(t.rows)}'})
            if idcol in t.header:t.set(r,**{idcol:len(t.rows)})
            if '*eol' in t.header:t.set(r,**{'*eol':'0'})
            t.rows.append(r)
    M[name]=t
    return d,t

def copy_asset(rel):
    p=DONOR/rel; q=MPQ/rel
    assert p.is_file(),p
    q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)

def main_tables():

    d,t,added=tagged('levels','Name',lambda k,r:k.startswith('RMAP '))
    assert len(added)==120
    oldids=indices(t,'Id')
    for r in added:
        old=num(t.get(r,'Id'));t.set(r,Id=old+LEVEL_SHIFT,OffsetY=num(t.get(r,'OffsetY'))+WORLD_Y_SHIFT)
        for i in range(8):
            k='Vis'+str(i);v=num(t.get(r,k))
            if 166<=v<=285:t.set(r,**{k:v+LEVEL_SHIFT})
        AUDIT['maps'].append({'id':old+LEVEL_SHIFT,'name':t.get(r,'Name')})
    ids=[t.get(r,'Id') for r in t.rows if t.get(r,'Id').isdigit()]
    assert len(ids)==len(set(ids))

    d,t=tables('lvlprest');start=max(num(t.get(r,'Def')) for r in t.rows)+1
    prest_map={};added=[]
    for r in d.rows:
        if d.get(r,'Name').startswith('RMAP '):
            old=num(d.get(r,'Def'));prest_map[old]=start;new=t.append_from(d,r)
            t.set(new,Def=start);start+=1
            lid=num(t.get(new,'LevelId'))
            if 166<=lid<=285:t.set(new,LevelId=lid+LEVEL_SHIFT)
            added.append(new)
    M['lvlprest']=t
    AUDIT['preset_map']=prest_map
    for name,key in [('lvlmaze','Name')]:
        d,t,add=tagged(name,key,lambda k,r:k.startswith('RMAP '))
        for r in add:t.set(r,Level=num(t.get(r,'Level'))+LEVEL_SHIFT)


    preserve_indexed('skills','skill','*Id',537,lambda k:k.startswith('rmap_'))
    preserve_indexed('states','state','*ID',293,lambda k:k.startswith('rmap_'))
    preserve_indexed('monprop','Id','',51,lambda k:k.startswith('rmap_'),last_count=508)
    d,t,monsters=tagged('monstats','Id',lambda k,r:k.startswith('rmap_'))
    dids=indices(d,'Id');tids=indices(t,'Id');mon_map={dids[k]:tids[k] for k in dids if k.startswith('rmap_')}
    for r in monsters:t.set(r,**{'*hcIdx':tids[t.get(r,'Id')]})
    AUDIT['monster_map']=mon_map
    d,t,_=tagged('monstats2','Id',lambda k,r:k.startswith('rmap_'))
    d,t,uniques=tagged('superuniques','Superunique',lambda k,r:k.startswith('rmap_'))
    di=indices(d,'Superunique');ti=indices(t,'Superunique');su_map={di[k]:ti[k] for k in di if k.startswith('rmap_')}

    for r in uniques:t.set(r,hcIdx=ti[t.get(r,'Superunique')])
    AUDIT['superunique_map']=su_map
    d,t,places=tagged('monpreset','Place',lambda k,r:k.startswith('rmap_'))

    oldact=[r for r in d.rows if d.get(r,'Act')=='5'];newact=[r for r in t.rows if t.get(r,'Act')=='5']
    oldplaces={d.get(r,'Place'):i for i,r in enumerate(oldact)};newplaces={t.get(r,'Place'):i for i,r in enumerate(newact)}
    AUDIT['monpreset_map']={oldplaces[k]:newplaces[k] for k in oldplaces if k.startswith('rmap_')}


    for name,key,prefix in [('itemtypes','Code',None),('misc','name','RMap'),
                            ('magicprefix','name','rmapaffix'),('magicsuffix','name','rmapaffix'),
                            ('missiles','Missile','rmap_')]:
        d,t=tables(name)
        if prefix is not None:pred=lambda k,r,p=prefix:k.startswith(p)
        elif name=='itemtypes':pred=lambda k,r:k in ['mapi','mcur']
        else:pred=lambda k,r:k=='maps'
        d,t,add=tagged(name,key,pred)
        if name=='itemtypes':
            for r in add:
                if 'UICategory' in t.header:t.set(r,UICategory='misc')
        if name=='missiles':
            for r in add:t.set(r,**{'*ID':native(t).index(r)})

    misc=M['misc'];shard=misc.by('code')['mws'];misc.set(shard,name='RMap Corrupted Worldstone Shard')


    for code in ['mor']:
        r=misc.by('code')[code];misc.set(r,spawnable='0')
    d,t,recipes=tagged('cubemain','description',lambda k,r:k.startswith('rmap '))
    assert len(recipes)==440
    for r in recipes:
        desc=t.get(r,'description')
        for i in range(1,8):
            k='input '+str(i)
            if k not in t.header:continue
            v=t.get(r,k)
            if v=='mor':t.set(r,**{k:'CO1'})
        out=t.get(r,'output')
        match=re.fullmatch(r'Red Portal,lvl=(\d+),qty=1',out)
        if match:t.set(r,output=f'Red Portal,lvl={int(match[1])+LEVEL_SHIFT},qty=1')


    d,t=tables('monlvl')
    assert all(str(i) in t.by('Level') for i in range(99,105))
    AUDIT['monlvl_policy']='Keep VanillaDeluxe rows 99..104 unchanged; map-owned monster multipliers imported.'

    return prest_map,mon_map,su_map

def dependencies():


    registry={'monstats':'Id','monstats2':'Id','monprop':'Id','skills':'skill',
              'states':'state','missiles':'Missile','properties':'code','itemstatcost':'Stat',
              'skilldesc':'skilldesc'}
    imported=[]
    def require(name,key):
        if not key or key in ['0','-1','none'] or key.isdigit():return
        d,t=tables(name);idkey=registry[name]
        if key in t.by(idkey):return
        source=d.by(idkey).get(key)
        if source is None:
            raise ValueError(f'Missing {name} dependency {key}')
        row=t.append_from(d,source);M[name]=t
        for c in ['*Id','*ID','*hcIdx']:
            if t.has(c):t.set(row,**{c:len(native(t))-1})
        imported.append((name,key))
        if name=='skills':

            for c in ['charclass','skilldesc']:
                if t.has(c):t.set(row,**{c:''})
        visit(name,row)
    visited=set()
    def visit(name,row):
        t=T[name];key=t.get(row,registry[name]);identity=(name,key)
        if identity in visited:return
        visited.add(identity)
        for i,col in enumerate(t.header):
            v=row[i];c=col.lower()
            if not v:continue
            if name=='monstats':
                if c in ['monstatsex']:require('monstats2',v)
                elif c=='monprop':require('monprop',v)
                elif c in ['baseid','nextinclass','minion1','minion2','spawn']:require('monstats',v)
                elif re.fullmatch(r'skill[1-8]',c):require('skills',v)
                elif re.fullmatch(r'miss[a-z0-9]*',c) and v in D['missiles'].by('Missile'):require('missiles',v)
            if name=='monstats2' and c=='resurrectskill':require('skills',v)
            if name=='skills':
                if c in ['aurastate','auratargetstate','passivestate']:require('states',v)
                elif 'missile' in c and not c.endswith(('calc','func')):require('missiles',v)
                elif c=='summon':require('monstats',v)
                elif re.fullmatch(r'(aura|passive)stat\d+',c):require('itemstatcost',v)
                elif c=='skilldesc':require('skilldesc',v)
            if name=='missiles':
                if 'submissile' in c or c in ['explosionmissile']:require('missiles',v)
                elif c=='skill':require('skills',v)
            if name=='properties' and re.fullmatch('stat[1-7]',c):require('itemstatcost',v)
            if name=='monprop' and re.fullmatch(r'prop\d+(?: \([NH]\))?',col):require('properties',v)
            for skill in re.findall(r"skill\('([^']+)'",v):require('skills',skill)
    for name in ['monstats','monstats2','skills','states','missiles','monprop']:
        for row in list(M[name].rows):
            key=M[name].get(row,registry[name])
            if key.startswith('rmap_') and not key.startswith('r3map_reserved'):visit(name,row)
    AUDIT['imported_dependencies']=imported

def treasure_classes():
    d,t=tables('treasureclassex');source=d.by('Treasure Class');dest=t.by('Treasure Class');busy=set()
    def add(key):
        if key in dest:return
        if key in busy:raise ValueError('TC recursion: '+key)
        busy.add(key);r=source[key]
        for i in range(1,11):
            v=d.get(r,'Item'+str(i))
            if v in source and v not in dest:add(v)
        out=t.append_from(d,r);dest[key]=out;busy.remove(key)
        for i in range(1,11):
            col='Item'+str(i)
            if t.get(out,col)=='mor':t.set(out,**{col:'CO1'})
    for key in source:
        if key.startswith('RMap'):add(key)


    level=T['levels'].by('Id')['167'];archetype=T['levels'].get(level,'nmon1')
    original_monsters=read(TEX,'monstats')
    loot_key=original_monsters.get(original_monsters.by('Id')[archetype],'TreasureClass(H)')
    assert loot_key in dest
    base_loot=read(TEX/'base','treasureclassex').by('Treasure Class')
    assert loot_key in base_loot,(loot_key,'missing classic bank loot')
    for tier in range(1,7):
        row=dest[f'RMap T{tier} Loot']
        t.set(row,Picks='1',NoDrop='0')
        for i in range(1,11):t.set(row,**{'Item'+str(i):loot_key if i==1 else '', 'Prob'+str(i):'1' if i==1 else ''})
    currency=dest['RMap Currency'];t.set(currency,Item4='Ma1',Prob4='2')
    AUDIT['vanilla_loot_source']={'monster':archetype,'treasure_class':loot_key,'policy':'All six map tiers use the existing VanillaDeluxe Dungeon Map monster loot; normal monsters, elites and Wardens all reach this class. Currency includes CO1 and Ma1.'}
    if 'Gem' in dest and 'Gem' not in read(TEX,'treasureclassex').by('Treasure Class'):

        r=dest['Gem']
        gems=['gpv','gpy','gpb','gpg','gpw','gpr','skz']
        assert all(code in M['misc'].by('code') for code in gems)
        for i in range(1,11):t.set(r,**{'Item'+str(i):gems[i-1] if i<=7 else '', 'Prob'+str(i):'1' if i<=7 else ''})
    M['treasureclassex']=t


    def tc(key,**values):
        assert key not in dest
        r=t.blank();t.set(r,**{'Treasure Class':key,'NoDrop':'0','*eol':'0',**values});t.rows.append(r);dest[key]=r
    tc('R3Map Starter',Picks='1',**{f'Item{i+1}':'x'+c+'1' for i,c in enumerate('dkcfws htei'.replace(' ',''))},
       **{f'Prob{i+1}':'1' for i in range(10)})
    monsters=M['monstats'];baal=monsters.by('Id')['baalcrab'];original=monsters.get(baal,'TreasureClass(H)')
    assert original in dest,original
    tc('R3Map Baal Test',Picks='-2',Item1=original,Prob1='1',Item2='R3Map Starter',Prob2='1')
    monsters.set(baal,**{'TreasureClass(H)':'R3Map Baal Test'})
    AUDIT['starter_drop']='Regular Hell Baal: existing TC plus one random T1 map (temporary acceptance setting).'

def merge_json_assets():


    for p in (DONOR/'data').rglob('*'):
        if not p.is_file():continue
        rel=p.relative_to(DONOR);s=str(rel).lower()
        if 'azmodan_core' in s:continue
        if '/maps/' in s or '/map/' in s or '/rmap' in s or '/map_' in s or re.search(r'/invmap[^/]*\.',s):copy_asset(rel)
    for rel in ['data/hd/character/monsters.json','data/hd/missiles/missiles.json','data/hd/items/items.json']:
        source=json.loads((DONOR/rel).read_text(encoding='utf-8-sig'))
        target=json.loads((TARGET/rel).read_text(encoding='utf-8-sig'))
        if isinstance(source,list):
            known={k for x in target for k in x}
            codes=set(M['misc'].by('code'))-set(read(TEX,'misc').by('code'))
            target.extend(x for x in source if any(k in codes and k not in known for k in x))
        else:
            missile_keys=set()
            if 'missiles' in rel:
                import warden_kits
                original=read(TEX,'missiles').by('Missile')
                missile_keys={warden_kits.hd_key(M['missiles'].get(r,'Missile')) for r in M['missiles'].rows if M['missiles'].get(r,'Missile') not in original}
            for k,v in source.items():
                if k.startswith('rmap_') or k.startswith('RMap') or k in missile_keys:target[k]=v
        q=MPQ/rel;q.parent.mkdir(parents=True,exist_ok=True);q.write_text(json.dumps(target,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

    existing_keys={};existing_ids=set()
    target_strings=TARGET/'data/local/lng/strings'
    for p in target_strings.glob('*.json'):
        try:data=json.loads(p.read_text(encoding='utf-8-sig'))
        except (ValueError,UnicodeError):continue
        if not isinstance(data,list):continue
        for r in data:
            if isinstance(r,dict):existing_keys[r.get('Key')]=r;existing_ids.add(r.get('id'))
    nextid=max(x for x in existing_ids if isinstance(x,int))+1
    added=[];groups=collections.defaultdict(list)
    for p in (DONOR/'data/local/lng/strings').glob('*.json'):
        data=json.loads(p.read_text(encoding='utf-8-sig'))
        if not isinstance(data,list):continue
        for r in data:
            if not isinstance(r,dict):continue
            key=r.get('Key','')
            if key in existing_keys:continue
            if key.startswith(('RMap','rmap')) or key in M['misc'].by('code') and key not in read(TEX,'misc').by('code'):
                new=dict(r);new['id']=nextid;nextid+=1
                for lang,value in list(new.items()):
                    if lang not in ['id','Key'] and isinstance(value,str):
                        value=value.replace('Horadric Sextant','Scroll of Corruption').replace('Horadric Orb','Scroll of Corruption')
                        value=value.replace('Horadric Astrolabe','Corrupted Worldstone Shard')
                        if key=='mws':value='Corrupted Worldstone Shard'
                        new[lang]=value
                existing_keys[key]=new;added.append(new)
                filename=p.name if (target_strings/p.name).exists() else 'item-names.json'
                groups[filename].append(new)


    for filename,newrows in groups.items():
        original=json.loads((target_strings/filename).read_text(encoding='utf-8-sig'))
        q=MPQ/'data/local/lng/strings'/filename;q.parent.mkdir(parents=True,exist_ok=True)
        q.write_text(json.dumps(original+newrows,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    AUDIT['localized_keys']=len(added)
    AUDIT['localization_files']={k:len(v) for k,v in groups.items()}

def decode_objpreset(path):
    raw=Path(path).read_bytes()
    assert raw[:8]==b'D2RLBIN\0'
    directory=struct.unpack_from('<I',raw,60)[0]
    count,stride=struct.unpack_from('<2I',raw,directory+12)
    data,size,links,linksize,strings,stringsize=struct.unpack_from('<6I',raw,directory+36)
    assert stride==12 and size==count*stride and linksize==count*16
    linknames={}
    for i in range(count):
        row,field,kind,off,length=struct.unpack_from('<IHHII',raw,links+i*16)
        assert field==2 and kind==21 and row<count and off+length<=stringsize
        linknames[row]=raw[strings+off:strings+off+length].decode('utf-8')
    t=Table.__new__(Table);t.path=TEX/'objpreset.txt';t.header=['Index','Act','ObjectClass','*Notes','*eol']
    t._columns={h.lower():i for i,h in enumerate(t.header)};t._by_cache={};t.rows=[]
    for i in range(count):
        index,act,compiled_class=struct.unpack_from('<3I',raw,data+i*stride)
        assert 1<=act<=5 and i in linknames
        t.rows.append([str(index),str(act),linknames[i],'Recovered from supplied VanillaDeluxe BIN','0'])
    return t

def recover_objpreset():
    global BASE_OBJECT_PRESETS
    import exterior
    t=decode_objpreset(TEX/'objpreset.bin');bank=decode_objpreset(TEX/'base/objpreset.bin')
    count=len(t.rows);orig=[list(r) for r in t.rows];baseorig=[list(r) for r in bank.rows]
    donor=read(DEX,'objpreset')
    needed=set()
    for p in (DONOR/'data/global/tiles/Maps').rglob('*.ds1'):
        if p.name=='azmodan_core.ds1':continue
        _,units,_,_=exterior.objects(p.read_bytes())
        needed.update(code for kind,code,*_ in units if kind==2)
    mapids={};classes={};nextid=1+max(num(z.get(r,'Index')) for z in [t,bank] for r in z.rows if z.get(r,'Act')=='5')
    for r in donor.rows:
        if donor.get(r,'Act')!='5':continue
        old=num(donor.get(r,'Index'))
        if old not in needed:continue
        key=donor.get(r,'ObjectClass').lower()
        if key not in classes:
            classes[key]=nextid;nextid+=1
            row=[str(classes[key]),'5',donor.get(r,'ObjectClass'),'R3Map private furniture','0']
            t.rows.append(row);bank.rows.append(list(row))
        mapids[old]=classes[key]
    assert t.rows[:count]==orig
    assert bank.rows[:len(baseorig)]==baseorig
    assert set(mapids)==needed
    BASE_OBJECT_PRESETS=bank
    M['objpreset']=t;AUDIT['objectpreset_recovery']={'original_records':count,'base_original_records':len(baseorig),'added_records':len(t.rows)-count,'source':'supplied main/base objpreset.bin records + original symbolic relocation names; private scene indices shared by both banks'}
    return mapids

def patch_map_scenes(objmap):
    import exterior
    monmap={int(k):v for k,v in AUDIT['monpreset_map'].items()}
    counts=collections.Counter()
    for source in (DONOR/'data/global/tiles/Maps').rglob('*.ds1'):
        if source.name=='azmodan_core.ds1':continue
        rel=source.relative_to(DONOR);target=MPQ/rel
        data=bytearray(source.read_bytes());at,objects,w,h=exterior.objects(data)
        assert struct.unpack_from('<I',data,12)[0]==4,(rel,'non Act5 scene')
        for i,(kind,code,x,y,flags) in enumerate(objects):
            if kind==1:
                assert code in monmap,(rel,'unexpected placed monster',code)
                new=monmap[code];counts['wardens']+=1
            elif kind==2:
                assert code in objmap,(rel,'unknown object preset',code)
                new=objmap[code];counts['furniture']+=1
            else:raise ValueError((rel,'unknown scene unit type',kind))
            struct.pack_into('<I',data,at+4+i*20+4,new)
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);counts['scenes']+=1
    assert counts['wardens']==60,counts
    AUDIT['scene_patches']=dict(counts)

def settings():
    source=CONFIG.read_text()
    for key in ['map_stash','reopen_maps']:
        source=re.sub(r'^'+key+r' = true$',key+' = false',source,flags=re.M)
    source=re.sub(r'^treasure_chance_percent = 8$','treasure_chance_percent = 0',source,flags=re.M)
    source=re.sub(r'^event_chance_percent = 30$','event_chance_percent = 0',source,flags=re.M)
    preamble='# VanillaDeluxe Maps V1 prototype: main maps / Wardens / modifiers only.\n# Events, treasure encounters, map stash and reopening are deliberately disabled.\n'
    q=OUT/'mods/VanillaDeluxe/d2rloader/config/maps.toml';q.parent.mkdir(parents=True,exist_ok=True);q.write_text(preamble+source)

def validate():


    for name,t in M.items():
        if name=='objpreset':continue
        path=TEX/(name+'.txt')
        if not path.exists():continue
        original=Table(path)
        assert len(t.rows)>=len(original.rows),name
        for i,r in enumerate(original.rows):
            now=t.rows[i]
            if name=='monstats' and original.get(r,'Id')=='baalcrab':
                for j,v in enumerate(r):
                    if original.header[j]!='TreasureClass(H)':assert now[j]==v,(name,i,j)
            else:assert now==r,('original row changed',name,i)

    for name,key in [('misc','code'),('itemtypes','Code'),('skills','skill'),('states','state'),('monstats','Id'),('levels','Id'),('lvlprest','Def')]:
        old=read(TEX,name);new=M[name]
        oc=collections.Counter(old.get(r,key) for r in old.rows if old.get(r,key));nc=collections.Counter(new.get(r,key) for r in new.rows if new.get(r,key))
        assert all(count<=max(1,oc[k]) for k,count in nc.items()),(name,'new duplicate identifier',[(k,n) for k,n in nc.items() if n>max(1,oc[k])][:5])

    levels=M['levels'];rects=[]
    for r in levels.rows:
        if levels.get(r,'Act')!='4' or not levels.get(r,'Id').isdigit():continue
        x=num(levels.get(r,'OffsetX'));y=num(levels.get(r,'OffsetY'))
        if x<0 or y<0:continue
        w=max(num(levels.get(r,'SizeX'+s)) for s in ['','(N)','(H)']);h=max(num(levels.get(r,'SizeY'+s)) for s in ['','(N)','(H)'])
        if not w or not h:continue
        rects.append((num(levels.get(r,'Id')),x,y,w,h))
    for i,a in enumerate(rects):
        for b in rects[i+1:]:
            if not (175<=a[0]<=294 or 175<=b[0]<=294):continue
            assert not (a[1]<b[1]+b[3] and b[1]<a[1]+a[3] and a[2]<b[2]+b[4] and b[2]<a[2]+a[4]),('world overlap',a,b)
        if 175<=a[0]<=294:assert max(a[1]+a[3],a[2]+a[4])*5<65536

    recipes=M['cubemain'];mine=[r for r in recipes.rows if recipes.get(r,'description').startswith('rmap ')]
    assert len(mine)==440
    counter=collections.Counter(recipes.get(r,'description').split()[1] for r in mine)
    assert counter=={'activate':120,'upgrade':160,'reroll':120,'corrupt':40},counter
    for r in mine:
        desc=recipes.get(r,'description');a=recipes.get(r,'input 1');reagent=recipes.get(r,'input 2');out=recipes.get(r,'output')
        if ' upgrade ' in desc:assert reagent=='CO1' and num(a.split(',')[0][-1])+1==num(out.split(',')[0][-1]),desc
        if ' corrupt ' in desc:assert reagent=='mws' and a.split(',')[0].endswith('5') and out.split(',')[0].endswith('6'),desc
        if ' activate ' in desc:
            lid=int(re.search(r'lvl=(\d+)',out)[1]);assert 175<=lid<=294 and (lid-175)%2==0

    local=[]
    for p in (MPQ/'data/local/lng/strings').glob('*.json'):local.extend(json.loads(p.read_text()))
    allkeys=set()
    for p in (TARGET/'data/local/lng/strings').glob('*.json'):
        try:rs=json.loads(p.read_text(encoding='utf-8-sig'))
        except ValueError:continue
        if isinstance(rs,list):allkeys.update(r.get('Key') for r in rs if isinstance(r,dict))
    allkeys.update(r['Key'] for r in local)
    for r in M['misc'].rows[len(read(TEX,'misc').rows):]:assert M['misc'].get(r,'namestr') in allkeys,('missing item name',M['misc'].get(r,'code'))
    all_item_codes=set()
    for name in ['misc','armor','weapons']:
        t=M.get(name) or read(TEX,name);all_item_codes.update(t.get(r,'code') for r in t.rows)
    all_tc=set(M['treasureclassex'].by('Treasure Class'))
    all_types=set(M['itemtypes'].by('Code'))
    missing=[]
    for r in M['treasureclassex'].rows[len(read(TEX,'treasureclassex').rows):]:
        for i in range(1,11):
            value=M['treasureclassex'].get(r,'Item'+str(i))
            if not value:continue
            code=value.split(',')[0]
            if code not in all_tc and code not in all_item_codes and code not in all_types:missing.append((M['treasureclassex'].get(r,'Treasure Class'),value))
    assert not missing,('unresolved TC entries',missing[:25])
    AUDIT['checks']=['All original TXT rows preserved (except added Hell Baal map reward)',
        'No new duplicate gameplay IDs','120 new level rectangles free of overlaps and within 16-bit world bounds',
        '120 activation / 160 upgrade / 120 reroll / 40 T6 recipes verified',
        'All new item-name keys resolve','All imported treasure-class references resolve',
        'Every binary edit matched a unique typed donor structure or a verified instruction immediate',
        '60 Warden scene placements remapped through target MonPreset',
        'Runtime acceptance in D2R / LAN NOT performed']
    AUDIT['recipe_counts']=dict(counter)

def build():
    if OUT.exists():shutil.rmtree(OUT)
    OUT.mkdir()
    p,m,s=main_tables();print('Tables merged',flush=True)
    dependencies();treasure_classes();print('Dependencies resolved',flush=True)
    patch_plugin(p,m,s);print('DLL statically validated',flush=True)
    merge_json_assets();objmap=recover_objpreset();patch_map_scenes(objmap);settings()
    print('Assets and scenes merged',flush=True)
    validate()
    for name,t in M.items():t.save()
    (OUT/'technical').mkdir(exist_ok=True)
    (OUT/'technical/port-audit.json').write_text(json.dumps(AUDIT,indent=2,ensure_ascii=False)+'\n')
    print('Validated and saved',len(M),'tables',flush=True)

class BinaryPort:
    def __init__(self):
        self.path=PLUGIN
        self.original=self.path.read_bytes();self.data=bytearray(self.original);self.pe=pefile.PE(data=self.original)
        AUDIT['donor_dll_sha256']=hashlib.sha256(self.original).hexdigest()
        self.ranges=[]
    def patch_at(self,label,offset,old,new):
        assert len(old)==len(new),(label,'size')
        assert self.original[offset:offset+len(old)]==old,(label,'original witness')
        assert self.data[offset:offset+len(old)]==old,(label,'overlapping patches')
        self.data[offset:offset+len(old)]=new
        AUDIT['binary_patches'].append({'name':label,'offset':hex(offset),'rva':hex(self.pe.get_rva_from_offset(offset)),
            'bytes':len(old),'changed_bytes':sum(a!=b for a,b in zip(old,new))})
        self.ranges.append((offset,offset+len(old)))
    def array(self,label,old,new):
        assert old and len(old)==len(new)
        assert self.original.count(old)==1,(label,'not unique or missing',len(old),self.original.count(old))
        offset=self.original.index(old);self.patch_at(label,offset,old,new)
    def ints(self,label,values,mapping,bits=16):
        fmt='<' + ('H' if bits==16 else 'I')*len(values)
        self.array(label,struct.pack(fmt,*values),struct.pack(fmt,*(mapping.get(v,v) for v in values)))
    def code_constants(self,old,new,label,expected):
        section=self.pe.sections[0];base=self.pe.OPTIONAL_HEADER.ImageBase
        cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);cs.detail=True;hits=[]
        for ins in cs.disasm(section.get_data(),base+section.VirtualAddress):
            if any(o.type==capstone.x86.X86_OP_IMM and o.imm==old for o in ins.operands):
                assert ins.mnemonic in ['cmp','mov'],(label,ins.mnemonic,ins.op_str)
                at=self.pe.get_offset_from_rva(ins.address-base)+ins.imm_offset
                n=ins.imm_size;before=old.to_bytes(n,'little');after=new.to_bytes(n,'little')
                self.patch_at(label+f' @ {hex(ins.address-base)}',at,before,after);hits.append(ins.address)
        assert len(hits)==expected,(label,len(hits),expected)
    def save(self):

        changed=[i for i,(a,b) in enumerate(zip(self.original,self.data)) if a!=b]
        assert all(any(a<=i<b for a,b in self.ranges) for i in changed)
        result=bytes(self.data);pefile.PE(data=result).close()
        p=OUT/'mods/VanillaDeluxe/d2rloader/plugins/d2rl-maps.dll';p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(result)
        AUDIT['adapted_dll_sha256']=hashlib.sha256(result).hexdigest()

def patch_plugin(prest_map,mon_map,su_map):
    import maps_config as cfg, exterior
    binary=BinaryPort();levels=D['levels'];levelrows=levels.by('Id')
    codes=['m'+k+str(t) for k in 'dkcfwshtei' for t in range(1,7)]
    olddefs=[];newdefs=[]
    for i,code in enumerate(codes):
        lid=166+2*i;r=levelrows[str(lid)]
        fields=(lid,lid+1,i//6,i%6+1,num(levels.get(r,'MonDen(H)')),num(levels.get(r,'MonUMin(H)')),num(levels.get(r,'MonUMax(H)')),code.encode()+b'\0\0')
        olddefs.append(struct.pack('<HHBBHHH5sx',*fields))
        newdefs.append(struct.pack('<HHBBHHH5sx',lid+LEVEL_SHIFT,lid+1+LEVEL_SHIFT,*fields[2:]))
    binary.array('Maps[60] level IDs',b''.join(olddefs),b''.join(newdefs))
    binary.code_constants(166,175,'FirstMapLevel',4)
    binary.code_constants(285,294,'LastMapLevel',4)
    source_mon=indices(D['monstats'],'Id');target_mon=indices(M['monstats'],'Id')
    binary.code_constants(len(native(D['monstats'])),len(native(M['monstats'])),'MonStats record count',4)
    binary.code_constants(len(D['monprop'].rows),len(M['monprop'].rows),'MonProp record count',4)
    binary.ints('WardenMonsterIds[60]',[source_mon['rmap_'+x+'_boss'] for x in codes],mon_map)
    su=indices(D['superuniques'],'Superunique')
    binary.ints('WardenSuperUniqueIds[60]',[su['rmap_'+x+'_warden'] for x in codes],su_map)

    old=[];new=[];target_levels=T['levels'].by('Id')
    for r in levels.rows:
        raw=levels.get(r,'Id')
        if not raw.isdigit() or 166<=int(raw)<=285:continue
        columns=[stem+suffix for stem in ['MonDen','MonUMin','MonUMax'] for suffix in ['','(N)','(H)']]
        if not all(levels.get(r,k).isdigit() for k in columns):continue
        old.append(struct.pack('<10H',int(raw),*(int(levels.get(r,k)) for k in columns)))
        lid=int(raw) if int(raw)<166 else 174
        tr=target_levels[str(lid)]
        vals=[num(T['levels'].get(tr,k)) for k in columns]
        new.append(struct.pack('<10H',lid,*vals))
    binary.array('LevelSamples (all native density witnesses)',b''.join(old),b''.join(new))
    profiles=[]
    for code in codes:
        theme=cfg.THEMES['dkcfwshtei'.index(code[1])]
        count=len(cfg.MAP_MONSTERS[theme['key']]);base=[source_mon[f'rmap_{code}_{i}'] for i in range(count)]
        for pop in range(len(cfg.EXPANSION_POPULATIONS)):
            ids=base if pop==0 else base[:-1]+[source_mon[f'rmap_v2_{code}_{pop}']]
            for _ in cfg.EXPANSION_REWARDS:profiles.extend(ids+[0]*(cfg.MAP_MONSTER_LIMIT-len(ids)))
    binary.ints('PopulationProfiles (all 720 population/reward combinations)',profiles,mon_map)
    world=[];newworld=[]
    for lid in range(166,286):
        r=levelrows[str(lid)];x=num(levels.get(r,'OffsetX'));y=num(levels.get(r,'OffsetY'))
        w=max(num(levels.get(r,'SizeX'+s)) for s in ['','(N)','(H)']);h=max(num(levels.get(r,'SizeY'+s)) for s in ['','(N)','(H)'])
        world.append(struct.pack('<4i',x,y,w,h));newworld.append(struct.pack('<4i',x,y+WORLD_Y_SHIFT,w,h))
    binary.array('MapWorldRects[60][2]',b''.join(world),b''.join(newworld))

    presets=D['lvlprest'];byname=presets.by('Name');ext=[];newext=[]
    for ti,theme in enumerate(cfg.THEMES):
        if not theme.get('exterior'):continue
        key=theme['key'];entry=theme.get('entry_preset',0);exitid=0
        if theme.get('exit_preset'):exitid=num(presets.get(byname[f"RMAP Exterior {key} {theme['exit_preset']} exit"],'Def'))
        if theme.get('stairs_preset'):
            entry=num(presets.get(byname[f"RMAP Exterior {key} {theme['stairs_preset']} entry"],'Def'))
            exitid=num(presets.get(byname[f"RMAP Exterior {key} {theme['stairs_preset']} exit"],'Def'))
        for tier in range(1,7):
            lid=166+ti*12+(tier-1)*2;r=levelrows[str(lid)]
            x=num(levels.get(r,'OffsetX'));y=num(levels.get(r,'OffsetY'));width,height=theme['body_size']
            fields=[lid,theme['body_template'],num(levels.get(r,'LevelType')),width,height,x,y,theme['initializer'],exitid,ti,entry]
            fmt='<7I4xQ3I4x';ext.append(struct.pack(fmt,*fields))
            fields[0]+=LEVEL_SHIFT;fields[6]+=WORLD_Y_SHIFT;fields[8]=prest_map.get(exitid,exitid);fields[10]=prest_map.get(entry,entry)
            newext.append(struct.pack(fmt,*fields))
    binary.array('ExteriorLayouts (levels, world positions, preset IDs)',b''.join(ext),b''.join(newext))
    mappings=[]
    for r in presets.rows:
        m=re.fullmatch(r'RMAP Exterior (\w+) (\d+)',presets.get(r,'Name'))
        if not m:continue
        key,source=m.groups();ti=next(i for i,t in enumerate(cfg.THEMES) if t['key']==key)
        warps=0
        for i in range(1,num(presets.get(r,'Files'))+1):
            path=DONOR/'data/global/tiles'/presets.get(r,'File'+str(i)).replace('\\','/')
            if any(exterior.warp_markers(path.read_bytes())):warps=1
        mappings.append([ti,int(source),num(presets.get(r,'Def')),warps])
    binary.array('ExteriorPresets (private preset mappings)',b''.join(struct.pack('<4I',*r) for r in mappings),
                 b''.join(struct.pack('<4I',r[0],r[1],prest_map[r[2]],r[3]) for r in mappings))
    swaps=[]
    for ti,theme in enumerate(cfg.THEMES):
        for tier in range(1,7):
            lid=166+ti*12+(tier-1)*2
            for src,repl in sorted(theme.get('body_presets',{}).items()):
                swaps.append([lid,num(levels.get(levelrows[str(lid)],'LevelType')),src,repl])
    binary.array('MazePresetSwaps (maze arrival/exit IDs)',b''.join(struct.pack('<4I',*r) for r in swaps),
                 b''.join(struct.pack('<4I',r[0]+LEVEL_SHIFT,*r[1:]) for r in swaps))

    groups=[('TreasureMonsterIds',[source_mon[f'rmap_treasure_{tier}_{spec["key"]}'] for tier in range(1,7) for spec in cfg.TREASURE_MONSTERS]),
            ('EventRaiderIds',[source_mon[f'rmap_event_raider_{tier}'] for tier in range(1,7)]),
            ('EventGuardIds',[source_mon[f'rmap_event_guard_{tier}'] for tier in range(1,7)]),
            ('EventWaveIds',[source_mon[f'rmap_event_wave_{code}'] for code in codes]),
            ('EventChallengerIds',[source_mon[f'rmap_event_challenger_{code}'] for code in codes]),
            ('EventKeeperIds',[source_mon[f'rmap_event_keeper_{code}_{reward}'] for code in codes for reward in ['crafting','equipment','maps']])]
    for label,arr in groups:binary.ints(label,arr,mon_map)
    eventnames=[name for name in source_mon if name.startswith('rmap_event_')]
    binary.ints('EventMonsterIds',[source_mon[name] for name in eventnames],mon_map)
    binary.ints('EventSuperUniqueIds',[su['rmap_event_challenger_'+x] for x in codes],su_map)
    binary.save()
    return binary

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target',type=Path,default=TARGET,help='Unmodified VanillaDeluxe.mpq directory')
    parser.add_argument('--donor',type=Path,default=DONOR,help='Unmodified ReimaginedLadder.mpq directory')
    parser.add_argument('--plugin',type=Path,default=PLUGIN,help='Unmodified donor d2rl-maps.dll')
    parser.add_argument('--config',type=Path,default=CONFIG,help='Donor maps.toml')
    parser.add_argument('--output',type=Path,default=OUT)
    args=parser.parse_args()
    TARGET,DONOR,PLUGIN,CONFIG,OUT=(p.resolve() for p in [args.target,args.donor,args.plugin,args.config,args.output])
    assert TARGET!=OUT and DONOR!=OUT and TARGET not in OUT.parents and DONOR not in OUT.parents
    DEX=DONOR/'data/global/excel';TEX=TARGET/'data/global/excel';MPQ=OUT/'mods/VanillaDeluxe/VanillaDeluxe.mpq'
    build()
