#!/usr/bin/env bash
export GOCODE_GOPACKAGESDRIVER=true
exec bazel run -- @io_bazel_rules_go//go/tools/gopackagesdriver "${@}"
