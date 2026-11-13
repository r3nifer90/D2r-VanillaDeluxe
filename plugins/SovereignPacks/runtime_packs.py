import json,struct,hashlib
from pathlib import Path
from runtime_core import DLL,MASK
from runtime_files import VisualRuntime,manifest,path,MAP,transforms
HERE=Path(__file__).parent;checks=[]
def check(n,v):assert v,n;checks.append(n)
def logs(r):return '\n'.join(s for _,s in r.logs)
class PackRuntime(VisualRuntime):
 @staticmethod
 def defaults():return {'poison_nova_log_descriptions':0,'plaguebearer_affix_chance_percent':0}
 def AcquireSRWLockShared(self):return 0
 def ReleaseSRWLockShared(self):return 0
 def __init__(self,rooms=16,loaded=False,**kw):
  super().__init__(table_count=manifest['total_rows'],**kw)
  self.positions={self.path:(500,500)};self.server_clients={1:self.client}
  self.uc.mem_write(self.client,struct.pack("<I",1))
  self.slot(0x820aa0,self.callback(self.resolve_client))
  self.slot(0x41163f,self.callback(lambda:self.positions.get(self.arg(0),(500,500))[0]))
  self.slot(0x411650,self.callback(lambda:self.positions.get(self.arg(0),(500,500))[1]))
  self.clock=1000;self.frame_callback=None;self.frame_unregistered=[]
  self.services[20]=self.service(88,1,[self.register_frame,self.unregister_frame]+[lambda:0]*8)
  self.named_ids=json.loads((HERE/"name-manifest.json").read_text())["leader_ids"]
  self.fake_mon_rows={};self.fake_named_rows={}
  self.room2s=[];self.active={};self.forwarded=[];self.save_calls=[];self.rects=[]
  self.drlg=self.put(bytes(0x210));self.uc.mem_write(self.drlg+0x1f8,struct.pack('<I',2))
  for n in range(rooms):
   room2=self.put(bytes(0xa0));self.room2s.append(room2);rect=(190+30*n,190+20*n,20,20);self.rects.append(tuple(v*5 for v in rect))
   self.uc.mem_write(room2+0x60,struct.pack('<4i',*rect));self.qwrite(room2+0x90,self.drlg)
   if n:self.qwrite(self.room2s[n-1]+0x48,room2)
   if not n:self.activate(n,self.room)
   elif loaded:self.activate(n)
  self.qwrite(self.drlg+0x10,self.room2s[0])
  self.population_original=self.callback(self.population);self.qwrite(self.core_base+0x7079c8,self.population_original)
  self.save_original=self.callback(self.save_native);self.qwrite(self.core_base+0x6fe5b0,self.save_original)
  self.regions.insert(0,(self.core_base+0x6fe000,self.core_base+0x6ff000,4))
  self.manual_returns.add(self.core_base+0x2601f0);self.callbacks[self.core_base+0x2601f0]=lambda:0
 def GetPrivateProfileStringW(self):
  assert self.read_string(self.arg(0),True)=='packs'
  key=self.read_string(self.arg(1),True);value=str(self.config.get(key,self.read_string(self.arg(2),True)))
  self.uc.mem_write(self.arg(3),value.encode('utf-16-le')+b'\0\0');return len(value)
 def register_frame(self):
  self.frame_callback=self.qread(self.arg(1)+16);self.qwrite(self.arg(2),444);return 0
 def unregister_frame(self):self.frame_unregistered.append(self.arg(1));self.frame_callback=None;return 0
 def GetTickCount64(self):return getattr(self,'clock',1000)
 def frame(self,elapsed=2000):
  self.clock+=elapsed
  if self.frame_callback:self.call(self.frame_callback,self.context,0,0)
  self.flush_console();self.flush_game()
 def get_table(self):
  if self.arg(2)==28:
   assert self.native_thread
   self.uc.mem_write(self.arg(3),struct.pack('<IIIIQQIIQ',48,0,28,self.arg(1),1,self.mon_rows,getattr(self,'named_count',100),60,0));return 0
  return super().get_table()
 def get_row(self):
  assert self.native_thread
  kind,index,out=self.arg(2),self.arg(3),self.arg(4)
  if kind==28:
   assert 70<=index<100
   if index not in self.fake_named_rows:
    n=self.named_ids.index(index);stock=manifest['packs'][n]['stock'];row=bytearray(60);struct.pack_into('<H',row,4,41040+stock%8);self.fake_named_rows[index]=self.put(row)
   pointer,size=self.fake_named_rows[index],60
  else:
   assert kind==22 and 815<=index<manifest['total_rows']
   if index not in self.fake_mon_rows:
    n=(index-815)//2;stock=manifest['packs'][n]['stock'];row=bytearray(508);struct.pack_into('<H',row,0x60,41040+stock%8+(8 if (index-815)%2 else 0));self.fake_mon_rows[index]=self.put(row)
   pointer,size=self.fake_mon_rows[index],508
  self.uc.mem_write(out,struct.pack('<IIIIQQIIQ',48,0,kind,self.arg(1),1,pointer,index,size,0));return 0
 def activate(self,index,active=None):
  active=active or self.put(bytes(0xc0));self.active[active]=index
  self.qwrite(active+0x18,self.room2s[index]);self.qwrite(self.room2s[index]+0x58,active);return active
 def population(self):self.forwarded.append((self.arg(0),self.arg(1)));return 0
 def populate(self,index):
  active=next((r for r,i in self.active.items() if i==index),None) or self.activate(index)
  self.native_thread=True
  try:self.call(self.core_exports[1167],self.game,active)
  finally:self.native_thread=False
 def save_native(self):
  self.save_calls.append([self.arg(n) for n in range(9)]);self.qwrite(self.arg(4),self.player);return getattr(self,'save_result',0)
 def save(self,complete=True,invalid=False,ambiguous=False,game=None,client=None):
  raw=bytearray(512);raw[:4]=b'\x55\xaa\x55\xaa';raw[32:42]=b'Woo!'+struct.pack('<IH',6,298)
  struct.pack_into('<H',raw,32+10+2*96+40*2,1 if complete else 0)
  if invalid:raw[:4]=b'BAD!'
  if ambiguous:raw+=raw[32:330]
  data=self.put(bytes(raw));out=self.put(bytes(8));self.native_thread=True
  try:return self.call(self.qread(self.core_base+0x6fe5b0),game or self.game,client or self.client,data,len(raw),out,123,234,345,105)
  finally:self.native_thread=False
 def allocate_monster(self):
  game,room,origin,monster,mode,unused,x,y,flags,e1,e2=struct.unpack('<QQQiIIiiIQQ',self.uc.mem_read(self.arg(0),64))
  assert self.native_thread and game==self.game and room in self.active and not origin
  assert 815<=monster<manifest['total_rows'] and mode==1 and not unused and flags==3 and not e1 and not e2
  assert struct.unpack('<I',self.uc.mem_read(self.tls+0x1958,4))[0]==1
  box=self.rects[self.active[room]];assert box[0]+2<=x<box[0]+box[2]-2 and box[1]+2<=y<box[1]+box[3]-2
  self.spawns.append({'class':monster,'leader':(monster-815)%2==0,'x':x,'y':y,'room':self.active[room],'success':not self.fail_spawns})
  if self.fail_spawns:self.fail_spawns-=1;return 0
  u=self.put(bytes(512));self.uc.mem_write(u,struct.pack('<III',1,monster,len(self.spawns)));self.qwrite(u+0x10,self.put(bytes(0x80)));self.qwrite(u+0x28,0x12345+len(self.spawns));self.mock_stats[u]={12:95,13:1000,7:25600,6:25600,21:100,22:200};return u
 def resolve_client(self):
  assert self.native_thread
  return self.server_clients.get(self.arg(0)&0xffffffff,0)
 def guest_client(self,level=2):
  client=super().guest_client(level);id=max(self.server_clients)+1
  self.uc.mem_write(client,struct.pack("<I",id));self.server_clients[id]=client;return client
 def event(self,*args,**kw):
  self.uc.mem_write(self.drlg+0x1f8,struct.pack('<I',kw.get('level',2)));return super().event(*args,**kw)
 def GetModuleHandleW(self):
  if not self.arg(0):return 0
  name=self.read_string(self.arg(0),True);return self.core_base if name=='D2RCore.dll' else 0

