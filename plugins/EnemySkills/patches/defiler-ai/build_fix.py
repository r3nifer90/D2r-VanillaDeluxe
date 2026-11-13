from pathlib import Path
import hashlib, struct, json, zipfile
import pefile

ROOT=Path(__file__).resolve().parent
INPUT=ROOT.parent/'kargor_today/d2rloader/plugins/EnemySkills.dll'
if not INPUT.exists(): INPUT=ROOT/'inputs/EnemySkills_1.2.6.dll'
EXPECTED='2c07372d54cb01c7831ed1c0826c998514b1a9ebd1eb4a09ec71abe89bf27f1a'
raw=INPUT.read_bytes()
assert hashlib.sha256(raw).hexdigest()==EXPECTED, 'Requires exact EnemySkills 1.2.6-summon-animation'
pe=pefile.PE(data=raw); data=bytearray(raw); changes=[]
def put(rva, code, label):
    at=pe.get_offset_from_rva(rva)
    changes.append(dict(rva=hex(rva),purpose=label,before=raw[at:at+len(code)].hex(),after=code.hex()))
    data[at:at+len(code)]=code
def jump(at,to): return b'\xe9'+struct.pack('<i',to-at-5)




put(0x4a45d,jump(0x4a45d,0x4a854),'Prefer pursuit and basic Attack over Health Link')
assert pe.get_data(0x4a8a6,3)==bytes.fromhex('83ff03')
put(0x4a8a8,b'\x02','Approach to two subtiles before melee')




at=0x4a480
code=bytearray(bytes.fromhex('488b4e50 488b5520 4589f0 4589f9'))
call_at=at+len(code)
code+=b'\xff\x15'+struct.pack('<i',0x114d570-(call_at+6))
code+=bytes.fromhex('85c0 b82c010000 b958020000 480f45c1 4c01e8 48894610')
code+=jump(at+len(code),0x4a8fa)
put(at,code,'Native coordinate pathfinding; retry 300ms, accepted walk 600ms')
put(0x4a8ca,jump(0x4a8ca,at),'Defiler movement uses coordinate pathfinder')

old=b'1.2.6-summon-animation\0';new=b'1.2.7-defiler-melee-ai\0'
assert len(old)==len(new) and raw.count(old)==1
version_at=raw.index(old);data[version_at:version_at+len(old)]=new
check=pefile.PE(data=bytes(data));struct.pack_into('<I',data,check.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),check.generate_checksum())


start=pe.get_offset_from_rva(0x4a3e0);end=pe.get_offset_from_rva(0x4aae4)
for section in pe.sections:
    if section.Name.startswith(b'.text'):
        a=section.PointerToRawData;b=a+section.SizeOfRawData
        assert data[a:start]==raw[a:start] and data[end:b]==raw[end:b]
        assert data[start:pe.get_offset_from_rva(0x4a45d)]==raw[start:pe.get_offset_from_rva(0x4a45d)]
patched=bytes(data)
output=ROOT/'release/mods/VanillaDeluxe/d2rloader/plugins/EnemySkills.dll'
output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(patched)
report=dict(input_sha256=EXPECTED,output_sha256=hashlib.sha256(patched).hexdigest(),changes=changes,game_tested=False,
            scope='Only existing registered monster-owned Summon Defiler branch; no shared table or player AI edits')
(ROOT/'patch_report.json').write_text(json.dumps(report,indent=2)+'\n')
print('Built',output)
