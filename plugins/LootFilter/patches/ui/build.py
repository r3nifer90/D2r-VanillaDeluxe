from pathlib import Path
import json,re,hashlib,zipfile,subprocess,tempfile
R=Path('work/lootfilter');D=Path('deliverables');D.mkdir(exist_ok=True)
M=Path('reference/extracted/mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/ui/layouts/_profilehd.json')
N=Path('reference/native/layouts/_profilehd.json')
def parse(s):
 s=re.sub(r'"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/',lambda m:'' if m[0].startswith(('/',)) else m[0],s)
 s=re.sub(r',\s*([}\]])',r'\1',s)
 return json.loads(s)
old=M.read_bytes();mod=parse(old.decode('utf-8-sig'));native=parse(N.read_text(encoding='utf-8-sig'))
keys=['LF_ProfileDropdownWidth','LF_ProfileDropdownBackgroundWidth','ConsoleUIScale','StyleSettingsLargeCarousel']
added={k:native[k] for k in keys};assert not(set(keys)&set(mod))

pos=old.rfind(b'}');assert pos>=0
insert=('  // Restore missing native loot filter UI constants.\r\n'+''.join('  '+json.dumps(k)+': '+json.dumps(v,ensure_ascii=False)+',\r\n' for k,v in added.items())).encode()
new=old[:pos]+insert+old[pos:];result=parse(new.decode('utf-8-sig'))
assert result=={**mod,**added}

checked=[]
for p in sorted(N.parent.glob('lootfilter*hd.json')):
 text=p.read_text(encoding='utf-8-sig');parse(text)
 refs=set(re.findall(r'"\$([A-Za-z0-9_]+)"',text))
 assert not ((refs & set(native))-set(result)),p.name
 checked.append(p.name)
for k,v in added.items():
 for ref in re.findall(r'\$([A-Za-z0-9_]+)',json.dumps(v)):
  assert ref in result or ref not in native,(k,ref)
P=R/'package';f=P/'mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/ui/layouts/_profilehd.json';f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(new)
(P/'CZYTAJ_NAJPIERW.txt').write_text('LOOT FILTER — poprawka UI\n\nZamknij grę i loader. Zrób kopię swojego pliku:\nmods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/ui/layouts/_profilehd.json\n\nSkopiuj folder mods z tej paczki do katalogu gry z nadpisaniem. Uruchom grę ponownie. Nie wymaga kompilacji.\n\nUzupełniono cztery brakujące definicje UI oryginalnymi wartościami z przesłanych plików gry: szerokości listy profili (627) i jej tła (625), skalę okien modalnych (1.0) oraz styl wyboru profilu w oknie tworzenia profilu. Wszystkie dotychczasowe ustawienia VanillaDeluxe zachowano.\n\nPrzetestowano składnię oraz kompletność odwołań do stałych profilu HD w 13 układach loot filtera; archiwum rozpakowuje się poprawnie. Nie wykonano testu w uruchomionej grze.\n',encoding='utf-8')
report={'added':added,'existing_definitions_preserved':len(mod),'native_loot_layouts_checked':checked,'live_game_tested':False,'source_mod_sha256':hashlib.sha256(old).hexdigest(),'output_sha256':hashlib.sha256(new).hexdigest()}
(R/'validation.json').write_text(json.dumps(report,indent=2))
S=R/'sources';S.mkdir(exist_ok=True);(S/'VanillaDeluxe_profilehd_before.json').write_bytes(old);(S/'native_profilehd.json').write_bytes(N.read_bytes())
for p in N.parent.glob('lootfilter*hd.json'):(S/p.name).write_bytes(p.read_bytes())
def pack(out,files):
 with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=False) as z:
  for name,p in files:
   i=zipfile.ZipInfo(name,(2026,11,10,1,0,0));i.create_system=0;i.external_attr=0x20;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,p.read_bytes())
 with zipfile.ZipFile(out) as z:
  assert z.testzip() is None
  with tempfile.TemporaryDirectory() as td:
   z.extractall(td)
   for name,p in files:assert (Path(td)/name).read_bytes()==p.read_bytes()
 c=subprocess.run(['unzip','-t',str(out)],capture_output=True);assert c.returncode==0
pack(D/'D2R_LootFilter_UI_FIX_MODS.zip',[(p.relative_to(P).as_posix(),p) for p in P.rglob('*') if p.is_file()])
pack(D/'D2R_LootFilter_UI_FIX_Source_Checkpoint.zip',[(p.relative_to(R).as_posix(),p) for p in R.rglob('*') if p.is_file()])
print(json.dumps(report,indent=2))
print('Both ZIP archives: CRC, extraction byte comparison and unzip checks passed.')
