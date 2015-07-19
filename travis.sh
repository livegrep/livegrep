#!/bin/bash
set -ex

mkdir -p deps

if ! test -d deps/linux; then
    git clone --depth=1 --branch v3.17 https://github.com/torvalds/linux deps/linux
fi

if ! test -d deps/gflags; then
    git clone -b v2.1.2 --depth=1 https://github.com/gflags/gflags.git deps/gflags
    cd deps/gflags
    mkdir build
    cd build
    cmake ..
    make -j2
    cd ../../..
fi

if ! test -d deps/libgit2; then
    cd deps
    wget https://github.com/libgit2/libgit2/archive/v0.21.0.tar.gz
    tar xzf v0.21.0.tar.gz
    mv libgit2-0.21.0 libgit2
    cd libgit2
    mkdir build
    cd build
    cmake -DBUILD_SHARED_LIBS=false -DCMAKE_INSTALL_PREFIX=$(pwd) ..
    cmake --build . --target install
    cd ../../..
fi
