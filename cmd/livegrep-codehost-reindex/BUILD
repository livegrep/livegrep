load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

go_library(
    name = "go_default_library",
    srcs = [
        "flags.go",
        "github.go",
        "main.go",
    ],
    importpath = "github.com/livegrep/livegrep/cmd/livegrep-codehost-reindex",
    visibility = ["//visibility:private"],
    deps = [
        "@com_github_google_go_github//github:go_default_library",
        "@org_golang_x_oauth2//:go_default_library",
    ],
)

go_binary(
    name = "livegrep-codehost-reindex",
    embed = [":go_default_library"],
    visibility = ["//visibility:public"],
)
