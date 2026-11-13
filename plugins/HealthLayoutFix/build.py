from pathlib import Path
import subprocess, sys, struct
ROOT=Path(__file__).resolve().parent
SDK=ROOT/'dependencies'
if not SDK.exists():SDK=ROOT.parent/'armageddon_probe_122/source/EnemySkills/third_party'
OUT=ROOT/'built';OUT.mkdir(exist_ok=True)
def run(args):subprocess.run([sys.executable,'-m','ziglang',*map(str,args)],check=True)
def resource(kind,name,data):
    return struct.pack('<IIHHHHIHHII',len(data),32,65535,kind,65535,name,0,0x30,0,0,0)+data+b'\0'*((-len(data))%4)
res=OUT/'HealthLayoutFix.res';res.write_bytes(resource(0,0,b'')+resource(10,1001,struct.pack('<I',4)))
objects=[]
for part in ['buffer','hook','trampoline','hde/hde64']:
    obj=OUT/(part.replace('/','_')+'.o')
    run(['cc','-target','x86_64-windows-gnu','-O2','-DNDEBUG','-I'+str(SDK/'minhook/include'),'-c',SDK/'minhook/src'/(part+'.c'),'-o',obj]);objects.append(obj)
for fixture in [False,True]:
    name='HealthLayoutFixture' if fixture else 'R3niMonsterHealthFix'
    args=['c++','-target','x86_64-windows-gnu','-std=c++20','-O2','-g','-fno-exceptions','-fno-rtti','-shared','-I'+str(SDK/'plugin-sdk/include'),'-I'+str(SDK/'minhook/include')]
    if fixture:args+=['-DHEALTH_LAYOUT_FIXTURE']
    run([*args,ROOT/'HealthLayoutFix.cpp',res,*objects,'-lkernel32','-o',OUT/(name+'.dll')])
    print('Built',name,flush=True)
