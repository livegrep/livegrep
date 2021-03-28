# gazelle:ignore
load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_test")

go_library(
    name = "go_default_library",
    srcs = [
        "api.go",
        "backend.go",
        "fileview.go",
        "json.go",
        "query.go",
        "server.go",
    ],
    data = [
        "//web:asset_hashes",
        "//web:htdocs",
        "//web:templates",
        "//web:webpack_build",
    ],
    importpath = "github.com/livegrep/livegrep/server",
    visibility = ["//visibility:public"],
    deps = [
        "//server/api:go_default_library",
        "//server/config:go_default_library",
        "//server/log:go_default_library",
        "//server/reqid:go_default_library",
        "//server/templates:go_default_library",
        "//src/proto:go_proto",
        "@com_github_bmizerany_pat//:go_default_library",
        "@com_github_honeycombio_libhoney_go//:go_default_library",
        "@org_golang_google_grpc//:go_default_library",
        "@org_golang_google_grpc//codes:go_default_library",
        "@org_golang_google_grpc//metadata:go_default_library",
        "@org_golang_x_net//context:go_default_library",
    ],
)

go_test(
    name = "go_default_test",
    srcs = [
        "query_test.go",
        "server_test.go",
    ],
    data = [
        "//web:htdocs",
        "//web:templates",
    ],
    embed = [":go_default_library"],
    deps = [
        "//src/proto:go_proto",
        "@io_bazel_rules_go//go/tools/bazel",
    ],
)
