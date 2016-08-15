
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_library(
name = "go_default_library",
  srcs = [
    "reqid.go",
  ],
  deps = [
    "//vendor/golang.org/x/net/context:go_default_library",
  ],
  visibility = ["//visibility:public"],
)


