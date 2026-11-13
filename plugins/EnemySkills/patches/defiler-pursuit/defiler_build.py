import hashlib
import struct
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

work = Path(__file__).resolve().parent
output = work.parent / 'D2R_EnemySkills_Defiler_Native_Pursuit_MODS.zip'
member = 'mods/VanillaDeluxe/d2rloader/plugins/EnemySkills.dll'
raw = (work / 'defiler_base.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest() == '28615e49c9b5e1c5c86be581f201b2619fa09629e6b3c78dc5c3c839fa4530d9'
pe = pefile.PE(data=raw, fast_load=True)
image = bytearray(raw)

def patch(rva, expected, replacement):
    pos = pe.get_offset_from_rva(rva)
    assert len(expected) == len(replacement) and image[pos:pos+len(expected)] == expected, hex(rva)
    image[pos:pos+len(expected)] = replacement

def branch(source, target, opcode=b'\xe9'):
    return opcode + struct.pack('<i', target - source - len(opcode) - 4)

site, stub, flank, native_slot = 0x4A48E, 0x16B2120, 0x16B3420, 0x114D5C8



code = bytearray.fromhex('ff8680000000 f6868000000001')
def emit(op, target):
    code.extend(branch(stub + len(code), target, op))
emit(b'\x0f\x84', flank)
code.extend(bytes.fromhex('48833d'))
code.extend(struct.pack('<i', native_slot - (stub + len(code) + 5)))
code.append(0)
emit(b'\x0f\x84', flank)
code.extend(bytes.fromhex('4c8b55c8 4d85d2'))
emit(b'\x0f\x84', flank)
code.extend(bytes.fromhex('4d89d0 4531c9'))
emit(b'\xff\x25', native_slot)
patch(stub, bytes(len(code)), code)
patch(site, branch(site, flank, b'\xe8'), branch(site, stub, b'\xe8'))


patch(0x4A8A6, bytes.fromhex('83ff02'), bytes.fromhex('83ff03'))
pe = pefile.PE(data=bytes(image), fast_load=True)
struct.pack_into('<I', image, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), pe.generate_checksum())
with ZipFile(output, 'w', ZIP_DEFLATED) as archive:
    archive.writestr(member, image)
with ZipFile(output) as archive:
    assert archive.testzip() is None and archive.namelist() == [member]
    assert pefile.PE(data=archive.read(member), fast_load=True).verify_checksum()
