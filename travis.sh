#!/bin/bash
set -e

bazel build //...
bazel test //...

bazel-bin/client/test/go_default_test -test.repo "$(pwd)/deps/linux"
