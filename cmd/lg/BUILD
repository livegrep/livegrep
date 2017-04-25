load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

go_library(
    name = "go_default_library",
    srcs = ["main.go"],
    visibility = ["//visibility:private"],
    deps = [
        "//server/api:go_default_library",
        "@com_github_nelhage_go_cli//config:go_default_library",
    ],
)

go_binary(
    name = "lg",
    library = ":go_default_library",
    visibility = ["//visibility:public"],
)
