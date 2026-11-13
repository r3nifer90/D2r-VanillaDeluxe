from pathlib import Path
import struct as s,subprocess,json
from zipfile import ZipFile,ZIP_DEFLATED
ROOT=Path('/workspace/scratch/09ae371c2096'); W=ROOT/'work/map_experience'
original=(ROOT/'work/rare-maps-cube-fix.dll').read_bytes(); b=bytearray(original)
pe=s.unpack_from('<I',b,0x3c)[0];opt=pe+24;count=s.unpack_from('<H',b,pe+6)[0];sh=opt+s.unpack_from('<H',b,pe+20)[0]
sections=[s.unpack_from('<8sIIII',b,sh+i*40) for i in range(count)]
base=s.unpack_from('<Q',b,opt+24)[0]
va=max((r+max(v,z)+4095)//4096*4096 for _,v,r,z,o in sections)
raw=(len(b)+511)//512*512
assert sh+(count+1)*40<=min(o for _,_,_,_,o in sections if o)
def off(r):
 for _,v,a,z,o in sections:
  if a<=r<a+z:return o+r-a
 raise ValueError(hex(r))
targets=dict(item_resume=0x2084,client_resume=0xa800,record_resume=0x3780,active_seed=0xf7c28,store_resume=0x16975,reset_resume=0x17517,get_level=0x101bd8,stat_epilogue=0x1719d,stat_resume=0x16f3b,tooltip_resume=0xc5c5,hud_resume=0x22e5,format_text=0x19990)
subprocess.run(['as','--64','-o',str(W/'experience.o'),str(W/'experience.s')],check=True)
subprocess.run(['ld','-Ttext',hex(base+va),'-e','item_roll','-o',str(W/'experience.elf'),*[f'--defsym={n}={hex(base+a)}' for n,a in targets.items()],str(W/'experience.o')],check=True)
subprocess.run(['objcopy','-O','binary','-j','.text',str(W/'experience.elf'),str(W/'experience.bin')],check=True)
symbols={n:int(a,16)-base for a,_,n in (x.split() for x in subprocess.check_output(['nm',str(W/'experience.elf')],text=True).splitlines())}
code=(W/'experience.bin').read_bytes();size=(len(code)+511)//512*512
b.extend(bytes(raw-len(b)));b.extend(code);b.extend(bytes(size-len(code)))
s.pack_into('<8sIIIIIIHHI',b,sh+count*40,b'.vdexp\0\0',len(code),va,size,raw,0,0,0,0,0xe0000060)
s.pack_into('<H',b,pe+6,count+1);s.pack_into('<I',b,opt+56,(va+len(code)+4095)//4096*4096)
s.pack_into('<I',b,opt+4,s.unpack_from('<I',b,opt+4)[0]+size)
changes=[]
for at,expected,name in [
 (0xc2c68,'488b8424e0000000','item_roll'),
 (0xc2ce6,'488b942408020000','client_roll'),
 (0xc2c9b,'440fb6490f440a4912','record_generation'),
 (0x16970,'48894c2408','store_levels'),
 (0x17510,'4881ecc8000000','reset_levels'),
 (0x16f33,'83bc24a80000007f','stat_bonus'),
 (0xc5bd,'c744244800000000','tooltip_bonus'),
 (0x22dd,'c744244400000000','hud_bonus'),
]:
 old=bytes.fromhex(expected);p=off(at);assert b[p:p+len(old)]==old,(hex(at),b[p:p+len(old)].hex())
 b[p:p+len(old)]=b'\xe9'+s.pack('<i',symbols[name]-at-5)+b'\x90'*(len(old)-5);changes.append((hex(at),name))
(W/'maps-experience.dll').write_bytes(b)
(W/'symbols.json').write_text(json.dumps(symbols,indent=2));print('Built',len(b),'bytes; new section',hex(va),len(code),'bytes;',changes)
