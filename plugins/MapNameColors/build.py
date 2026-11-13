from pathlib import Path
import struct as s,subprocess,json
w=Path('work/map_colors_fix05');b=bytearray(Path('work/mapnamecolors.dll').read_bytes());base=0x180000000
pe=s.unpack_from('<I',b,60)[0];op=pe+24;num=s.unpack_from('<H',b,pe+6)[0];sh=op+s.unpack_from('<H',b,pe+20)[0];ss=[s.unpack_from('<8sIIII',b,sh+40*i) for i in range(num)]
def off(a):
 for _,v,r,n,p in ss:
  if r<=a<r+n:return p+a-r
 raise ValueError(hex(a))
name,v,a,n,o=ss[-1];assert name.startswith(b'.rsrc');start=(v+15)//16*16;va=a+start
(w/'layout.ld').write_text('SECTIONS { . = '+hex(base+va)+'; .text : { *(.text*) *(.rodata*) *(.data*) *(.bss*) } /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) } }')
subprocess.run(['ld','-T',str(w/'layout.ld'),'--defsym=original=0x180006000','-o',str(w/'colors.elf'),str(w/'colors.o')],check=True)
subprocess.run(['objcopy','-O','binary','-j','.text',str(w/'colors.elf'),str(w/'colors.bin')],check=True)
sym={name:int(addr,16)-base for addr,_,name in (x.split() for x in subprocess.check_output(['nm',str(w/'colors.elf')],text=True).splitlines())}
code=(w/'colors.bin').read_bytes();rawsize=(start+len(code)+511)//512*512
b.extend(bytes(max(0,o+rawsize-len(b))));b[o+start:o+start+len(code)]=code
s.pack_into('<I',b,sh+(num-1)*40+8,start+len(code));s.pack_into('<I',b,sh+(num-1)*40+16,rawsize);s.pack_into('<I',b,sh+(num-1)*40+36,0xe0000060);s.pack_into('<I',b,op+56,(va+len(code)+4095)//4096*4096)

er,es=s.unpack_from('<II',b,op+112);e=off(er);nf,nn,fa,na,oa=s.unpack_from('<IIIII',b,e+20)
for i in range(nn):
 nr=s.unpack_from('<I',b,off(na)+4*i)[0];p=off(nr);name=b[p:b.index(0,p)].decode();ordinal=s.unpack_from('<H',b,off(oa)+2*i)[0]
 if name=='D2RLoaderLoadPlugin':s.pack_into('<I',b,off(fa)+4*ordinal,sym['load_colors'])
assert b.count(b'0.1.0\0')==1;b=b.replace(b'0.1.0\0',b'0.5.0\0')
(w/'R3niMapNameColors.dll').write_bytes(b);(w/'symbols.json').write_text(json.dumps(sym));print('Built color fix DLL',len(b),'bytes')
