from pathlib import Path
import struct,subprocess,sys,ziglang
root=Path(__file__).resolve().parent
out=root/'built';out.mkdir(exist_ok=True)
rt=out/'compiler_rt.lib'
if not rt.exists():
 subprocess.run([sys.executable,'-m','ziglang','build-lib',str(Path(ziglang.__file__).parent/'lib/compiler_rt.zig'),'-target','x86_64-windows-gnu','-O','ReleaseFast','-femit-bin='+str(rt)],check=True)
source=root/'source';deps=source/'third_party'
def entry(kind,name,data):return struct.pack('<IIHHHHIHHII',len(data),32,65535,kind,65535,name,0,0x30,0,0,0)+data+b'\0'*((-len(data))%4)
res=out/'plugin.res';res.write_bytes(struct.pack('<IIHHHHIHHII',0,32,65535,0,65535,0,0,0,0,0,0)+entry(10,1001,struct.pack('<I',4)))
objects=[]
for part in ['buffer','hook','trampoline','hde/hde64']:
 obj=out/(part.replace('/','-')+'.o')
 subprocess.run([sys.executable,'-m','ziglang','cc','-target','x86_64-windows-gnu','-O2','-DNDEBUG','-fno-builtin','-fno-stack-protector','-I'+str(deps/'minhook/include'),'-c',str(deps/'minhook/src'/(part+'.c')),'-o',str(obj)],check=True);objects.append(str(obj))
obj=out/'TerrorEncountersForsaken.obj'
subprocess.run([sys.executable,'-m','ziglang','c++','-target','x86_64-windows-gnu','-std=c++20','-O2','-g','-fno-exceptions','-fno-rtti','-fno-builtin','-fno-stack-protector','-I'+str(deps/'plugin-sdk/include'),'-I'+str(deps/'minhook/include'),'-c',str(source/'TerrorEncountersForsaken.cpp'),'-o',str(obj)],check=True)
subprocess.run([sys.executable,'-m','ziglang','c++','-target','x86_64-windows-gnu','-shared','-nostdlib',str(obj),str(res),*objects,str(rt),'-lkernel32','-Wl,--image-base,0x190000000','-Wl,-e,DllMain','-o',str(out/'TerrorEncountersForsaken.dll')],check=True)
print('Built',out/'TerrorEncountersForsaken.dll')
