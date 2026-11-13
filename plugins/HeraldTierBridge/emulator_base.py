from pathlib import Path
import struct,json,hashlib
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[2]
DLL=Path(__file__).with_name('HeraldProgressProbe_0_1_2.dll')
CORE=ROOT/'recovered/D2R mods/D2RCore(3).dll'
checks=[]
def check(name,result):
 assert result,name
 checks.append(name)
class Test:
 def __init__(self,mode):
  self.u=Uc(UC_ARCH_X86,UC_MODE_64);self.mode=mode;self.p=pefile.PE(str(DLL));self.base=0x180000000
  self.u.mem_map(self.base,(self.p.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);self.u.mem_write(self.base,self.p.get_memory_mapped_image(ImageBase=self.base))
  self.c=pefile.PE(str(CORE));self.cb=self.c.OPTIONAL_HEADER.ImageBase
  self.u.mem_map(self.cb,(self.c.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);self.u.mem_write(self.cb,self.c.get_memory_mapped_image())
  self.low=0x700000000;self.u.mem_map(self.low,0x100000);self.free=self.low+0x20000;self.stop=self.low+0x100;self.callbacks={};self.stub=self.low+0x1000;self.registered=0;self.log_calls=0
  self.u.hook_add(UC_HOOK_CODE,self.hook)
  for group in self.p.DIRECTORY_ENTRY_IMPORT:
   for imp in group.imports:self.qwrite(self.base+imp.address-self.p.OPTIONAL_HEADER.ImageBase,self.bind(imp.name.decode()))

  self.ctx=self.alloc(0x200);self.api=self.alloc(224);self.write(self.api,struct.pack('<I',224));self.qwrite(self.ctx+8,self.api)
  self.qwrite(self.api+8,self.bind('logInfo'));self.qwrite(self.api+160,self.bind('registerConsole'))
  self.exports={x.name.decode():self.base+x.address for x in self.p.DIRECTORY_ENTRY_EXPORT.symbols}
  self.original={r:self.read(self.cb+r,n) for r,n in [(0x81f7b0,19),(0x81fef0,21)]}
  if mode=='conflict':self.write(self.cb+0x81fef0,b'\xcc')
 def alloc(self,n):
  a=self.free;self.free+=(n+15)&~15;return a
 def read(self,a,n):return bytes(self.u.mem_read(a,n))
 def write(self,a,b):self.u.mem_write(a,bytes(b))
 def qwrite(self,a,n):self.write(a,struct.pack('<Q',n))
 def qread(self,a):return struct.unpack('<Q',self.read(a,8))[0]
 def arg(self,i):return self.u.reg_read([UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9][i]) if i<4 else self.qread(self.u.reg_read(UC_X86_REG_RSP)+8+8*i)
 def bind(self,name):
  a=self.stub;self.stub+=16;self.callbacks[a]=name;return a
 def cstr(self,a):
  out=b''
  while (b:=self.read(a+len(out),1))!=b'\0':out+=b
  return out
 def hook(self,uc,a,size,unused):
  if a==self.stop:uc.emu_stop();return
  name=self.callbacks.get(a)
  if not name:return
  out=1
  if name=='GetModuleFileNameW':
   path='C:\\mods\\VanillaDeluxe\\d2rloader\\plugins\\HeraldProgressProbe.dll';self.write(self.arg(1),path.encode('utf-16le')+b'\0\0');out=len(path)
  elif name=='GetModuleHandleW':out=self.cb if self.mode!='absent' else 0
  elif name=='GetCurrentProcess':out=0xffffffffffffffff
  elif name=='GetLocalTime':self.write(self.arg(0),bytes(16));out=0
  elif name=='CreateFileW':out=100
  elif name=='ReadProcessMemory':
   try:
    self.write(self.arg(2),self.read(self.arg(1),self.arg(3)))
    if self.arg(4):self.qwrite(self.arg(4),self.arg(3))
   except Exception:out=0
  elif name=='VirtualAlloc':out=self.alloc(4096)
  elif name=='VirtualProtect':self.write(self.arg(3),struct.pack('<I',0x20))
  elif name=='memcpy':self.write(self.arg(0),self.read(self.arg(1),self.arg(2)));out=self.arg(0)
  elif name=='memcmp':
   x,y=self.read(self.arg(0),self.arg(2)),self.read(self.arg(1),self.arg(2));out=(x>y)-(x<y)
  elif name in ('strlen','core_strlen'):out=len(self.cstr(self.arg(0)))
  elif name=='wcscpy':
   k=0
   while self.read(self.arg(1)+k,2)!=b'\0\0':k+=2
   self.write(self.arg(0),self.read(self.arg(1),k+2));out=self.arg(0)
  elif name in ('_snprintf','_vsnprintf'):
   self.write(self.arg(0),b'x\0');out=1
  elif name=='registerConsole':self.registered=self.arg(1)
  elif name=='logInfo':self.log_calls+=1
  elif name in ('core_strcmp','core_strnicmp'):
   x,y=self.cstr(self.arg(0)).lower(),self.cstr(self.arg(1)).lower()
   if name=='core_strnicmp':x,y=x[:self.arg(2)],y[:self.arg(2)]
   out=(x>y)-(x<y)
  elif name=='unexpected_validation_failure':raise AssertionError('Core rejected plugin metadata')
  elif name not in ('AcquireSRWLockExclusive','ReleaseSRWLockExclusive','CloseHandle','DisableThreadLibraryCalls','FlushInstructionCache','VirtualFree','WriteFile'):raise AssertionError(name)
  self.u.reg_write(UC_X86_REG_RAX,out&0xffffffffffffffff);sp=self.u.reg_read(UC_X86_REG_RSP);self.u.reg_write(UC_X86_REG_RIP,self.qread(sp));self.u.reg_write(UC_X86_REG_RSP,sp+8)
 def call(self,a,*args):
  sp=self.low+0xf0000;self.u.reg_write(UC_X86_REG_RSP,sp);self.qwrite(sp,self.stop)
  for i,arg in enumerate(args):
   if i<4:self.u.reg_write([UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9][i],arg)
   else:self.qwrite(sp+8+8*i,arg)
  self.u.emu_start(a,self.stop,count=2000000)
  assert self.u.reg_read(UC_X86_REG_RIP)==self.stop,'did not return'
  return self.u.reg_read(UC_X86_REG_RAX)
 def core_validate(self,meta):
  self.callbacks[self.cb+0x53acc0]='core_strlen';self.callbacks[self.cb+0x53aca0]='core_strcmp';self.callbacks[self.cb+0x476550]='unexpected_validation_failure';self.qwrite(self.cb+0x650d38,self.bind('core_strnicmp'))
  return self.call(self.cb+0x477470,meta,0,1,self.alloc(256))
