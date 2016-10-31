
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_binary(
name = "livegrep-github-reindex",
  srcs = [
    "flags.go",
    "main.go",
  ],
  deps = [
    "//vendor/github.com/google/go-github/github:go_default_library",

    "@org_golang_x_oauth2//:go_default_library",
    "@org_golang_x_net//context:go_default_library",
  ],
  visibility = ["//visibility:public"],
)
