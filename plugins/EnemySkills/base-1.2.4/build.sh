#!/usr/bin/env bash
set -euo pipefail
TERROR_COMPILER_DIR=${1:?usage: build.sh /path/to/llvm-mingw/bin}
TERROR_SOURCE_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TERROR_INCLUDE="$TERROR_SOURCE_DIR/third_party/plugin-sdk/include"
TERROR_BUILD_DIR="$TERROR_SOURCE_DIR/build"
mkdir -p "$TERROR_BUILD_DIR"
for TERROR_FILE in buffer hook trampoline hde/hde64; do
 "$TERROR_COMPILER_DIR/x86_64-w64-mingw32-clang" -O2 -DNDEBUG -fno-builtin -fno-stack-protector -I "$TERROR_SOURCE_DIR/third_party/minhook/include" -c "$TERROR_SOURCE_DIR/third_party/minhook/src/$TERROR_FILE.c" -o "$TERROR_BUILD_DIR/$(basename "$TERROR_FILE").o"
done
"$TERROR_COMPILER_DIR/x86_64-w64-mingw32-windres" -I "$TERROR_INCLUDE" "$TERROR_SOURCE_DIR/plugin.rc" -O coff -o "$TERROR_SOURCE_DIR/plugin.res"
TERROR_BUILTINS=$("$TERROR_COMPILER_DIR/x86_64-w64-mingw32-clang++" --print-libgcc-file-name)
"$TERROR_COMPILER_DIR/x86_64-w64-mingw32-clang++" -std=c++20 -O2 -Wall -Wextra -Werror -Wno-missing-designated-field-initializers -fno-exceptions -fno-rtti -fno-builtin -fno-stack-protector -shared -nostdlib -I "$TERROR_INCLUDE" "$TERROR_SOURCE_DIR/EnemySkills.cpp" "$TERROR_SOURCE_DIR/plugin.res" "$TERROR_BUILD_DIR/buffer.o" "$TERROR_BUILD_DIR/hook.o" "$TERROR_BUILD_DIR/trampoline.o" "$TERROR_BUILD_DIR/hde64.o" "$TERROR_BUILTINS" -lkernel32 -Wl,-e,DllMain -Wl,--no-insert-timestamp -o "$TERROR_SOURCE_DIR/../d2rloader/plugins/EnemySkills.dll"
