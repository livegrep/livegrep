load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

go_library(
    name = "go_default_library",
    srcs = [
        "flags.go",
        "main.go",
    ],
    importpath = "github.com/livegrep/livegrep/cmd/livegrep-github-reindex",
    visibility = ["//visibility:private"],
    deps = [
        "//src/proto:go_config_proto",
        "@com_github_google_go_github//github:go_default_library",
        "@org_golang_x_net//context:go_default_library",
        "@org_golang_x_oauth2//:go_default_library",
    ],
)

go_binary(
    name = "livegrep-github-reindex",
    embed = [":go_default_library"],
    visibility = ["//visibility:public"],
)
