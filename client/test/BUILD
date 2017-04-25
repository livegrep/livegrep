load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_test")

go_library(
    name = "go_default_library",
    srcs = ["testutil.go"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/proto/go_proto:go_default_library",
        "@org_golang_google_grpc//:go_default_library",
    ],
)

go_test(
    name = "go_default_test",
    srcs = [
        "bench_test.go",
        "integration_test.go",
    ],
    library = ":go_default_library",
    deps = [
        "//src/proto/go_proto:go_default_library",
        "@in_gopkg_check_v1//:go_default_library",
    ],
)
