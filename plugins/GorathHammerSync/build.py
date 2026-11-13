from pathlib import Path
import struct, subprocess, sys
root=Path(__file__).resolve().parent
project=root.parent
sdk=root/'third_party/plugin-sdk/include'
if not sdk.exists():sdk=project/'kargor_full/source/source/DungeonLegends/third_party/plugin-sdk/plugin-sdk/include'
mh=root/'third_party/minhook'
if not mh.exists():mh=project/'kargor_full/source/source/DungeonLegends/third_party/minhook'
out=root/'built';out.mkdir(exist_ok=True)
def run(args):
    subprocess.run([sys.executable,'-m','ziglang',*map(str,args)],check=True)
objects=[]
for name in ['buffer','hook','trampoline','hde/hde64']:
    obj=out/(name.replace('/','_')+'.o')
    run(['cc','-target','x86_64-windows-gnu','-O2','-DNDEBUG','-c',mh/'src'/f'{name}.c','-o',obj])
    objects.append(obj)
def entry(k,i,d):return struct.pack('<IIHHHHIHHII',len(d),32,65535,k,65535,i,0,0x30,0,0,0)+d+b'\0'*((-len(d))%4)
res=out/'GorathHammerSync.res'
res.write_bytes(entry(0,0,b'')+entry(10,1001,struct.pack('<I',4)))
name='ProbeFixture' if '--fixture' in sys.argv else 'GorathHammerSync'
source='test_fixture.cpp' if '--fixture' in sys.argv else 'GorathHammerSync.cpp'
run(['c++','-target','x86_64-windows-gnu','-std=c++20','-O2','-g','-fno-exceptions','-fno-rtti',
     '-shared','-I'+str(sdk),'-I'+str(mh/'include'),root/source,*objects,res,
     '-lkernel32','-o',out/(name+'.dll')])
print('Built',out/(name+'.dll'))
