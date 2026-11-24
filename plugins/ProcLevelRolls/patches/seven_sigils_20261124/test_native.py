from pathlib import Path
import struct,json,pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64
from unicorn.x86_const import *
p=pefile.PE('work/sigil-procs/package/mods/VanillaDeluxe/d2rloader/plugins/ProcLevelRolls.dll');base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_64);u.mem_map(base,(p.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,p.get_memory_mapped_image());u.mem_map(0x200000,0x20000)
end=0x210000;stack=0x20e000;frame=0x204000;cache=0x202000;prop=0x201000;tier=0x203000
ranges=[]
for idx,skill,lo,hi,chance in [(7,53,50,60,10),(8,47,60,63,10),(9,64,40,50,10),(10,92,60,63,10),(11,84,40,50,10),(12,247,40,50,5)]:
 row=struct.unpack('<QQIIQQ',p.get_data(0x9500+idx*40,40));name=p.get_string_at_rva(row[0]-base).decode();assert row[2:4]==(skill,198) and row[4]==lo+(hi<<32)
 # Execute the shipped item-specific rule matcher, not a duplicate Python implementation.
 u.mem_write(cache+0x6014+1,bytes([idx+1]));u.mem_write(cache+0x14+2,struct.pack('<H',80));u.mem_write(frame+8,struct.pack('<Q',prop));u.mem_write(prop,struct.pack('<Iiii',77,skill,chance,lo));u.mem_write(frame+0xa0,struct.pack('<I',198));u.mem_write(stack,struct.pack('<Q',end))
 for reg,val in [(UC_X86_REG_RSP,stack),(UC_X86_REG_RBP,frame),(UC_X86_REG_R13,cache),(UC_X86_REG_RAX,1)]:u.reg_write(reg,val)
 u.emu_start(base+0x43dc,base+0x47a2,count=100000)
 assert u.reg_read(UC_X86_REG_RIP)==base+0x47a2
 assert u.reg_read(UC_X86_REG_RBX)==lo+(hi<<32)
 levels=set()
 # Actual inclusive rejection-sampled native RNG used by Select.
 for seed in range(512):
  u.mem_write(stack,struct.pack('<Q',end));u.reg_write(UC_X86_REG_RSP,stack);u.reg_write(UC_X86_REG_RCX,seed);u.reg_write(UC_X86_REG_RDX,row[4]);u.emu_start(base+0x54b0,end,count=10000);v=u.reg_read(UC_X86_REG_RAX);assert lo<=v<=hi;levels.add(v)
 assert levels==set(range(lo,hi+1))
 ranges.append({'name':name,'skill':skill,'stat':198,'levels':sorted(levels),'chance':chance})
Path('work/sigil-procs/native_test.json').write_text(json.dumps(ranges,indent=2));print('PASS: six production rule matches and 3072 native RNG rolls; exact inclusive ranges')
