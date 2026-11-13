from pathlib import Path
import struct,subprocess,json,ziglang,sys
R=Path(__file__).resolve().parent;out=R/'built';out.mkdir(exist_ok=True)
def run(args):subprocess.run([sys.executable,'-m','ziglang',*args],check=True)
rt=out/'compiler_rt.lib'
if not rt.exists():run(['build-lib',str(Path(ziglang.__file__).parent/'lib/compiler_rt.zig'),'-target','x86_64-windows-gnu','-O','ReleaseFast','-femit-bin='+str(rt)])
def res_entry(kind,name,data):
 return struct.pack('<IIHHHHIHHII',len(data),32,65535,kind,65535,name,0,0x30,0,0,0)+data+b'\0'*((-len(data))%4)
for name in sys.argv[1:] or ['EnemySkills']:
 source_name='EnemySkills' if name=='EnemySkillsFixture' else 'DungeonLegendsExtra' if name=='DungeonLegendsExtraFixture' else name
 p=R/'base-1.2.4';raw=res_entry(0,0,b'')+res_entry(10,1001,struct.pack('<I',4))
 if source_name=='DungeonLegendsExtra':
  for i,n in enumerate(['monstats.tsv','skills.tsv','superuniques.tsv','treasureclassex.tsv','base_monstats.tsv','base_skills.tsv','base_superuniques.tsv','base_treasureclassex.tsv','strings.json']):raw+=res_entry(10,1100+i,(p/'resources'/n).read_bytes())
 res=out/(name+'.res');res.write_bytes(raw);extra=[]
 if name in ('EnemySkills','EnemySkillsFixture','DungeonLegendsExtra','DungeonLegendsExtraFixture'):
  for part in ['buffer','hook','trampoline','hde/hde64']:
   obj=out/(name+'-'+part.replace('/','-')+'.o')
   run(['cc','-target','x86_64-windows-gnu','-O2','-DNDEBUG','-fno-builtin','-fno-stack-protector','-I'+str(p/'third_party/minhook/include'),'-c',str(p/'third_party/minhook/src'/(part+'.c')),'-o',str(obj)]);extra.append(str(obj))
 obj=out/(name+'.obj');cpp='extra_fixture.cpp' if name=='DungeonLegendsExtraFixture' else 'bridge_fixture.cpp' if name=='EnemySkillsFixture' else 'DungeonLegends.cpp' if name=='DungeonLegendsExtra' else name+'.cpp'
 run(['c++','-target','x86_64-windows-gnu','-std=c++20','-O2','-g','-fno-exceptions','-fno-rtti','-fno-builtin','-fno-stack-protector','-I'+str(p/'third_party/plugin-sdk/include'),'-c',str(p/cpp),'-o',str(obj)])
 run(['c++','-target','x86_64-windows-gnu','-shared','-nostdlib',str(obj),str(res),*extra,str(rt),'-lkernel32','-Wl,-e,DllMain','-o',str(out/(name+'.dll'))])
 print('BUILT',name,(out/(name+'.dll')).stat().st_size,flush=True)
