from pathlib import Path
import struct
from sys import argv
from zipfile import ZipFile

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RDX, UC_X86_REG_RIP

with ZipFile(argv[1]) as package:
    dlls = {Path(name).name: package.read(name) for name in package.namelist() if name.endswith(".dll")}
enemy = pefile.PE(data=dlls["EnemySkills.dll"])
dungeon = pefile.PE(data=dlls["DungeonLegends.dll"])
assert enemy.verify_checksum() and dungeon.verify_checksum()
assert struct.unpack("<3H", dungeon.get_data(0x3EA60, 6)) == (446, 474, 448)
assert struct.unpack("<3H", dungeon.get_data(0x3EE50, 6)) == (473, 474, 475)
assert dungeon.get_data(0x3EA5C, 3) == bytes([4, 0, 1])

base = enemy.OPTIONAL_HEADER.ImageBase
uc = Uc(UC_ARCH_X86, UC_MODE_64)
uc.mem_map(base, enemy.OPTIONAL_HEADER.SizeOfImage)
uc.mem_write(base, enemy.get_memory_mapped_image())
endpoints = {0x4CC9C, 0x4CD1A}
uc.hook_add(UC_HOOK_CODE, lambda emu, address, size, _: emu.emu_stop() if address-base in endpoints else None)
for skill in (326, 221, 70, 80, 405, 420):
    uc.reg_write(UC_X86_REG_RDX, skill)
    uc.emu_start(base + 0x4CD10, base + 0x4CD20, count=12)
    expected = 0x4CC9C if skill in (326, 221) else 0x4CD1A
    assert uc.reg_read(UC_X86_REG_RIP) == base + expected
assert enemy.get_data(0x3195F, 5) == bytes.fromhex("bae2000000")
assert enemy.get_data(0x31A0E, 10) == bytes.fromhex("837f08010f85a1030000")
print("PASS: Grizzly and Raven exclusions, unrelated pet candidates, skeleton cast target, existing self-buff guard, and both Hammer profiles.")
