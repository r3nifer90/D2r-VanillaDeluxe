import hashlib
import struct
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

import pefile

work = Path(__file__).resolve().parent
output = work.parent / 'D2R_EnemySkills_Sovereign_Blessed_Hammer_MODS.zip'
member = 'mods/VanillaDeluxe/d2rloader/plugins/EnemySkills.dll'
raw = (work / 'sovereign_base.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest() == 'e190c18c34788b01c8a5c8b66cdbb5fe83d4a1705ca3d128488a94a56c3c2cc8'
pe = pefile.PE(data=raw, fast_load=True)
image = bytearray(raw)

def patch(rva, expected, replacement):
    pos = pe.get_offset_from_rva(rva)
    assert len(expected) == len(replacement) and image[pos:pos+len(expected)] == expected, hex(rva)
    image[pos:pos+len(expected)] = replacement

def branch(source, target, opcode=b'\xe9'):
    return opcode + struct.pack('<i', target - source - len(opcode) - 4)

def route(site, stub, original, accept, reject):


    code = bytearray()
    def emit(hex_bytes):
        code.extend(bytes.fromhex(hex_bytes))
    def go(target, op=b'\xe9'):
        code.extend(branch(stub + len(code), target, op))
    emit('8d88ddf8ffff babf010000 83f910')
    go(accept, b'\x0f\x82')
    emit('8d884df8ffff bada010000 83f910')
    go(accept, b'\x0f\x82')
    emit('2d2f030000 3dc4030000')
    go(reject, b'\x0f\x83')
    emit('a801')
    go(reject, b'\x0f\x85')
    emit('ba70000000')
    go(accept)
    assert len(code) <= 0x60
    patch(stub, bytes(len(code)), code)
    patch(site, original, branch(site, stub) + b'\x90' * (len(original)-5))

route(0x16B3099, 0x16B2000, bytes.fromhex('8d88ddf8ffffbabf01000083f9107213054df8ffffbada01000083f8100f83b9020000'),
      0x16B30BC, 0x16B3375)
route(0x16B3A3B, 0x16B2060, bytes.fromhex('8d88ddf8ffff83f9100f92c1054df8ffff83f8100f92c008c80f8401030000'),
      0x16B3A5A, 0x16B3D5B)
route(0x16B4858, 0x16B20C0, bytes.fromhex('8d88ddf8ffffbabf01000083f9107213054df8ffffbada01000083f8100f8312030000'),
      0x16B487B, 0x16B4B8D)
patch(0x34A6A, branch(0x34A6A, 0x66E50, b'\xe8'), branch(0x34A6A, 0x16B39A0, b'\xe8'))

pe = pefile.PE(data=bytes(image), fast_load=True)
struct.pack_into('<I', image, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), pe.generate_checksum())
with ZipFile(output, 'w', ZIP_DEFLATED) as archive:
    archive.writestr(member, image)
with ZipFile(output) as archive:
    assert archive.testzip() is None and archive.namelist() == [member]
    assert pefile.PE(data=archive.read(member), fast_load=True).verify_checksum()
