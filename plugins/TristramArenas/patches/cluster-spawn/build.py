import hashlib
import struct
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

root = Path(__file__).resolve().parent.parent
plugin = root / 'fresh/d2rloader/plugins/TristramArenas.dll'
output = root / 'D2R_Corrupted_Tristram_Cluster_Spawn_MODS.zip'
member = 'mods/VanillaDeluxe/d2rloader/plugins/TristramArenas.dll'
raw = plugin.read_bytes()
assert hashlib.sha256(raw).hexdigest() == '4f55be9777747239eb6a1ffca29f0ce0a25d2a61aa963f27ff2b70b9ff161cde'
pe = pefile.PE(data=raw, fast_load=True)
section = pe.sections[0]
assert section.Name.startswith(b'.text')
image = bytearray(raw)
cursor = 0x28860

def patch(rva, original, replacement):
    pos = pe.get_offset_from_rva(rva)
    assert len(original) == len(replacement) and image[pos:pos+len(original)] == original, hex(rva)
    image[pos:pos+len(original)] = replacement

def jump(source, target, op=b'\xe9'):
    return op + struct.pack('<i', target-source-len(op)-4)

def detour(site, original, instructions):
    global cursor
    code = bytearray()
    for instruction in instructions:
        if isinstance(instruction, str):
            code.extend(bytes.fromhex(instruction))
        else:
            target, op = instruction
            code.extend(jump(cursor+len(code), target, op))
    assert cursor + len(code) <= section.VirtualAddress + section.SizeOfRawData
    patch(cursor, b'\xcc' * len(code), code)
    before = bytes.fromhex(original)
    patch(site, before, jump(site, cursor) + b'\x90' * (len(before)-5))
    cursor += len(code)



detour(0x3EAB, '3dc8040000', [
    '817e08ad000000', (0x28875,b'\x0f\x85'),
    '83f840', (0x3EB0,b'\xe9'),
    '3dc8040000', (0x3EB0,b'\xe9'),
])



detour(0x4652, 'bb08000000', [
    'bb08000000 81fead000000 ba18000000 0f44da', (0x4657,b'\xe9'),
])
detour(0x46FF, '83fb080f84ac000000', [
    'b808000000 81fead000000 b918000000 0f44c1 39c3',
    (0x47B4,b'\x0f\x84'), (0x4708,b'\xe9'),
])
radius_stub = cursor
detour(0x4758, '4c8b9df800000049f7f3448b8514010000', [
    '4c8b9df8000000 448b8514010000',
    '81fead000000', (radius_stub+47,b'\x0f\x85'),
    '83fb10', (radius_stub+47,b'\x0f\x87'),
    '41bb19000000 41b8f4ffffff',
    '49f7f3', (0x4769,b'\xe9'),
])

struct.pack_into('<I', image, section.get_field_absolute_offset('Misc_VirtualSize'),
                 cursor-section.VirtualAddress)
patched = pefile.PE(data=bytes(image), fast_load=True)
struct.pack_into('<I', image, patched.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),
                 patched.generate_checksum())
with ZipFile(output, 'w', ZIP_DEFLATED) as archive:
    archive.writestr(member, image)
with ZipFile(output) as archive:
    assert archive.testzip() is None and archive.namelist() == [member]
    assert pefile.PE(data=archive.read(member), fast_load=True).verify_checksum()
