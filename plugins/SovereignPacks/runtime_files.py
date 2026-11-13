from runtime_core import *
import json
HERE=Path(__file__).parent
fixture=(HERE/"fixtures/enemy-definition.json").read_bytes()
manifest=json.loads((HERE/"manifest.json").read_text())
STOCKS=manifest["models"]
MAP="data/hd/character/monsters.json"
ROOT="D:\\Game\\mods\\VanillaDeluxe\\VanillaDeluxe.mpq\\"
original_mapping=json.dumps({key:kind["hd_key"] for kind in manifest["packs"] for key in (kind["leader_key"],kind["minion_key"])}).encode()
def path(relative):return ROOT+relative.replace("/","\\")
def transforms(obj):
 if isinstance(obj,dict):
  if obj.get("name")=="entity_root_TransformDefinition":yield obj
  for v in obj.values():yield from transforms(v)
 elif isinstance(obj,list):
  for v in obj:yield from transforms(v)
class VisualRuntime(Runtime):
    def __init__(self,files=None,missing=None,fail_save=None,bad_definition=None,**kw):
        self.files=dict(files or {path(MAP):original_mapping})
        self.file_handles={};self.archive_handles={};self.archive_closed=[];self.opened=[]
        self.fail_save=fail_save
        self.archive={}
        for n,stock in enumerate(STOCKS):
            obj=json.loads(fixture)
            root=next(transforms(obj))
            root['scale']={'z':0.95+n/10,'x':1+n/10,'y':1.2+n/10}

            raw=json.dumps(obj,ensure_ascii=False).encode().replace(b'"x": 1.0',b'"x": 1e0',1)
            self.archive[f'data/hd/character/enemy/{stock}.json']=raw
        if missing:self.archive.pop(f'data/hd/character/enemy/{missing}.json')
        if bad_definition is not None:self.archive['data/hd/character/enemy/diablo.json']=bad_definition
        super().__init__(**kw)
        self.qwrite(self.context+48,self.string('D:\\Game\\mods\\VanillaDeluxe',True))
        for rva,cb in [(0x29e9a6,self.archive_open),(0x29e9b8,self.archive_size),(0x29ea4a,self.archive_read),(0x29ea6c,self.archive_close)]:self.slot(rva,self.callback(cb))
    def GetFileAttributesW(self):
        p=self.read_string(self.arg(0),True)
        if p==ROOT[:-1]:return 0x10
        if p in self.files:return 0x20
        if p.endswith('.ini'):return 0x20
        return 0xffffffff
    def CreateFileW(self):
        p=self.read_string(self.arg(0),True)
        if p.endswith('SovereignPacks.runtime.log'):return 0xc001
        if self.fail_save and self.fail_save in p:return MASK
        if (self.arg(4)&0xffffffff)==3 and p not in self.files:return MASK
        h=0xd000+len(self.file_handles);self.file_handles[h]=p
        if (self.arg(4)&0xffffffff)==2:self.files[p]=b''
        return h
    def GetFileSizeEx(self):
        self.qwrite(self.arg(1),len(self.files[self.file_handles[self.arg(0)]]));return 1
    def ReadFile(self):
        data=self.files[self.file_handles[self.arg(0)]];self.uc.mem_write(self.arg(1),data[:self.arg(2)])
        self.uc.mem_write(self.arg(3),struct.pack('<I',min(len(data),self.arg(2))));return 1
    def WriteFile(self):
        if self.arg(0)==0xc001:return super().WriteFile()
        data=bytes(self.uc.mem_read(self.arg(1),self.arg(2)));self.files[self.file_handles[self.arg(0)]]+=data
        self.uc.mem_write(self.arg(3),struct.pack('<I',len(data)));return 1
    def CreateDirectoryW(self):return 1
    def CopyFileW(self):
        src=self.read_string(self.arg(0),True);dst=self.read_string(self.arg(1),True)
        assert dst not in self.files
        self.files[dst]=self.files[src];return 1
    def MoveFileExW(self):
        src=self.read_string(self.arg(0),True);dst=self.read_string(self.arg(1),True)
        self.files[dst]=self.files.pop(src);return 1
    def DeleteFileW(self):self.files.pop(self.read_string(self.arg(0),True),None);return 1
    def archive_open(self):
        p=self.read_string(self.arg(0));self.opened.append(p)
        if p not in self.archive:return 0
        h=0xe000+len(self.archive_handles);self.archive_handles[h]=p;return h
    def archive_size(self):return len(self.archive[self.archive_handles[self.arg(0)]])
    def archive_read(self):
        data=self.archive[self.archive_handles[self.arg(0)]];self.uc.mem_write(self.arg(1),data)
        self.qwrite(self.arg(3),len(data));return 1
    def archive_close(self):self.archive_closed.append(self.arg(0));return 0
    def prepare(self):
        assert self.load()
        self.call(self.table_listener,self.context,0,0)
    def logs_text(self):return '\n'.join(s for _,s in self.logs)
