import argparse
import hashlib
from pathlib import Path
import struct
import re
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

DLL_PATH = "mods/VanillaDeluxe/d2rloader/plugins/EnemySkills.dll"
INPUT_HASH = "9cfe92f8d5792085b15d83ef07fbc920d47cb397e01dd4324106e0c5e1125039"
ORIGINAL_REJECT = 0x529A0
RETURN_GUARD = 0x529A2
# Failure exits after the existing Dungeon class, registration and live-unit
# checks. The ordinary cast guard still gets first chance to restore the used
# skill before the mode switch is denied.
AFTER_ROOT = {
    0x527F3: b"\x0f\x84",  # no active cast
    0x52806: b"\x0f\x84",  # optional cast guard is disabled
    0x52813: b"\x0f\x85",  # no recorded cast frame
    0x52831: b"\x0f\x84",  # cannot read current frame
    0x52848: b"\x0f\x87",  # cast window elapsed
    0x52860: b"\x0f\x84",  # cannot read current mode
    0x52872: b"\x0f\x85",  # no longer in the tracked cast mode
    0x52882: b"\x0f\x85",  # unit is free to act
    0x52898: b"\x0f\x84",  # no matching skill to restore
}


def branch(opcode, at, target):
    return opcode + struct.pack("<i", target - at - len(opcode) - 4)


def patch(raw):
    if hashlib.sha256(raw).hexdigest() != INPUT_HASH:
        raise ValueError("Expected the EnemySkills.dll from Gorath Hammer / Grizzly")
    pe = pefile.PE(data=raw, fast_load=True)
    output = bytearray(raw)

    def write(rva, before, after):
        offset = pe.get_offset_from_rva(rva)
        assert len(before) == len(after) and output[offset:offset + len(before)] == before
        output[offset:offset + len(after)] = after

    section = pe.sections[0]
    assert section.Name.startswith(b".text")
    entry = section.VirtualAddress + section.Misc_VirtualSize
    # ESI contains the requested mode, and EDI is the original return value.
    # GetHit (3) and Knockback (13) return true to ChangeMode's rejection
    # guard. Block (6) and all other modes keep their original treatment.
    # Gorath's original MonStatsEx already has both reactions disabled.
    # Keep his entire previous behavior while isolating the regression.
    # The class came from the guarded unit identity read at RBP+0x134.
    code = bytes.fromhex("8b8534010000 2d23070000 83f810")
    code += branch(b"\x0f\x82", entry + len(code), ORIGINAL_REJECT)
    code += bytes.fromhex("83fe03 7409 83fe0d")
    code += branch(b"\x0f\x85", entry + len(code), ORIGINAL_REJECT)
    code += bytes.fromhex("bf01000000")
    code += branch(b"\xe9", entry + len(code), RETURN_GUARD)
    assert section.Misc_VirtualSize + len(code) <= section.SizeOfRawData
    write(entry, b"\xcc" * len(code), code)
    struct.pack_into("<I", output, section.get_field_absolute_offset("Misc_VirtualSize"),
                     section.Misc_VirtualSize + len(code))
    for rva, opcode in AFTER_ROOT.items():
        write(rva, branch(opcode, rva, ORIGINAL_REJECT), branch(opcode, rva, entry))
    pe = pefile.PE(data=bytes(output), fast_load=True)
    struct.pack_into("<I", output, pe.OPTIONAL_HEADER.get_field_absolute_offset("CheckSum"),
                     pe.generate_checksum())
    return bytes(output)


MONSTERS = re.compile(rb"(?m)^r3tl3v(?:1[0-5]|[0-9])\t[^\r\n]*")
HAMMER = b"\tr3tl12s1\tA1\t"
ORIGINAL = b"\tr3tl3s1\tr3tl_seq_3_1\t"
DUNGEON_PATH = DLL_PATH.replace("EnemySkills.dll", "DungeonLegends.dll")
DUNGEON_HASH = "b576f3a20f574d768a33aacb388ac2747e78331543d6340bdfa248b646305312"


def restore_table(raw):
    def restore(match):
        row = match[0]
        assert row.count(HAMMER) == 1
        return row.replace(HAMMER, ORIGINAL, 1)

    result, count = MONSTERS.subn(restore, raw)
    assert count == 16
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("previous_package", type=Path)
    parser.add_argument("original_dungeon", type=Path)
    parser.add_argument("output_package", type=Path)
    args = parser.parse_args()
    dungeon = args.original_dungeon.read_bytes()
    assert hashlib.sha256(dungeon).hexdigest() == DUNGEON_HASH
    with ZipFile(args.previous_package) as source:
        entries = {
            DLL_PATH: patch(source.read(DLL_PATH)),
            DUNGEON_PATH: dungeon,
            **{name: restore_table(source.read(name)) for name in source.namelist()
               if name.endswith("/monstats.txt")},
        }
    assert len(entries) == 4
    with ZipFile(args.output_package, "w", ZIP_DEFLATED) as output:
        for name, data in entries.items():
            output.writestr(name, data)
