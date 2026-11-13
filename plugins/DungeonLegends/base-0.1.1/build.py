from pathlib import Path
import os,subprocess,sys,shutil
ROOT=Path(__file__).resolve().parents[1]
TC=Path(os.environ.get('R3NI_MINGW_ROOT',ROOT/'toolchain/root/usr'))
if not (TC/'bin/x86_64-w64-mingw32-g++-posix').exists():
 found=shutil.which('x86_64-w64-mingw32-g++-posix')
 if not found:raise SystemExit('Install mingw-w64 x86_64 POSIX GCC, or set R3NI_MINGW_ROOT to its usr directory.')
 TC=Path(found).resolve().parents[1]
PREFIX=TC/'bin/x86_64-w64-mingw32-'
GCC=Path(subprocess.check_output([str(PREFIX)+'gcc-posix','-print-libgcc-file-name'],text=True).strip()).parent
NATIVE=TC/'x86_64-w64-mingw32'
ENV=os.environ.copy();ENV['PATH']=str(TC/'bin')+os.pathsep+ENV.get('PATH','')
def run(args,cwd=None):subprocess.run([str(x) for x in args],cwd=cwd,env=ENV,check=True)
def build(name,source):
 source=Path(source); build=source/'build';build.mkdir(exist_ok=True); include=source/'third_party/plugin-sdk/include'
 obj=[]
 if name=='EnemySkills':
  for f in ['buffer','hook','trampoline','hde/hde64']:
   dst=build/(Path(f).name+'.o');run([str(PREFIX)+'gcc-posix','-O2','-DNDEBUG','-fno-builtin','-fno-stack-protector','-I',NATIVE/'include','-c',source/('third_party/minhook/src/'+f+'.c'),'-o',dst]);obj.append(dst)
 rc=source/'plugin.rc'
 if rc.exists():
  res=build/'plugin.res';run([str(PREFIX)+'windres','--preprocessor='+str(PREFIX)+'gcc-posix','--preprocessor-arg=-E','--preprocessor-arg=-xc','--preprocessor-arg=-DRC_INVOKED','-I',include,'-I',NATIVE/'include','-i',rc,'-O','coff','-o',res],cwd=source);obj.append(res)
 dest=ROOT/'package/mods/VanillaDeluxe/d2rloader/plugins'/f'{name}.dll';dest.parent.mkdir(parents=True,exist_ok=True)
 run([str(PREFIX)+'g++-posix','-std=c++20','-O2','-Wall','-Wextra','-Wno-misleading-indentation','-Wno-missing-field-initializers','-fno-exceptions','-fno-rtti','-fno-builtin','-fno-stack-protector','-shared','-nostdlib','-I',include,'-I',NATIVE/'include','-B',str(GCC)+'/',source/(name+'.cpp'),*obj,GCC/'libgcc.a','-L',NATIVE/'lib','-lkernel32','-Wl,-e,DllMain','-Wl,--no-insert-timestamp','-o',dest])
 print('BUILT',name,dest.stat().st_size)
if __name__=='__main__':
 names=sys.argv[1:] or ['DungeonLegends']
 for name in names:build(name,ROOT/'base-0.1.1' if name=='DungeonLegends' else ROOT/'compat'/name/'source')
