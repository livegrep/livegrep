#!/bin/sh

set -eux

here="$(dirname "$0")"
cd "$here"


rm -rf re2
hg -R "$HOME/code/re2" archive -t tar -r tip - | tar x
mv re2-* re2
