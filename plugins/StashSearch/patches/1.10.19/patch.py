from pathlib import Path
import hashlib,struct,pefile,json
r=Path('work/stash-fix/fix');src=Path('upload/d2rl-stash-search(2).dll');b=bytearray(src.read_bytes());assert hashlib.sha256(b).hexdigest()=='3742f7eb44e870f452382dbab90ef226b3759c157873350110ca96e52d26dad4'
p=pefile.PE(data=bytes(b));old=0x179c4;new=0x179c8
assert p.get_data(old,4)==b'rch\0' and p.get_data(new,4)==b'\0'*4
patches=[]
for rva in [0x338d,0xbfa5,0xdf89,0xe004]:
 offset=p.get_offset_from_rva(rva);ins=bytes(b[offset:offset+7]);assert ins[:3] in [bytes.fromhex('4c8d05'),bytes.fromhex('488d15'),bytes.fromhex('488d0d')]
 assert rva+7+struct.unpack('<i',ins[3:])[0]==old
 b[offset+3:offset+7]=struct.pack('<i',new-(rva+7));patches.append({'instruction_rva':hex(rva),'old_literal':hex(old),'new_literal':hex(new)})
version=b'1.10.18\0';assert b.count(version)==1;b=b.replace(version,b'1.10.19\0')
p=pefile.PE(data=bytes(b));struct.pack_into('<I',b,p.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),p.generate_checksum())
out=r/'package/mods/VanillaDeluxe/d2rloader/plugins/d2rl-stash-search.dll';out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(b)
report={'version':'1.10.19','input_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'output_sha256':hashlib.sha256(b).hexdigest(),'patches':patches,'existing_ui_resource_lookup_preserved':True,'game_addresses_unchanged':True,'live_game_tested':False}
(r/'patch_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
