from pathlib import Path
import struct,subprocess,sys,json,ziglang
R=Path(__file__).resolve().parent;O=R/'built';O.mkdir(exist_ok=True)
def run(*a):subprocess.run([sys.executable,'-m','ziglang',*map(str,a)],check=True)
rt=O/'compiler_rt.lib';run('build-lib',Path(ziglang.__file__).parent/'lib/compiler_rt.zig','-target','x86_64-windows-gnu','-O','ReleaseFast','-femit-bin='+str(rt))
def resource(kind,name,data):return struct.pack('<IIHHHHIHHII',len(data),32,65535,kind,65535,name,0,0x30,0,0,0)+data+b'\0'*((-len(data))%4)
for source,name in [('spell-source','SpellProcs')]:
 S=R/source;obj=O/(name+'.obj');res=O/(name+'.res');dll=O/('R3ni'+name+'.dll')
 run('c++','-target','x86_64-windows-gnu','-std=c++20','-O2','-g','-fno-exceptions','-fno-rtti','-fno-builtin','-fno-stack-protector','-I'+str(S/'third_party/plugin-sdk/include'),'-c',S/(name+'.cpp'),'-o',obj)
 res.write_bytes(resource(0,0,b'')+resource(10,1001,struct.pack('<I',4)))
 run('c++','-target','x86_64-windows-gnu','-shared','-nostdlib',obj,res,rt,'-lkernel32','-Wl,-e,DllMain','-o',dll)
 print(json.dumps({'built':str(dll),'bytes':dll.stat().st_size}),flush=True)
