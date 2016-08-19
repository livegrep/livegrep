#!/bin/bash
set -e

bazel build //...
bazel test --test_arg=-test.v //...

bazel-bin/client/test/go_default_test -test.repo "$(pwd)/deps/linux"
