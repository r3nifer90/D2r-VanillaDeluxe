import hashlib
import struct
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

import pefile

WORK = Path(__file__).resolve().parent
DLL = 'mods/VanillaDeluxe/d2rloader/plugins/EnemySkills.dll'
OUTPUT = WORK.parent / 'D2R_EnemySkills_Skeletal_Mage_Repeat_Animation_MODS.zip'
WRAPPER = 0x16B5160
HOOK_SLOT = 0x770B8
ORIGINAL = 0x2AF30
CONTEXT = 0x114CBB0

raw = (WORK / 'current.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest() == '3a4c246d6140e15527c9613022a4017d44565e378f426d34e5ed3cef540d7da0'
pe = pefile.PE(data=raw)
assert any(item.rva == HOOK_SLOT and item.type == 10
           for block in pe.DIRECTORY_ENTRY_BASERELOC for item in block.entries)
image = bytearray(raw)
base = pe.OPTIONAL_HEADER.ImageBase

def patch(rva, expected, replacement):
    offset = pe.get_offset_from_rva(rva)
    assert image[offset:offset+len(expected)] == expected
    assert len(expected) == len(replacement)
    image[offset:offset+len(expected)] = replacement

code = bytearray((WORK / 'client_mode.bin').read_bytes())
assert code[:7] == bytes.fromhex('4c8b1d9f181201')
struct.pack_into('<i', code, 3, CONTEXT - (WRAPPER + 7))
code += b'\xe9' + struct.pack('<i', ORIGINAL - (WRAPPER + len(code) + 5))
patch(WRAPPER, b'\xcc' * len(code), code)
patch(HOOK_SLOT, struct.pack('<Q', base + ORIGINAL), struct.pack('<Q', base + WRAPPER))
pe = pefile.PE(data=bytes(image), fast_load=True)
struct.pack_into('<I', image, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), pe.generate_checksum())

with ZipFile(OUTPUT, 'w', ZIP_DEFLATED) as archive:
    archive.writestr(DLL, image)
with ZipFile(OUTPUT) as archive:
    assert archive.testzip() is None and archive.namelist() == [DLL]
    assert pefile.PE(data=archive.read(DLL), fast_load=True).verify_checksum()
