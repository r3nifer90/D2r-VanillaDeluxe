import argparse
import os
import configparser
import json
import struct
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve()
PACKAGE = HERE.parent.parent if HERE.parent.name == 'source' else ROOT / 'terror-encounters'
DLL = PACKAGE / 'd2rloader/plugins/SovereignPacks.dll'
CORE = Path(os.environ.get('D2RCORE_PATH',ROOT / 'research/shrines/loader/D2RCore.dll'))
REPORT = PACKAGE / 'source/native-validation.json'
FUNCTIONS, MEMORY, STACK, TEB = 0x700000000, 0x710000000, 0x720000000, 0x730000000
STOP = FUNCTIONS + 0xFF00
MASK = (1 << 64) - 1


class Runtime:
    def __init__(self, config=None, mod='VanillaDeluxe', corrupt_core=None,
                 table_count=815, skill_count=437, remote=False,
                 api_size=224, context_size=200, service_version=1,
                 fail_spawns=0, log_writable=True, balance=None, cost_count=None):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.callbacks = {}; self.manual_returns = set(); self.allocations={}
        self.regions = []
        for base, size in [(FUNCTIONS, 0x10000), (MEMORY, 0x1000000), (STACK, 0x100000), (TEB, 0x10000)]:
            self.map(base, size, base == FUNCTIONS)
        self.uc.mem_write(STOP, b'\xC3')
        self.next_memory = MEMORY + 0x1000
        self.logs, self.commands, self.console_queue, self.game_queue = [], {}, [], []
        self.listeners, self.table_listener, self.handles = {}, None, []
        self.spawns, self.runtime_log, self.closed_handles = [], bytearray(), []
        self.config = self.defaults() | (config or {})
        self.balance = balance or {}; self.cost_count = cost_count
        self.table_count, self.skill_count, self.remote = table_count, skill_count, remote
        self.fail_spawns, self.log_writable = fail_spawns, log_writable
        self.native_thread = False
        self.remote_host = None
        self.unit_levels = {}; self.mock_stats={};self.death_calls=[];self.death_listener=None
        self.level_id, self.difficulty = 2, 2
        self.map_pe(DLL, 'dll')
        self.map_pe(CORE, 'core')
        self.core_exports = {e.ordinal: self.core_base + e.address for e in self.core_pe.DIRECTORY_ENTRY_EXPORT.symbols}
        self.uc.hook_add(UC_HOOK_CODE, self.hook)
        for desc in self.dll_pe.DIRECTORY_ENTRY_IMPORT:
            assert desc.dll.lower() == b'kernel32.dll'
            for imp in desc.imports:
                self.qwrite(imp.address, self.callback(getattr(self, imp.name.decode())))


        self.tls = self.put(bytes(0x2000))
        tls_array = self.put(struct.pack('<Q', self.tls))
        self.qwrite(TEB + 0x58, tls_array)
        self.uc.reg_write(UC_X86_REG_GS_BASE, TEB)
        self.uc.mem_write(self.core_base + 0x7DF224, struct.pack('<I', 0))
        self.slot(0x41163F, self.callback(lambda: 1000))
        self.slot(0x411650, self.callback(lambda: 1000))
        self.slot(0x411A18, self.callback(self.allocate_monster))
        self.qwrite(self.core_base+0x707e78,self.callback(self.native_death_drop))
        self.regions.insert(0,(self.core_base+0x707000,self.core_base+0x708000,4))
        self.manual_returns.update([self.core_exports[1242],self.core_exports[1243],self.core_exports[1249]])
        self.callbacks[self.core_exports[1242]]=self.read_stat
        self.callbacks[self.core_exports[1243]]=self.read_stat
        self.callbacks[self.core_exports[1276]]=lambda:self.mock_stats.get(self.arg(0),{}).get(172,0)
        self.manual_returns.add(self.core_exports[1276])
        self.callbacks[self.core_exports[1249]]=self.write_stat
        self.callbacks[self.core_exports[2000]] = lambda: self.unit_levels.get(self.arg(0), self.level_id)
        self.callbacks[self.core_exports[14]] = self.execute_console
        for address in [self.core_exports[2000], self.core_exports[14]]:
            self.uc.mem_write(address, b'\xC3')
        if corrupt_core is not None:
            address = self.core_base + corrupt_core
            self.uc.mem_write(address, bytes([self.uc.mem_read(address, 1)[0] ^ 1]))


        self.affix_calls=[];self.affix_eligible_calls=[];self.native_unique_count=0
        self.umod_rows=self.put(bytes(32*32));self.affix_tables=self.put(bytes(0x1500))
        self.qwrite(self.affix_tables+0x1448,self.umod_rows);self.qwrite(self.affix_tables+0x1450,32)
        for n in range(1,32): self.uc.mem_write(self.umod_rows+n*32+24,struct.pack('<H',1))
        self.slot(0x40c95d,self.callback(lambda:self.affix_tables))
        self.slot(0x40ca1f,self.callback(self.affix_eligible))
        self.slot(0x40f0aa,self.callback(self.assign_affix))
        self.room = self.put(bytes(128))
        self.path = self.put(bytes(128)); self.qwrite(self.path + 0x20, self.room)
        self.player = self.put(bytes(256)); self.qwrite(self.player + 0x38, self.path)
        self.level = self.put(bytes(0x300))
        self.levels = self.put(bytes(1024 * 8))
        self.game = self.put(bytes(0x300)); self.qwrite(self.game + 0x1D0, self.levels)
        self.uc.mem_write(self.game + 0x106, b'\x02')
        self.uc.mem_write(self.game + 0x104, b'\x02')
        self.client = self.put(bytes(0x300))
        self.qwrite(self.client + 0x2B0, self.game); self.qwrite(self.client + 0x278, self.player)
        self.monster = self.put(bytes(512))
        self.mon_rows = self.put(bytes(815 * 16)); self.skill_rows = self.put(bytes(437 * 16))

        life = self.service(56, service_version, [self.register_tables, self.unregister,
                             self.register_gameplay, self.unregister, self.register_death, self.unregister])
        thread = self.service(24, service_version, [lambda: 2, self.queue_game])
        table = self.service(40, service_version, [self.get_table, self.get_row, lambda: 2, lambda: 2])
        encounter = self.service(32, service_version, [lambda: 0, lambda: 0, self.core_base + 0x410FB0])
        self.services = {1: life, 13: thread, 8: table, 18: encounter}
        api = bytearray(224); struct.pack_into('<I', api, 0, api_size)
        for offset, kind in [(8, 'INFO'), (16, 'WARN'), (24, 'ERROR')]:
            struct.pack_into('<Q', api, offset, self.callback(lambda kind=kind: self.log(kind)))
        struct.pack_into('<Q', api, 160, self.callback(self.register_console))
        struct.pack_into('<Q', api, 216, self.callback(self.query_service))
        ctx = bytearray(200)
        struct.pack_into('<IIQIIQ', ctx, 0, context_size, 4, self.put(api), 1, 0, 0x140000000)
        struct.pack_into('<Q', ctx, 32, self.string('vanilla-terror-encounters'))
        struct.pack_into('<Q', ctx, 40, self.string(mod))
        struct.pack_into('<Q', ctx, 112, self.string('D:\\gry żółć & [test]\\mods\\VanillaDeluxe\\d2rloader\\plugins', True))
        self.context = self.put(ctx)
        assert self.call(self.dll_entry, self.dll_base, 1, 0) == 1

    @staticmethod
    def defaults():
        p = configparser.ConfigParser(); p.read(PACKAGE / 'd2rloader/plugins/TerrorEncounters.ini')

        return dict(p['encounters']) | {'random_spawn_location':'0'}

    def map(self, address, size, executable=False):
        size = (size + 4095) & ~4095
        self.uc.mem_map(address, size); self.regions.append((address, address + size, 0x20 if executable else 4))

    def map_pe(self, filename, label):
        p = pefile.PE(str(filename)); base = p.OPTIONAL_HEADER.ImageBase
        self.map(base, p.OPTIONAL_HEADER.SizeOfImage, True)
        self.uc.mem_write(base, p.get_memory_mapped_image())
        setattr(self, label + '_base', base); setattr(self, label + '_pe', p)
        if label == 'dll':
            self.dll_entry = base + p.OPTIONAL_HEADER.AddressOfEntryPoint
            self.exports = {e.name.decode(): base + e.address for e in p.DIRECTORY_ENTRY_EXPORT.symbols}

    def put(self, data):
        address = self.next_memory; self.next_memory += (len(data) + 15) & ~15
        assert self.next_memory < MEMORY + 0x1000000
        self.uc.mem_write(address, bytes(data)); return address

    def qwrite(self, address, value): self.uc.mem_write(address, struct.pack('<Q', value))
    def qread(self, address): return struct.unpack('<Q', self.uc.mem_read(address, 8))[0]
    def string(self, value, wide=False): return self.put(value.encode('utf-16-le' if wide else 'utf-8') + (b'\0\0' if wide else b'\0'))
    def read_string(self, address, wide=False):
        step = 2 if wide else 1; data = bytearray()
        for i in range(0, 8192, step):
            b = self.uc.mem_read(address + i, step)
            if b == bytes(step): return data.decode('utf-16-le' if wide else 'utf-8')
            data.extend(b)
        raise AssertionError('unterminated string')

    def callback(self, function):
        p = FUNCTIONS + 0x100 + len(self.callbacks) * 16
        self.callbacks[p] = function; self.uc.mem_write(p, b'\xC3'); return p

    def service(self, size, version, functions):
        return self.put(struct.pack('<II', size, version) + b''.join(struct.pack('<Q', f if isinstance(f, int) else self.callback(f)) for f in functions))

    def slot(self, rva, function):
        displacement = struct.unpack('<i', self.uc.mem_read(self.core_base + rva + 2, 4))[0]
        self.qwrite(self.core_base + rva + 6 + displacement, function)

    def arg(self, i):
        return self.uc.reg_read([UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9][i]) if i < 4 else self.qread(self.uc.reg_read(UC_X86_REG_RSP) + 8 + i * 8)

    def hook(self, uc, address, size, data):
        if address == STOP: uc.emu_stop()
        elif address in self.callbacks:
            result = self.callbacks[address]()
            uc.reg_write(UC_X86_REG_RAX, 0 if result is None else result)
            if address in self.manual_returns:
                rsp=uc.reg_read(UC_X86_REG_RSP)
                uc.reg_write(UC_X86_REG_RIP,self.qread(rsp));uc.reg_write(UC_X86_REG_RSP,rsp+8)

    def call(self, address, *args):
        rsp = STACK + 0xF0000 - 8; self.qwrite(rsp, STOP)
        self.uc.reg_write(UC_X86_REG_RSP, rsp)
        for i, value in enumerate(args):
            if i < 4: self.uc.reg_write([UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9][i], value)
            else: self.qwrite(rsp + 8 + i * 8, value)
        try: self.uc.emu_start(address, STOP + 1, count=300_000_000)
        except Exception as exc:
            raise AssertionError(f'emulation fault call={address:#x}, rip={self.uc.reg_read(UC_X86_REG_RIP):#x}, rsp={self.uc.reg_read(UC_X86_REG_RSP):#x}') from exc
        assert self.uc.reg_read(UC_X86_REG_RIP) == STOP, hex(self.uc.reg_read(UC_X86_REG_RIP))
        return self.uc.reg_read(UC_X86_REG_RAX)

    def load(self): return self.call(self.exports['D2RLoaderLoadPlugin'], self.context) & 0xFF
    def unload(self): return self.call(self.exports['D2RLoaderUnloadPlugin'])
    def log(self, kind): self.logs.append((kind, self.read_string(self.arg(1))))
    def AcquireSRWLockExclusive(self): return 0
    def ReleaseSRWLockExclusive(self): return 0
    def GetProcessHeap(self): return 0xCAFE
    def HeapAlloc(self):
        size=self.arg(2);p=self.put(bytes(size));self.allocations[p]=size;return p
    def HeapFree(self): return 1
    def HeapReAlloc(self):

        old=self.arg(2);size=self.arg(3);data=bytes(self.uc.mem_read(old,min(size,self.allocations.get(old,0))))
        new=self.put(data+bytes(size-len(data)));self.allocations[new]=size;return new

    def HeapCreate(self): return 0xBABE
    def HeapDestroy(self): return 1
    def GetCurrentProcess(self): return MASK
    def GetCurrentThreadId(self): return 42
    def GetSystemInfo(self): self.uc.mem_write(self.arg(0),struct.pack('<IIQQQIIIHH',9,4096,0x10000,0x7fffffffffff,1,1,8664,0x10000,6,0))
    def VirtualAlloc(self):
        a=self.arg(0)
        assert a and a%4096==0
        try:self.map(a,self.arg(1),True)
        except Exception:return 0
        return a
    def VirtualFree(self): return 1
    def VirtualProtect(self):
        if getattr(self,'hook_fail',False):return 0
        self.uc.mem_write(self.arg(3),struct.pack('<I',0x20));return 1
    def FlushInstructionCache(self):self.uc.ctl_remove_cache(self.arg(1),self.arg(1)+self.arg(2));return 1
    def CreateToolhelp32Snapshot(self):self.enum_index=0;return 0xD001
    def Thread32First(self):self.uc.mem_write(self.arg(1),struct.pack('<IIIIIii',28,0,42,1234,0,0,0));return 1
    def Thread32Next(self):
        self.enum_index+=1
        if self.enum_index==1:self.uc.mem_write(self.arg(1),struct.pack('<IIIIIii',28,0,43,1234,0,0,0));return 1
        return 0
    def OpenThread(self): return 0xF001
    def SuspendThread(self):self.suspend_count=getattr(self,'suspend_count',0)+1;return 0
    def ResumeThread(self):self.resume_count=getattr(self,'resume_count',0)+1;return 0
    def GetThreadContext(self):self.qwrite(self.arg(1)+0xF8,getattr(self,'other_rip',0));return 1
    def SetThreadContext(self):self.other_rip=self.qread(self.arg(1)+0xF8);return 1
    def Sleep(self): return 0
    def ReadFile(self): return 0
    def GetFileSizeEx(self): return 0
    def CreateDirectoryW(self): return 0
    def GetLastError(self): return 18 if getattr(self,'enum_index',0)>=2 else 2
    def CopyFileW(self): return 0
    def DeleteFileW(self): return 0
    def MoveFileExW(self): return 0
    def QueryPerformanceCounter(self): self.qwrite(self.arg(0), 0x123456789AB); return 1
    def GetTickCount64(self): return 0x98765
    def GetCurrentProcessId(self): return 1234
    def GetModuleHandleW(self):
        if not self.arg(0):return 0
        assert self.read_string(self.arg(0), True) == 'D2RCore.dll'; return self.core_base
    def GetProcAddress(self): return self.core_exports.get(self.arg(1), 0)
    def GetFileAttributesW(self): return 0x20
    def GetPrivateProfileStringW(self):
        section=self.read_string(self.arg(0), True);key=self.read_string(self.arg(1), True)
        if self.read_string(self.arg(5),True).endswith('TerrorEncountersBalance.ini'):
            value=str(self.balance.get(section,{}).get(key,''))
        else:
            assert section=='encounters'
            value=str(self.config.get(key,self.read_string(self.arg(2),True)))
        value=value[:self.arg(4)-1]
        self.uc.mem_write(self.arg(3), value.encode('utf-16-le') + b'\0\0'); return len(value)
    def GetModuleFileNameW(self):
        assert self.arg(0) == self.dll_base
        value = 'D:\\gry żółć & [test]\\mods\\VanillaDeluxe\\d2rloader\\plugins\\SovereignPacks.dll'
        self.uc.mem_write(self.arg(1), value.encode('utf-16-le') + b'\0\0'); return len(value)
    def CreateFileW(self):
        assert self.read_string(self.arg(0), True).endswith('\\SovereignPacks.runtime.log')
        return 0xC001 if self.log_writable else MASK
    def WriteFile(self):
        assert self.arg(0) == 0xC001
        self.runtime_log.extend(self.uc.mem_read(self.arg(1), self.arg(2)))
        self.uc.mem_write(self.arg(3), struct.pack('<I', self.arg(2))); return 1
    def FlushFileBuffers(self): return 1
    def CloseHandle(self): self.closed_handles.append(self.arg(0)); return 1
    def VirtualQuery(self):
        address, out = self.arg(0), self.arg(1)
        region = next((r for r in self.regions if r[0] <= address < r[1]), None)
        if region is None:
            begin, end, protect, state = address & ~4095, (address & ~4095) + 4096, 0, 0x10000
        else: begin, end, protect = region; state = 0x1000
        self.uc.mem_write(out, struct.pack('<QQIIQIIII', begin, begin, protect, 0, end - begin, state, protect, 0, 0)); return 48
    def query_service(self):
        assert self.arg(2) == 1
        self.qwrite(self.arg(3), self.services.get(self.arg(1), 0)); return 0 if self.arg(1) in self.services else 2
    def register_console(self):
        p = self.arg(1); assert struct.unpack('<II', self.uc.mem_read(p, 8)) == (88, 3)
        assert struct.unpack('<II', self.uc.mem_read(p + 56, 8)) == (1, 0)
        self.commands[self.read_string(self.qread(p + 8))] = (self.qread(p + 24), self.qread(p + 32)); return 1
    def register_gameplay(self):
        p = self.arg(1); kind = struct.unpack('<I', self.uc.mem_read(p + 8, 4))[0]
        self.listeners[kind] = self.qread(p + 16); self.qwrite(self.arg(2), kind); return 0
    def register_tables(self):
        self.table_listener = self.qread(self.arg(1) + 8); self.qwrite(self.arg(2), 100); return 0
    def unregister(self): self.handles.append(self.arg(1)); return 0
    def execute_console(self): self.console_queue.append(self.read_string(self.arg(0))); return 1
    def queue_game(self):
        if self.remote: return 2
        self.game_queue.append((self.arg(1), self.arg(2))); return 0
    def get_table(self):
        assert self.native_thread
        bank, kind, out = self.arg(1), self.arg(2), self.arg(3)
        assert bank in [1, 2, 3] and kind in [22, 12, 35, 44]
        if kind==44:return 3
        count, rows = (self.table_count, self.mon_rows) if kind == 22 else (self.skill_count, self.skill_rows) if kind == 12 else ((447 if bank==3 else 446) if self.cost_count is None else self.cost_count,self.skill_rows)
        self.uc.mem_write(out, struct.pack('<IIIIQQIIQ', 48, 0, kind, bank, 1, rows, count, 16, 0)); return 0
    def get_row(self):
        assert self.native_thread and self.arg(2) == 22
        index = self.arg(3); out = self.arg(4)
        assert 793 <= index <= 814
        self.uc.mem_write(out, struct.pack('<IIIIQQIIQ', 48, 0, 22, self.arg(1), 1, self.mon_rows + index * 16, index, 16, 0)); return 0
    def register_death(self):
        self.death_listener=self.qread(self.arg(1)+8);self.qwrite(self.arg(2),101);return 0
    def native_death_drop(self):
        change=self.arg(1);unit=self.qread(change+8)
        self.death_calls.append({'game':self.arg(0),'change':change,'third':self.arg(2),'fourth':self.arg(3),'unit':unit,'flags':struct.unpack('<I',self.uc.mem_read(unit+0x124,4))[0]})
        return 0
    def read_stat(self):return self.mock_stats.get(self.arg(0),{}).get(self.arg(1),0)&0xffffffff
    def write_stat(self):
        stats=self.mock_stats.setdefault(self.arg(0),{});old=stats.get(self.arg(1),0);v=self.arg(2)&0xffffffff;stats[self.arg(1)]=v;return int(v!=old)
    def allocate_monster(self):
        assert self.native_thread
        assert struct.unpack('<I', self.uc.mem_read(self.tls + 0x1958, 4))[0] == 1
        values = struct.unpack('<QQQiIIiiIQQ', self.uc.mem_read(self.arg(0), 64))
        game, room, origin, monster, mode, unused, x, y, flags, extra1, extra2 = values
        assert game == self.game and room == self.room and origin == 0
        assert 793 <= monster <= 814 and mode == 1 and unused == 0 and flags == 3 and extra1 == extra2 == 0
        self.spawns.append({'monster': monster, 'x': x, 'y': y, 'success': self.fail_spawns == 0})
        if self.fail_spawns: self.fail_spawns -= 1; return 0
        self.uc.mem_write(self.monster,struct.pack('<III',1,monster,len(self.spawns)));self.mock_stats[self.monster]={7:25600,6:25600}
        return self.monster

    def affix_eligible(self):
        unit,row,expansion=self.arg(0),self.arg(1),self.arg(2)
        assert self.native_thread and expansion==1
        n=(row-self.umod_rows)//32;self.affix_eligible_calls.append((unit,n))
        return 0 if n in getattr(self,'excluded_affixes',set()) else 1

    def assign_affix(self):
        game,unit,n,unique=(self.arg(i) for i in range(4))
        assert self.native_thread and game==self.game and unique==1
        data=self.qread(unit+0x10);flags=struct.unpack('<H',self.uc.mem_read(data+0x1a,2))[0]
        if not flags&8:self.native_unique_count+=1
        self.uc.mem_write(data+0x1a,struct.pack('<H',flags|8))
        mods=list(self.uc.mem_read(data+0x20,9));mods[mods.index(0)]=n
        self.uc.mem_write(data+0x20,bytes(mods));self.affix_calls.append((unit,n,unique))

        return 0

    def event(self, kind, generation=1, level=2, difficulty=2, terror=True, event_difficulty=0, flush=True):
        self.level_id, self.difficulty = level, difficulty
        self.uc.mem_write(self.game + 0x104, bytes([difficulty]))
        self.qwrite(self.levels + level * 8, self.level)
        self.uc.mem_write(self.level + 0x2E8, bytes([int(terror)]))
        event = self.put(struct.pack('<IIIIQiiII', 40, 0, kind, 1, generation, -1, level, event_difficulty, 0))
        self.call(self.listeners[kind], self.context, event, 0)
        if flush: self.flush_console(); self.flush_game()

    def invoke_command(self, text, client=None):
        name, args = text.split(' ', 1)
        callback, user = self.commands[name]
        c = self.put(struct.pack('<IIIIQQQQ4I', 64, len(text), len(name), len(args), self.string(text), self.string(name), self.string(args), self.context, 0, 0, 0, 0))
        return self.call(callback, self.client if client is None else client, c, user)

    def guest_client(self, level=2):
        player = self.put(bytes(256)); self.qwrite(player + 0x38, self.path)
        self.unit_levels[player] = level
        client = self.put(bytes(0x300))
        self.qwrite(client + 0x2B0, self.game); self.qwrite(client + 0x278, player)
        self.qwrite(self.levels + level * 8, self.level)
        self.uc.mem_write(self.level + 0x2E8, b'\x01')
        return client

    def flush_console(self):
        while self.console_queue:
            text = self.console_queue.pop(0)
            if self.remote and self.remote_host is not None:
                assert self.remote_host.invoke_command(text, self.remote_server_client) == 0
                self.remote_host.flush_game()
            else:
                assert self.invoke_command(text) == 0

    def flush_game(self):
        self.native_thread = True
        while self.game_queue:
            callback, data = self.game_queue.pop(0); self.call(callback, self.context, data)
        assert struct.unpack('<I', self.uc.mem_read(self.tls + 0x1958, 4))[0] == 0
        self.native_thread = False


HIGH = {k: 100 for k in ['act_boss_chance_percent', 'diablo_clone_chance_percent', 'uber_group_chance_percent', 'uber_tristram_share_percent', 'high_rune_bonus_chance_percent', 'annihilus_chance_percent', 'torch_chance_percent', 'ancient_reward_chance_percent']}
