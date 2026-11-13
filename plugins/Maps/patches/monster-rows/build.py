from pathlib import Path
import struct,hashlib,json
import pefile

W=Path(__file__).resolve().parent;ROOT=W.parent
SRC=ROOT/'map_repair_inputs/d2rloader/plugins/d2rl-maps.dll'
raw=SRC.read_bytes();assert hashlib.sha256(raw).hexdigest()=='ed2c47920664a777a04b123090c321d6be3bc64310de08358896ca52a9bf0413'
p=pefile.PE(data=raw);data=bytearray(raw)


patches=[(0x2a622,0x2a62a,'817c24786d0d00007456','treasure MonProp calibration'),
         (0x2c230,0x2c23b,'81bcc4c80000006d0d00007405','monster level calibration'),
         (0x53043,0x5304e,'81bc24880000006d0d00007407','map monster reward validation')]

patches[1]=(0x2c230,0x2c23b,'81bc24c80000006d0d00007405',patches[1][3])
report=[]
for start,jump,expected,label in patches:
    assert p.get_data(start,len(bytes.fromhex(expected)))==bytes.fromhex(expected),(hex(start),p.get_data(start,12).hex())
    off=p.get_offset_from_rva(jump);assert data[off]==0x74;data[off]=0x73
    report.append({'validator':label,'rva':jump,'before':'JE (rowCount == 3437)','after':'JAE (rowCount >= 3437)'})
assert data.count(b'0.12.4\0')==1;data=data.replace(b'0.12.4\0',b'0.12.5\0')
q=pefile.PE(data=bytes(data));struct.pack_into('<I',data,q.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),q.generate_checksum())
dest=W/'package/mods/VanillaDeluxe/d2rloader/plugins/d2rl-maps.dll';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
(W/'build.json').write_text(json.dumps({'source_sha256':hashlib.sha256(raw).hexdigest(),'output_sha256':hashlib.sha256(data).hexdigest(),'patches':report,'native_guard_lower_bound':3437,'version':'0.12.5','game_test':False},indent=2))
print(json.dumps(report,indent=2))
