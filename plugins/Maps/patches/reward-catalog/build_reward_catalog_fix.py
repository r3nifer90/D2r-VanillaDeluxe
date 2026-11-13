from pathlib import Path
import hashlib, json, struct
import pefile

ROOT = Path(__file__).resolve().parents[2]
src = ROOT / 'upload/d2rl-maps.dll'
out = ROOT / 'work/map_open_fix/d2rl-maps.dll'
b = bytearray(src.read_bytes())
assert hashlib.sha256(b).hexdigest() == 'eba730b7073b2bc79172749aed85f5aa608e3423f8a36b50b5e66af5ef6c9f0b'
p = pefile.PE(data=b)
base = p.OPTIONAL_HEADER.ImageBase
section = next(s for s in p.sections if s.Name.rstrip(b'\0') == b'.vdexp')

rva = section.VirtualAddress + section.Misc_VirtualSize
name = b'VDMB Elite Gear\0'
assert rva == 0x161ba0
offset = p.get_offset_from_rva(rva)
assert offset + len(name) <= section.PointerToRawData + section.SizeOfRawData
assert not any(b[offset:offset + len(name)])
b[offset:offset + len(name)] = name
struct.pack_into('<I', b, section.get_file_offset() + 8, section.Misc_VirtualSize + len(name))
relocations = {entry.rva for block in p.DIRECTORY_ENTRY_BASERELOC for entry in block.entries if entry.type == 10}
changes = []
for i in range(60):
    at = 0xd07b0 + i * 24 + 8
    pos = p.get_offset_from_rva(at)
    old, = struct.unpack_from('<Q', b, pos)
    assert p.get_data(old - base, 64).split(b'\0')[0] == b'R3DB O0284'
    assert at in relocations
    struct.pack_into('<Q', b, pos, base + rva)
    changes.append(hex(at))
out.write_bytes(b)
report = {
    'input_sha256': hashlib.sha256(src.read_bytes()).hexdigest(),
    'output_sha256': hashlib.sha256(b).hexdigest(),
    'change': 'Update all 60 per-map elite reward validation names to the existing VDMB Elite Gear wrapper. Preserve the native structural validation and relocation entries.',
    'pointer_rvas': changes,
    'new_string_rva': hex(rva),
    'tables_modified': False,
    'machine_code_modified': False,
}
out.with_name('build_reward_catalog_fix.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
