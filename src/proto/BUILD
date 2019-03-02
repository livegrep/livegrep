package(default_visibility = ["//visibility:public"])

load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@build_stack_rules_proto//cpp:cpp_grpc_library.bzl", "cpp_grpc_library")

proto_library(
    name = "livegrep_proto",
    srcs = ["livegrep.proto"],
    deps = [":config_proto"],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
)

go_proto_library(
    name = "go_config_proto",
    importpath = "github.com/livegrep/livegrep/src/proto/config",
    proto = ":config_proto",
)

go_proto_library(
    name = "go_proto",
    compilers = ["@io_bazel_rules_go//proto:go_grpc"],
    importpath = "github.com/livegrep/livegrep/src/proto/go_proto",
    proto = ":livegrep_proto",
    visibility = ["//visibility:public"],
    deps = [":go_config_proto"],
)

cpp_grpc_library(
    name = "cc_proto",
    deps = [":livegrep_proto"],
)

cc_proto_library(
    name = "cc_config_proto",
    deps = [":config_proto"],
)
