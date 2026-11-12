import struct
import hashlib
from sys import argv
from zipfile import ZipFile

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX
from unicorn.x86_const import UC_X86_REG_RIP, UC_X86_REG_RSI, UC_X86_REG_RSP
from build import DUNGEON_HASH, DUNGEON_PATH, DLL_PATH, restore_table, patch

with ZipFile(argv[1]) as archive:
    original = archive.read(DLL_PATH)
    tables = {name: archive.read(name) for name in archive.namelist() if name.endswith("/monstats.txt")}
with ZipFile(argv[2]) as archive:
    assert archive.testzip() is None and set(archive.namelist()) == {DLL_PATH, DUNGEON_PATH, *tables}
    assert hashlib.sha256(archive.read(DUNGEON_PATH)).hexdigest() == DUNGEON_HASH
    for name, original_table in tables.items():
        assert archive.read(name) == restore_table(original_table)
    updated = archive.read(DLL_PATH)
assert updated == patch(original)
pe = pefile.PE(data=updated)
old = pefile.PE(data=original)
assert pe.verify_checksum()
assert old.get_data(0x52797, 0x527EB - 0x52797) == pe.get_data(0x52797, 0x527EB - 0x52797)
for start, count in [(0x72958, 27), (0x4CD10, 10), (0x3195F, 21), (0x4A3E0, 0x704)]:
    assert old.get_data(start, count) == pe.get_data(start, count)

base = pe.OPTIONAL_HEADER.ImageBase
scratch = 0x200000000
emu = Uc(UC_ARCH_X86, UC_MODE_64)
emu.mem_map(base, pe.OPTIONAL_HEADER.SizeOfImage)
emu.mem_write(base, pe.get_memory_mapped_image())
emu.mem_map(scratch, 0x10000)
unit, boss, stop = scratch + 0x100, scratch + 0x1000, scratch + 0x2000

def put32(address, value):
    emu.mem_write(address, struct.pack("<I", value))

def ret(value):
    stack = emu.reg_read(UC_X86_REG_RSP)
    destination, = struct.unpack("<Q", emu.mem_read(stack, 8))
    emu.reg_write(UC_X86_REG_RSP, stack + 8)
    emu.reg_write(UC_X86_REG_RAX, value)
    emu.reg_write(UC_X86_REG_RIP, destination)

def run(mode, kind=0x703, registered=True, alive=True, cast_active=False, sovereign_result=False):
    emu.reg_write(UC_X86_REG_RSP, scratch + 0xFFF8)
    emu.mem_write(scratch + 0xFFF8, struct.pack("<Q", stop))
    emu.reg_write(UC_X86_REG_RCX, scratch + 0x4000)
    emu.reg_write(UC_X86_REG_RDX, unit)
    from unicorn.x86_const import UC_X86_REG_R8
    emu.reg_write(UC_X86_REG_R8, mode)
    put32(boss + 0xC0, int(cast_active))
    put32(boss + 0xC4, 0)
    put32(boss + 0xA4, 488)

    def mock(uc, address, size, user):
        offset = address - base
        if address == stop:
            uc.emu_stop()
        elif offset == 0x6FAD0:
            dst = uc.reg_read(UC_X86_REG_RDX)
            uc.mem_write(dst, struct.pack("<3I", 1, kind, 5))
            ret(1)
        elif offset == 0x13B60:
            ret(boss if registered else 0)
        elif offset == 0x13CD0:
            ret(int(alive))
        elif offset == 0x52100:
            ret(1)
        elif offset == 0x52D30:
            ret(int(sovereign_result))

    hook = emu.hook_add(UC_HOOK_CODE, mock)
    emu.emu_start(base + 0x526D0, stop, count=260)
    emu.hook_del(hook)
    assert emu.reg_read(UC_X86_REG_RIP) == stop
    return emu.reg_read(UC_X86_REG_EAX)

for mode in range(16):
    assert run(mode) == (mode in (3, 13))
    assert run(mode, cast_active=True) == (mode in (3, 13))
    assert run(mode, registered=False) == 0
    assert run(mode, alive=False) == 0
    assert run(mode, kind=16) == 0
for kind in range(0x723, 0x733):
    assert run(3, kind=kind) == run(13, kind=kind) == 0
assert run(3, kind=0x722) == run(13, kind=0x733) == 1
assert run(3, kind=0x32F, sovereign_result=True) == 1
assert run(13, kind=0x32F, sovereign_result=False) == 0
print("PASS: living registered Dungeon Legends reject GetHit/Knockback only; rejected registrations, ordinary monsters, Sovereign handling and prior fixes verified.")
