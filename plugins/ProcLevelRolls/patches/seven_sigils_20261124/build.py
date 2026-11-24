from pathlib import Path
import csv,io,json,struct,hashlib,zipfile
import pefile
r=Path(__file__).parent;pkg=r/'package';prefix=Path('mods/VanillaDeluxe/VanillaDeluxe.mpq/data');src=Path('work/glow-fix/package')/prefix
names={'tc1':'Storm','tc2':'Inferno','tc3':'Glacier','tc4':'Blight','tc5':'Bone','tc6':'Reaper','tc7':'Wild'}
def put(rel,data):p=pkg/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
report=[]
for bank in ['','base/']:
 p=src/'global/excel'/bank/'uniqueitems.txt';rd=csv.DictReader(io.StringIO(p.read_text(encoding='utf-8-sig')),delimiter='\t');cols=rd.fieldnames;rows=list(rd);before=[dict(x) for x in rows];changed=[]
 for idx,row in enumerate(rows):
  code=row['code']
  if code not in names:continue
  slots=[n for n in range(1,13) if row['prop'+str(n)]=='att-skill'];assert len(slots)==1
  n=str(slots[0]);row['prop'+n]='hit-skill'
  if code=='tc1':row['max'+n]='50'
  if code in ['tc2','tc4']:row['max'+n]='60'
  changed.append(row['index'])
  delta={k:[before[idx][k],row[k]] for k in cols if before[idx][k]!=row[k]};assert set(delta)<={'prop'+n,'max'+n}
  report.append({'bank':bank,'row':idx,'name':names[code],'changes':delta})
 assert len(changed)==14
 assert all(a==b for a,b in zip(before,rows) if a['index'] not in changed)
 buf=io.StringIO(newline='');w=csv.DictWriter(buf,fieldnames=cols,delimiter='\t',lineterminator='\r\n');w.writeheader();w.writerows(rows);data=buf.getvalue().encode();assert data.startswith(b'index\t');put(prefix/'global/excel'/bank/'uniqueitems.txt',data)
# Existing item-specific rules in the user's latest DLL. No new code, imports or hooks.
p=Path('work/drop-rebalance-nov23/source/d2rloader/plugins/ProcLevelRolls.dll');original=p.read_bytes();assert hashlib.sha256(original).hexdigest()=='6e80308b81edf42c185b5b5aacf671635598ecaccb44bcee8a2d326626141005'
pe=pefile.PE(data=original);data=bytearray(original);rules={7:('terror-charms:Yellow',53,50,60),8:('terror-charms:Red',47,60,63),9:('terror-charms:Blue',64,40,50),10:('terror-charms:Green',92,60,63),11:('terror-charms:White',84,40,50),12:('terror-charms:Brown',247,40,50)};changedbytes=set()
for index,(name,skill,lo,hi) in rules.items():
 at=pe.get_offset_from_rva(0x9500+index*40);ptr,alias,oldskill,stat,lvl,chance=struct.unpack_from('<QQIIQQ',original,at)
 assert pe.get_string_at_rva(ptr-pe.OPTIONAL_HEADER.ImageBase).decode()==name and oldskill==skill and stat==195
 struct.pack_into('<III',data,at+20,198,lo,hi);changedbytes.update(range(at+20,at+32))
checksum=pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum');q=pefile.PE(data=bytes(data));struct.pack_into('<I',data,checksum,q.generate_checksum());changedbytes.update(range(checksum,checksum+4))
assert all(a==b for i,(a,b) in enumerate(zip(original,data)) if i not in changedbytes)
put(Path('mods/VanillaDeluxe/d2rloader/plugins/ProcLevelRolls.dll'),bytes(data))
# Latest cumulative UI strings, plus only requested Sigil descriptions.
p=src/'local/lng/strings/ui.json';rows=json.loads(p.read_text(encoding='utf-8-sig'));before=json.loads(json.dumps(rows));ids={515:'Storm',516:'Inferno',517:'Glacier',518:'Blight',519:'Bone',520:'Reaper',521:'Wild'};alt={549:518,550:519,551:517,552:516,553:520,554:515,555:521};updated=[]
for row in rows:
 key=row['Key'];which=next((n for n in ids if key=='R3LIB_U'+str(n)+'_a'),None)
 if which is None:which=next((v for k,v in alt.items() if key=='R3C65_Entry'+str(k)+'_a'),None)
 if which is None:continue
 for lang,text in list(row.items()):
  if lang in ['Key','id']:continue
  assert 'on attack' in text or 'przy ataku' in text
  text=text.replace('on attack','on striking').replace('przy ataku','przy trafieniu')
  if which==515:text=text.replace('40–50','50–60')
  if which in [516,518]:text=text.replace('40–50','60–63')
  row[lang]=text
 updated.append(key)
assert len(updated)==14
assert all(a==b for a,b in zip(before,rows) if a['Key'] not in updated)
put(prefix/'local/lng/strings/ui.json',json.dumps(rows,ensure_ascii=False,indent=2).encode('utf-8-sig'))
readme='''SIGILE — ON STRIKING\n\nWszystkie siedem wymienionych charmów: on attack zmienione na on striking.\nStorm: poziom 50–60.\nInferno i Blight: poziom 60–63.\nGlacier, Bone i Wild: dotychczasowe 40–50.\nReaper: dotychczasowy poziom 20.\nProcentowe szanse, pozostałe statystyki oraz dropy pozostają bez zmian.\n\nZmieniono obie istniejące wersje rekordów, bez dodawania przedmiotów ani zmiany ID. Zaktualizowano istniejące reguły ProcLevelRolls.dll (wyłącznie dane sześciu reguł, bez nowych hooków lub diagnostyki) i opisy kompendium. Poprawka dotyczy nowo wygenerowanych przedmiotów. Statystyki starych sztuk zapisanych w save nie są przepisywane.\n\nZachowano ostatnie zmiany Dark Abyss, Death Cleavera i Robe des Zeus w tabelach oraz poprawkę Hell Conquest Glow w ui.json. Osobna poprawka Dark Abyss w item-modifiers.json pozostaje zainstalowana i nie jest nadpisywana tą paczką.\n\nINSTALACJA\nPrzy zamkniętej grze i D2RLoader skopiuj folder mods do:\nD:\\Battle.net\\gry\\Diablo II Resurrected\\\nPotwierdź zastąpienie plików.\nUruchom przez loader i w jego konsoli w menu głównym wykonaj:\nd2rl compile VanillaDeluxe\nPo udanej kompilacji całkowicie zamknij grę i loader, następnie uruchom ponownie.\nW LAN wgraj tę samą paczkę i skompiluj dane na obu komputerach.\n\nTest w uruchomionej grze pozostaje do wykonania.\n'''
(pkg/'CZYTAJ.txt').write_text(readme,encoding='utf-8-sig')
out=Path('output/D2R_Seven_Sigils_On_Striking_MODS.zip')
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(pkg.rglob('*')):
  if p.is_file():z.write(p,p.relative_to(pkg))
with zipfile.ZipFile(out) as z:assert z.testzip() is None and len(z.namelist())==5
(r/'verification.json').write_text(json.dumps({'table_changes':report,'ui_keys':updated,'dll_input_sha256':hashlib.sha256(original).hexdigest(),'dll_output_sha256':hashlib.sha256(data).hexdigest(),'dll_data_only':True},indent=2));print(out)
