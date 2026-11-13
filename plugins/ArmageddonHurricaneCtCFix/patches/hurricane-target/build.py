import hashlib
import struct
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

work = Path(__file__).resolve().parent
root = work.parents[1]
output = root / 'D2R_Hurricane_CtC_Windhammer_MODS.zip'
member = 'mods/VanillaDeluxe/d2rloader/plugins/d2rl-ruffneckk-armageddon-hurricane-ctc-fix.dll'
raw = (work / 'current.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest() == 'a83741472bff4208645f175695f6cc4c1923ece39bf2735ae076bba3ec72c17e'
pe = pefile.PE(data=raw, fast_load=True)
image = bytearray(raw)
opt = pe.OPTIONAL_HEADER
align = lambda n,a: (n+a-1)//a*a
stub = opt.SizeOfImage
site, original_end = 0xBBE0, 0xBBB8
old = bytes.fromhex('4c8b1529ed010041ffd2ebcc')
stub_code = bytearray()
def emit(code):
    stub_code.extend(bytes.fromhex(code))
def jump(dest, opcode=b'\xe9'):
    next_rva = stub + len(stub_code) + len(opcode) + 4
    stub_code.extend(opcode + struct.pack('<i', dest-next_rva))


emit('81fbfa000000')
jump(stub+0x42,b'\x0f\x85')
emit('66813efa00')
jump(stub+0x42,b'\x0f\x85')
emit('6683be7801000001')
jump(stub+0x42,b'\x0f\x85')
emit('66c786780100000000')
jump(0x2A910,b'\xff\x15')
emit('66c786780100000100')
jump(original_end)
assert len(stub_code) == 0x42
jump(0x2A910,b'\xff\x15')
jump(original_end)

offset = pe.get_offset_from_rva(site)
assert image[offset:offset+len(old)] == old
image[offset:offset+len(old)] = b'\xe9'+struct.pack('<i',stub-site-5)+b'\x90'*(len(old)-5)

section_table_end = pe.sections[-1].get_file_offset()+40
assert section_table_end+40 <= opt.SizeOfHeaders
assert image[section_table_end:section_table_end+40] == bytes(40)
raw_offset = align(len(image),opt.FileAlignment)
raw_size = align(len(stub_code),opt.FileAlignment)
image.extend(bytes(raw_offset-len(image)))
image.extend(stub_code)
image.extend(bytes(raw_size-len(stub_code)))
struct.pack_into('<8sIIIIIIHHI',image,section_table_end,b'.ctcfix',len(stub_code),stub,
                 raw_size,raw_offset,0,0,0,0,0x60000020)
struct.pack_into('<H',image,pe.FILE_HEADER.get_field_absolute_offset('NumberOfSections'),len(pe.sections)+1)
struct.pack_into('<I',image,opt.get_field_absolute_offset('SizeOfImage'),stub+align(len(stub_code),opt.SectionAlignment))
struct.pack_into('<I',image,opt.get_field_absolute_offset('SizeOfCode'),opt.SizeOfCode+raw_size)
patched = pefile.PE(data=bytes(image), fast_load=True)
struct.pack_into('<I',image,patched.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),patched.generate_checksum())
with ZipFile(output,'w',ZIP_DEFLATED) as archive:
    archive.writestr(member,image)
with ZipFile(output) as archive:
    assert archive.testzip() is None and archive.namelist()==[member]
    assert pefile.PE(data=archive.read(member),fast_load=True).verify_checksum()
