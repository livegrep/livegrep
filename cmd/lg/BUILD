
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_binary(
name = "lg",
  srcs = [
    "main.go",
  ],
  deps = [
    "//server/api:go_default_library",
    "//vendor/github.com/nelhage/go.cli/config:go_default_library",
  ],
  visibility = ["//visibility:public"],
)


