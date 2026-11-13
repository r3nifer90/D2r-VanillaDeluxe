import hashlib
import struct
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

work = Path(__file__).resolve().parent
output = work.parent / 'D2R_EnemySkills_Skeletal_Mage_Native_Attack_MODS.zip'
member = 'mods/VanillaDeluxe/d2rloader/plugins/EnemySkills.dll'
raw = (work / 'current.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest() == '3a4c246d6140e15527c9613022a4017d44565e378f426d34e5ed3cef540d7da0'
pe = pefile.PE(data=raw, fast_load=True)
image = bytearray(raw)

def patch(rva, old, new):
    offset = pe.get_offset_from_rva(rva)
    assert len(old) == len(new)
    assert image[offset:offset+len(old)] == old
    image[offset:offset+len(old)] = new

def jump(start, end):
    return b'\xe9' + struct.pack('<i', end - start - 5)

site, native_call, stub = 0x4D5BE, 0x4D5CB, 0x16B5160
before = bytes.fromhex('4c8b85b80100004d89ee4589f1')
assert len(before) == native_call - site
code = (work / 'server_mode.bin').read_bytes()
assert code.startswith(before)
code += jump(stub + len(code), native_call)
patch(stub, b'\xcc' * len(code), code)
patch(site, before, jump(site, stub) + b'\x90' * (len(before) - 5))
pe = pefile.PE(data=bytes(image), fast_load=True)
struct.pack_into('<I', image, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), pe.generate_checksum())
with ZipFile(output, 'w', ZIP_DEFLATED) as archive:
    archive.writestr(member, image)
with ZipFile(output) as archive:
    assert archive.testzip() is None and archive.namelist() == [member]
    assert pefile.PE(data=archive.read(member), fast_load=True).verify_checksum()
