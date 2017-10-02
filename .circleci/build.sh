#!/bin/bash
set -e

gofmt=$(bazel info output_base)/external/io_bazel_rules_go_toolchain/bin/gofmt
format_errors=$(find . -name '*.go' -print0 | xargs -0 "$gofmt" -l -e)
if [ "$format_errors" ]; then
    echo "=== misformatted files (run gofmt) ==="
    echo "$format_errors"
    exit 1
fi

bazel build //...
bazel test --test_arg=-test.v //...

# bazel-bin/client/test/go_default_test -test.repo "$(pwd)/deps/linux"
