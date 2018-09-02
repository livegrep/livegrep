package(default_visibility = ["//visibility:public"])

load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cc_proto_library")

proto_library(
    name = "livegrep_proto",
    srcs = ["livegrep.proto"],
)

go_proto_library(
    name = "go_proto",
    compilers = ["@io_bazel_rules_go//proto:go_grpc"],
    importpath = "github.com/livegrep/livegrep/src/proto/go_proto",
    proto = ":livegrep_proto",
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "cc_proto",
    protos = ["livegrep.proto"],
    with_grpc = True,
)
