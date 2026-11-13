import argparse
import hashlib
from pathlib import Path
import struct
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

MOD = "mods/VanillaDeluxe/"
PLUGINS = MOD + "d2rloader/plugins/"
EXCEL = MOD + "VanillaDeluxe.mpq/data/global/excel/"
META = 0x7B0F8
PROFILE_KIND = 0x3EA5D
EMBEDDED_ROWS = ((0x136BA1, 0x13C7FE), (0x1B9ED1, 0x1BFB2E))


def open_pe(raw):
    return pefile.PE(data=raw, fast_load=True), bytearray(raw)


def put(pe, data, rva, expected, replacement):
    assert len(expected) == len(replacement)
    at = pe.get_offset_from_rva(rva)
    assert data[at:at + len(expected)] == expected, hex(rva)
    data[at:at + len(expected)] = replacement


def checksum(data):
    pe = pefile.PE(data=bytes(data), fast_load=True)
    struct.pack_into("<I", data, pe.OPTIONAL_HEADER.get_field_absolute_offset("CheckSum"), pe.generate_checksum())
    return bytes(data)


def hammer_row(template, header, budget=None):
    fields = template.split(b"\t")
    assert len(fields) == len(header) and fields[:2] == [b"r3tl12s1", b"474"]
    fields[:2] = [b"r3tl3s1", b"447"]
    if budget is not None:

        excess = len(b"\t".join(fields)) - budget
        for i, key in enumerate(header):
            if excess <= 0:
                break
            if key.startswith(b"*") and b"desc" in key.lower():
                trim = min(len(fields[i]), excess)
                fields[i] = fields[i][:-trim] if trim else fields[i]
                excess -= trim
        assert excess == 0
    return b"\t".join(fields)


def edit_skills(raw):
    lines = raw.splitlines(keepends=True)
    header = lines[0].rstrip(b"\r\n").split(b"\t")
    old, = (i for i, line in enumerate(lines) if line.startswith(b"r3tl3s1\t447\t"))
    template, = (line.rstrip(b"\r\n") for line in lines if line.startswith(b"r3tl12s1\t474\t"))
    line = lines[old]
    lines[old] = hammer_row(template, header) + line[len(line.rstrip(b"\r\n")):]
    return b"".join(lines), header


def edit_enemy(raw):
    pe, data = open_pe(raw)
    leap = struct.pack("<4H4B", 447, 132, 40, 77, 14, 3, 0, 0)
    hammer = struct.pack("<4H4B", 474, 112, 0, 73, 4, 0, 0, 0)
    assert pe.get_data(0x7B23C, 12) == hammer
    put(pe, data, META, leap, struct.pack("<H", 447) + hammer[2:])
    return checksum(data)


def edit_dungeon(raw, header):
    assert hashlib.sha256(raw).hexdigest() == "b576f3a20f574d768a33aacb388ac2747e78331543d6340bdfa248b646305312"
    pe, data = open_pe(raw)

    put(pe, data, PROFILE_KIND, bytes.fromhex("030100be01bf01c001"), bytes.fromhex("000100be01bf01c001"))
    for old_at, template_at in EMBEDDED_ROWS:
        old = pe.get_data(old_at, 1000).split(b"\r\n", 1)[0]
        template = pe.get_data(template_at, 1000).split(b"\r\n", 1)[0]
        assert old.startswith(b"r3tl3s1\t447\t")
        put(pe, data, old_at, old, hammer_row(template, header, len(old)))
    return checksum(data)


def build(restore, skeleton, output):
    with ZipFile(restore) as source, ZipFile(skeleton) as skills:
        entries = {name: source.read(name) for name in source.namelist()}
        entries[PLUGINS + "EnemySkills.dll"] = edit_enemy(entries[PLUGINS + "EnemySkills.dll"])
        headers = []
        for rel in ("skills.txt", "base/skills.txt"):
            path = EXCEL + rel
            entries[path], header = edit_skills(skills.read(path))
            headers.append(header)
        assert headers[0] == headers[1]
        entries[PLUGINS + "DungeonLegends.dll"] = edit_dungeon(entries[PLUGINS + "DungeonLegends.dll"], headers[0])
    with ZipFile(output, "w", ZIP_DEFLATED) as package:
        for name, raw in entries.items():
            package.writestr(name, raw)
    with ZipFile(output) as package:
        assert package.testzip() is None and {name: package.read(name) for name in package.namelist()} == entries


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("restore", "skeleton", "output"):
        parser.add_argument(name, type=Path)
    build(**vars(parser.parse_args()))
