#!/bin/sh

set -eux

rev=$(git rev-parse HEAD | head -c10)
builddir="livegrep-$rev"
tgz="${1-livegrep-$rev.tgz}"

make -j4 LDFLAGS=-static all

tar --xform s,^,"$builddir/",S -czf "$tgz" bin web
