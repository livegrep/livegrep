load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "go_default_library",
    srcs = ["reqid.go"],
    importpath = "github.com/livegrep/livegrep/server/reqid",
    visibility = ["//visibility:public"],
    deps = ["@org_golang_x_net//context:go_default_library"],
)
