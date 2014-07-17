#!/bin/sh

set -eux

rev=$(git rev-parse HEAD | head -c10)
builddir="livegrep-$rev"
tgz="${1-livegrep-$rev.tgz}"

env GOPATH= make -j4 all

tar --xform s,^,"$builddir/",S -czf "$tgz" bin web
