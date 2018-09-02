# gazelle:ignore
load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_test")

go_library(
    name = "go_default_library",
    srcs = ["testutil.go"],
    importpath = "github.com/livegrep/livegrep/client",
    visibility = ["//visibility:public"],
    deps = [
        "//src/proto:go_proto",
        "@org_golang_google_grpc//:go_default_library",
    ],
)

go_test(
    name = "go_default_test",
    srcs = [
        "bench_test.go",
        "integration_test.go",
    ],
    embed = [":go_default_library"],
    deps = [
        "//src/proto:go_proto",
        "@in_gopkg_check_v1//:go_default_library",
    ],
)
