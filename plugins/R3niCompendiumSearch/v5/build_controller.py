from pathlib import Path
import subprocess,sys,struct,shutil
R=Path('work/v5')
def run(*args):subprocess.run([sys.executable,'-m','ziglang',*map(str,args)],check=True)
run('c++','-target','x86_64-windows-gnu','-std=c++20','-O2','-fno-exceptions','-fno-rtti','-fno-builtin','-fno-stack-protector','-Iwork/compendium/sdk/include','-c',R/'Search.cpp','-o',R/'search.o')
def res(kind,name,data):return struct.pack('<IIHHHHIHHII',len(data),32,65535,kind,65535,name,0,0x30,0,0,0)+data+b'\0'*((-len(data))%4)
(R/'plugin.res').write_bytes(res(0,0,b'')+res(10,1001,struct.pack('<I',4)))
run('c++','-target','x86_64-windows-gnu','-shared','-nostdlib',R/'search.o',R/'plugin.res','-lkernel32','-Wl,-e,DllMain','-o',R/'Search.dll')
shutil.copy2(R/'Search.dll',R/'package/mods/VanillaDeluxe/d2rloader/plugins/R3niCompendiumSearch.dll')
