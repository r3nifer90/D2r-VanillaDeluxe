import configparser
import re
import struct
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

import pefile

ROOT = Path(__file__).resolve().parent.parent
MOD = 'mods/VanillaDeluxe/'
PLUGINS = MOD + 'd2rloader/plugins/'
EXCEL = MOD + 'VanillaDeluxe.mpq/data/global/excel/'
DONOR = ROOT / 'D2R_Gorath_Blessed_Hammer_Private_Skill_MODS.zip'
ENEMY = ROOT / 'D2R_EnemySkills_Skarnath_Melee_MODS.zip'
CONFIG = ROOT / 'D2R_Mordrath_Health_Buff_MODS.zip'
OUTPUT = ROOT / 'D2R_Mordrath_BaseClass_Defiler_Melee_MODS.zip'
ROW = re.compile(rb'(?m)^(r3tl1v(?:1[0-5]|[0-9])\t\d+\t)nihlathakboss(\t[^\r\n]*\t0)(?=\r?\n)')

BASE_ID = b'r3tl1v0'

def mordrath(raw, copies, embedded=False):
    def detach(match):
        row = match[1] + BASE_ID + match[2]
        if embedded:
            row += b'0' * (len(match[0]) - len(row))
            assert len(row) == len(match[0])
        return row
    updated, count = ROW.subn(detach, raw)
    assert count == 16 * copies, count
    return updated

def jump(source, target, opcode=b'\xe9'):
    return opcode + struct.pack('<i', target - source - len(opcode) - 4)

def checksum(raw):
    pe = pefile.PE(data=bytes(raw), fast_load=True)
    struct.pack_into('<I', raw, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), pe.generate_checksum())
    return bytes(raw)

def defiler(raw):
    pe = pefile.PE(data=raw, fast_load=True)
    data = bytearray(raw)

    def put(rva, expected, change):
        offset = pe.get_offset_from_rva(rva)
        assert data[offset:offset + len(expected)] == expected, hex(rva)
        assert len(expected) == len(change)
        data[offset:offset + len(change)] = change



    for rva in (0x16B3480, 0x16B348B, 0x16B349E, 0x16B34A9):
        put(rva, b'\x04', b'\x07')




    site, stub = 0x4A9E5, 0x16B5130
    code = bytes.fromhex('85ff 751c 488b4520 8b400c 83e801 83f801 7605 83f80e 7508 4989dd')
    code += jump(stub + len(code), 0x4A480)
    assert len(code) == 32
    original = bytes.fromhex('488d8320030000')
    code += original
    code += jump(stub + len(code), site + len(original))
    put(stub, b'\xcc' * len(code), code)
    put(site, original, jump(site, stub) + b'\x90\x90')
    return checksum(data)

def standard_health(raw):
    text = raw.decode('utf-8-sig')
    config = configparser.ConfigParser()
    config.read_string(text)
    assert config['nihlathak']['health_percent'] == '1200'
    updated = text.replace('health_percent=1200\r\n', '', 1)
    config.read_string(updated)
    assert 'health_percent=' + config['defaults']['health_percent'] in updated
    assert 'health_percent=' not in updated.split('[nihlathak]', 1)[1].split('[', 1)[0]
    return updated.encode('utf-8-sig')

def build():
    with ZipFile(DONOR) as donor, ZipFile(ENEMY) as enemy, ZipFile(CONFIG) as config:
        dungeon = PLUGINS + 'DungeonLegends.dll'
        skills = PLUGINS + 'EnemySkills.dll'
        settings = PLUGINS + 'DungeonLegends.ini'
        entries = {
            dungeon: checksum(bytearray(mordrath(donor.read(dungeon), 2, embedded=True))),
            skills: defiler(enemy.read(skills)),
            settings: standard_health(config.read(settings)),
        }
        for path in (EXCEL + 'monstats.txt', EXCEL + 'base/monstats.txt'):
            entries[path] = mordrath(donor.read(path), 1)

    with ZipFile(OUTPUT, 'w', ZIP_DEFLATED) as archive:
        for path, data in entries.items():
            archive.writestr(path, data)

if __name__ == '__main__':
    build()
