"""Stage the four existing boss-only skeleton skill rows for handler 56.

This is the table part of the change. EnemySkills must also route Sovereigns
to rows 452/453 and select a living cast target for both boss families.
"""
import argparse
from pathlib import Path

BOSS_ROWS = {
    "452": ("r3tl5s0", "necroskeleton"),
    "453": ("r3tl5s1", "necromage"),
    "514": ("r3dx4s2", "necroskeleton"),
    "523": ("r3dx7s2", "necroskeleton"),
}
FIELDS = {
    "srvstfunc": "",
    "srvdofunc": "56",
    "cltstfunc": "",
    "cltdofunc": "",
    "SelectProc": "",
    "TargetCorpse": "",
}

def prepare(data):
    lines = data.splitlines(keepends=True)
    columns = lines[0].decode("utf-8-sig").rstrip("\r\n").split("\t")
    index = {name: columns.index(name) for name in
             ("*Id", "skill", "summon", *FIELDS)}
    touched = set()
    for line_number, line in enumerate(lines[1:], 1):
        record = line.rstrip(b"\r\n")
        values = record.split(b"\t")
        if len(values) <= index["*Id"]:
            continue
        skill_id = values[index["*Id"]].decode("ascii")
        if skill_id not in BOSS_ROWS:
            continue
        skill, summon = BOSS_ROWS[skill_id]
        assert skill_id not in touched
        assert values[index["skill"]] == skill.encode("ascii")
        assert values[index["summon"]] == summon.encode("ascii")
        assert values[index["srvstfunc"]] == b"15"
        assert values[index["srvdofunc"]] == b"31"
        assert values[index["TargetCorpse"]] == b"1"
        for key, value in FIELDS.items():
            values[index[key]] = value.encode("ascii")
        lines[line_number] = b"\t".join(values) + line[len(record):]
        touched.add(skill_id)
    assert touched == BOSS_ROWS.keys(), touched
    return b"".join(lines)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    options = parser.parse_args()
    assert options.source.resolve() != options.output.resolve()
    result = prepare(options.source.read_bytes())
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_bytes(result)
