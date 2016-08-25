
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_library(
name = "go_default_library",
  srcs = [
    "urlfetch.go",
  ],
  deps = [
    "@org_golang_x_net//:context",

    "//vendor/github.com/golang/protobuf/proto:go_default_library",
    "//vendor/google.golang.org/appengine/internal/urlfetch:go_default_library",
    "//vendor/google.golang.org/appengine/internal:go_default_library",
  ],
  visibility = ["//visibility:public"],
)
