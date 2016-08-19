
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

go_library(
name = "go_default_library",
  srcs = [
    "datastore_v3.pb.go",
  ],
  deps = [
    "//vendor/github.com/golang/protobuf/proto:go_default_library",
  ],
  visibility = ["//visibility:public"],
)


