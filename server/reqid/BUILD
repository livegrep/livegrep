
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
    "@org_golang_x_net//context:go_default_library",
  ],
  visibility = ["//visibility:public"],
)
