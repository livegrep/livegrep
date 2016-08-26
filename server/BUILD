
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_library(
name = "go_default_library",
  srcs = [
    "api.go",
    "backend.go",
    "json.go",
    "query.go",
    "server.go",
    "templates.go",
  ],
  deps = [
    "//server/api:go_default_library",
    "//server/config:go_default_library",
    "//server/log:go_default_library",
    "//server/reqid:go_default_library",
    "//server/templates:go_default_library",
    "//src/proto:go_proto",
    "//vendor/github.com/bmizerany/pat:go_default_library",
    "//vendor/github.com/honeycombio/libhoney-go:go_default_library",
    "@org_golang_google_grpc//:go_default_library",
    "@org_golang_google_grpc//:metadata",
    "@org_golang_x_net//:context",
  ],
  visibility = ["//visibility:public"],
)


go_test(
  name = "go_default_test",
  srcs = [
    "query_test.go",
  ],
  deps = [
  ],
  library = ":go_default_library",
)
