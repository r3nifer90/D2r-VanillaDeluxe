from pathlib import Path
import csv, io, json, re, hashlib

ROOT=Path(__file__).resolve().parents[1]
S=ROOT/'source'; R=S/'resources'; R.mkdir(parents=True,exist_ok=True)
CURRENT=ROOT/'current/mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel'
BALANCE=ROOT/'upstream/dropbalance/package/mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel'
SP=ROOT/'upstream/r3ni-SovereignPacks-0.3.10-source/source'
PROFILES=[
 ('izual','Veyrath, the Fallen Seraph','izual',[83,100,101,102],['Zeal','Fanaticism','Charge'],[4,7,3], 'Veyrath breaks his oath; a fallen seraph descends into hatred.'),
 ('nihlathak','Mordrath, Warden of Bone','nihlathakboss',[29,30,31,34,35,36,37],['Bone Armor','Bone Prison','Teeth'],[1,0,0], 'Mordrath stirs beneath the monastery; the dead will not rest.'),
 ('blood_raven','Vespara, the Crimson Huntress','bloodraven',[6,7],['Strafe','Valkyrie','Slow Missiles'],[0,5,0], 'Vespara draws her bow; the crimson hunt has begun.'),
 ('griswold','Gorath, the Forsaken Smith','griswold',[42,44,56,57,60],['Bash','Leap','War Cry'],[4,3,1], 'Gorath rises from the dust; his hammer remembers every betrayal.'),
 ('smith','Kharvok, the Ashen Butcher','smith',[106,107],['Berserk','Frenzy','Battle Cry'],[4,4,1], 'Kharvok leaves the furnace; the ashes demand fresh blood.'),
 ('radament','Azharak, Keeper of the Dead','radament',[18,19,21,22,23,24,25],['Raise Skeleton','Raise Skeletal Mage','Bone Spirit'],[6,6,0], 'Azharak opens the sealed tomb; a thousand bones answer his call.'),
 ('fire_eye','Zharun, the Venomblade','sandraider3',[76,77,78],['Fade','Burst of Speed','Venom'],[1,1,1], 'Zharun slips through the jungle; venom gleams upon his blades.'),
 ('summoner','Malachar, the Starless Seer','summoner',[122,123,124],['Armageddon','Spirit of Barbs','Summon Spirit Wolf'],[1,5,5], 'Malachar tears open the starless sky; ruin follows his prophecy.'),
 ('stormtree','Thornagar, the Withered Heart','thornhulk3',[4,5],['Hurricane','Molten Boulder','Oak Sage'],[1,0,5], 'Thornagar wakes among the withered roots; the storm has found a heart.'),
 ('lord_de_seis','Vaelthos, the Riftbinder','doomknight3',[74,46],['Miasma Bolt','Flame Wave','Blade Warp'],[0,0,0], 'Vaelthos unseals the rift; flame and miasma cross the veil.'),
 ('hephasto','Vorgath, the Infernal Forger','hephasto',[111,118],['Ring of Fire','Summon Tainted','Miasma Chains'],[0,5,0], 'Vorgath strikes the infernal anvil; burning chains reach for Sanctuary.'),
 ('infector_of_souls','Nharzul, the Plague Maw','megademon3',[79,80,81],['Poison Nova','Poison Explosion','Lower Resist'],[1,2,0], 'Nharzul exhales beneath Kurast; the air itself begins to rot.'),
 ('shenk','Kargor, the Siege Tyrant','overseer1',[110,112],['Might','Blessed Hammer','Summon Goatman'],[7,0,5], 'Kargor raises the siege standard; his hammers will shatter your refuge.'),
 ('sharptooth_slayer','Drazhul, the Ashen Shepherd','overseer3',[112,117],['Summon Defiler','Holy Fire','Armageddon'],[5,7,1], 'Drazhul calls his flock from the embers; the mountain will burn.'),
 ('frozenstein','Hrimgor, the Frozen Colossus','snowyeti4',[113,115],['Chilling Armor','Frozen Orb','Glacial Spike'],[1,0,0], 'Hrimgor breaks the ancient ice; winter has taken flesh.'),
 ('thresh_socket','Kharzug, the Molten Breaker','siegebeast3',[110,111],['Volcano','Firestorm','Fissure'],[0,0,0], 'Kharzug shakes the frozen earth; molten blood rises from its wounds.'),
 ('pindleskin','Valdrak, the Oathless Revenant','reanimatedhorde5',[122,123],['Raven','Charge','Vengeance'],[5,3,4], 'Valdrak returns without an oath; black wings mark the road to vengeance.'),
 ('snapchip_shatter','Skarnath, the Rimeborn','frozenhorror1',[114,117],['Shiver Armor','Blizzard','Ice Blast'],[1,0,0], 'Skarnath gathers the killing frost; every breath becomes a blade.'),
 ('lister_the_tormentor','Rhazgor, the Tempest Maw','baalminion1',[128,129,130,131],['Cyclone Armor','Tornado','Carrion Vine'],[1,0,5], 'Rhazgor howls before the Worldstone; the tempest hungers for the living.'),
 ('countess','Velisara, the Blade Matron','corruptrogue3',[28,32,33],['Lightning Sentry','Blade Shield','Blade Sentinel'],[0,1,0], 'Velisara claims the ruined cloisters; hidden blades await your blood.'),
]

