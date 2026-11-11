import argparse
import hashlib
import struct
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

from prepare_skills import prepare

SHA256 = "0d2b35d7fe7ab349ec662b84ce96a1f9d7e8bac1db43041343607e946c53566e"

def patch_dll(raw):
    assert hashlib.sha256(raw).hexdigest() == SHA256
    pe = pefile.PE(data=raw, fast_load=True)
    data = bytearray(raw)

    def patch(rva, original, updated):
        assert len(original) == len(updated)
        offset = pe.get_offset_from_rva(rva)
        assert data[offset:offset + len(original)] == original, hex(rva)
        data[offset:offset + len(original)] = updated

    # Sovereign pool uses the two private Dungeon Legends spell rows.
    # Balance/tuning canonicalizes those rows back to 70 and 80.
    for rva, skill, private in ((0x7ba00, 70, 452), (0x7ba20, 80, 453)):
        patch(rva, struct.pack("<II", skill, 4), struct.pack("<II", private, 4))
        patch(rva + 16, b"\x06\x01\x00", b"\x05\x01\x00")
        patch(rva + 22, b"\x0f\x00\x1f\x00", b"\x00\x00\x38\x00")

    for rva, skill, original, mode in ((0x7b134, 452, 70, 7), (0x7b140, 453, 80, 7),
                                       (0x7b3f8, 514, 70, 4), (0x7b464, 523, 70, 4)):
        patch(rva, struct.pack("<4H3B", skill, original, 15, 31, mode, 6, 1),
              struct.pack("<4H3B", skill, original, 0, 56, mode, 5, 1))

    # Dungeon Legends registrations still pass kind 6 for skeletons.
    # Select the caster for kind 6 and retain corpse targeting for kind 2.
    patch(0x3195f, bytes.fromhex("baa2000000"), bytes.fromhex("bae2000000"))
    patch(0x31970, bytes.fromhex("4183c8044183f806"), bytes.fromhex("0f1f40004183f802"))

    # Only the two skeleton spell families previously entered these corpse
    # paths. All spells still run the same owner, cast, count and limit checks.
    patch(0x1703f, b"\x74\x0f", b"\x90\x90")
    patch(0x1704e, b"\x75\x78", b"\xeb\x78")
    patch(0x171bd, bytes.fromhex("740983f846"),
          b"\xe9" + struct.pack("<i", 0x1728d - 0x171bd - 5))

    old = b"1.2.7-defiler-melee-ai\0"
    new = b"1.2.8-boss-skel-batch\0"
    assert raw.count(old) == 1
    patch(pe.get_rva_from_offset(raw.index(old)), old, new.ljust(len(old), bytes(1)))
    # Recompute from the modified image, including its retained prior fixes.
    updated = pefile.PE(data=bytes(data), fast_load=True)
    struct.pack_into("<I", data, updated.OPTIONAL_HEADER.get_field_absolute_offset("CheckSum"),
                     updated.generate_checksum())
    return bytes(data)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--excel", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    dll = patch_dll(args.dll.read_bytes())
    mod = "mods/VanillaDeluxe/"
    entries = {mod + "d2rloader/plugins/EnemySkills.dll": dll}
    for table in ("skills.txt", "base/skills.txt"):
        key = mod + "VanillaDeluxe.mpq/data/global/excel/" + table
        entries[key] = prepare((args.excel / table).read_bytes())
    output = args.out
    with ZipFile(output, "w", ZIP_DEFLATED) as archive:
        for name, content in entries.items():
            archive.writestr(name, content)
    with ZipFile(output) as archive:
        assert archive.testzip() is None
        assert {name: archive.read(name) for name in archive.namelist()} == entries
    print(output)
