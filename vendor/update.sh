#!/bin/sh

set -eux

here="$(dirname "$0")"
cd "$here"

# We assume an re2 checkout parallel to livegrep

rm -rf re2
hg -R "$here/../../re2" archive -t tar -r tip - | tar x
mv re2-* re2
