#!/bin/sh

set -eux

here="$(dirname "$0")"
cd "$here"

rm -rf re2
curl -L https://github.com/google/re2/archive/master.tar.gz | tar xzv
mv re2-master re2
