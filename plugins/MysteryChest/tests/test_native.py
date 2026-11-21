"""Run compiled fix functions against the captured 95032 native Chronicle code.
Windows calls, memory allocator and bank accessor are mocked explicitly.
Gameplay, graphics, and the live loader's hook installer are not emulated.
"""
from pathlib import Path
import argparse,re
import struct,json,datetime
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import *
root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native-text',type=Path,required=True,help='Build-95032 unpacked .text bytes, starting at RVA 0x1000; not supplied')
parser.add_argument('--dll',type=Path,default=root/'tests/native_tests.dll')
parser.add_argument('--audit',type=Path,help='Optional offline audit JSON; no user data is supplied')
args=parser.parse_args()
native=args.native_text.read_bytes()
p=pefile.PE(str(args.dll))
u=Uc(UC_ARCH_X86,UC_MODE_64)
NB=0x140000000;DB=p.OPTIONAL_HEADER.ImageBase;CB=0x148000000;AB=0x200000000;SB=0x300000000
u.mem_map(NB,0x4000000);u.mem_write(NB+0x1000,native)
u.mem_map(DB,(p.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(DB,p.get_memory_mapped_image())
u.mem_map(CB,0x100000);u.mem_map(AB,0x2000000);u.mem_map(SB,0x200000)
def write(at,fmt,*values):u.mem_write(at,struct.pack(fmt,*values))
def read(at,fmt):return struct.unpack(fmt,u.mem_read(at,struct.calcsize(fmt)))
exports={s.name.decode():DB+s.address for s in p.DIRECTORY_ENTRY_EXPORT.symbols if s.name}
api={};nextstub=AB+0x100;stop=AB+0x80;heap=AB+0x300000
def regstub(at,name):api[at]=name;u.mem_write(at,b'\xc3');return at
for dll in p.DIRECTORY_ENTRY_IMPORT:
 for imp in dll.imports:
  name=imp.name.decode() if imp.name else '#'+str(imp.ordinal)
  write(imp.address,'<Q',regstub(nextstub,name));nextstub+=16
# Set up the DLL's Windows TLS template for the single emulated thread.
teb=AB+0x10000;slots=AB+0x11000;tls=AB+0x12000
u.reg_write(UC_X86_REG_GS_BASE,teb);write(teb+0x58,'<Q',slots)
if hasattr(p,'DIRECTORY_ENTRY_TLS'):
 t=p.DIRECTORY_ENTRY_TLS.struct;write(t.AddressOfIndex,'<I',0);write(slots,'<Q',tls)
 u.mem_write(tls,bytes(u.mem_read(t.StartAddressOfRawData,t.EndAddressOfRawData-t.StartAddressOfRawData)))
table=AB+0x20000;rows=AB+0x30000;allocator=AB+0x2000;vtable=AB+0x2100
write(table+0x13c8,'<QQQ',rows,2149,2149);write(table+0x13b0,'<Q',219)
for i in range(2149):write(rows+i*0x15c,'<H',i)
write(allocator,'<Q',vtable)
allocstub=regstub(nextstub,'native_alloc');nextstub+=16;freestub=regstub(nextstub,'native_free');nextstub+=16
write(vtable+8,'<Q',allocstub);write(vtable+0x20,'<Q',freestub)
for r,name in [(0x12063c0,'native_allocator'),(0x300a90,'native_bank'),(0x12d1150,'native_cookie'),(0x34b9d0,'native_unit_type'),(0x34a500,'native_item_data'),(0x12d3660,'memcpy')]:regstub(NB+r,name)
trace=[];calls=0;rng_calls=[];applied_records=[];sent_packets=[];cleared=[]
NOW=int(datetime.datetime(2026,11,21,2,tzinfo=datetime.timezone.utc).timestamp()/60)
def leave(value=0):
 u.reg_write(UC_X86_REG_RAX,value&0xffffffffffffffff)
 sp=u.reg_read(UC_X86_REG_RSP);ret=read(sp,'<Q')[0];u.reg_write(UC_X86_REG_RSP,sp+8);u.reg_write(UC_X86_REG_RIP,ret)
def callback(uc,at,size,_):
 global heap,calls
 calls+=1;trace.append(at)
 if len(trace)>16:trace.pop(0)
 if at==stop:uc.emu_stop();return
 if at not in api:return
 name=api[at];a=uc.reg_read(UC_X86_REG_RCX);b=uc.reg_read(UC_X86_REG_RDX);c=uc.reg_read(UC_X86_REG_R8);d=uc.reg_read(UC_X86_REG_R9)
 if name in ['AcquireSRWLockExclusive','AcquireSRWLockShared','ReleaseSRWLockExclusive','ReleaseSRWLockShared','native_free','native_cookie']:leave();return
 if name=='GetCurrentProcess':leave(-1);return
 if name=='GetSystemTimeAsFileTime':write(a,'<Q',NOW*600000000+116444736000000000);leave();return
 if name=='ReadProcessMemory':
  try:
   uc.mem_write(c,bytes(uc.mem_read(b,d)));out=read(uc.reg_read(UC_X86_REG_RSP)+0x28,'<Q')[0]
   if out:write(out,'<Q',d)
   leave(1)
  except Exception:leave(0)
  return
 if name=='native_allocator':leave(allocator);return
 if name=='native_bank':leave(table);return
 if name=='native_unit_type':leave(read(a,'<I')[0]);return
 if name=='native_item_data':leave(read(a+0x10,'<Q')[0]);return
 if name=='native_base_id':leave(0);return
 if name=='native_base_row':leave(AB+0x180000);return
 if name=='native_version':leave(100);return
 if name in ['native_false','native_flags']:leave(0);return
 if name=='native_true':leave(1);return
 if name=='native_level':leave(99);return
 if name=='native_seed':leave(AB+0x181000);return
 if name=='native_rng':rng_calls.append(c);leave(3);return
 if name=='native_cast':leave(a);return
 if name=='native_properties':
  unitdata=read(c+0x10,'<Q')[0];applied_records.append(read(unitdata+0x34,'<I')[0]);leave();return
 if name=='native_client':leave(AB+0x1c0000);return
 if name=='native_send':sent_packets.append(bytes(uc.mem_read(b,21)));leave();return
 if name=='native_clear_history':cleared.append(a);leave();return
 if name=='native_alloc':
  n=b;ptr=heap;heap=(heap+max(n,16)+31)&~31;assert heap<AB+0x1f00000;leave(ptr);return
 if name in ['memcpy','memmove']:
  uc.mem_write(a,bytes(uc.mem_read(b,c)));leave(a);return
 if name=='memset':uc.mem_write(a,bytes([b&255])*c);leave(a);return
 raise RuntimeError('Unmocked Windows/CRT call '+name)
u.hook_add(UC_HOOK_CODE,callback)
def call(name,*args):
 at=exports[name] if isinstance(name,str) else name
 sp=SB+0x1f0008;write(sp,'<Q',stop)
 for i,reg in enumerate([UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9]):u.reg_write(reg,args[i] if i<len(args) else 0)
 for i,value in enumerate(args[4:]):write(sp+0x28+8*i,'<Q',value)
 u.reg_write(UC_X86_REG_RSP,sp)
 try:u.emu_start(at,stop,count=10000000)
 except Exception:
  print('Failure RIP',hex(u.reg_read(UC_X86_REG_RIP)),'trace',[hex(x) for x in trace]);raise
 assert u.reg_read(UC_X86_REG_RIP)==stop,'instruction budget exhausted'
 return u.reg_read(UC_X86_REG_RAX)
# Clone original functions before routing their native entry points to the fix.
md=Cs(CS_ARCH_X86,CS_MODE_64)
def clone(rva,size,dest):
 data=bytearray(native[rva-0x1000:rva-0x1000+size])
 for i in md.disasm(data,NB+rva):
  if i.bytes[0]==0xe8:
   target=int(i.op_str,16);struct.pack_into('<i',data,i.address-(NB+rva)+1,target-(dest+i.address-(NB+rva)+5))
 u.mem_write(dest,bytes(data));return dest
findclone=clone(0x3d87d0,0x96,CB+0x1000);insertclone=clone(0x3d8060,0x5a,CB+0x2000)
def jump(rva,target):
 u.mem_write(NB+rva,b'\xff\x25\0\0\0\0'+struct.pack('<Q',target))
 u.ctl_remove_cache(NB+rva,NB+rva+0x100)
jump(0x3d87d0,exports['test_find']);jump(0x3d8060,exports['test_insert'])
call('test_setup',NB,findclone,insertclone)
player=AB+0x130000;data=AB+0x131000;history=data+0x220
write(player,'<III',0,0,777);write(player+0x10,'<Q',data)
for kind in range(3):
 array=AB+0x150000+kind*0x1000;write(history+kind*0x20,'<QQQf',8,array,0,1.0)
audit=json.loads(args.audit.read_text()) if args.audit else {'chronicle':{'set':{},'unique':{},'runeword':{}},'named':[]}
header=(root/'record_map.h').read_text()
def array(name):return [(1 if x=='true' else 0 if x=='false' else int(x.rstrip('u'))) for x in re.search(name+r'\[\]\s*=\s*\{([^}]*)}',header).group(1).split(',') if x]
canonical_rows=array('UniqueCanonical');visibility=array('UniqueVisible')
mapping=[{'id':i,'canonical':c,'visible':bool(visibility[i])} for i,c in enumerate(canonical_rows)]
canon=lambda kind,id:mapping[id]['canonical'] if kind==1 else id
expected={0:{},1:{},2:{}}
entry=AB+0x140000
for kind,key in enumerate(['set','unique','runeword']):
 for rawid,e in audit['chronicle'][key].items():
  id=int(rawid);write(entry,'<IHHI',id,kind,e['source'],e['time'])
  assert call('test_stage',player,entry)&255,'a merged entry must not abort the archive loader'
  target=canon(kind,id);old=expected[kind].get(target)
  if old is None or e['time']<old['time']:expected[kind][target]=e
for kind,items in expected.items():
 for id,e in items.items():
  ptr=call('test_find',history,kind,id);assert ptr
  actual=read(ptr,'<IHHI');assert actual==(id,kind,e['source'],e['time']),(id,actual,e)
print('PASS: actual native map/stage accepted every saved entry; aliases merge without archive abort')
# Identified owned Arctic Mitts, including original item-owned source/date.
item=AB+0x160000;itemdata=AB+0x161000;meta=AB+0x162000
write(item,'<III',4,0,123);write(item+0x10,'<Q',itemdata);write(itemdata,'<I',5);write(itemdata+0x18,'<I',0x10800010);write(itemdata+0x34,'<I',57);write(itemdata+0xd0,'<Q',meta)
write(meta,'<H',3060);write(meta+4,'<I',29904203)
# The real shared-stash archive reader invokes 41f0f0 directly, bypassing
# 41f0b0. Reproduce the previous missing scope, then route that exact native
# call site through the new production wrapper. Only item decoding is mocked.
game=AB+0x190000;write(game+0x106,'<B',3)
shared=AB+0x1d0000
write(shared,'<I H',105,20)
for page in range(20):write(shared+6+page*4,'<I',8);write(shared+86+page*8,'<Q',item)
assert not call('test_find',history,0,57)
jump(0x41f0f0,exports['test_shared_unscoped'])
assert call(NB+0x530590,game,player,246,shared)==0
call('test_recover',player);assert not call('test_find',history,0,57)
jump(0x41f0f0,exports['test_shared_hook'])
assert call(NB+0x530590,game,player,246,shared)==0
call('test_recover',player)
assert call('test_find',history,0,57)
print('PASS: real shared-page archive route reproduces 2.1.0 omission; 2.1.1 restores Arctic Mitts')
ptr=call('test_find',history,0,57);assert ptr and read(ptr,'<IHHI')==(57,0,3060,29904203)
before=bytes(u.mem_read(itemdata,0xd8));call('test_recover',player);assert bytes(u.mem_read(itemdata,0xd8))==before
print('PASS: Arctic Mitts restored with original history; repeated recovery leaves item bytes intact')
# Canonicalize only fresh native generation, never an existing saved item.
write(itemdata,'<I',7);write(itemdata+0x34,'<I',732)
call('test_set_record',item,732,0);assert read(itemdata+0x34,'<I')[0]==732
call('test_set_record',item,732,1);assert read(itemdata+0x34,'<I')[0]==207
assert call('test_physical',2036)==2036
print('PASS: generated legacy record redirects; saved items and skill-specific jewels retain identity')
# The canonical definition provides every property; only the source's native
# repeated-drop permission survives the remainder of this generation.
target=rows+207*0x15c;source=rows+732*0x15c
u.mem_write(target,bytes((i*13)%256 for i in range(0x15c)));write(target,'<H',207);write(target+0x2c,'<B',0x08);write(source+0x2c,'<B',0x02)
ptr=call('test_generation_record',207,732);actual=bytes(u.mem_read(ptr,0x15c));want=bytearray(u.mem_read(target,0x15c));want[0x2c]|=2;assert actual==bytes(want)
print('PASS: all canonical definition bytes retained; source repeat rule avoids failed unique rolls')
packet=AB+0x170000;assert call('test_packet',packet,777,207,2231,29920000)==21
assert bytes(u.mem_read(packet,21))==struct.pack('<BBIIH BII',0x87,0,777,0xffffffff,207,1,29920000,2231)
print('PASS: native discovery packet layout')
# Execute the real native random unique generator with an already-dropped
# canonical unique and its repeatable legacy source. Preserve the same draw,
# return a successful unique, and apply properties for the canonical record.
setclone=clone(0x36dc80,0x87,CB+0x3000);rowclone=clone(0x194320,0xc0,CB+0x4000)
call('test_original_set',setclone);call('test_original_record',rowclone)
jump(0x36dc80,exports['test_set_hook']);jump(0x194320,exports['test_record_hook'])
for r,name in [(0x349860,'native_base_id'),(0x314110,'native_base_row'),(0x3730d0,'native_version'),(0x373540,'native_false'),(0x36a840,'native_true'),(0x403fe0,'native_true'),(0x36ccf0,'native_level'),(0x36cc80,'native_seed'),(0xf32b0,'native_rng'),(0x84970,'native_cast'),(0x36d8f0,'native_flags'),(0x3d3670,'native_properties')]:regstub(NB+r,name)
u.mem_write(rows,b'\0'*(2149*0x15c))
code=int.from_bytes(b'xsk ','little')
for id,flags in [(207,1),(732,11)]:
 row=rows+id*0x15c;write(row,'<H',id);write(row+0x24,'<H',100);write(row+0x28,'<I',code);write(row+0x2c,'<B',flags)
 write(row+0x34,'<I',2);write(row+0x38,'<H',1);write(row+0x90,'<i',-1)
game=AB+0x190000;spec=AB+0x1a0000;write(game+0x106,'<B',3)
write(AB+0x180000+0x80,'<I',code)
write(game+0x3638+(207//32)*4,'<I',1<<(207%32))
for i in range(32):write(NB+0x1d996d0+i*4,'<I',1<<i)
write(itemdata,'<I',7);write(itemdata+0x34,'<i',-1)
result=call('test_unique',game,item,spec)
assert result==1 and applied_records==[207],(result,applied_records,hex(call('test_seen_scope')),hex(call('test_seen_item')))
assert rng_calls==[4],rng_calls
assert read(itemdata+0x34,'<I')[0]==207
print('PASS: actual random unique generator keeps its draw/weights and succeeds with canonical properties on repeated legacy rolls')
# Every interchangeable same-base legacy definition follows the same real
# native forced-unique route, preserving the selected slot and final base.
physical_cases=0
for record in mapping:
 old=record['id'];target=call('test_physical',old)
 if target==old:continue
 write(rows+old*0x15c+0x28,'<I',code);write(rows+old*0x15c+0x24,'<H',100);write(rows+old*0x15c+0x2c,'<B',11)
 write(rows+target*0x15c+0x28,'<I',code);write(rows+target*0x15c+0x24,'<H',100);write(rows+target*0x15c+0x2c,'<B',1)
 write(spec+0x3c,'<I',1);write(spec+0x50,'<I',old)
 assert call('test_unique',game,item,spec)==1
 assert applied_records[-1]==target,(old,target,applied_records[-1])
 physical_cases+=1
print('PASS: real native generation canonicalized',physical_cases,'interchangeable definitions')
# Restore owned items under their player, then synchronize only after ready.
for x in audit['named']:
 r=x['record'];kind=int(x['category']=='unique')
 write(itemdata,'<I',7 if kind else 5);write(itemdata+0x18,'<I',r['flags']);write(itemdata+0x34,'<I',x['id'])
 write(itemdata+0xd0,'<Q',meta if 'time' in r else 0)
 if 'time' in r:write(meta,'<H',r['source']);write(meta+4,'<I',r['time'])
 call('test_saved_scope',game,player,item)
for r,name in [(0x3351b0,'native_false'),(0x48fde0,'native_client'),(0x477eb0,'native_send'),(0x3751c0,'native_clear_history')]:regstub(NB+r,name)
write(game+0x2230,'<Q',player)
assert call('test_refresh',777)&255
assert sent_packets and all(struct.unpack_from('<I',p,2)[0]==777 for p in sent_packets)
owned=set()
for x in audit['named']:
 kind=int(x['category']=='unique');id=canon(kind,x['id']);r=x['record']
 if not r['flags']&0x10 or (kind==1 and not mapping[id]['visible']):continue
 assert call('test_find',history,kind,id),(kind,id,x['name'])
 owned.add((kind,id))
print('PASS: scoped save decoding restored every eligible identified owned item and emitted ready-time native updates')
# Mercenary save equipment belongs to the same restoring player.
merc=AB+0x1b0000
write(merc,'<III',1,271,999)
mercid=next(i for i,r in enumerate(mapping) if r['visible'] and not call('test_find',history,1,i))
write(itemdata,'<I',7);write(itemdata+0x18,'<I',0x10);write(itemdata+0x34,'<I',mercid);write(itemdata+0xd0,'<Q',0)
call('test_saved_scope',game,merc,item);call('test_recover',player)
assert call('test_find',history,1,mercid)
print('PASS: mercenary saved equipment recovers for its restoring owner')
# Network item decoding alone is never evidence of owning/discovering an item.
unused=next(i for i,r in enumerate(mapping) if r['visible'] and not call('test_find',history,1,i))
write(itemdata,'<I',7);write(itemdata+0x18,'<I',0x10);write(itemdata+0x34,'<I',unused);write(itemdata+0xd0,'<Q',0)
call('test_network_decode',item);call('test_recover',player);assert not call('test_find',history,1,unused)
# New Mystery Chest reward: actual native player registration and send routing.
before_packets=len(sent_packets);assert call('test_discover',game,item,777)&255
ptr=call('test_find',history,1,unused);assert ptr and read(ptr,'<IHHI')==(unused,1,2231,NOW)
assert len(sent_packets)==before_packets+1 and cleared==[item]
assert struct.unpack('<BBIIH BII',sent_packets[-1])==(0x87,0,777,123,unused,1,NOW,2231)
print('PASS: new cube reward is registered through native Chronicle functions; unrelated network items are ignored')
(root/'tests/native-test-result.json').write_text(json.dumps({'native_saved_records':sum(len(x) for x in audit['chronicle'].values()),'canonical_counts':{k:len(v) for k,v in expected.items()},'arctic_mitts':True,'no_archive_abort':True,'saved_item_bytes_unchanged':True,'canonical_generation':True,'native_forced_alias_cases':physical_cases,'owned_eligible_entries_verified':len(owned),'source_repeat_rule':True,'native_random_unique_roll':True,'unchanged_rng_calls_and_weights':rng_calls==[4],'packet_layout':True,'ready_time_snapshot':True,'new_cube_reward_registration':True,'unrelated_network_items_ignored':True,'emulated_instructions':calls,'real_shared_archive_reader_regression':True,'live_game_test':False},indent=2))
