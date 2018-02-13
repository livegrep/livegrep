load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_test")

go_library(
    name = "go_default_library",
    srcs = [
        "gitops.go",
        "indexer.go",
    ],
    importpath = "github.com/livegrep/livegrep/blameworthy",
    visibility = ["//visibility:public"],
)

go_test(
    name = "go_default_test",
    size = "small",
    srcs = [
        "gitops_test.go",
        "indexer_test.go",
    ],
    data = glob(["test_data/*"]),
    embed = [":go_default_library"],
    importpath = "github.com/livegrep/livegrep/blameworthy",
)
