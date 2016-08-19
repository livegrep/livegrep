
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_library(
name = "go_default_library",
  srcs = [
    "doc.go",
    "libhoney.go",
    "response.go",
    "test_helpers.go",
    "transmission.go",
  ],
  deps = [
    "//vendor/github.com/facebookgo/muster:go_default_library",
    "//vendor/gopkg.in/alexcesaro/statsd.v2:go_default_library",
  ],
  visibility = ["//visibility:public"],
)


