
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
    "request_log.go",
    "server.go",
    "templates.go",
  ],
  deps = [
    "//client:go_default_library",
    "//server/api:go_default_library",
    "//server/config:go_default_library",
    "//server/log:go_default_library",
    "//server/reqid:go_default_library",
    "//server/templates:go_default_library",
    "//vendor/github.com/bmizerany/pat:go_default_library",
    "//vendor/github.com/honeycombio/libhoney-go:go_default_library",
    "//vendor/golang.org/x/net/context:go_default_library",
  ],
  visibility = ["//visibility:public"],
)


go_test(
  name = "go_default_test",
  srcs = [
    "backend_test.go",
    "query_test.go",
  ],
  deps = [
    "//client/test:go_default_library",
    "//client:go_default_library",
  ],
  library = ":go_default_library",
)

