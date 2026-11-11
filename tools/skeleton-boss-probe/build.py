import argparse
from pathlib import Path
import struct
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--sdk", required=True, type=Path, help="D2RLoader SDK include directory")
parser.add_argument("--out", required=True, type=Path, help="Output directory")
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)

def resource_entry(kind, identifier, data):
    header = struct.pack("<IIHHHHIHHII", len(data), 32, 65535, kind, 65535, identifier, 0, 0x30, 0, 0, 0)
    return header + data

resource = args.out / "SkeletonBossProbe.res"
resource.write_bytes(resource_entry(0, 0, b"") + resource_entry(10, 1001, struct.pack("<I", 4)))
subprocess.run([
    sys.executable, "-m", "ziglang", "c++", "-target", "x86_64-windows-gnu",
    "-std=c++20", "-O2", "-fno-exceptions", "-fno-rtti", "-shared",
    "-I", str(args.sdk), str(Path(__file__).with_name("SkeletonBossProbe.cpp")), str(resource),
    "-lkernel32", "-o", str(args.out / "SkeletonBossProbe.dll"),
], check=True)
