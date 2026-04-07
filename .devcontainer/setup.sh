#!/bin/bash
set -e

# 安装构建依赖
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    build-essential cmake \
    libliquid-dev libfec-dev libjansson-dev libsndfile1-dev portaudio19-dev

# 安装 libquiet（从挂载的本地构建产物）
sudo cp /quiet/build/lib/libquiet.so /quiet/build/lib/libquiet.a /usr/local/lib/
sudo cp /quiet/include/quiet.h /quiet/include/quiet-portaudio.h /usr/local/include/
sudo ldconfig

# 构建 pipe 版本（用独立目录避免与宿主机 build/ 冲突）
cmake -S . -B build-dev
cmake --build build-dev --target proxy_client_pipe proxy_server_pipe
