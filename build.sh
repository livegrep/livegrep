#!/bin/sh

set -eux

rev=$(git rev-parse HEAD | head -c10)
builddir="livegrep-$rev"

rm -rf "$builddir"
mkdir -p "$builddir"

make -j4 LDFLAGS=-static

cp -a codesearch analyze-re inspect-index "$builddir/"
go build -o "$builddir/livegrep" "github.com/nelhage/livegrep/livegrep"

cp -a web "$builddir/"

tar cvzf "livegrep-$rev.tgz" "$builddir/"
rm -rf "$builddir"
