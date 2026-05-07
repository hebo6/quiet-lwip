#!/usr/bin/env bash
# 宿主机交叉编译 Windows x86_64 版 proxy_client.exe。
# 依赖（liquid-dsp / jansson / portaudio / quiet）通过 quiet-lwip/CMakeLists.txt
# 顶部的 add_subdirectory 链一次性联编，无需手动 install prefix。
#
# 前置：宿主机已安装 mingw-w64 工具链
#   pacman -S mingw-w64-gcc mingw-w64-winpthreads cmake ninja
#
# 产物：
#   quiet-lwip/build-win64/bin/proxy_client.exe
#   quiet-lwip/build-win64/dist/  （exe + 全部运行时 DLL + quiet-profiles.json）

set -euo pipefail

ROOT=/home/hebo/IdeaProjects/quiet-project
TC=$ROOT/cmake/toolchain-mingw64.cmake
BUILD=$ROOT/quiet-lwip/build-win64
JOBS=$(nproc)

command -v x86_64-w64-mingw32-gcc >/dev/null \
    || { echo "missing toolchain: x86_64-w64-mingw32-gcc not in PATH" >&2; exit 1; }
[ -f "$TC" ] || { echo "toolchain file not found: $TC" >&2; exit 1; }

stage() { printf '\n==== %s ====\n' "$*"; }

stage "configure"
cmake -S "$ROOT/quiet-lwip" -B "$BUILD" \
      -DCMAKE_TOOLCHAIN_FILE="$TC" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5

stage "build proxy_client / proxy_server"
cmake --build "$BUILD" -j "$JOBS" --target proxy_client proxy_server

stage "pack dist"
EXE=$BUILD/bin/proxy_client.exe
EXE_SRV=$BUILD/bin/proxy_server.exe
DIST=$BUILD/dist
mkdir -p "$DIST"
cp "$EXE" "$DIST/"
cp "$EXE_SRV" "$DIST/"
find "$BUILD/external" -type f -name '*.dll' -exec cp -v {} "$DIST/" \;

cp -v /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll "$DIST/"
cp -v /usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll  "$DIST/"

cp "$ROOT/quiet/quiet-profiles.json" "$DIST/quiet-profiles.json"

stage "verify"
for f in "$EXE" "$EXE_SRV"; do
    echo "-- $f --"
    x86_64-w64-mingw32-objdump -f "$f" | grep -E 'file format|architecture'
    x86_64-w64-mingw32-objdump -p "$f" | grep 'DLL Name'
    if x86_64-w64-mingw32-objdump -p "$f" | grep -qi 'msvcrt.dll'; then
        echo 'FAIL: msvcrt.dll detected (UCRT switch broken)' >&2
        exit 1
    fi
done
echo
ls -la "$DIST"
echo
echo "OK: $DIST"
