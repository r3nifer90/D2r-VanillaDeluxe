from pathlib import Path
import struct,hashlib,json,subprocess,csv,io
import pefile
W=Path(__file__).resolve().parent;ROOT=W.parents[1]
raw=(ROOT/'work/map_open_fix/d2rl-maps.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest()=='2782581a766ebac8d1609d16be524634975aec1d0bb3271968e3bab05c7b097f'
b=bytearray(raw);p=pefile.PE(data=raw);base=p.OPTIONAL_HEADER.ImageBase
align=lambda n,a:(n+a-1)//a*a
va=align(max(s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData) for s in p.sections),p.OPTIONAL_HEADER.SectionAlignment)
off=align(len(b),p.OPTIONAL_HEADER.FileAlignment)
targets=dict(store_continue=0x16119a,reset_continue=0x1611e4,xp_table=0x1613a0,
 fortune_continue=0x27365,xp_stat_continue=0x16120b,stat_original=0x161266,
 stat_epilogue=0x1719d,get_level=0x101bd8)
subprocess.run(['as','--64','-o',str(W/'fortune.o'),str(W/'fortune.s')],check=True)
subprocess.run(['ld','-Ttext',hex(base+va),'-e','stat_mf',*[f'--defsym={k}={hex(base+v)}' for k,v in targets.items()],'-o',str(W/'fortune.elf'),str(W/'fortune.o')],check=True)
subprocess.run(['objcopy','-O','binary','-j','.text',str(W/'fortune.elf'),str(W/'fortune.bin')],check=True)
symbols={name:int(a,16)-base for a,_,name in (s.split() for s in subprocess.check_output(['nm',str(W/'fortune.elf')],text=True).splitlines())}
code=(W/'fortune.bin').read_bytes();size=align(len(code),p.OPTIONAL_HEADER.FileAlignment)
header=p.sections[-1].get_file_offset()+40
assert header+40<=min(s.PointerToRawData for s in p.sections if s.PointerToRawData)
b.extend(bytes(off-len(b)));b.extend(code);b.extend(bytes(size-len(code)))
struct.pack_into('<8sIIIIIIHHI',b,header,b'.vdmf\0\0\0',len(code),va,size,off,0,0,0,0,0xe0000060)
struct.pack_into('<H',b,p.FILE_HEADER.get_field_absolute_offset('NumberOfSections'),len(p.sections)+1)
struct.pack_into('<I',b,p.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfImage'),align(va+len(code),p.OPTIONAL_HEADER.SectionAlignment))
struct.pack_into('<I',b,p.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfCode'),p.OPTIONAL_HEADER.SizeOfCode+size)
struct.pack_into('<I',b,p.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),0)
patches=[]
for at,old,label in [(0x161195,'48894c2408','store_mf'),(0x1611dd,'488d05bc010000','reset_mf'),(0x161203,'83bc24a800000055','stat_mf'),(0x2735e,'8b52188954080c','fortune_marker')]:
 expected=bytes.fromhex(old);assert p.get_data(at,len(expected))==expected,(hex(at),p.get_data(at,len(expected)).hex())
 pos=p.get_offset_from_rva(at);b[pos:pos+len(expected)]=b'\xe9'+struct.pack('<i',symbols[label]-at-5)+b'\x90'*(len(expected)-5)
 patches.append({'rva':at,'old':old,'target':label})
dst=W/'package/mods/VanillaDeluxe/d2rloader/plugins/d2rl-maps.dll';dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(b)
diffs=[]
for bank in ['', 'base/']:
 source=ROOT/'work/mf_v2/package/mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel'/bank/'skills.txt'
 original=source.read_bytes();text=original.decode('utf-8-sig');lines=text.splitlines(keepends=True);head=lines[0].rstrip('\r\n').split('\t');col=head.index('aurastatcalc1');count=0
 for i,line in enumerate(lines[1:],1):
  cells=line.rstrip('\r\n').split('\t')
  if cells[0]!='rmap_fortune':continue
  assert cells[col]=='(lvl*5)+(max(0,lvl-50)*5)';cells[col]='1';count+=1
  lines[i]='\t'.join(cells)+ ('\r\n' if line.endswith('\r\n') else '\n' if line.endswith('\n') else '')
 assert count==1
 output=(b'\xef\xbb\xbf' if original.startswith(b'\xef\xbb\xbf') else b'')+''.join(lines).encode()
 before=list(csv.DictReader(io.StringIO(text),delimiter='\t'));after=list(csv.DictReader(io.StringIO(output.decode('utf-8-sig')),delimiter='\t'))
 changes=[(a['skill'],k,a[k],z[k]) for a,z in zip(before,after) for k in a if a[k]!=z[k]]
 assert changes==[('rmap_fortune','aurastatcalc1','(lvl*5)+(max(0,lvl-50)*5)','1')]
 target=W/'package/mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel'/bank/'skills.txt';target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(output);diffs.append({'bank':bank or 'main','changes':changes})
(W/'symbols.json').write_text(json.dumps(symbols,indent=2))
(W/'build.json').write_text(json.dumps({'source_sha256':hashlib.sha256(raw).hexdigest(),'output_sha256':hashlib.sha256(b).hexdigest(),'patches':patches,'tables':diffs,'section_rva':va},indent=2))
print('Built V3, section',hex(va),'bytes',len(code))
