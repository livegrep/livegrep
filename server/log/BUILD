load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "go_default_library",
    srcs = ["log.go"],
    importpath = "github.com/livegrep/livegrep/server/log",
    visibility = ["//visibility:public"],
    deps = [
        "//server/reqid:go_default_library",
        "@org_golang_x_net//context:go_default_library",
    ],
)
