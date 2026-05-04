#!/usr/bin/env bash
# 宿主机 Arch 原生编译 Linux x86_64 版 proxy_client。
# 依赖（liquid-dsp / jansson / portaudio / quiet）通过 quiet-lwip/CMakeLists.txt
# 顶部的 add_subdirectory 链一次性联编。portaudio 在 Linux 自动启用 ALSA 后端。
#
# 前置：宿主机已装 cmake、gcc、alsa-lib（头文件 /usr/include/alsa/asoundlib.h）
#
# 产物：
#   quiet-lwip/build-linux64/bin/proxy_client
#   quiet-lwip/build-linux64/dist/  （proxy_client + 全部 .so + quiet-profiles.json + run.sh）

set -euo pipefail

ROOT=/home/hebo/IdeaProjects/quiet-project
BUILD=$ROOT/quiet-lwip/build-linux64
JOBS=$(nproc)

stage() { printf '\n==== %s ====\n' "$*"; }

stage "configure"
cmake -S "$ROOT/quiet-lwip" -B "$BUILD" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5

stage "build proxy_client"
cmake --build "$BUILD" -j "$JOBS" --target proxy_client

stage "pack dist"
EXE=$BUILD/bin/proxy_client
DIST=$BUILD/dist
mkdir -p "$DIST/lib"
cp "$EXE" "$DIST/proxy_client"

# 收集 _deps 下所有 .so / .so.* 到 dist/lib
find "$BUILD/_deps" -type f \( -name '*.so' -o -name '*.so.*' \) -exec cp -P {} "$DIST/lib/" \;
find "$BUILD/_deps" -type l \( -name '*.so' -o -name '*.so.*' \) -exec cp -P {} "$DIST/lib/" \;

cp "$ROOT/org.quietmodem.Quiet/quiet/src/main/res/raw/quiet_profiles.json" \
   "$DIST/quiet-profiles.json"

cat > "$DIST/run.sh" <<'EOF'
#!/usr/bin/env bash
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
export LD_LIBRARY_PATH="$HERE/lib:${LD_LIBRARY_PATH:-}"
exec "$HERE/proxy_client" "$@"
EOF
chmod +x "$DIST/run.sh"

stage "verify"
readelf -h "$EXE" | grep -E 'Class|Machine|Type:'
echo
echo "-- ldd (LD_LIBRARY_PATH=$DIST/lib) --"
LD_LIBRARY_PATH="$DIST/lib" ldd "$EXE" || true
echo
ls -la "$DIST" "$DIST/lib"
echo
echo "OK: $DIST"
echo "运行： $DIST/run.sh <android-ip> --profile audible"
