load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

go_library(
    name = "go_default_library",
    srcs = ["livegrep.go"],
    importpath = "github.com/livegrep/livegrep/cmd/livegrep",
    visibility = ["//visibility:private"],
    deps = [
        "//server:go_default_library",
        "//server/config:go_default_library",
        "//server/middleware:go_default_library",
        "@com_github_honeycombio_libhoney_go//:go_default_library",
    ],
)

go_binary(
    name = "livegrep",
    embed = [":go_default_library"],
    visibility = ["//visibility:public"],
)
