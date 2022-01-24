#!/bin/bash
set -ex
mkdir -p builds
rev=$(git rev-parse HEAD | head -c10)
builddir="livegrep-$rev"
rm -rf "$builddir" && mkdir "$builddir"
bazel build :livegrep
tar -C "$builddir" -xf "$(bazel info bazel-bin)"/livegrep.tar
tar -czf "builds/$builddir.tgz" "$builddir"
rm -rf "$builddir"

# send the name of the built file, so that github actions can upload it
echo $builddir
