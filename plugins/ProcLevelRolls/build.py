from pathlib import Path
import struct,subprocess,sys,ziglang
R=Path(__file__).resolve().parent;B=R/'built';B.mkdir(exist_ok=True)
def run(*args):subprocess.run([sys.executable,'-m','ziglang',*map(str,args)],check=True)
def resource(kind,name,data):return struct.pack('<IIHHHHIHHII',len(data),32,65535,kind,65535,name,0,0x30,0,0,0)+data+b'\0'*((-len(data))%4)
rt=B/'compiler_rt.lib'
if not rt.exists():run('build-lib',Path(ziglang.__file__).parent/'lib/compiler_rt.zig','-target','x86_64-windows-gnu','-O','ReleaseFast','-femit-bin='+str(rt))
res=B/'plugin.res';res.write_bytes(resource(0,0,b'')+resource(10,1001,struct.pack('<I',4)))
for name,S,file in [('ProcLevelRolls',R/'rolls','ProcLevelRolls.cpp')]:
    extra=[]
    if name=='ProcLevelRolls':
        for part in ['buffer','hook','trampoline','hde/hde64']:
            obj=B/(part.replace('/','-')+'.obj');run('cc','-target','x86_64-windows-gnu','-O2','-DNDEBUG','-fno-builtin','-fno-stack-protector','-I'+str(S/'third_party/minhook/include'),'-c',S/'third_party/minhook/src'/(part+'.c'),'-o',obj);extra.append(obj)
    obj=B/(name+'.obj');run('c++','-target','x86_64-windows-gnu','-std=c++20','-O2','-g','-fno-exceptions','-fno-rtti','-fno-builtin','-fno-stack-protector','-I'+str(S/'third_party/plugin-sdk/include'),'-c',S/file,'-o',obj)
    dll=B/f'{name}.dll';dll.parent.mkdir(parents=True,exist_ok=True)
    run('c++','-target','x86_64-windows-gnu','-shared','-nostdlib',obj,res,*extra,rt,'-lkernel32','-Wl,-e,DllMain','-Wl,--image-base,0x180000000','-o',dll)
    print('Built',name,dll.stat().st_size,flush=True)
