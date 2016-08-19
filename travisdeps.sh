#!/bin/bash
set -ex

mkdir -p deps

if ! test -d deps/linux; then
    git clone --depth=1 --branch v3.17 https://github.com/torvalds/linux deps/linux
fi
