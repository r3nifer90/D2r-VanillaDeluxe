from pathlib import Path
import csv, hashlib, json, re

H = Path(__file__).parent
P = H.parent
OUT = P / 'installer/mods/VanillaDeluxe/VanillaDeluxe.mpq'
inputs = H / 'inputs'
manifest = json.loads((H / 'manifest.json').read_text())
stock_names = {}
used_ids = set()
for p in sorted((inputs / 'localization').glob('*.json')):
    for row in json.loads(p.read_text(encoding='utf-8-sig')):
        used_ids.add(int(row['id']))
        stock_names[row['Key']] = row

original_strings = json.loads((inputs / 'monsters.json').read_text(encoding='utf-8-sig'))
strings = original_strings.copy()
locales = [k for k in strings[0] if k not in ('id', 'Key')]
existing_keys = {r['Key'] for r in strings}
original_tables = {}
for bank in ('', 'base/'):
    p = inputs / 'excel' / bank / 'monstats.txt'
    original_tables[bank] = list(csv.DictReader(p.open(encoding='utf-8-sig'), delimiter='\t'))
stock = {int(r['*hcIdx']): r for r in original_tables[''] if r['*hcIdx']}
new_keys = {}
next_id = 60000
for item in manifest['packs']:
    base = stock[item['stock']]
    source = stock_names.get(base['NameStr'])
    assert source is not None, (base['Id'], base['NameStr'])
    key = 'r3ni_sp_minion_kind_' + str(item['stock'])
    assert key not in existing_keys
    while next_id in used_ids:
        next_id += 1
    assert next_id < 65536
    row = {'id': next_id, 'Key': key}
    for lang in locales:
        text = source.get(lang) or source.get('enUS')
        assert text
        row[lang] = 'ÿc8' + re.sub(r'ÿc[0-9A-Za-z]', '', text) + 'ÿc0'
    used_ids.add(next_id)
    next_id += 1
    strings.append(row)
    new_keys[item['minion_key']] = key

reports = []
for bank in ('', 'base/'):
    input_path = inputs / 'excel' / bank / 'monstats.txt'
    raw = input_path.read_bytes()
    lines = raw.splitlines(keepends=True)
    fields = lines[0].decode('utf-8-sig').strip('\r\n').split('\t')
    ii, ni = fields.index('Id'), fields.index('NameStr')
    changed = 0
    for n, line in enumerate(lines[1:], 1):
        body = line.rstrip(b'\r\n')
        ending = line[len(body):]
        cells = body.decode('utf-8').split('\t')
        if cells[ii] in new_keys:
            cells[ni] = new_keys[cells[ii]]
            lines[n] = '\t'.join(cells).encode('utf-8') + ending
            changed += 1
    assert changed == 482
    target = OUT / 'data/global/excel' / bank / 'monstats.txt'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(b''.join(lines))
    reports.append({'bank': bank, 'minions_renamed': changed,
                    'input_sha256': hashlib.sha256(raw).hexdigest(),
                    'output_sha256': hashlib.sha256(target.read_bytes()).hexdigest()})

target = OUT / 'data/local/lng/strings/monsters.json'
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text(json.dumps(strings, ensure_ascii=False, indent=2) + '\n', encoding='utf-8-sig')
assert strings[:len(original_strings)] == original_strings
(H / 'minion-names-manifest.json').write_text(json.dumps({
    'author': 'r3ni', 'version': '0.3.4-native-affixes',
    'original_localizations_preserved': len(original_strings),
    'new_names': len(new_keys), 'keys': new_keys, 'tables': reports,
    'examples': [{k: r[k] for k in ('Key', 'enUS', 'plPL')} for r in strings[-482:-472]],
}, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({'minions_per_bank': 482, 'new_names': len(new_keys),
                  'original_localizations_preserved': len(original_strings)}))
