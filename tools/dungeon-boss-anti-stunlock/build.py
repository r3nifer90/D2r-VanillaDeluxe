import argparse
import hashlib
from pathlib import Path
import struct
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
    code = bytes.fromhex("83fe03 7409 83fe0d")
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


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("previous_package", type=Path)
    parser.add_argument("output_package", type=Path)
    args = parser.parse_args()
    with ZipFile(args.previous_package) as archive:
        dll = patch(archive.read(DLL_PATH))
    with ZipFile(args.output_package, "w", ZIP_DEFLATED) as archive:
        archive.writestr(DLL_PATH, dll)