def read_table(p):
 text=p.read_text(encoding='utf-8-sig');reader=csv.DictReader(io.StringIO(text),delimiter='\t');return reader.fieldnames,list(reader)
def emit_table(name, fields, rows):
 b=io.StringIO(newline='');w=csv.DictWriter(b,fields,delimiter='\t',lineterminator='\r\n');w.writeheader();w.writerows(rows);(R/name).write_bytes(b.getvalue().encode('utf-8-sig'))
def cstr(s):return json.dumps(s,ensure_ascii=True)

sf,skills=read_table(CURRENT/'skills.txt');skill_by={r['skill'].lower():r for r in skills}
skill_ids={int(r['*Id']):r for r in skills if r['*Id']}
EXPECTED_SKILLS=[[106,122,107],[68,88,67],[26,32,17],[126,132,154],[152,147,146],[70,80,93],[267,258,278],[249,246,227],[250,229,226],[395,398,390],[394,376,399],[92,83,91],[98,112,373],[377,102,249],[60,64,55],[244,225,234],[221,107,111],[50,59,45],[235,245,231],[271,277,257]]
mf,monsters=read_table(BALANCE/'monstats.txt');mon_by={r['Id']:r for r in monsters}
_,levels=read_table(CURRENT/'levels.txt');area_by={int(r['Id']):r for r in levels if r['Id']}
_,uniques=read_table(CURRENT/'uniqueitems.txt');unique_by={r['index']:r for r in uniques if r['index']}
FIRST_MON=1779; VARIANTS=16; FIRST_SKILL=437; FIRST_SU=100
assert max(int(r['*hcIdx']) for r in monsters if r['*hcIdx'])==1778
assert max(int(r['*Id']) for r in skills if r['*Id'])==436
uber=mon_by['vdtz_uberdiablo']; mon_out=[];skill_out=[];profiles=[]
for i,(slug,name,source,areas,names,kinds,message) in enumerate(PROFILES):
 assert source in mon_by and not mon_by[source]['npc'],source
 assert len(names)==len(kinds)==3 and len(message.encode('ascii'))<200
 areas=[146,150,151,153,154,155,156,159,161,162,163,164]
 resolved=[]
 for j,(sn,kind) in enumerate(zip(names,kinds)):
  sr=skill_ids[EXPECTED_SKILLS[i][j]];clone=sr.copy();sid=FIRST_SKILL+i*3+j; clone['skill']=f'r3tl{i}s{j}';clone['*Id']=str(sid);clone['charclass']='';clone['skilldesc']='';clone['reqlevel']='1';clone['reqskill1']=clone['reqskill2']=clone['reqskill3']=''

  clone['monanim']='A1'
  clone['ItemEffect']=sr.get('ItemEffect') or '1'
  clone['ItemCltEffect']=sr.get('ItemCltEffect') or '1'
  if sr['anim']=='SQ':clone['anim']='A1';clone['seqnum']='';clone['seqtrans']=''
  for field in ['manashift','mana','lvlmana','minmana']:clone[field]='0'
  if sr.get('summon') and sr.get('petmax'):clone['petmax']=f"min(({sr['petmax']}),3)"


  for field in ['DmgSymPerCalc','EDmgSymPerCalc']:
   if field in clone:
    old=clone[field] or '0';clone[field]=f'({old})*(100+stat(\'r3ni_tz_dmg_pct\'.accr))/100+stat(\'r3ni_tz_dmg_pct\'.accr)'
  skill_out.append(clone)
  resolved.append({'name':sn,'original_id':int(sr['*Id']),'private_id':sid,'kind':kind,'corpse_required':kind in [2,6],'summon':sr.get('summon',''),'srvstfunc':sr.get('srvstfunc',''),'srvdofunc':sr.get('srvdofunc',''),'srvmissile':sr.get('srvmissile','')})
 for v in range(VARIANTS):
  r=mon_by[source].copy();idx=FIRST_MON+i*VARIANTS+v;r['Id']=f'r3tl{i}v{v}';r['*hcIdx']=str(idx);r['NameStr']=f'r3tl_name_{i}';r['NextInClass']='';r['AI']='Zombie';r['boss']='1';r['primeevil']='1';r['npc']='';r['inventory']='';r['killable']='1';r['Align']='0';r['TransLvl']=mon_by[source]['TransLvl']
  for field in mf:
   if field.lower()=='splenddeath':r[field]='0'
   if field.lower()=='noratio':r[field]='1'
  for field in mf:
   if re.match(r'^(Level|MinHP|MaxHP|AC|Exp|A[12](MinD|MaxD|TH)|Res|ToBlock|TH)',field):r[field]=uber[field]
  for j in range(1,9):
   for field in [f'Skill{j}',f'Sk{j}mode',f'Sk{j}lvl']:
    if field in r:r[field]=''
  for j in range(1,4):r[f'Skill{j}']=f'r3tl{i}s{j-1}';r[f'Sk{j}mode']='A1';r[f'Sk{j}lvl']='30'
  for field in mf:
   if field.startswith('TreasureClass'):r[field]='R3TL Root'

  for field in mf:
   if re.match(r'^(minion|spawn|Party)',field,re.I) and field!='spawned':r[field]=''
  mon_out.append(r)
 profiles.append({'index':i,'id':slug,'name':name,'source':source,'source_class':int(mon_by[source]['*hcIdx']),'areas':[{'id':a,'name':area_by[a]['LevelName']} for a in areas],'skills':resolved,'announcement':message,'first_class':FIRST_MON+i*VARIANTS,'class_capacity':VARIANTS,'superunique_index':FIRST_SU+i,'scope':'VanillaDeluxe Dungeon Map combat floors only','model_policy':'all native palette entries, preserve cold/poison status slots; no invented recolors'})

