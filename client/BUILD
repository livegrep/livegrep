
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_library(
name = "go_default_library",
  srcs = [
    "client.go",
    "types.go",
  ],
  deps = [
    "//jsonframe:go_default_library",
  ],
  visibility = ["//visibility:public"],
)


go_test(
  name = "go_default_test",
  srcs = [
    "client_test.go",
  ],
  deps = [
    "//vendor/gopkg.in/check.v1:go_default_library",
  ],
  library = ":go_default_library",
)

