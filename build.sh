#!/bin/sh

set -eux

rev=$(git rev-parse HEAD | head -c10)
builddir="livegrep-$rev"
tgz="${1-livegrep-$rev.tgz}"

rm -rf "$builddir"
mkdir -p "$builddir"

make -j4 LDFLAGS=-static all

cp -a codesearch analyze-re inspect-index "$builddir/"
go build -o "$builddir/livegrep" "github.com/nelhage/livegrep/livegrep"

cp -a web "$builddir/"

tar cvzf "$tgz" "$builddir/"
rm -rf "$builddir"