uf,su=read_table(BALANCE/'superuniques.txt');assert max(int(r['hcIdx']) for r in su if r['hcIdx'])==99
su_out=[]
for p in profiles:
 r={k:'' for k in uf};r.update({'Superunique':f"r3tl_name_{p['index']}",'Name':f"r3tl_name_{p['index']}",'Class':f"r3tl{p['index']}v0",'hcIdx':str(p['superunique_index']),'MinGrp':'0','MaxGrp':'0','AutoPos':'0','Stacks':'0','Replaceable':'0','*eol':'0'})
 for k in uf:
  if k.startswith('TC'):r[k]='R3TL Root'
 su_out.append(r)


tf,tc_rows=read_table(BALANCE/'treasureclassex.txt');tc={r['Treasure Class']:r for r in tc_rows};added={};clean={};excluded=[]
_,misc=read_table(CURRENT/'misc.txt');misc_by={r['code']:r for r in misc if r['code']}
blocked=set(['tsc','isc','tbk','ibk','rvs','rvl','CO1','Ma1','rhm','cm1','cm2'])
blocked.update(r['code'] for r in misc if re.match(r'^(hp|mp)[1-5]$',r.get('code','')) or 'pot' in r.get('type','').lower() or r.get('type') in ['hpot','mpot','rpot','apot','wpot','elix'])
blocked.update(r['code'] for r in misc if r['code'].startswith('H') and len(r['code'])==3)
forbidden_names=['Annihilus','Hellfire Torch','r3ni Charm Roll','r3ni Charm Pool']
def new_tc(name, picks, items, **extra):
 r={k:'' for k in tf};r.update({'Treasure Class':name,'Picks':str(picks),'NoDrop':'0','*eol':'0'});r.update({k:str(v) for k,v in extra.items()})
 for i,(item,weight) in enumerate(items,1):assert i<=10;r[f'Item{i}']=item;r[f'Prob{i}']=str(weight)
 added[name]=r;return name
def sanitize(name, stack=()):
 if name in forbidden_names:return None
 if name not in tc:
  if name.split(',')[0] in blocked:excluded.append(name);return None
  return name
 if name in clean:return clean[name]
 assert name not in stack,('TC cycle',stack,name)
 key=f'R3TL Gear {len(clean):04}';clean[name]=key;row=tc[name].copy();row['Treasure Class']=key;kept=[]
 for i in range(1,11):
  item=row.get(f'Item{i}','');weight=row.get(f'Prob{i}','')
  if item and weight and int(weight)>0:
   target=sanitize(item,stack+(name,))
   if target:kept.append((target,weight))
 for i in range(1,11):row[f'Item{i}']=row[f'Prob{i}']=''
 if not kept:row['Picks']='0';row['NoDrop']='0'
 for i,(item,weight) in enumerate(kept,1):row[f'Item{i}']=item;row[f'Prob{i}']=weight
 added[key]=row;return key
