load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

go_library(
    name = "go_default_library",
    srcs = ["main.go"],
    data = [
        "//src/tools:codesearch",
    ],
    importpath = "github.com/livegrep/livegrep/cmd/livegrep-fetch-reindex",
    visibility = ["//visibility:private"],
    deps = [
        "//src/proto:go_config_proto",
        "//src/proto:go_proto",
        "@org_golang_google_grpc//:go_default_library",
        "@org_golang_x_sync//errgroup:go_default_library",
    ],
)

go_binary(
    name = "livegrep-fetch-reindex",
    embed = [":go_default_library"],
    visibility = ["//visibility:public"],
)
