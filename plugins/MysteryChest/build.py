from pathlib import Path
import subprocess,sys,struct
r=Path(__file__).parent

def res(k,n,d):return struct.pack('<IIHHHHIHHII',len(d),32,65535,k,65535,n,0,0x30,0,0,0)+d
(r/'manifest.res').write_bytes(res(0,0,b'')+res(10,1001,struct.pack('<I',4)))
subprocess.run([sys.executable,'-m','ziglang','c++','-target','x86_64-windows-gnu','-std=c++20','-O2','-fno-exceptions','-fno-rtti','-shared','-I'+str(r/'SDK/include'),str(r/'MysteryChest.cpp'),str(r/'manifest.res'),'-lbcrypt','-o',str(r/'MysteryChest.dll')],check=True)