gear=sanitize('Diablo (H)');added[gear]['Picks']='2'
elite_uniques=sanitize('VDTZ Elite Uniques');runes=sanitize('VDTZ Elite Rune Bonus')
marker="stat('r3ni_tz_charm_drop'.accr)";low=f'({marker}-({marker}/8)*8)'
top=new_tc('R3TL Top',1,[(elite_uniques,1)],ConditionCalc=f'{marker}>=8')
special_roots=['CO1','Ma1','rhm','R3DB Weak','R3HS Ist M','R3HS Ist H','R3HS Zod Skills']
special=[]
for i,item in enumerate(special_roots,1):assert item in tc or item in misc_by;special.append((new_tc(f'R3TL Special {i}',1,[(item,1)],ConditionCalc=f'{low}=={i}'),1))
special_tc=new_tc('R3TL Special Bundle',-7,special)
new_tc('R3TL Root',-4,[(top,1),(special_tc,1),(runes,1),(gear,1)])



aliases={"Deaths's Web":"Death's Web",'Fathom':'Fathom'}
for row in added.values():
 for i in range(1,11):
  k=f'Item{i}';value=row.get(k,'')
  if value in aliases and aliases[value] in unique_by:row[k]=aliases[value]
top_names=set()
def collect(name, out, seen=None):
 seen=set() if seen is None else seen
 if name in seen:return
 seen.add(name)
 if name not in added:out.add(name);return
 for i in range(1,11):
  if added[name].get(f'Item{i}'):collect(added[name][f'Item{i}'],out,seen)
collect(elite_uniques,top_names)
assert "Griffon's Eye" in top_names and 'The Grandfather' in top_names and 'The Stone of Jordan' in top_names
assert not set(forbidden_names)&top_names
gear_leaves=set();collect(gear,gear_leaves)
assert not {x.split(',')[0] for x in gear_leaves}&blocked
assert len(profiles)==20 and len(skill_out)==60 and len(mon_out)==320
for bank in ['', 'base/']:
 for f,fields,rows in [('monstats',mf,mon_out),('skills',sf,skill_out),('superuniques',uf,su_out),('treasureclassex',tf,list(added.values()))]:

  actual,_=read_table((BALANCE if f in ['monstats','superuniques','treasureclassex'] else CURRENT)/(bank+f+'.txt'));assert actual==fields,(bank,f)
  emit_table(('base_' if bank else '')+f+'.tsv',fields,rows)

languages=['enUS','zhTW','deDE','esES','frFR','itIT','koKR','plPL','esMX','jaJP','ptBR','ruRU','zhCN']
strings=[{'id':62000+p['index'],'Key':f"r3tl_name_{p['index']}",**{lang:'ÿc1'+p['name']+'ÿc0' for lang in languages}} for p in profiles]
(R/'strings.json').write_text(json.dumps(strings,ensure_ascii=False,indent=2)+'\n',encoding='utf-8-sig')
(S/'profiles.json').write_text(json.dumps({'author':'r3ni','version':'0.1.0-beta','profiles':profiles,'loot_top_pool':sorted(top_names)},ensure_ascii=False,indent=2)+'\n')

header=['#pragma once','namespace TLData {',f'constexpr uint32_t FirstMon={FIRST_MON}, Variants={VARIANTS}, TotalMon={FIRST_MON+len(mon_out)}, FirstSkill={FIRST_SKILL}, TotalSkill={FIRST_SKILL+len(skill_out)}, FirstSU={FIRST_SU}, TotalSU=120, Count=20;',
 'struct Profile {const char* key;const wchar_t* section;const char* name;const char* source;const char* message;uint32_t sourceClass;uint16_t areas[16];uint8_t kinds[3];uint16_t skills[3];bool refresh[3];};','constexpr Profile Profiles[]={']
for p in profiles:
 areas=','.join(str(a['id']) for a in p['areas']);kinds=','.join(str(s['kind']) for s in p['skills']);ids=','.join(str(s['private_id']) for s in p['skills'])
 refresh=','.join('true' if skill_ids[s['original_id']].get('aurastate') or s['kind'] in [5,6,7] else 'false' for s in p['skills'])
 header.append('{'+','.join([cstr(p['id']),'L'+cstr(p['id']),cstr(p['name']),cstr(p['source']),cstr(p['announcement']),str(p['source_class']),'{'+areas+'}','{'+kinds+'}','{'+ids+'}','{'+refresh+'}'])+'},')
header+=['};','}'];(S/'profiles_generated.h').write_text('\n'.join(header)+'\n')
report={'author':'r3ni','profiles':20,'skills':60,'reserved_monster_rows':320,'superunique_rows':20,'private_loot_rows':len(added),'gear_exclusions':sorted(set(excluded)),'top_pool':sorted(top_names),'native_first_indices':{'monstats':1779,'skills':437,'superuniques':100},'runtime_tested':False}
(S/'data_validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
