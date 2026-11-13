import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

ENTRY = "mods/VanillaDeluxe/d2rloader/plugins/EnemySkills.dll"
BASE = 0x16B2000
SOURCE_SHA = "46c62294e9f73c3de0062fe564d9ae7848011fe81581c7f65b3b7853946f680e"


def align(value, boundary):
    return (value + boundary - 1) // boundary * boundary


def displacement(instruction, target, length):
    return struct.pack("<i", target - instruction - length)


def integrate(original, extension):
    assert hashlib.sha256(original).hexdigest() == SOURCE_SHA
    pe = pefile.PE(data=original)
    extra = pefile.PE(data=extension)
    assert pe.OPTIONAL_HEADER.SizeOfImage == BASE
    assert pe.FILE_HEADER.Machine == extra.FILE_HEADER.Machine == 0x8664
    assert not getattr(extra, "DIRECTORY_ENTRY_IMPORT", None)
    assert not getattr(extra, "DIRECTORY_ENTRY_TLS", None)
    opts = pe.OPTIONAL_HEADER
    file_align, vm_align = opts.FileAlignment, opts.SectionAlignment
    result = bytearray(original)
    section_header = pe.sections[-1].get_file_offset() + 40
    section_count = pe.FILE_HEADER.NumberOfSections
    image_end = BASE
    code_added = initialized_added = uninitialized_added = 0

    def add_section(name, rva, raw, virtual_size, characteristics):
        nonlocal section_header, section_count, image_end, code_added, initialized_added, uninitialized_added
        assert rva == image_end and rva % vm_align == 0
        assert section_header + 40 <= opts.SizeOfHeaders
        assert not any(result[section_header:section_header + 40])
        offset = align(len(result), file_align) if raw else 0
        size = align(len(raw), file_align)
        if raw:
            result.extend(bytes(offset - len(result)))
            result.extend(raw)
            result.extend(bytes(size - len(raw)))
        struct.pack_into("<8sIIIIIIHHI", result, section_header, name, virtual_size, rva,
                         size, offset, 0, 0, 0, 0, characteristics)
        section_header += 40
        section_count += 1
        image_end = align(rva + max(virtual_size, size), vm_align)
        if characteristics & 0x20:
            code_added += size
        if characteristics & 0x40:
            initialized_added += size
        if characteristics & 0x80:
            uninitialized_added += virtual_size

    blocks = {}
    payload_sections = {}
    for sec in extra.sections:
        name = sec.Name.rstrip(b"\0")
        if name in {b".text", b".rdata", b".data"}:
            payload_sections[name] = (sec, bytearray(sec.get_data() if sec.SizeOfRawData else b""))

    for block in extra.DIRECTORY_ENTRY_BASERELOC:
        for relocation in block.entries:
            if relocation.type == 0:
                continue
            assert relocation.type == 10
            source_rva = relocation.rva
            sec = extra.get_section_by_rva(source_rva)
            _, raw = payload_sections[sec.Name.rstrip(b"\0")]
            offset = source_rva - sec.VirtualAddress
            value, = struct.unpack_from("<Q", raw, offset)
            assert extra.OPTIONAL_HEADER.ImageBase <= value < extra.OPTIONAL_HEADER.ImageBase + extra.OPTIONAL_HEADER.SizeOfImage
            value += opts.ImageBase + BASE - extra.OPTIONAL_HEADER.ImageBase
            struct.pack_into("<Q", raw, offset, value)
            target = BASE + source_rva
            blocks.setdefault(target & ~0xFFF, []).append(0xA000 | (target & 0xFFF))

    ordered_sections = sorted(payload_sections.items(), key=lambda entry: entry[1][0].VirtualAddress)
    for index, (name, (sec, raw)) in enumerate(ordered_sections):
        flags = {b".text": 0x60000020, b".rdata": 0x40000040, b".data": 0xC0000080 if not raw else 0xC0000040}

        prefix = BASE + sec.VirtualAddress - image_end
        assert prefix >= 0 and (not prefix or raw)
        virtual_size = prefix + sec.Misc_VirtualSize
        if index + 1 < len(ordered_sections):
            next_section = ordered_sections[index + 1][1][0]
            next_rva = BASE + next_section.VirtualAddress
            assert image_end + align(max(virtual_size, prefix + len(raw)), vm_align) <= next_rva
            virtual_size = next_rva - image_end
        add_section(b".gh" + name[1:], image_end, bytes(prefix) + raw, virtual_size, flags[name])

    old_exceptions, new_exceptions = opts.DATA_DIRECTORY[3], extra.OPTIONAL_HEADER.DATA_DIRECTORY[3]
    pdata = bytearray(pe.get_data(old_exceptions.VirtualAddress, old_exceptions.Size))
    for begin, end, unwind in struct.iter_unpack("<III", extra.get_data(new_exceptions.VirtualAddress, new_exceptions.Size)):
        assert begin < end and unwind & 3 == 0
        assert extra.get_data(unwind, 1)[0] >> 3 == 0, "Payload unwind records must not embed handlers"
        pdata.extend(struct.pack("<III", BASE + begin, BASE + end, BASE + unwind))
    assert sorted(struct.iter_unpack("<III", pdata)) == list(struct.iter_unpack("<III", pdata))
    exceptions_rva = image_end

    old_reloc = opts.DATA_DIRECTORY[5]
    reloc = bytearray(pe.get_data(old_reloc.VirtualAddress, old_reloc.Size))
    for page, entries in sorted(blocks.items()):
        if len(entries) % 2:
            entries.append(0)
        reloc.extend(struct.pack("<II", page, 8 + len(entries) * 2))
        reloc.extend(struct.pack("<" + "H" * len(entries), *entries))
    pad = align(len(pdata), 4) - len(pdata)
    reloc_rva = exceptions_rva + len(pdata) + pad
    meta = pdata + bytes(pad) + reloc
    add_section(b".ghmeta", exceptions_rva, meta, len(meta), 0x40000040)

    exports = {symbol.name: BASE + symbol.address for symbol in extra.DIRECTORY_ENTRY_EXPORT.symbols}
    assert exports.keys() == {b"ObserveStep", b"ObserveBirth", b"CreateClientHammer",
                             b"WaitForPetAction", b"WalkDefiler", b"SkarnathApproachRange"}

    def redirect(rva, opcode, old_target, export):
        length = len(opcode) + 4
        offset = pe.get_offset_from_rva(rva)
        assert result[offset:offset + length] == opcode + displacement(rva, old_target, length)
        result[offset:offset + length] = opcode + displacement(rva, exports[export], length)

    redirect(0x1919B, b"\xe8", 0x19270, b"ObserveBirth")
    redirect(0x34A03, b"\xe8", 0x66E50, b"ObserveStep")
    redirect(0x4C687, b"\xe8", 0x4AF70, b"WaitForPetAction")
    defiler_walk = 0x4A48E
    offset = pe.get_offset_from_rva(defiler_walk)
    assert result[offset:offset + 6] == b"\xff\x15" + displacement(defiler_walk, 0x114D570, 6)
    result[offset:offset + 6] = b"\xe8" + displacement(defiler_walk, exports[b"WalkDefiler"], 5) + b"\x90"
    pursuit = 0x54D6D
    offset = pe.get_offset_from_rva(pursuit)
    original_range = bytes.fromhex("41f6c00141b809000000ba03000000410f45d0")
    assert result[offset:offset + len(original_range)] == original_range
    result[offset:offset + len(original_range)] = (
        b"\xe8" + displacement(pursuit, exports[b"SkarnathApproachRange"], 5)
        + b"\x90" * (len(original_range) - 5)
    )
    redirect(0x6C0EA, bytes.fromhex("488d15"), 0x2A4B0, b"CreateClientHammer")
    factory_slot = 0x770B0
    assert any(entry.rva == factory_slot and entry.type == 10
               for block in pe.DIRECTORY_ENTRY_BASERELOC for entry in block.entries)
    factory_offset = pe.get_offset_from_rva(factory_slot)
    assert struct.unpack_from("<Q", result, factory_offset) == (opts.ImageBase + 0x2A4B0,)
    struct.pack_into("<Q", result, factory_offset, opts.ImageBase + exports[b"CreateClientHammer"])

    for field, value in {
        "SizeOfImage": image_end, "SizeOfCode": opts.SizeOfCode + code_added,
        "SizeOfInitializedData": opts.SizeOfInitializedData + initialized_added,
        "SizeOfUninitializedData": opts.SizeOfUninitializedData + uninitialized_added,
    }.items():
        struct.pack_into("<I", result, opts.get_field_absolute_offset(field), value)
    struct.pack_into("<H", result, pe.FILE_HEADER.get_field_absolute_offset("NumberOfSections"), section_count)
    for directory, rva, size in [(old_exceptions, exceptions_rva, len(pdata)), (old_reloc, reloc_rva, len(reloc))]:
        struct.pack_into("<II", result, directory.get_file_offset(), rva, size)
    updated = pefile.PE(data=bytes(result))
    struct.pack_into("<I", result, opts.get_field_absolute_offset("CheckSum"), updated.generate_checksum())
    return bytes(result)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("previous_zip", type=Path)
    parser.add_argument("output_zip", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    payload = root / "payload.dll"
    subprocess.run([sys.executable, "-m", "ziglang", "c++", "-target", "x86_64-windows-gnu", "-std=c++20", "-O2",
                    "-ffreestanding", "-fno-stack-protector", "-fno-exceptions", "-fno-rtti", "-nostdlib",
                    "-shared", "-Wl,--entry,ObserveStep", str(root / "payload.cpp"), "-o", str(payload)], check=True)
    with ZipFile(args.previous_zip) as source:
        result = integrate(source.read(ENTRY), payload.read_bytes())
    with ZipFile(args.output_zip, "w", ZIP_DEFLATED) as output:
        output.writestr(ENTRY, result)
