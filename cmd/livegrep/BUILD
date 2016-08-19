
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_binary(
name = "livegrep",
  srcs = [
    "livegrep.go",
  ],
  deps = [
    "//server/config:go_default_library",
    "//server/middleware:go_default_library",
    "//server:go_default_library",
    "//vendor/github.com/honeycombio/libhoney-go:go_default_library",
  ],
  visibility = ["//visibility:public"],
)


