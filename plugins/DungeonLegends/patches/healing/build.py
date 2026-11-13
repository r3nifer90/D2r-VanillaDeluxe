from pathlib import Path
import csv,json,hashlib,shutil,re
W=Path(__file__).resolve().parent;root=W.parent
rel=Path('mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel')
changes=[]
for bank in ['', 'base/']:
 source=root/'dungeon_hp/package'/rel/bank/'monstats.txt';text=source.read_text(encoding='utf-8-sig');header=text.splitlines()[0].split('\t');rows=list(csv.DictReader(text.splitlines(),delimiter='\t'))
 changed=[]
 for r in rows:
  if re.fullmatch(r'r3(?:tl|dx)\d+v\d+',r['Id']):
   assert r['DamageRegen']=='4';r['DamageRegen']='0';changed.append(r['Id'])
 assert len(changed)==496
 out=W/'package'/rel/bank/'monstats.txt';out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(('\r\n'.join(['\t'.join(header)]+['\t'.join(r.get(k,'') for k in header) for r in rows])+'\r\n').encode('utf-8'))
 shutil.copyfile(root/'dungeon_hp/package'/rel/bank/'levels.txt',out.parent/'levels.txt')
 changes.append({'bank':bank or 'main','DamageRegen_before':4,'DamageRegen_after':0,'rows':changed})
source=W/'latest_plugins/d2rloader/plugins/EnemySkills.ini';b=source.read_bytes();start=b.index(b'[Skills.Dungeon]');assert b[start:].count(b'CarrionVine.level=15')==1
b=b[:start]+b[start:].replace(b'CarrionVine.level=15',b'CarrionVine.level=1',1)
out=W/'package/mods/VanillaDeluxe/d2rloader/plugins/EnemySkills.ini';out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(b)
(W/'changes.json').write_text(json.dumps(changes,indent=2))
print('Patched DamageRegen 4->0 for 496 Dungeon Legends variants per bank; Dungeon CarrionVine level15->1; retained complete preceding dungeon HP progression.')
