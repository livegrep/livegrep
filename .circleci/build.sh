#!/bin/bash
set -e

if [ "$GCLOUD_SERVICE_KEY" ]; then
    echo "$GCLOUD_SERVICE_KEY" | base64 --decode --ignore-garbage > ${HOME}/gcloud-service-key.json
    /usr/local/google-cloud-sdk/bin/gcloud auth activate-service-account --key-file ${HOME}/gcloud-service-key.json
    /usr/local/google-cloud-sdk/bin/gcloud config set project livegrep
fi

cat .bazelrc.circle >> .bazelrc
cat .bazelrc.ci >> .bazelrc

bazel fetch //cmd/...

gofmt=$(bazel info output_base)/external/go_sdk/bin/gofmt
format_errors=$(find . -name '*.go' -print0 | xargs -0 "$gofmt" -l -e)
if [ "$format_errors" ]; then
    echo "=== misformatted files (run gofmt) ==="
    echo "$format_errors"
    exit 1
fi

bazel test --test_arg=-test.v //...
bazel build //...

# bazel-bin/client/test/go_default_test -test.repo "$(pwd)/deps/linux"

if [ "$GCLOUD_SERVICE_KEY" ]; then
    ./package.sh
    /usr/local/google-cloud-sdk/bin/gsutil cp -a public-read -r builds/ gs://livegrep.appspot.com/
fi
