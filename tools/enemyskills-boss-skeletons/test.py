import struct
from pathlib import Path
from sys import argv
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *
import pefile
from build import patch_dll

before = Path(argv[1]).read_bytes()
after = patch_dll(before)
base = 0x180000000
scratch = 0x200000000
pe = pefile.PE(data=after)
check = Uc(UC_ARCH_X86, UC_MODE_64)
check.mem_map(base, 0x1700000)
check.mem_write(base, pe.get_memory_mapped_image())
check.mem_map(scratch, 0x10000)

def drive(begin, end, branches=()):
    visited = []
    handle = check.hook_add(UC_HOOK_CODE, lambda u,a,s,d: visited.append(a-base))
    check.emu_start(base+begin, base+end, count=70)
    check.hook_del(handle)
    assert check.reg_read(UC_X86_REG_RIP) == base+end
    assert all(b not in visited for b in branches)

for kind, target in [(0, scratch+0x1000), (1, scratch+0x2000), (5,scratch+0x2000),
                     (6,scratch+0x2000), (7,scratch+0x2000)]:
    check.reg_write(UC_X86_REG_RDI,scratch)
    check.reg_write(UC_X86_REG_RBP,scratch+0x3000)
    check.reg_write(UC_X86_REG_R12,scratch+0x1000)
    check.mem_write(scratch+8,struct.pack("<I",kind))
    check.mem_write(scratch+0x34a0,struct.pack("<Q",scratch+0x2000))
    drive(0x3194b,0x31993,(0x3197a,))
    assert check.reg_read(UC_X86_REG_RAX)==target
# Kind 2 still enters the original corpse lookup.
check.mem_write(scratch+8,struct.pack("<I",2))
drive(0x3194b,0x3197a)
for skill in (70,80,373):
    check.mem_write(scratch+0x31b8,struct.pack("<I",skill))
    drive(0x17036,0x170c8,(0x17050,))
    assert check.mem_read(scratch+0x31b4,4)==bytes(4)
    drive(0x171ad,0x1728d,(0x171c8,))
# Defiler AI body and original player skill metadata are byte-identical.
old = pefile.PE(data=before,fast_load=True)
assert pe.get_data(0x4a3e0,0x704)==old.get_data(0x4a3e0,0x704)
assert pe.get_data(0x7c26c,24)==old.get_data(0x7c26c,24)
print("PASS: cast targeting, both batch branches, retained Defiler AI and original skill metadata")
