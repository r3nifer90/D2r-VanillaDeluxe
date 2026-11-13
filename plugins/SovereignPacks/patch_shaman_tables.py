import csv,io,json,hashlib,argparse
from pathlib import Path

def patch(data):
    rows=list(csv.DictReader(io.StringIO(data.decode('utf-8-sig')),delimiter='\t'))
    by={r['Id']:r for r in rows}; changes={}
    for row in rows:
        if not row['Id'].startswith('r3sp') or row['BaseId']!='fetishshaman1':continue
        stock=int(row['Id'][4:-1]);original=next(r for r in rows if r.get('*hcIdx')==str(stock))
        assert original['SplEndDeath']=='1' and row['SplEndDeath']=='1'
        target=original['minion1'];assert target and target in by and by[target]['enabled']=='1' and by[target]['AI']=='Fetish'
        assert row['minion1'] in ('',target),(row['Id'],'unexpected pre-existing transition')
        changes[row['Id']]=target
    assert len(changes)==20
    lines=data.splitlines(keepends=True);fields=lines[0].decode('utf-8-sig').rstrip('\r\n').split('\t')
    ii,mi=fields.index('Id'),fields.index('minion1');updated=0
    for n,line in enumerate(lines[1:],1):
        body=line.rstrip(b'\r\n');ending=line[len(body):];cells=body.decode('utf-8').split('\t')
        if cells[ii] not in changes or cells[mi]==changes[cells[ii]]:continue
        cells[mi]=changes[cells[ii]];lines[n]='\t'.join(cells).encode('utf-8')+ending;updated+=1
    output=b''.join(lines)
    patched=list(csv.DictReader(io.StringIO(output.decode('utf-8-sig')),delimiter='\t'))
    delta=[(a['Id'],k) for a,b in zip(rows,patched) for k in a if a[k]!=b[k]]
    assert len(rows)==len(patched) and len(delta)==updated
    assert all(k=='minion1' and name in changes for name,k in delta)
    assert [r['Id'] for r in rows]==[r['Id'] for r in patched]
    return output,{'author':'r3ni','version':'0.3.8-shaman-native','updated':updated,'transitions':changes,'input_sha256':hashlib.sha256(data).hexdigest(),'output_sha256':hashlib.sha256(output).hexdigest(),'only_changed_column':'minion1','native_death_handler':'SplEndDeath=1','carrier':'matching ordinary Fetish; native reinitialization and AI'}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('input',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    output,report=patch(args.input.read_bytes());args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(output);print(json.dumps(report,indent=2))
