import argparse
import hashlib
from pathlib import Path
import re
import struct
from zipfile import ZIP_DEFLATED, ZipFile

import pefile


def image(raw, expected):
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError("DLL differs from the supported build")
    return pefile.PE(data=raw, fast_load=True), bytearray(raw)


def put(pe, data, rva, before, after):
    offset = pe.get_offset_from_rva(rva)
    assert len(before) == len(after) and data[offset:offset + len(before)] == before, hex(rva)
    data[offset:offset + len(after)] = after


def finish(data):
    pe = pefile.PE(data=bytes(data), fast_load=True)
    offset = pe.OPTIONAL_HEADER.get_field_absolute_offset("CheckSum")
    struct.pack_into("<I", data, offset, pe.generate_checksum())
    return bytes(data)


def jump(at, to, opcode=b"\xe9"):
    return opcode + struct.pack("<i", to - at - len(opcode) - 4)


def patch_enemy(raw):
    pe, data = image(raw, ENEMY_HASH)




    site, stub = 0x4CD10, 0x72958
    code = bytes.fromhex("6681fadd00")
    code += jump(stub + len(code), 0x4CC9C, b"\x0f\x84")
    code += bytes.fromhex("6681fa4601")
    code += jump(stub + len(code), 0x4CC9C, b"\x0f\x84")
    code += jump(stub + len(code), 0x4CD1A)
    section = pe.sections[0]
    assert section.Name.startswith(b".text") and stub == section.VirtualAddress + section.Misc_VirtualSize
    assert section.Misc_VirtualSize + len(code) <= section.SizeOfRawData
    put(pe, data, stub, b"\xcc" * len(code), code)
    struct.pack_into("<I", data, section.get_field_absolute_offset("Misc_VirtualSize"),
                     section.Misc_VirtualSize + len(code))
    put(pe, data, site, bytes.fromhex("0fb7c23ddd0000007482"), jump(site, stub) + b"\x90" * 5)
    return finish(data)


MONSTER_ROW = re.compile(rb"(?m)^r3tl3v(?:1[0-5]|[0-9])\t[^\r\n]*")


def patch_monsters(raw, embedded=False):
    def patch_row(match):
        old = match[0]
        assert old.count(b"\tr3tl3s1\tr3tl_seq_3_1\t") == 1
        new = old.replace(b"\tr3tl3s1\tr3tl_seq_3_1\t", b"\tr3tl12s1\tA1\t")
        if embedded:


            assert new.endswith(b"\t0")
            new += b"0" * (len(old) - len(new))
            assert len(new) == len(old)
        return new

    updated, count = MONSTER_ROW.subn(patch_row, raw)
    assert count == (32 if embedded else 16), count
    return updated


def patch_dungeon(raw):
    pe, data = image(raw, DUNGEON_HASH)

    put(pe, data, 0x3EA5D, bytes.fromhex("030100be01bf01c001"), bytes.fromhex("000100be01da01c001"))
    return finish(bytearray(patch_monsters(bytes(data), embedded=True)))


def build(enemy, dungeon, excel, output):
    mod = "mods/VanillaDeluxe/"
    entries = {
        mod + "d2rloader/plugins/EnemySkills.dll": patch_enemy(enemy.read_bytes()),
        mod + "d2rloader/plugins/DungeonLegends.dll": patch_dungeon(dungeon.read_bytes()),
    }
    for rel in ("monstats.txt", "base/monstats.txt"):
        entries[mod + "VanillaDeluxe.mpq/data/global/excel/" + rel] = patch_monsters((excel / rel).read_bytes())
    with ZipFile(output, "w", ZIP_DEFLATED) as archive:
        for path, data in entries.items():
            archive.writestr(path, data)
    with ZipFile(output) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == entries.keys()


ENEMY_HASH = "33e5fff8f3835c3658228938091cba1a0e9dd86e5fccbff2c5fb898ddc20d76a"
DUNGEON_HASH = "b576f3a20f574d768a33aacb388ac2747e78331543d6340bdfa248b646305312"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("enemy", "dungeon", "excel", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    build(**vars(parser.parse_args()))
