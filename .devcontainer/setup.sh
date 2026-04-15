#!/bin/bash
set -e

# 安装构建依赖
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    build-essential cmake git autoconf automake \
    libfec-dev libjansson-dev libsndfile1-dev portaudio19-dev

# 编译 liquid-dsp 1.7.0（与宿主机版本一致）
git clone --depth 1 --branch v1.7.0 https://github.com/jgaeddert/liquid-dsp.git /tmp/liquid-dsp
cd /tmp/liquid-dsp
./bootstrap.sh
./configure --prefix=/usr/local
make -j"$(nproc)"
sudo make install
sudo ldconfig
cd /workspaces/quiet-lwip

# 安装 libquiet（从挂载的本地构建产物）
sudo cp /quiet/build/lib/libquiet.so /quiet/build/lib/libquiet.a /usr/local/lib/
sudo cp /quiet/include/quiet.h /quiet/include/quiet-portaudio.h /usr/local/include/
sudo ldconfig

# 构建 pipe 版本（用独立目录避免与宿主机 build/ 冲突）
cmake -S . -B build-dev
cmake --build build-dev --target proxy_client_pipe proxy_server_pipe
